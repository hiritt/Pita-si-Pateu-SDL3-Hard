# SDL3 Space Shooter 🚀

A simple 2D space shooter built with **C++17 and SDL3**. Move your ship with the mouse, shoot incoming enemies, and use the Repulsor to push enemy projectiles away.

The game runs in a resizable window (initially **700 × 800**) and does not require any external images or sounds.

## ▶️ How to download and run the game

### 1. Install Visual Studio

You need **Visual Studio 2022 or newer** on Windows, with the **Desktop development with C++** workload and its **CMake tools for Windows** component.

If Visual Studio is already installed, you can add these through **Visual Studio Installer → Modify**.

### 2. Download the project

1. On this GitHub repository page, click the green **Code** button.
2. Choose **Download ZIP**.
3. Extract the ZIP file somewhere on your computer (for example, inside `Documents`).

Alternatively, clone the repository with Git if you prefer.

### 3. Open the project in Visual Studio

1. Start Visual Studio.
2. Choose **File → Open → Folder...**.
3. Select the extracted project folder — the one containing **`CMakeLists.txt`** and **`main.cpp`**.
4. Let Visual Studio finish configuring CMake. On the first run, this can take a little longer.
5. In the startup-item dropdown near the green Play button, select **`SDL3_HardTask.exe`** (or `SDL3_HardTask`).
6. Press **Ctrl + F5** to run, or **F5** to run with the debugger.

**You do not need to install SDL3 manually.** The included CMake configuration looks for an existing SDL3 installation and, if none is found, automatically downloads **SDL3 3.4.16**. You need an internet connection for that first download.

> **Note:** Open the *folder*, not just `main.cpp`. This is a CMake project, so there is no `.sln` file to open.

## 🎮 Controls

| Input | Action |
| --- | --- |
| Move mouse | Move the green spaceship with inertia |
| Hold **left mouse button** | Continuously fire red projectiles |
| **Right-click** | Activate the Repulsor once per click |
| **R** | Restart **after** winning or losing |
| **Esc** | Quit the game |

While the game is running, the cursor is hidden and mouse movement is captured by the game window. The ship accelerates when you move the mouse and slows down when you stop.

The **Repulsor** changes the velocity of existing magenta enemy projectiles away from the player. It has **no blue visual effect** — right-click to use it, and release and click again to repeat.

## 🕹️ How to play

- You control the **green** ship near the bottom of the screen.
- **Orange** enemies descend from the top in three lanes and fire **magenta** projectiles.
- Shoot enemies with your **red** projectiles while avoiding enemies and their shots.
- Destroy the **last active enemy** to win: your ship turns **blue**.
- If an enemy or enemy projectile touches your ship, you lose: your ship turns **red**.
- Once the game ends, press **R** to play again.

Enemy lanes can spawn replacements every three seconds while the game continues. Enemies that leave the bottom of the screen can be replaced; escaping does not count as destroying them.

## 📁 Project files

| File | Purpose |
| --- | --- |
| `main.cpp` | Game logic, drawing, input, collisions and game loop |
| `CMakeLists.txt` | Builds the game and obtains SDL3 if needed |
| `README.md` | Setup and gameplay instructions (this file) |
| `QA_REPORT.md` | Notes about code checks and testing |
| `.gitignore` | Excludes generated build files from Git, if present |

## 🛠️ Troubleshooting

**“Please select a valid startup item” / the Play button has no game listed**

- Let CMake finish configuring first.
- Choose **`SDL3_HardTask.exe`** from the startup-item dropdown.
- If it still is not listed, use **Project → Delete Cache and Reconfigure** and check **View → Output → CMake** for errors.

**CMake reports that its cache refers to a different folder**

This can happen if you move or rename the project. Close Visual Studio, delete the generated **`out`** folder inside the project, reopen the project folder and let CMake configure again. Do **not** delete `main.cpp` or `CMakeLists.txt`.

**SDL3 download or CMake configuration fails**

Make sure your internet connection works and GitHub downloads are not blocked. Then use **Project → Delete Cache and Reconfigure**. If SDL3 is already installed, CMake can also use that installation.

**The mouse cursor disappears**

This is expected while playing: the game captures relative mouse movement. Press **Esc** to close the game.

## ⚙️ Technical details

- Language: **C++17**
- Graphics/input: **SDL3**
- Build system: **CMake 3.24+**
- Default window size: **700 × 800**, resizable
- Player speed limit: **500 px/s**
- Player shots: **every 0.3 seconds**, moving at **500 px/s**
- Enemy projectiles: **100 px/s** in a three-shot spread
- Background: built-in scrolling landscape inspired by the assignment demo; no external assets required

The source is intended to run for extended periods without accumulating unlimited off-screen projectiles. The game uses a fixed timestep for gameplay updates.
