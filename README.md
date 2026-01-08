# COMP3016-CW2

## Youtube Link
https://youtu.be/mOGbZOc-TKM

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
- irrKlang – Audio playback

All dependencies are statically linked or bundled so the final executable runs without Visual Studio 2022.

## Use of AI
AI tools were used as development support, not as a replacement for implementation. AI assisted with:
- Explaining OpenGL concepts and rendering pipelines
- Debugging shader and logic errors
- Suggesting improvements for structure and performance
- Helping design UML diagrams and documentation

AI declaration form:
https://github.com/JacksonHau/COMP3016-CW2/blob/main/AI%20declaration%20form/Student%20Declaration%20of%20AI%20Tool%20use%20in%20this%20Assessment.pdf

## Game programming patterns
Patterns used:
- Game Loop Pattern: Update → Input → Physics → Render → Repeat
- Component-like Architecture: Systems such as rendering, input, lighting, and audio are logically separated
- State-Based Logic: Flashlight on or off
- Day or night lighting: Mouse locked or unlocked
- Data-Driven Rendering: Scene instances store transform data while meshes remain reusable

## Game mechanics and how they are coded
Player Movement:
- WASD controls
- Mouse controls yaw and pitch
- Gravity and jumping handled using vertical velocity
- Speed modifiers for walking and running

Collision System:
- Radius-based collision checks
- Prevents player from clipping through trees and rocks
- World bounds clamp player inside the map

Flashlight:
- Toggled using a key press
- Uses a spotlight-style light source attached to the camera
- Plays sound effects via irrKlang when toggled

Day and Night Cycle:
- Time-based directional light rotation
- Changes sky colour and light intensity
- Switches ambient audio state

## UML design diagram
![Image Alt](https://github.com/JacksonHau/COMP3016-CW2/blob/main/UML/System%20UML.png)

## Sample screens
Day Environment
![Image Alt](https://github.com/JacksonHau/COMP3016-CW2/blob/main/Screenshot%20evidence/Daytime%20environment.png)
Day with flashlight
![Image Alt](https://github.com/JacksonHau/COMP3016-CW2/blob/main/Screenshot%20evidence/Daytime%20with%20flashlight.png)
Night Environment
![Image Alt](https://github.com/JacksonHau/COMP3016-CW2/blob/main/Screenshot%20evidence/Night%20environment.png)
Night with flashlight
![Image Alt](https://github.com/JacksonHau/COMP3016-CW2/blob/main/Screenshot%20evidence/Night%20environment%20with%20flashlight.png)
Collisions with an object
![Image Alt](https://github.com/JacksonHau/COMP3016-CW2/blob/main/Screenshot%20evidence/Collisions.png)

## Exception handling and test cases
Exception Handling:
- Shader compilation errors are checked and logged
- Asset loading failures are reported via console output
- Input bounds prevent camera flipping or instability

Test Cases:
- Player cannot pass through environmental objects
- Flashlight toggles reliably on repeated presses
- Player remains grounded on uneven terrain
- Day–night cycle transitions smoothly without visual glitches

## How my prototype works
When the application starts, the window and OpenGL context are initialised. Assets such as shaders, models, and textures are loaded before entering the main game loop.

Each frame:
1. User input is processed
2. Player movement and collision are updated
3. Environmental systems such as lighting are recalculated
4. The scene is rendered
5. Audio states are updated

This loop continues until the application is closed.

## What you I have achieved, and what I would do differently, knowing what you now know. 
Achievements
- Fully functional 3D first-person prototype
- Terrain generation with collision-aware movement
- Dynamic lighting and day–night system
- Flashlight mechanic with audio feedback
- Clear and structured game loop

What I Would Do Differently
- Split the project into multiple source files instead of a single main file
- Add a basic UI or instruction screen for first-time players
- Improve collision accuracy using bounding boxes instead of radius checks
- Add more gameplay depth such as objectives or NPC interactions

## Assets used
Tree:
https://sketchfab.com/3d-models/pine-tree-e52769d653cd4e52a4acff3041961e65

Rock:
https://sketchfab.com/3d-models/stylized-rocks-9473fba51b57482983e12ad64db9f471

Flashlight:
https://sketchfab.com/3d-models/flashlight-5fa9a65e7b0141ee877ed18f4f42d953

Grass: 
https://opengameart.org/content/grass-texture

Flashlight on/off:
https://pixabay.com/sound-effects/flashlight-clicking-on-105809/
