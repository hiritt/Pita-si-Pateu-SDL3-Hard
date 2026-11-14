#include <iostream>
#include <vector>
#include <array>
#include <algorithm>
#include <cmath>
#include <random>

#include <SDL3/SDL.h>
#include "rectangle.h"
#include "circle.h"


constexpr int WINDOW_WIDTH{ 700 };
constexpr int WINDOW_HEIGHT{ 800 };


constexpr float VERTICAL_SCROLLING_SPEED{ 100.0f };
constexpr int VERTICAL_SCROLLING_GRID_SQUARE_SIZE{ 20 };
std::vector<Rectangle> scrollingGrid;


float DT{ 0.01f };
Uint64 startTime{ 0 };
Uint64 endTime{ 0 };


SDL_Window* window{ nullptr };
SDL_Renderer* renderer{ nullptr };
SDL_Event currentEvent;
SDL_Color backgroundColor{ 255, 255, 255, 255 };

bool quit{ false };
float displayScale{ 1.0f };

namespace {

    constexpr int ENEMY_LANES = 3;
    constexpr float SHIP_SIZE = 50.0f;
    constexpr float HALF_SHIP = SHIP_SIZE / 2.0f;
    constexpr float BULLET_RADIUS = 10.0f;

    constexpr float MOUSE_ACCELERATION = 400.0f;   
    constexpr float MOUSE_BRAKING = 800.0f;
    constexpr float MAX_SHIP_SPEED = 500.0f;       
    constexpr float MOUSE_DEADZONE_PX = 0.05f;      

    constexpr float PLAYER_BULLET_SPEED = 500.0f;
    constexpr float ENEMY_BULLET_SPEED = PLAYER_BULLET_SPEED / 5.0f;
    constexpr float PLAYER_FIRE_INTERVAL = 0.3f;
    constexpr float ENEMY_FIRE_INTERVAL = PLAYER_FIRE_INTERVAL * 10.0f; 
    constexpr float ENEMY_FIRST_SHOT_DELAY = 1.0f;
    constexpr float RESPAWN_INTERVAL = 3.0f;
    constexpr float REPULSOR_PUSH = 10.0f;         
    constexpr float FAN_ANGLE = 15.0f * 3.14159265358979323846f / 180.0f;

    constexpr float FIXED_DT = 1.0f / 120.0f;
    constexpr Uint64 FOCUS_CLICK_GUARD_NS = 50'000'000ULL;
    constexpr float BULLET_MAX_AGE = 30.0f;
    constexpr std::size_t MAX_PLAYER_BULLETS = 256;
    constexpr std::size_t MAX_ENEMY_BULLETS = 1024;

    float vecLength(glm::vec2 v) {
        return std::sqrt(v.x * v.x + v.y * v.y);
    }

    glm::vec2 unitOr(glm::vec2 v, glm::vec2 fallback = glm::vec2(0.0f, 1.0f)) {
        const float mag = vecLength(v);
        return (mag > 0.00001f) ? (v / mag) : fallback;
    }

    glm::vec2 clampSpeed(glm::vec2 velocity, float maxSpeed) {
        const float speed = vecLength(velocity);
        return (speed > maxSpeed) ? (velocity * (maxSpeed / speed)) : velocity;
    }

    glm::vec2 rotateVector(glm::vec2 v, float angle) {
        const float c = std::cos(angle);
        const float s = std::sin(angle);
        return glm::vec2(v.x * c - v.y * s, v.x * s + v.y * c);
    }

    bool rectanglesOverlap(glm::vec2 a, glm::vec2 b) {
        return std::abs(a.x - b.x) < SHIP_SIZE &&
            std::abs(a.y - b.y) < SHIP_SIZE;
    }

    bool circleHitsShip(glm::vec2 circleCenter, glm::vec2 shipCenter) {
        const float closestX = std::clamp(circleCenter.x, shipCenter.x - HALF_SHIP, shipCenter.x + HALF_SHIP);
        const float closestY = std::clamp(circleCenter.y, shipCenter.y - HALF_SHIP, shipCenter.y + HALF_SHIP);
        const float dx = circleCenter.x - closestX;
        const float dy = circleCenter.y - closestY;
        return (dx * dx + dy * dy) <= (BULLET_RADIUS * BULLET_RADIUS);
    }

    enum class Outcome { Playing, Victory, Defeat };

    struct Ship {
        glm::vec2 position{ 0.0f, 0.0f };
        glm::vec2 velocity{ 0.0f, 0.0f };
    };

    struct Enemy {
        bool active = false;
        glm::vec2 position{ 0.0f, 0.0f };
        float laneFraction = 0.5f;
        float fallSpeed = 0.0f;
        float aliveSeconds = 0.0f;
        float nextFireTime = ENEMY_FIRST_SHOT_DELAY;
    };

    struct Bullet {
        glm::vec2 position{ 0.0f, 0.0f };
        glm::vec2 velocity{ 0.0f, 0.0f };
        float age = 0.0f;
        bool active = true;
    };

    struct GameState {
        float sceneWidth = static_cast<float>(WINDOW_WIDTH);
        float sceneHeight = static_cast<float>(WINDOW_HEIGHT);
        std::mt19937 random{ std::random_device{}() };

        Ship player{};
        std::array<Enemy, ENEMY_LANES> enemies{};
        std::vector<Bullet> playerBullets{};
        std::vector<Bullet> enemyBullets{};

        Outcome outcome = Outcome::Playing;
        bool firing = false;
        bool rightMouseHeld = false;
        float nextPlayerShotIn = PLAYER_FIRE_INTERVAL;
        float respawnClock = 0.0f;
        Uint64 lastFocusGainNs = 0;

        double accumulator = 0.0;
        glm::vec2 bufferedMouseMovement{ 0.0f, 0.0f };
    };

    GameState game;

    void updateSceneDimensions() {
        int rawW = WINDOW_WIDTH;
        int rawH = WINDOW_HEIGHT;
        if (window) {
            SDL_GetWindowSize(window, &rawW, &rawH);
        }
        const float scale = (displayScale > 0.0f) ? displayScale : 1.0f;
        game.sceneWidth = static_cast<float>(rawW) / scale;
        game.sceneHeight = static_cast<float>(rawH) / scale;
    }

    void constrainPlayer() {
        const float minX = HALF_SHIP;
        const float minY = HALF_SHIP;
        const float maxX = std::max(minX, game.sceneWidth - HALF_SHIP);
        const float maxY = std::max(minY, game.sceneHeight - HALF_SHIP);

        const float oldX = game.player.position.x;
        const float oldY = game.player.position.y;

        game.player.position.x = std::clamp(oldX, minX, maxX);
        game.player.position.y = std::clamp(oldY, minY, maxY);

        if (game.player.position.x != oldX) game.player.velocity.x = 0.0f;
        if (game.player.position.y != oldY) game.player.velocity.y = 0.0f;
    }

    void updateEnemyLanePositions() {
        const float laneWidth = game.sceneWidth / ENEMY_LANES;
        for (int lane = 0; lane < ENEMY_LANES; ++lane) {
            Enemy& enemy = game.enemies[lane];
            if (!enemy.active) continue;
            const float left = lane * laneWidth;
            enemy.position.x = std::clamp(left + enemy.laneFraction * laneWidth,
                left + HALF_SHIP,
                left + laneWidth - HALF_SHIP);
        }
    }

    void spawnEnemy(int lane, bool initiallyVisible = false) {
        const float laneWidth = game.sceneWidth / ENEMY_LANES;
        const float laneLeft = lane * laneWidth;
        const float centerMin = laneLeft + HALF_SHIP;
        const float centerMax = std::max(centerMin, laneLeft + laneWidth - HALF_SHIP);

        std::uniform_real_distribution<float> xRandom(centerMin, centerMax);
        std::uniform_real_distribution<float> speedRandom(50.0f, 150.0f);

        Enemy& enemy = game.enemies[lane];
        enemy = {};
        enemy.active = true;
        enemy.position = glm::vec2(xRandom(game.random), initiallyVisible ? HALF_SHIP : -HALF_SHIP);
        enemy.laneFraction = (laneWidth > 0.0f) ? (enemy.position.x - laneLeft) / laneWidth : 0.5f;
        enemy.fallSpeed = speedRandom(game.random);
        enemy.nextFireTime = ENEMY_FIRST_SHOT_DELAY;
    }

    void shootPlayer() {
        if (game.playerBullets.size() < MAX_PLAYER_BULLETS) {
            game.playerBullets.push_back({ game.player.position, glm::vec2(0.0f, -PLAYER_BULLET_SPEED) });
        }
    }

    void shootEnemy(const Enemy& enemy) {
        const glm::vec2 aim = unitOr(game.player.position - enemy.position);
        const std::array<float, 3> angles = { 0.0f, FAN_ANGLE, -FAN_ANGLE };
        for (float angle : angles) {
            if (game.enemyBullets.size() >= MAX_ENEMY_BULLETS) return;
            game.enemyBullets.push_back({
                enemy.position,
                rotateVector(aim, angle) * ENEMY_BULLET_SPEED
                });
        }
    }

    void useRepulsor() {
        for (Bullet& bullet : game.enemyBullets) {
            const glm::vec2 away = unitOr(bullet.position - game.player.position,
                unitOr(-bullet.velocity));
            bullet.velocity += away * REPULSOR_PUSH;
        }
    }

    void eraseExpiredBullets() {
        const float left = -BULLET_RADIUS;
        const float top = -BULLET_RADIUS;
        const float right = game.sceneWidth + BULLET_RADIUS;
        const float bottom = game.sceneHeight + BULLET_RADIUS;

        auto isExpired = [=](const Bullet& b) {
            return !b.active || b.age > BULLET_MAX_AGE ||
                b.position.x < left || b.position.x > right ||
                b.position.y < top || b.position.y > bottom;
            };

        game.playerBullets.erase(
            std::remove_if(game.playerBullets.begin(), game.playerBullets.end(), isExpired),
            game.playerBullets.end());
        game.enemyBullets.erase(
            std::remove_if(game.enemyBullets.begin(), game.enemyBullets.end(), isExpired),
            game.enemyBullets.end());
    }

    bool noEnemiesActive() {
        return std::none_of(game.enemies.begin(), game.enemies.end(),
            [](const Enemy& e) { return e.active; });
    }

    void endGame(Outcome result) {
        game.outcome = result;
        game.player.velocity = glm::vec2(0.0f, 0.0f);
        for (Enemy& enemy : game.enemies) {
            enemy.active = false;
        }
        game.playerBullets.clear();
        game.enemyBullets.clear();
        game.firing = false;
        game.rightMouseHeld = false;
        game.bufferedMouseMovement = glm::vec2(0.0f, 0.0f);

        SDL_SetWindowRelativeMouseMode(window, false);
        SDL_SetWindowTitle(window, result == Outcome::Victory
            ? "VICTORY! | Press R to restart, Esc to exit"
            : "DEFEAT! | Press R to restart, Esc to exit");
    }

    void stepSimulation(float dt, glm::vec2 mouseAcceleration, bool mouseMoved) {
        if (game.outcome != Outcome::Playing) {
            return;
        }

        if (mouseMoved) {
            game.player.velocity += mouseAcceleration * dt;
        }
        else {
            const float speed = vecLength(game.player.velocity);
            const float newSpeed = std::max(0.0f, speed - MOUSE_BRAKING * dt);
            game.player.velocity = (speed > 0.0f)
                ? game.player.velocity * (newSpeed / speed)
                : glm::vec2(0.0f, 0.0f);
        }
        game.player.velocity = clampSpeed(game.player.velocity, MAX_SHIP_SPEED);
        game.player.position += game.player.velocity * dt;
        constrainPlayer();

        if (game.firing) {
            game.nextPlayerShotIn -= dt;
            while (game.nextPlayerShotIn <= 0.0f) {
                shootPlayer();
                game.nextPlayerShotIn += PLAYER_FIRE_INTERVAL;
            }
        }

        for (int lane = 0; lane < ENEMY_LANES; ++lane) {
            Enemy& enemy = game.enemies[lane];
            if (!enemy.active) continue;

            enemy.position.y += enemy.fallSpeed * dt;
            enemy.aliveSeconds += dt;

            while (enemy.aliveSeconds >= enemy.nextFireTime) {
                shootEnemy(enemy);
                enemy.nextFireTime += ENEMY_FIRE_INTERVAL;
            }

            if (enemy.position.y - HALF_SHIP > game.sceneHeight) {
                enemy.active = false;
            }
        }

        for (Bullet& bullet : game.playerBullets) {
            bullet.position += bullet.velocity * dt;
            bullet.age += dt;
        }
        for (Bullet& bullet : game.enemyBullets) {
            bullet.position += bullet.velocity * dt;
            bullet.age += dt;
        }

        for (const Enemy& enemy : game.enemies) {
            if (enemy.active && rectanglesOverlap(game.player.position, enemy.position)) {
                endGame(Outcome::Defeat);
                return;
            }
        }
        for (const Bullet& bullet : game.enemyBullets) {
            if (bullet.active && circleHitsShip(bullet.position, game.player.position)) {
                endGame(Outcome::Defeat);
                return;
            }
        }

        bool destroyedEnemyThisFrame = false;
        for (Bullet& bullet : game.playerBullets) {
            if (!bullet.active) continue;
            for (Enemy& enemy : game.enemies) {
                if (enemy.active && circleHitsShip(bullet.position, enemy.position)) {
                    bullet.active = false;
                    enemy.active = false;
                    destroyedEnemyThisFrame = true;
                    break;
                }
            }
        }

        eraseExpiredBullets();

        if (destroyedEnemyThisFrame && noEnemiesActive()) {
            endGame(Outcome::Victory);
            return;
        }

        game.respawnClock += dt;
        while (game.respawnClock >= RESPAWN_INTERVAL) {
            game.respawnClock -= RESPAWN_INTERVAL;
            for (int lane = 0; lane < ENEMY_LANES; ++lane) {
                if (!game.enemies[lane].active) {
                    spawnEnemy(lane, false);
                }
            }
        }
    }

} // namespace

void initScollingRectangles() {
    scrollingGrid.clear();
    const int height = static_cast<int>(std::ceil(game.sceneHeight));
    const int numGridRectangles = (height / VERTICAL_SCROLLING_GRID_SQUARE_SIZE) / 2 + 2;

    for (int i = 0; i < numGridRectangles; i++) {
        Rectangle newGridRectangleLeft, newGridRectangleRight;

        newGridRectangleLeft.width = newGridRectangleLeft.height = VERTICAL_SCROLLING_GRID_SQUARE_SIZE;
        newGridRectangleLeft.pos.x = newGridRectangleLeft.width / 2.0f;
        newGridRectangleLeft.pos.y = game.sceneHeight - newGridRectangleLeft.height / 2.0f - 2.0f * i * newGridRectangleLeft.height;
        newGridRectangleLeft.vel = glm::vec2(0.0f, VERTICAL_SCROLLING_SPEED);
        newGridRectangleLeft.color = SDL_Color{ 235, 235, 235, 255 };

        newGridRectangleRight.width = newGridRectangleRight.height = VERTICAL_SCROLLING_GRID_SQUARE_SIZE;
        newGridRectangleRight.pos.x = game.sceneWidth - newGridRectangleRight.width / 2.0f;
        newGridRectangleRight.pos.y = game.sceneHeight - newGridRectangleRight.height / 2.0f - 2.0f * i * newGridRectangleRight.height;
        newGridRectangleRight.vel = glm::vec2(0.0f, VERTICAL_SCROLLING_SPEED);
        newGridRectangleRight.color = SDL_Color{ 235, 235, 235, 255 };

        scrollingGrid.push_back(newGridRectangleLeft);
        scrollingGrid.push_back(newGridRectangleRight);
    }
}

void resetGame() {
    updateSceneDimensions();
    game.outcome = Outcome::Playing;
    game.player.position = glm::vec2(game.sceneWidth * 0.5f, game.sceneHeight - HALF_SHIP);
    game.player.velocity = glm::vec2(0.0f, 0.0f);
    game.enemies = {};
    game.playerBullets.clear();
    game.enemyBullets.clear();
    game.firing = false;
    game.rightMouseHeld = false;
    game.nextPlayerShotIn = PLAYER_FIRE_INTERVAL;
    game.respawnClock = 0.0f;
    game.accumulator = 0.0;
    game.bufferedMouseMovement = glm::vec2(0.0f, 0.0f);

    for (int lane = 0; lane < ENEMY_LANES; ++lane) {
        spawnEnemy(lane, true);
    }

    initScollingRectangles();

    SDL_SetWindowTitle(window, "SDL3 Hard Task | LMB: Fire | RMB: Repulsor | Esc: Exit");
    SDL_SetWindowRelativeMouseMode(window, true);
}

bool initWindow() {
    bool success{ true };

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL initialization failed: %s\n", SDL_GetError());
        success = false;
    }
    else {
        displayScale = SDL_GetDisplayContentScale(1);
        if (displayScale <= 0.0f) displayScale = 1.0f;

        if (!SDL_CreateWindowAndRenderer(
            "SDL3 Hard Task",
            static_cast<int>(displayScale * WINDOW_WIDTH),
            static_cast<int>(displayScale * WINDOW_HEIGHT),
            SDL_WINDOW_RESIZABLE,
            &window, &renderer)) {
            SDL_Log("Failed to create window and renderer: %s\n", SDL_GetError());
            success = false;
        }
        else {
            SDL_SetWindowMinimumSize(
                window,
                static_cast<int>(displayScale * ENEMY_LANES * SHIP_SIZE),
                static_cast<int>(displayScale * SHIP_SIZE));
            SDL_SetRenderScale(renderer, displayScale, displayScale);
            SDL_SetRenderVSync(renderer, 1);

            SDL_SetRenderDrawColor(renderer, backgroundColor.r, backgroundColor.g, backgroundColor.b,
                backgroundColor.a);
            SDL_RenderClear(renderer);
        }
    }

    return success;
}

void processEvents() {
    glm::vec2 frameMouseMovement(0.0f, 0.0f);
    const float scale = (displayScale > 0.0f) ? displayScale : 1.0f;

    while (SDL_PollEvent(&currentEvent)) {
        if (currentEvent.type == SDL_EVENT_QUIT) {
            quit = true;
            return;
        }

        if (currentEvent.type == SDL_EVENT_WINDOW_RESIZED ||
            currentEvent.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
            updateSceneDimensions();
            constrainPlayer();
            updateEnemyLanePositions();
            initScollingRectangles();
            continue;
        }

        if (currentEvent.type == SDL_EVENT_WINDOW_FOCUS_LOST) {
            game.firing = false;
            game.rightMouseHeld = false;
            continue;
        }

        if (currentEvent.type == SDL_EVENT_WINDOW_FOCUS_GAINED) {
            game.lastFocusGainNs = SDL_GetTicksNS();
            if (game.outcome == Outcome::Playing) {
                SDL_SetWindowRelativeMouseMode(window, true);
            }
            continue;
        }

        if (currentEvent.type == SDL_EVENT_KEY_DOWN && !currentEvent.key.repeat) {
            if (currentEvent.key.key == SDLK_ESCAPE) {
                quit = true;
                return;
            }
            if (currentEvent.key.key == SDLK_R && game.outcome != Outcome::Playing) {
                resetGame();
                return;
            }
        }

        if (game.outcome != Outcome::Playing) {
            continue;
        }

        if (currentEvent.type == SDL_EVENT_MOUSE_MOTION) {
            frameMouseMovement.x += currentEvent.motion.xrel / scale;
            frameMouseMovement.y += currentEvent.motion.yrel / scale;
        }
        else if (currentEvent.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
            if (SDL_GetTicksNS() - game.lastFocusGainNs < FOCUS_CLICK_GUARD_NS) {
                continue;
            }
            if (currentEvent.button.button == SDL_BUTTON_LEFT && !game.firing) {
                game.firing = true;
                shootPlayer();
                game.nextPlayerShotIn = PLAYER_FIRE_INTERVAL;
            }
            else if (currentEvent.button.button == SDL_BUTTON_RIGHT && !game.rightMouseHeld) {
                game.rightMouseHeld = true;
                useRepulsor();
            }
        }
        else if (currentEvent.type == SDL_EVENT_MOUSE_BUTTON_UP) {
            if (currentEvent.button.button == SDL_BUTTON_LEFT) {
                game.firing = false;
            }
            else if (currentEvent.button.button == SDL_BUTTON_RIGHT) {
                game.rightMouseHeld = false;
            }
        }
    }

    game.bufferedMouseMovement += frameMouseMovement;
}

void animate() {
    if (game.outcome != Outcome::Playing) {
        return; 
    }

    
    for (std::size_t i = 0; i < scrollingGrid.size(); i++) {
        Rectangle& gridRectangle = scrollingGrid.at(i);
        gridRectangle.pos += gridRectangle.vel * DT;

        if (gridRectangle.pos.y > (game.sceneHeight + gridRectangle.height / 2.0f)) {
            gridRectangle.pos.y -= gridRectangle.height * static_cast<float>(scrollingGrid.size());
        }
    }

    
    game.accumulator += static_cast<double>(DT);
    if (game.accumulator >= FIXED_DT) {
        const float motion = vecLength(game.bufferedMouseMovement);
        const bool mouseMoved = motion > MOUSE_DEADZONE_PX;
        const glm::vec2 mouseAcceleration = mouseMoved
            ? unitOr(game.bufferedMouseMovement) * MOUSE_ACCELERATION
            : glm::vec2(0.0f, 0.0f);

        while (game.accumulator >= FIXED_DT) {
            stepSimulation(FIXED_DT, mouseAcceleration, mouseMoved);
            game.accumulator -= FIXED_DT;
        }
        game.bufferedMouseMovement = glm::vec2(0.0f, 0.0f);
    }
}

void drawFrame() {
    
    SDL_SetRenderDrawColor(renderer, backgroundColor.r, backgroundColor.g, backgroundColor.b, backgroundColor.a);
    SDL_RenderClear(renderer);

    
    for (std::size_t i = 0; i < scrollingGrid.size(); i++) {
        Rectangle& gridRectangle = scrollingGrid.at(i);
        gridRectangle.draw(renderer);
    }

    
    if (game.outcome == Outcome::Playing) {
        for (const Enemy& enemy : game.enemies) {
            if (enemy.active) {
                Rectangle enemyRect(SHIP_SIZE, SHIP_SIZE, enemy.position, SDL_Color{ 255, 150, 0, 255 });
                FillRectangle(renderer, enemyRect);
            }
        }

        for (const Bullet& bullet : game.playerBullets) {
            if (bullet.active) {
                Circle bulletCircle(BULLET_RADIUS, bullet.position, SDL_Color{ 255, 0, 0, 255 });
                FillCircle(renderer, bulletCircle);
            }
        }

        for (const Bullet& bullet : game.enemyBullets) {
            if (bullet.active) {
                Circle bulletCircle(BULLET_RADIUS, bullet.position, SDL_Color{ 245, 0, 225, 255 });
                FillCircle(renderer, bulletCircle);
            }
        }
    }

   
    SDL_Color playerColor{ 0, 230, 0, 255 };
    if (game.outcome == Outcome::Victory) {
        playerColor = SDL_Color{ 0, 40, 255, 255 };
    }
    else if (game.outcome == Outcome::Defeat) {
        playerColor = SDL_Color{ 255, 0, 0, 255 };
    }

    Rectangle playerRect(SHIP_SIZE, SHIP_SIZE, game.player.position, playerColor);
    FillRectangle(renderer, playerRect);

    
    SDL_RenderPresent(renderer);
}

void cleanup() {
    if (renderer) {
        SDL_DestroyRenderer(renderer);
        renderer = nullptr;
    }

    if (window) {
        SDL_DestroyWindow(window);
        window = nullptr;
    }

    SDL_Quit();
}

int main() {
    if (!initWindow()) {
        std::cout << "Failed to initialize" << std::endl;
        return -1;
    }

    SDL_zero(currentEvent);
    game.playerBullets.reserve(MAX_PLAYER_BULLETS);
    game.enemyBullets.reserve(MAX_ENEMY_BULLETS);

    resetGame();
    startTime = SDL_GetTicksNS();

    while (!quit) {
        const Uint64 now = SDL_GetTicksNS();
        DT = static_cast<float>(std::clamp((now - startTime) / 1'000'000'000.0, 0.0, 0.25));
        startTime = now;

        processEvents();
        animate();
        drawFrame();
    }

    cleanup();
    return 0;
}