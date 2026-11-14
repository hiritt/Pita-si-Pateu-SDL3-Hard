# SDL3 Hard Task - source review and simulated gameplay checks

Review date: 2026-11-14

## Checked

- ZIP contents and C++17 syntax checked (the syntax check used a lightweight SDL3-compatible stand-in, not SDL3 binaries).
- Confirmed the CMake download URL/version and SHA-256 refer to official SDL3 3.4.16 source release.
- Simulated and passed 14 scenario groups: initial positions and lane bounds; enemy firing delay; projectile speeds; Repulsor strength and one-shot-per-click behavior; left-button fire timing; R reset restriction; victory/freeze; resized lane positioning; defeat on projectile contact; defeat on enemy contact; +/-15-degree enemy volley; ship speed cap.
- Corrected a high-refresh-rate input issue: mouse movement is now buffered until a fixed-rate simulation update occurs.

## Still needs verification

1. **Real SDL3/Windows build and playtest:** A Visual Studio + SDL3 runtime was not available for this check. Compile and run the project on your Windows PC.
2. **Original scrolling landscape:** Only a video reference was provided, not the course template code. The project recreates the visual rather than using the exact template implementation.
3. **Repulsor interpretation:** Each right-click gives projectiles an outward *radial velocity change* of 10 pixels/sec. This pushes incoming shots away and can eventually reverse them, but it is not a guaranteed 10 pixels/sec reduction in total speed for shots travelling tangentially or outward.
4. **Victory/respawn interpretation:** The assignment calls for endless enemy respawns and also victory when all active enemies are destroyed. The game declares victory when the last active enemy is destroyed, rather than permitting another respawn.

These exceptions should be checked against your instructor's expected behavior before submission.

## Changes in this faster/no-pulse version

- Increased only the **background's** scroll speed from 130 to 170 pixels/s.
- Increased mouse input responsiveness by lowering the mouse movement rate at which acceleration hits its prescribed maximum, without increasing the maximum acceleration or top speed.
- Deleted the Repulsor flash state, timer, draw call, and blue ring drawing function entirely; repulsor collision/velocity logic is unchanged.
- All original numeric enemy, projectile, shooting, repulsor and maximum ship movement rules remain unchanged.
