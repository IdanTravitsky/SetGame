# SET Game Solver

## Overview

SET Game Solver is an interactive desktop application that helps players analyze and play the classic card game SET. The game challenges players to find "sets" of cards based on four attributes: shape, color, number, and shading.

![image](https://github.com/user-attachments/assets/a49b6452-105b-4ccf-ac58-260215266903)


## Features

- Interactive game board with visual card representations
- Real-time game statistics tracking
- Find all possible SETs on the current board
- Hint system to help players identify potential sets
- Edit mode for manual card manipulation
- Add additional cards to the board
- Time tracking and set counting

## Game Rules

In the SET game, a "set" is defined as three cards where each attribute (shape, color, number, shading) is either:
- All the same across the three cards, or
- All different across the three cards

## Prerequisites

- C++ compiler with C++11 support
- OpenGL 3.0+
- GLFW 3.x
- Dear ImGui
- stb_image library

## Dependencies

- [GLFW](https://www.glfw.org/)
- [Dear ImGui](https://github.com/ocornut/imgui)
- [stb_image](https://github.com/nothings/stb)

## Build Instructions

### Prerequisites
1. Install required libraries:
   - GLFW
   - ImGui
   - OpenGL development libraries

### Compilation (Example for Linux/macOS)
```bash
# Clone the repository
git clone https://github.com/yourusername/set-game-solver.git
cd set-game-solver

# Compile (adjust based on your specific setup)
g++ -std=c++11 set_game.cpp -o set_game \
    -lglfw -lGL -limgui -lstb_image
```

### Windows
- Use Visual Studio or MinGW
- Ensure all dependencies are properly linked
- You may need to adjust include and library paths

## Usage

### Running the Game
```bash
./set_game
```

### Game Controls
- **Find All SETs**: Highlight all possible sets on the board
- **New Game**: Start a fresh game
- **Hint**: Show a potential set
- **Deal 3 More Cards**: Add three more cards to the board
- **Edit Mode**: Manually modify card attributes

## Card Attributes
- **Shapes**: Diamond, Oval, Squiggle
- **Colors**: Purple, Green, Red
- **Numbers**: One, Two, Three
- **Shading**: Solid, Striped, Outline

## Troubleshooting

- Ensure `cards.png` texture file is in the same directory as the executable
- Check that all library dependencies are correctly installed
- Verify OpenGL and graphics drivers are up to date

## Acknowledgments

- [Dear ImGui](https://github.com/ocornut/imgui)
- [GLFW](https://www.glfw.org/)
- [stb_image](https://github.com/nothings/stb)
