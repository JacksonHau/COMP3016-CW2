# COMP3016-CW2

## Youtube Link
Link here

## Gameplay description
The prototype is a first-person exploration game set in an outdoor environment with terrain, scattered props, and dynamic lighting.
The player can freely move, look around using the mouse, run, jump, and interact with the environment using a toggleable flashlight.

Core focus:
- Immersion through lighting and atmosphere
- Clear player feedback via sound and visuals
- Smooth first-person controls
- The experience is inspired by modern indie horror and exploration games where environmental awareness matters.

## Dependencies used
The project is written in C++ with OpenGL, using the following libraries:
- GLFW – window creation and input handling.
- GLAD – OpenGL function loading.
- GLM – vector and matrix maths.
- stb_image – texture loading.
- Assimp – 3D model importing.
- winmm (PlaySound) – basic audio playback on Windows.

All dependencies are statically linked or bundled so the final executable runs without Visual Studio 2022.

## Use of AI
AI tools were used as a development assistant, not as an automated code generator.

Used for:
- Debugging OpenGL errors
- Explaining lighting math and camera logic
- UML design interpretation
- Refactoring advice

Not used for:
- Copy pasting full systems blindly
- Auto-generating assets
- Replacing understanding

All final code was written, tested, and integrated manually.

## Game programming patterns
Patterns used:
- Game Loop Pattern: Update → Input → Physics → Render → Repeat
- Component-like Architecture: Systems such as rendering, input, lighting, and audio are logically separated
- State-Based Logic: Flashlight on or off
- Day or night lighting: Mouse locked or unlocked
- Data-Driven Rendering: Scene instances store transform data while meshes remain reusable

## Game mechanics and how they are coded
Game mec

## UML design diagram
![Image Alt](https://github.com/JacksonHau/COMP3016-CW2/blob/main/UML/System%20UML.png)

## Sample screens
Screenshots

## Exception handling and test cases
Handling

## How my prototype works
Prototype

## What you I have achieved, and what I would do differently, knowing what you now know. 
What I achieved
