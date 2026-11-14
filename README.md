# SDL3 Hard Task — Visual Studio C++ project

This is a complete, asset-free **SDL3/C++17** version of the assignment.
The built-in white scrolling background and side markers recreate the supplied
reference video (the original landscape template source was not supplied). If you have your course template, you can use its original
background code instead (see `drawScrollingBackground()` in `main.cpp`).

## Run in Visual Studio 2022 (recommended)

1. Install **Visual Studio 2022** with the **Desktop development with C++** workload (CMake support included).
2. Unzip this folder to any writable location on your PC.
3. In Visual Studio choose **File > Open > Folder...**, then select the unzipped `SDL3_HardTask_VisualStudio` folder (which contains `CMakeLists.txt`).
4. Allow CMake to configure. **The first configuration needs internet access** if SDL3 is not already installed; CMake fetches SDL3 release 3.4.16 automatically.
5. Select the `SDL3_HardTask` startup item and press **F5** or **Ctrl+F5**.

This is a CMake project, so you do **not** need to manually make a `.sln` or configure Include/Library paths.

If your school requires a traditional `.sln` project instead, create an **Empty C++ Project**, add `main.cpp`, then install/configure SDL3 separately (headers, SDL3 library, SDL3.dll). CMake is simpler.

## Pace changes in this version

- The scrolling landscape moves at **170 px/s**, up from **130 px/s** (about 31% faster visually).
- Mouse movement reaches its full allowed acceleration sooner (at **250** instead of **400** relative mouse pixels/second), so the ship feels more responsive without exceeding the required **400 px/s²** acceleration or **500 px/s** speed limit.
- The Repulsor still pushes enemy projectiles by **10 px/s per right click**, but **draws no blue pulse/rings/lines**. The blue ship on victory is retained.
- Enemy speed (random 50–150 px/s), projectile speeds, and firing intervals are **unchanged** to preserve the assignment rules.

## Controls

- **Move mouse:** inertial ship movement (mouse is hidden and captured during play)
- **Hold left mouse:** fire red projectiles every 0.3 seconds (first shot immediately)
- **Right click:** repulsor changes every magenta projectile’s radial velocity by 10 px/s away from the ship per click (no visual pulse)
- **R:** reset, but **only after victory or defeat**
- **Esc:** quit

## Rules implemented

- Resizable window, initially 700 x 800; player and enemies are center-placed 50 x 50 rectangles.
- Green ship starts at `(windowWidth / 2, windowHeight - 25)` and stays inside the window.
- Three enemy lanes; one orange enemy per lane maximum, with a random horizontal position and 50–150 px/s descent speed. Starting enemies are visible at the upper edge; replacements enter from just above it.
- Every 3 seconds, empty lanes get a new enemy.
- Mouse acceleration up to 400 px/s², speed capped at 500 px/s; no motion applies 800 px/s² braking. Relative mouse input is buffered between fixed simulation steps so high-refresh displays do not lose movements.
- Player shoots red radius-10 circles upwards at 500 px/s, once per 0.3 seconds while held.
- Enemies start firing 1 second after spawning; they fire a trio every 3 seconds, aimed at the player's position at the firing instant, rotated by 0°, +15°, and -15°, at 100 px/s.
- Right-click repulsor increases the outward radial component of each existing enemy shot’s velocity by 10 px/s (equivalently, decreases its inward radial component by 10 px/s). Repeated clicks can reverse an approaching projectile; no blue rings or lines are drawn. This does not guarantee a 10 px/s reduction in the total speed of a sideways-moving projectile.
- Player shots destroy enemies on impact. Enemy ships or shots touching player are a loss.
- Victory is triggered **when the last active enemy is destroyed**; enemies that escape off the bottom are not counted as kills and may respawn.
- At game over all gameplay animations freeze, enemies and bullets vanish, and the player turns blue (win) or red (loss). The window title shows the result and `R` restart hint.
- Projectiles are cleaned up when offscreen or older than 30 seconds; vectors have emergency caps, so an endlessly running game does not accumulate unbounded bullets.

## Clarification about the specification

The assignment simultaneously requires **indefinite enemy respawning** and a
**victory when all enemies are destroyed**. This implementation makes these rules
compatible by checking for victory **immediately after killing the last active
enemy**, before any future periodic respawn. If one of the enemies leaves the
screen without being shot, its lane becomes available for later respawning.

## How to use the course template's scrolling landscape

Replace the body of `Game::drawScrollingBackground()` with your provided
template's background drawing logic, and update its scrolling offset *only* in
`Game::update()`, which already returns immediately after victory/defeat.
The original template source was not included in the uploaded materials,
so the background here is a faithful standalone approximation rather than a
byte-for-byte copy of the course template.

## SDL3 API references

- https://wiki.libsdl.org/SDL3/SDL_SetWindowRelativeMouseMode
- https://wiki.libsdl.org/SDL3/SDL_MouseMotionEvent
- https://wiki.libsdl.org/SDL3/README-cmake