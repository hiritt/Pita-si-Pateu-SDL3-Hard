// SDL3 Hard Task - complete single-file 2D game.
// C++17. Open the accompanying CMakeLists.txt as a folder in Visual Studio.

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <random>
#include <vector>

namespace {

constexpr int INITIAL_WIDTH = 700;
constexpr int INITIAL_HEIGHT = 800;
constexpr int ENEMY_LANES = 3;             // Three lanes, matching the demo.
constexpr float SHIP_SIZE = 50.0f;
constexpr float HALF_SHIP = SHIP_SIZE / 2.0f;
constexpr float BULLET_RADIUS = 10.0f;

constexpr float MOUSE_ACCELERATION = 400.0f;   // pixels / second^2
constexpr float MOUSE_BRAKING = 800.0f;        // 2 * acceleration
constexpr float MAX_SHIP_SPEED = 500.0f;      // pixels / second
constexpr float PLAYER_BULLET_SPEED = 500.0f;
constexpr float ENEMY_BULLET_SPEED = PLAYER_BULLET_SPEED / 5.0f;
constexpr float PLAYER_FIRE_INTERVAL = 0.3f;
constexpr float ENEMY_FIRE_INTERVAL = PLAYER_FIRE_INTERVAL * 10.0f;
constexpr float ENEMY_FIRST_SHOT_DELAY = 1.0f;
constexpr float RESPAWN_INTERVAL = 3.0f;
constexpr float REPULSOR_PUSH = 10.0f;       // outward radial velocity change per click
constexpr float FAN_ANGLE = 15.0f * 3.14159265358979323846f / 180.0f;
constexpr float FIXED_DT = 1.0f / 120.0f;

// Slightly quicker visual pace. These aren't constrained by the assignment.
constexpr float BACKGROUND_SCROLL_SPEED = 170.0f; // previously 130 px/s
// Reach the assignment's 400 px/s^2 acceleration with a smaller mouse motion.
// The acceleration and ship top speed themselves are still capped as specified.
constexpr float MOUSE_FULL_ACCELERATION_AT_PX_PER_SEC = 250.0f; // previously 400


// Finite lifetime keeps the game stable even if repulsor clicks stop a bullet.
constexpr float BULLET_MAX_AGE = 30.0f;
constexpr std::size_t MAX_PLAYER_BULLETS = 256;
constexpr std::size_t MAX_ENEMY_BULLETS = 1024;

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;
};

Vec2 operator+(Vec2 a, Vec2 b) { return { a.x + b.x, a.y + b.y }; }
Vec2 operator-(Vec2 a, Vec2 b) { return { a.x - b.x, a.y - b.y }; }
Vec2 operator*(Vec2 a, float factor) { return { a.x * factor, a.y * factor }; }
Vec2& operator+=(Vec2& a, Vec2 b) { a.x += b.x; a.y += b.y; return a; }

float squaredLength(Vec2 v) { return v.x * v.x + v.y * v.y; }
float length(Vec2 v) { return std::sqrt(squaredLength(v)); }

Vec2 unitOr(Vec2 v, Vec2 fallback = { 0.0f, 1.0f }) {
    const float magnitude = length(v);
    return (magnitude > 0.00001f) ? v * (1.0f / magnitude) : fallback;
}

Vec2 clampSpeed(Vec2 velocity, float maxSpeed) {
    const float speed = length(velocity);
    return (speed > maxSpeed) ? velocity * (maxSpeed / speed) : velocity;
}

Vec2 rotateVector(Vec2 v, float angle) {
    const float c = std::cos(angle);
    const float s = std::sin(angle);
    return { v.x * c - v.y * s, v.x * s + v.y * c };
}

bool rectanglesOverlap(Vec2 a, Vec2 b) {
    return std::abs(a.x - b.x) < SHIP_SIZE &&
           std::abs(a.y - b.y) < SHIP_SIZE;
}

bool circleHitsShip(Vec2 circle, Vec2 shipCenter) {
    // Closest point on a 50x50 square to the center of the circle.
    const float closestX = std::clamp(circle.x, shipCenter.x - HALF_SHIP,
                                     shipCenter.x + HALF_SHIP);
    const float closestY = std::clamp(circle.y, shipCenter.y - HALF_SHIP,
                                     shipCenter.y + HALF_SHIP);
    const float dx = circle.x - closestX;
    const float dy = circle.y - closestY;
    return dx * dx + dy * dy <= BULLET_RADIUS * BULLET_RADIUS;
}

enum class Outcome { Playing, Victory, Defeat };

struct Ship {
    Vec2 position{};
    Vec2 velocity{};
};

struct Enemy {
    bool active = false;
    Vec2 position{};
    float laneFraction = 0.5f;  // X position relative to its current lane.
    float fallSpeed = 0.0f;
    float aliveSeconds = 0.0f;
    float nextFireTime = ENEMY_FIRST_SHOT_DELAY;
};

struct Bullet {
    Vec2 position{};
    Vec2 velocity{};
    float age = 0.0f;
    bool active = true;
};

class Game {
public:
    Game(SDL_Window* window, SDL_Renderer* renderer)
        : window_(window), renderer_(renderer), random_(std::random_device{}()) {
        SDL_GetWindowSize(window_, &windowWidth_, &windowHeight_);
        reset();
    }

    void handleEvent(const SDL_Event& event, Vec2& frameMouseMovement, bool& quit) {
        if (event.type == SDL_EVENT_QUIT) {
            quit = true;
            return;
        }

        if (event.type == SDL_EVENT_WINDOW_RESIZED ||
            event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
            SDL_GetWindowSize(window_, &windowWidth_, &windowHeight_);
            constrainPlayer();
            updateEnemyLanePositions();
            return;
        }

        if (event.type == SDL_EVENT_WINDOW_FOCUS_LOST) {
            firing_ = false;  // Avoid a stuck mouse button after Alt+Tab.
            rightMouseHeld_ = false;
            return;
        }

        if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat) {
            if (event.key.key == SDLK_ESCAPE) {
                quit = true;
            } else if (event.key.key == SDLK_R && outcome_ != Outcome::Playing) {
                // Reset is deliberately forbidden during play.
                reset();
            }
            return;
        }

        if (outcome_ != Outcome::Playing) {
            return;
        }

        if (event.type == SDL_EVENT_MOUSE_MOTION) {
            frameMouseMovement.x += event.motion.xrel;
            frameMouseMovement.y += event.motion.yrel;
        } else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
            if (event.button.button == SDL_BUTTON_LEFT && !firing_) {
                firing_ = true;
                shootPlayer();               // Shoot immediately on the first click.
                nextPlayerShotIn_ = PLAYER_FIRE_INTERVAL;
            } else if (event.button.button == SDL_BUTTON_RIGHT && !rightMouseHeld_) {
                rightMouseHeld_ = true;
                useRepulsor();               // Exactly once per right button press.
            }
        } else if (event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
            if (event.button.button == SDL_BUTTON_LEFT) {
                firing_ = false;
            } else if (event.button.button == SDL_BUTTON_RIGHT) {
                rightMouseHeld_ = false;
            }
        }
    }

    void update(float dt, Vec2 mouseAcceleration, bool mouseMoved) {
        if (outcome_ != Outcome::Playing) {
            return;                         // Freeze every animation after game over.
        }

        // Mouse movement changes velocity rather than teleporting the ship.
        if (mouseMoved) {
            player_.velocity += mouseAcceleration * dt;
        } else {
            const float speed = length(player_.velocity);
            const float newSpeed = std::max(0.0f, speed - MOUSE_BRAKING * dt);
            player_.velocity = (speed > 0.0f)
                ? player_.velocity * (newSpeed / speed)
                : Vec2{};
        }
        player_.velocity = clampSpeed(player_.velocity, MAX_SHIP_SPEED);
        player_.position += player_.velocity * dt;
        constrainPlayer();

        backgroundOffset_ = std::fmod(backgroundOffset_ + BACKGROUND_SCROLL_SPEED * dt, 40.0f);

        if (firing_) {
            nextPlayerShotIn_ -= dt;
            while (nextPlayerShotIn_ <= 0.0f) {
                shootPlayer();
                nextPlayerShotIn_ += PLAYER_FIRE_INTERVAL;
            }
        }

        for (int lane = 0; lane < ENEMY_LANES; ++lane) {
            Enemy& enemy = enemies_[lane];
            if (!enemy.active) continue;

            enemy.position.y += enemy.fallSpeed * dt;
            enemy.aliveSeconds += dt;

            // Starting 1 second after entering, fire a 3-projectile fan
            // every 3 seconds, aimed at the player's current position.
            while (enemy.aliveSeconds >= enemy.nextFireTime) {
                shootEnemy(enemy);
                enemy.nextFireTime += ENEMY_FIRE_INTERVAL;
            }

            // The enemy has left the scene: free its lane for a respawn.
            if (enemy.position.y - HALF_SHIP > static_cast<float>(windowHeight_)) {
                enemy.active = false;
            }
        }

        for (Bullet& bullet : playerBullets_) {
            bullet.position += bullet.velocity * dt;
            bullet.age += dt;
        }
        for (Bullet& bullet : enemyBullets_) {
            bullet.position += bullet.velocity * dt;
            bullet.age += dt;
        }

        // Enemy ships or enemy projectiles touching the player cause a loss.
        // Loss is prioritized in the rare case of simultaneous collisions.
        for (const Enemy& enemy : enemies_) {
            if (enemy.active && rectanglesOverlap(player_.position, enemy.position)) {
                endGame(Outcome::Defeat);
                return;
            }
        }
        for (const Bullet& bullet : enemyBullets_) {
            if (bullet.active && circleHitsShip(bullet.position, player_.position)) {
                endGame(Outcome::Defeat);
                return;
            }
        }

        bool destroyedEnemyThisFrame = false;
        for (Bullet& bullet : playerBullets_) {
            if (!bullet.active) continue;
            for (Enemy& enemy : enemies_) {
                if (enemy.active && circleHitsShip(bullet.position, enemy.position)) {
                    bullet.active = false;
                    enemy.active = false;
                    destroyedEnemyThisFrame = true;
                    break;                 // One bullet can destroy only one enemy.
                }
            }
        }

        eraseExpiredBullets();

        // Victory occurs as soon as the *last active* enemy is shot down,
        // before a later 3-second respawn opportunity can occur.
        if (destroyedEnemyThisFrame && noEnemiesActive()) {
            endGame(Outcome::Victory);
            return;
        }

        respawnClock_ += dt;
        while (respawnClock_ >= RESPAWN_INTERVAL) {
            respawnClock_ -= RESPAWN_INTERVAL;
            for (int lane = 0; lane < ENEMY_LANES; ++lane) {
                if (!enemies_[lane].active) {
                    spawnEnemy(lane);
                }
            }
        }
    }

    void render() const {
        drawScrollingBackground();

        if (outcome_ == Outcome::Playing) {
            for (const Enemy& enemy : enemies_) {
                if (enemy.active) {
                    setColor(255, 150, 0);  // Orange enemies.
                    drawShip(enemy.position);
                }
            }

            setColor(255, 0, 0);             // Red player bullets.
            for (const Bullet& bullet : playerBullets_) {
                if (bullet.active) drawFilledCircle(bullet.position);
            }

            setColor(245, 0, 225);           // Magenta enemy bullets.
            for (const Bullet& bullet : enemyBullets_) {
                if (bullet.active) drawFilledCircle(bullet.position);
            }
        }

        if (outcome_ == Outcome::Victory) {
            setColor(0, 40, 255);            // Blue = victory.
        } else if (outcome_ == Outcome::Defeat) {
            setColor(255, 0, 0);             // Red = defeat.
        } else {
            setColor(0, 230, 0);             // Green = playing.
        }
        drawShip(player_.position);
        SDL_RenderPresent(renderer_);
    }

private:
    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    int windowWidth_ = INITIAL_WIDTH;
    int windowHeight_ = INITIAL_HEIGHT;
    std::mt19937 random_;
    Ship player_{};
    std::array<Enemy, ENEMY_LANES> enemies_{};
    std::vector<Bullet> playerBullets_{};
    std::vector<Bullet> enemyBullets_{};
    Outcome outcome_ = Outcome::Playing;
    bool firing_ = false;
    bool rightMouseHeld_ = false;
    float nextPlayerShotIn_ = PLAYER_FIRE_INTERVAL;
    float respawnClock_ = 0.0f;
    float backgroundOffset_ = 0.0f;

    void reset() {
        SDL_GetWindowSize(window_, &windowWidth_, &windowHeight_);
        outcome_ = Outcome::Playing;
        player_ = { { windowWidth_ * 0.5f, windowHeight_ - HALF_SHIP }, {} };
        enemies_ = {};
        playerBullets_.clear();
        enemyBullets_.clear();
        firing_ = false;
        rightMouseHeld_ = false;
        nextPlayerShotIn_ = PLAYER_FIRE_INTERVAL;
        respawnClock_ = 0.0f;
        backgroundOffset_ = 0.0f;

        for (int lane = 0; lane < ENEMY_LANES; ++lane) {
            spawnEnemy(lane, true);
        }

        SDL_SetWindowTitle(window_, "SDL3 Hard Task | LMB: Fire | RMB: Repulsor | Esc: Exit");
        if (!SDL_SetWindowRelativeMouseMode(window_, true)) {
            std::fprintf(stderr, "Relative mouse mode: %s\n", SDL_GetError());
        }
    }

    void spawnEnemy(int lane, bool initiallyVisible = false) {
        const float laneWidth = static_cast<float>(windowWidth_) / ENEMY_LANES;
        const float laneLeft = lane * laneWidth;
        const float centerMin = laneLeft + HALF_SHIP;
        const float centerMax = laneLeft + laneWidth - HALF_SHIP;
        std::uniform_real_distribution<float> xRandom(centerMin, centerMax);
        std::uniform_real_distribution<float> speedRandom(50.0f, 150.0f);

        Enemy& enemy = enemies_[lane];
        enemy = {};
        enemy.active = true;
        // At reset all three enemies are visible in the upper edge of the scene.
        // Respawns enter from just above the screen as before.
        enemy.position = { xRandom(random_), initiallyVisible ? HALF_SHIP : -HALF_SHIP };
        enemy.laneFraction = (enemy.position.x - laneLeft) / laneWidth;
        enemy.fallSpeed = speedRandom(random_);
        enemy.nextFireTime = ENEMY_FIRST_SHOT_DELAY;
    }

    void updateEnemyLanePositions() {
        const float laneWidth = static_cast<float>(windowWidth_) / ENEMY_LANES;
        for (int lane = 0; lane < ENEMY_LANES; ++lane) {
            Enemy& enemy = enemies_[lane];
            if (!enemy.active) continue;
            const float left = lane * laneWidth;
            enemy.position.x = std::clamp(left + enemy.laneFraction * laneWidth,
                                          left + HALF_SHIP,
                                          left + laneWidth - HALF_SHIP);
        }
    }

    void constrainPlayer() {
        const float minX = HALF_SHIP;
        const float minY = HALF_SHIP;
        const float maxX = std::max(minX, windowWidth_ - HALF_SHIP);
        const float maxY = std::max(minY, windowHeight_ - HALF_SHIP);
        const float oldX = player_.position.x;
        const float oldY = player_.position.y;
        player_.position.x = std::clamp(oldX, minX, maxX);
        player_.position.y = std::clamp(oldY, minY, maxY);
        if (player_.position.x != oldX) player_.velocity.x = 0.0f;
        if (player_.position.y != oldY) player_.velocity.y = 0.0f;
    }

    void shootPlayer() {
        if (playerBullets_.size() < MAX_PLAYER_BULLETS) {
            playerBullets_.push_back({ player_.position, { 0.0f, -PLAYER_BULLET_SPEED } });
        }
    }

    void shootEnemy(const Enemy& enemy) {
        // Target is captured ONCE when this volley is fired, not homing.
        const Vec2 aim = unitOr(player_.position - enemy.position);
        const std::array<float, 3> angles = { 0.0f, FAN_ANGLE, -FAN_ANGLE };
        for (float angle : angles) {
            if (enemyBullets_.size() >= MAX_ENEMY_BULLETS) return;
            enemyBullets_.push_back({ enemy.position,
                                      rotateVector(aim, angle) * ENEMY_BULLET_SPEED });
        }
    }

    void useRepulsor() {
        // Apply the Repulsor only once per distinct right-button press.
        // No blue pulse or other visual effect is rendered.
        for (Bullet& bullet : enemyBullets_) {
            // Split velocity into radial (toward/away from player) and
            // tangential components. Reduce inward radial velocity by 10 px/s;
            // repeated presses eventually make that radial part point outward.
            // This equals a 10 px/s velocity impulse away from the ship,
            // irrespective of the bullet's current direction.
            const Vec2 away = unitOr(bullet.position - player_.position,
                                     unitOr(bullet.velocity * -1.0f));
            const float radialSpeed = bullet.velocity.x * away.x +
                                      bullet.velocity.y * away.y;
            const Vec2 tangential = bullet.velocity - away * radialSpeed;
            bullet.velocity = tangential + away * (radialSpeed + REPULSOR_PUSH);
        }
    }

    void eraseExpiredBullets() {
        const float left = -BULLET_RADIUS;
        const float top = -BULLET_RADIUS;
        const float right = windowWidth_ + BULLET_RADIUS;
        const float bottom = windowHeight_ + BULLET_RADIUS;
        auto removeBullet = [=](const Bullet& b) {
            return !b.active || b.age > BULLET_MAX_AGE ||
                   b.position.x < left || b.position.x > right ||
                   b.position.y < top || b.position.y > bottom;
        };
        playerBullets_.erase(std::remove_if(playerBullets_.begin(), playerBullets_.end(),
                                            removeBullet), playerBullets_.end());
        enemyBullets_.erase(std::remove_if(enemyBullets_.begin(), enemyBullets_.end(),
                                           removeBullet), enemyBullets_.end());
    }

    bool noEnemiesActive() const {
        return std::none_of(enemies_.begin(), enemies_.end(),
                            [](const Enemy& e) { return e.active; });
    }

    void endGame(Outcome result) {
        outcome_ = result;
        player_.velocity = {};
        for (Enemy& enemy : enemies_) enemy.active = false;
        playerBullets_.clear();
        enemyBullets_.clear();
        firing_ = false;
        rightMouseHeld_ = false;
        SDL_SetWindowRelativeMouseMode(window_, false);
        SDL_SetWindowTitle(window_, result == Outcome::Victory
            ? "VICTORY! | Press R to restart, Esc to exit"
            : "DEFEAT! | Press R to restart, Esc to exit");
    }

    void setColor(Uint8 r, Uint8 g, Uint8 b) const {
        SDL_SetRenderDrawColor(renderer_, r, g, b, 255);
    }

    void drawShip(Vec2 center) const {
        const SDL_FRect box{ center.x - HALF_SHIP, center.y - HALF_SHIP,
                             SHIP_SIZE, SHIP_SIZE };
        SDL_RenderFillRect(renderer_, &box);
    }

    void drawFilledCircle(Vec2 center) const {
        // Draw a filled circle without SDL_ttf or SDL_image dependencies.
        const int radius = static_cast<int>(BULLET_RADIUS);
        for (int dy = -radius; dy <= radius; ++dy) {
            const float halfWidth = std::sqrt(BULLET_RADIUS * BULLET_RADIUS -
                                              static_cast<float>(dy * dy));
            SDL_RenderLine(renderer_, center.x - halfWidth, center.y + dy,
                            center.x + halfWidth, center.y + dy);
        }
    }

    void drawScrollingBackground() const {
        // Matches the supplied video's white vertical-scrolling landscape
        // with repeated pale-grey outlined side markers. The course template
        // source itself was not supplied, so this is a standalone recreation.
        setColor(255, 255, 255);
        SDL_RenderClear(renderer_);

        constexpr float TILE_PERIOD = 40.0f;
        constexpr float TILE_WIDTH = 19.0f;
        constexpr float TILE_HEIGHT = 20.0f;
        for (float y = backgroundOffset_ - TILE_PERIOD;
             y < static_cast<float>(windowHeight_); y += TILE_PERIOD) {
            const SDL_FRect left{ 0.0f, y, TILE_WIDTH, TILE_HEIGHT };
            const SDL_FRect right{ static_cast<float>(windowWidth_) - TILE_WIDTH,
                                   y, TILE_WIDTH, TILE_HEIGHT };
            setColor(236, 236, 236);
            SDL_RenderFillRect(renderer_, &left);
            SDL_RenderFillRect(renderer_, &right);
            setColor(171, 171, 171);
            SDL_RenderRect(renderer_, &left);
            SDL_RenderRect(renderer_, &right);
        }
    }

};

} // namespace

int main(int, char**) {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow("SDL3 Hard Task",
                                          INITIAL_WIDTH, INITIAL_HEIGHT,
                                          SDL_WINDOW_RESIZABLE);
    if (!window) {
        std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    // Each lane must remain at least 50 pixels wide, even after resizing.
    SDL_SetWindowMinimumSize(window, ENEMY_LANES * static_cast<int>(SHIP_SIZE),
                              static_cast<int>(SHIP_SIZE));

    SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);
    if (!renderer) {
        std::fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    SDL_SetRenderVSync(renderer, 1);

    {
        Game game(window, renderer);
        bool quit = false;
        Uint64 lastTicks = SDL_GetTicksNS();
        double accumulator = 0.0;
        // Keep mouse input until a physics step actually consumes it. Without
        // buffering, high refresh rates can drop mouse events on frames that
        // do not yet contain a 120 Hz simulation step.
        Vec2 bufferedMouseMovement{};
        double bufferedMouseSeconds = 0.0;

        while (!quit) {
            const Uint64 now = SDL_GetTicksNS();
            const double elapsedSeconds = std::clamp(
                (now - lastTicks) / 1'000'000'000.0, 0.0, 0.25);
            lastTicks = now;
            accumulator += elapsedSeconds;

            Vec2 frameMouseMovement{};
            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                game.handleEvent(event, frameMouseMovement, quit);
            }
            if (quit) break;

            // Preserve input across render frames until simulation advances.
            // E.g., a 240 Hz display may render a frame without a 120 Hz
            // physics step; its mouse motion must not be discarded.
            bufferedMouseMovement += frameMouseMovement;
            bufferedMouseSeconds += elapsedSeconds;

            if (accumulator >= FIXED_DT) {
                // Mouse motion is converted to capped acceleration. Use the
                // full elapsed input interval to avoid refresh-rate effects.
                const float motion = length(bufferedMouseMovement);
                const bool mouseMoved = motion > 0.0001f;
                const float pixelsPerSecond = static_cast<float>(
                    motion / std::max(bufferedMouseSeconds, 0.001));
                const float strength = MOUSE_ACCELERATION *
                    std::min(1.0f, pixelsPerSecond / MOUSE_FULL_ACCELERATION_AT_PX_PER_SEC);
                const Vec2 mouseAcceleration = mouseMoved
                    ? unitOr(bufferedMouseMovement) * strength : Vec2{};

                // 120 Hz fixed simulation: movement and fire rates do not
                // depend on the refresh rate of the display.
                while (accumulator >= FIXED_DT) {
                    game.update(FIXED_DT, mouseAcceleration, mouseMoved);
                    accumulator -= FIXED_DT;
                }
                bufferedMouseMovement = {};
                bufferedMouseSeconds = 0.0;
            }
            game.render();
            SDL_Delay(1);
        }
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
