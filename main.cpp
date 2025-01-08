#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>
#include <vector>
#include <array>
#include <random>
#include <algorithm>
#include <cstdint>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <chrono>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

struct Card {
    int shape;      // 0: diamond, 1: oval, 2: squiggle
    int color;      // 0: purple, 1: green, 2: red
    int number;     // 0: one, 1: two, 2: three
    int shading;    // 0: solid, 1: striped, 2: outline

    bool operator==(const Card& other) const {
        return shape == other.shape && color == other.color &&
            number == other.number && shading == other.shading;
    }
};

struct GameStats {
    int setsFound;
    std::chrono::steady_clock::time_point gameStart;
    int hintsUsed;
    int cardsDealt;
    
    GameStats() : setsFound(0), hintsUsed(0), cardsDealt(12) {
        gameStart = std::chrono::steady_clock::now();
    }

    std::string getElapsedTime() const {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - gameStart);
        int minutes = elapsed.count() / 60;
        int seconds = elapsed.count() % 60;
        std::stringstream ss;
        ss << std::setfill('0') << std::setw(2) << minutes << ":" 
           << std::setfill('0') << std::setw(2) << seconds;
        return ss.str();
    }
};

class TextureManager {
private:
    GLuint textureID;
    std::vector<ImVec2> cardUVs;

public:
    TextureManager() : textureID(0) {
        const float cardsPerRow = 9;
        const float cardsPerCol = 9;
        const float uvWidth = 1.0f / cardsPerRow;
        const float uvHeight = 1.0f / cardsPerCol;

        for (int row = 0; row < cardsPerCol; row++) {
            for (int col = 0; col < cardsPerRow; col++) {
                ImVec2 topLeft(col * uvWidth, row * uvHeight);
                ImVec2 bottomRight((col + 1) * uvWidth, (row + 1) * uvHeight);
                cardUVs.push_back(topLeft);
                cardUVs.push_back(bottomRight);
            }
        }
    }

    bool loadTexture(const char* filename) {
        if (!filename) {
            std::cerr << "No filename provided for texture loading" << std::endl;
            return false;
        }

        int width, height, channels;
        stbi_set_flip_vertically_on_load(true);
        unsigned char* data = stbi_load(filename, &width, &height, &channels, STBI_rgb_alpha);

        if (!data) {
            std::cerr << "Failed to load texture: " << filename << std::endl;
            std::cerr << "STB Error: " << stbi_failure_reason() << std::endl;
            return false;
        }

        glGenTextures(1, &textureID);
        if (textureID == 0) {
            std::cerr << "Failed to generate texture" << std::endl;
            stbi_image_free(data);
            return false;
        }

        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

        stbi_image_free(data);

        GLenum error = glGetError();
        if (error != GL_NO_ERROR) {
            std::cerr << "OpenGL error after texture creation: " << error << std::endl;
            return false;
        }

        std::cout << "Successfully loaded texture: " << filename << std::endl;
        std::cout << "Dimensions: " << width << "x" << height << " channels: " << channels << std::endl;
        return true;
    }

    void renderCard(const Card& card, const std::vector<ImVec4>& highlights, bool* clicked = nullptr) {
        if (textureID == 0) {
            std::cerr << "Attempting to render with invalid texture" << std::endl;
            return;
        }

        int index = card.color * 27 + card.shading * 9 + card.shape * 3 + card.number;
        if (index * 2 + 1 >= cardUVs.size()) {
            std::cerr << "Invalid card index: " << index << std::endl;
            return;
        }

        ImVec2 topLeft = cardUVs[index * 2];
        ImVec2 bottomRight = cardUVs[index * 2 + 1];
        ImVec2 size(93 * 1.5f, 53 * 1.5f);

        ImGui::BeginGroup();
        ImGui::Image((ImTextureID)(uint64_t)textureID, size, topLeft, bottomRight);
        
        if (clicked) {
            *clicked = ImGui::IsItemClicked();
        }

        if (!highlights.empty()) {
            ImVec2 min = ImGui::GetItemRectMin();
            ImVec2 max = ImGui::GetItemRectMax();
            
            if (highlights.size() == 1) {
                ImGui::GetWindowDrawList()->AddRectFilled(
                    min, max,
                    ImGui::GetColorU32(highlights[0])
                );
            } else {
                float width = (max.x - min.x) / highlights.size();
                for (size_t i = 0; i < highlights.size(); i++) {
                    ImVec2 rectMin(min.x + width * i, min.y);
                    ImVec2 rectMax(min.x + width * (i + 1), max.y);
                    ImGui::GetWindowDrawList()->AddRectFilled(
                        rectMin, rectMax,
                        ImGui::GetColorU32(highlights[i])
                    );
                }
            }
        }

        ImGui::EndGroup();
    }

    ~TextureManager() {
        if (textureID != 0) {
            glDeleteTextures(1, &textureID);
        }
    }
};

class SetGame {
private:
    std::vector<Card> board;
    std::vector<Card> deck;
    bool editMode;
    GameStats stats;
    bool showHint;
    std::array<int, 3> hintSet;

    bool isSetPrivate(const Card& c1, const Card& c2, const Card& c3) const {
        bool shapeValid = ((c1.shape + c2.shape + c3.shape) % 3 == 0);
        bool colorValid = ((c1.color + c2.color + c3.color) % 3 == 0);
        bool numberValid = ((c1.number + c2.number + c3.number) % 3 == 0);
        bool shadingValid = ((c1.shading + c2.shading + c3.shading) % 3 == 0);
        return shapeValid && colorValid && numberValid && shadingValid;
    }

public:
    SetGame() : editMode(false), showHint(false) {
        initializeDeck();
        dealCards(12);
        hintSet = {-1, -1, -1};
    }

    void toggleEditMode() { editMode = !editMode; }
    bool isEditMode() const { return editMode; }

    bool isSet(const Card& c1, const Card& c2, const Card& c3) const {
        return isSetPrivate(c1, c2, c3);
    }

    void setCard(size_t index, const Card& card) {
        if (index < board.size()) {
            board[index] = card;
        }
    }

    Card& getCardAt(size_t index) {
        static Card dummy{ 0,0,0,0 };
        if (index < board.size()) {
            return board[index];
        }
        return dummy;
    }

    void initializeDeck() {
        deck.clear();
        for (int color = 0; color < 3; color++) {
            for (int shading = 0; shading < 3; shading++) {
                for (int shape = 0; shape < 3; shape++) {
                    for (int number = 0; number < 3; number++) {
                        deck.push_back({ shape, color, number, shading });
                    }
                }
            }
        }
        std::random_device rd;
        std::mt19937 gen(rd());
        std::shuffle(deck.begin(), deck.end(), gen);
    }

    void dealCards(int count) {
        for (int i = 0; i < count && !deck.empty(); i++) {
            board.push_back(deck.back());
            deck.pop_back();
            stats.cardsDealt++;
        }
    }

    std::vector<std::array<int, 3>> findAllSets() {
        std::vector<std::array<int, 3>> sets;
        for (size_t i = 0; i < board.size(); i++) {
            for (size_t j = i + 1; j < board.size(); j++) {
                for (size_t k = j + 1; k < board.size(); k++) {
                    if (isSetPrivate(board[i], board[j], board[k])) {
                        sets.push_back({ static_cast<int>(i),
                                       static_cast<int>(j),
                                       static_cast<int>(k) });
                    }
                }
            }
        }
        return sets;
    }

    const std::vector<Card>& getBoard() const { return board; }
    
    size_t getDeckSize() const { return deck.size(); }
    
    const GameStats& getStats() const { return stats; }

    void removeSet(const std::array<int, 3>& indices) {
        std::vector<Card> newBoard;
        for (size_t i = 0; i < board.size(); i++) {
            if (std::find(indices.begin(), indices.end(), i) == indices.end()) {
                newBoard.push_back(board[i]);
            }
        }
        board = newBoard;
        dealCards(3);
        stats.setsFound++;
        showHint = false;  // Clear hint when a set is removed
    }

    void toggleHint() {
        if (!showHint) {
            auto sets = findAllSets();
            if (!sets.empty()) {
                hintSet = sets[0];
                stats.hintsUsed++;
            }
        }
        showHint = !showHint;
    }

    bool isHintCard(int index) const {
        return showHint && std::find(hintSet.begin(), hintSet.end(), index) != hintSet.end();
    }

    void addThreeCards() {
        if (deck.size() >= 3) {
            dealCards(3);
        }
    }
};

int main() {
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    GLFWwindow* window = glfwCreateWindow(1280, 720, "SET Game Solver", NULL, NULL);
    if (!window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    TextureManager textureManager;
    if (!textureManager.loadTexture("cards.png")) {
        std::cerr << "Failed to load card textures" << std::endl;
        glfwTerminate();
        return -1;
    }

    SetGame game;
    std::vector<std::array<int, 3>> currentSets;
    int selectedCardIndex = -1;
    bool editMode = false;
    std::vector<int> selectedCards;

    // Define highlight colors for different sets
    const std::vector<ImVec4> setColors = {
        ImVec4(0.0f, 1.0f, 0.0f, 0.3f),  // Green
        ImVec4(1.0f, 0.0f, 0.0f, 0.3f),  // Red
        ImVec4(0.0f, 0.0f, 1.0f, 0.3f),  // Blue
        ImVec4(1.0f, 1.0f, 0.0f, 0.3f),  // Yellow
        ImVec4(1.0f, 0.0f, 1.0f, 0.3f),  // Magenta
        ImVec4(0.0f, 1.0f, 1.0f, 0.3f),  // Cyan
    };

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowSize(ImVec2(1000, 800), ImGuiCond_FirstUseEver);
        ImGui::Begin("SET Game Solver");

        // Add stats display
        const auto& stats = game.getStats();
        ImGui::Text("Time: %s", stats.getElapsedTime().c_str());
        ImGui::Text("Sets Found: %d", stats.setsFound);
        ImGui::Text("Cards Dealt: %d", stats.cardsDealt);
        ImGui::Text("Hints Used: %d", stats.hintsUsed);

        if (ImGui::Button("Find All SETs")) {
            currentSets = game.findAllSets();
        }

        ImGui::SameLine();
        if (ImGui::Button("New Game")) {
            game = SetGame();
            currentSets.clear();
            selectedCardIndex = -1;
            selectedCards.clear();
        }

        ImGui::SameLine();
        if (ImGui::Button("Hint")) {
            game.toggleHint();
        }

        ImGui::SameLine();
        if (ImGui::Button("Deal 3 More Cards")) {
            game.addThreeCards();
            currentSets.clear(); // Reset found sets when dealing new cards
        }

        ImGui::SameLine();
        ImGui::Checkbox("Edit Mode", &editMode);

        ImGui::Separator();

        // Display deck size
        ImGui::Text("Cards in Deck: %zu", game.getDeckSize());
        ImGui::Text("Sets on Board: %zu", currentSets.size());

        // Handle manual set selection
        if (!editMode) {
            ImGui::Text("Click cards to select a set manually");
        }

        const auto& board = game.getBoard();
        float cardWidth = 93 * 1.5f;
        float cardHeight = 53 * 1.5f;
        float padding = 20;

        // Render board
        for (size_t i = 0; i < board.size(); i++) {
            if (i % 3 != 0) ImGui::SameLine(i % 3 * (cardWidth + padding));
            if (i % 3 == 0 && i != 0) ImGui::Dummy(ImVec2(0.0f, padding));

            std::vector<ImVec4> highlights;
            
            // Add selection highlight
            bool isSelected = std::find(selectedCards.begin(), selectedCards.end(), i) != selectedCards.end();
            if (isSelected) {
                highlights.push_back(ImVec4(1.0f, 1.0f, 1.0f, 0.3f));
            }

            // Add set highlights
            for (size_t setIdx = 0; setIdx < currentSets.size(); setIdx++) {
                const auto& set = currentSets[setIdx];
                if (std::find(set.begin(), set.end(), i) != set.end()) {
                    highlights.push_back(setColors[setIdx % setColors.size()]);
                }
            }

            // Add hint highlight
            if (game.isHintCard(i)) {
                highlights.push_back(ImVec4(1.0f, 1.0f, 0.0f, 0.3f));
            }

            bool clicked = false;
            textureManager.renderCard(board[i], highlights, &clicked);

            if (clicked) {
                if (editMode) {
                    selectedCardIndex = i;
                } else {
                    // Handle set selection
                    auto it = std::find(selectedCards.begin(), selectedCards.end(), i);
                    if (it != selectedCards.end()) {
                        selectedCards.erase(it);
                    } else if (selectedCards.size() < 3) {
                        selectedCards.push_back(i);
                    }
                }
            }
        }

        // Check selected cards for a set
        if (selectedCards.size() == 3) {
            std::array<int, 3> selectedSet = {selectedCards[0], selectedCards[1], selectedCards[2]};
            const auto& board = game.getBoard();
            if (game.isSet(board[selectedSet[0]], board[selectedSet[1]], board[selectedSet[2]])) {
                game.removeSet(selectedSet);
                currentSets.clear();
                selectedCards.clear();
            }
            // Automatically clear invalid selections after a brief delay
            static auto lastInvalidSelection = std::chrono::steady_clock::now();
            if (std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - lastInvalidSelection).count() > 1000) {
                selectedCards.clear();
            }
            lastInvalidSelection = std::chrono::steady_clock::now();
        }

        // Card editor popup
        if (editMode && selectedCardIndex >= 0) {
            ImGui::OpenPopup("Edit Card");
        }

        if (ImGui::BeginPopup("Edit Card")) {
            Card& card = game.getCardAt(selectedCardIndex);
            bool modified = false;

            modified |= ImGui::Combo("Shape", &card.shape, "Diamond\0Oval\0Squiggle\0");
            modified |= ImGui::Combo("Color", &card.color, "Purple\0Green\0Red\0");
            modified |= ImGui::Combo("Number", &card.number, "One\0Two\0Three\0");
            modified |= ImGui::Combo("Shading", &card.shading, "Solid\0Striped\0Outline\0");

            if (ImGui::Button("Close")) {
                selectedCardIndex = -1;
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }

        ImGui::End();

        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.45f, 0.55f, 0.60f, 1.00f);
        glClear(GL_COLOR_BUFFER_BIT);

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}