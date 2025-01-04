#include "game.h"
#include <chrono>
#include <cstring>
#include <raylib.h>
#include "util.h"
#include <format>

using namespace std::chrono_literals;


int renderWidth = 1280;
int renderHeight = 960;
float viewHeight = 50.f;

constexpr float playerSpeed = 100.f;
constexpr float playerRadius = 2.f;
constexpr float enemyRadius = 1.f;

float aspectRatio() {
    return 1.0f * renderWidth / renderHeight;
}

rl::Vector2 screenCenter() {
    return rl::Vector2{renderWidth/2.f, renderHeight/2.f};
}

float hpx() {
    return renderHeight / viewHeight;
}

rl::Vector2 Game::worldPosToScreenCoord(rl::Vector2 pos) {
    const rl::Vector2 d = pos - player.pos;
    return screenCenter() + hpx() * d;
}

rl::Vector2 Game::screenCoordToWorldPos(rl::Vector2 coord) {
    const auto d = coord - screenCenter();
    return player.pos + d / hpx();
}

Game::Game(const char* serverAddr)
{}

void Game::init() {
    rl::InitWindow(renderWidth, renderHeight, "Verilandia");
    rl::SetTargetFPS(144);
    rl::HideCursor();

    prevUpdate = Clock::now();
    player.pos.x = 0;
    player.pos.y = 0;

    for (int i = 0; i < 10; ++i) {
        enemies.emplace_back(
            rl::Vector2{RandFloat(100) - 50, RandFloat(200) - 100},
            rl::Vector2{RandFloat(10) - 5, RandFloat(10) - 5}
        );
    }
}

void Game::update() {
    renderWidth = rl::GetRenderWidth();
    renderHeight = rl::GetRenderHeight();

    const auto now = Clock::now();
    const float dt = std::chrono::duration_cast<std::chrono::duration<float, std::chrono::seconds::period>>(now - prevUpdate).count();
    prevUpdate = now;

    if (rl::IsKeyDown(rl::KEY_A)) {
        player.pos.x -= dt * playerSpeed;
    }
    if (rl::IsKeyDown(rl::KEY_D)) {
        player.pos.x += dt * playerSpeed;
    }
    if (rl::IsKeyDown(rl::KEY_W)) {
        player.pos.y -= dt * playerSpeed;
    }
    if (rl::IsKeyDown(rl::KEY_S)) {
        player.pos.y += dt * playerSpeed;
    }

    if (rl::IsMouseButtonPressed(rl::MOUSE_BUTTON_LEFT)) {
        printf("shoot!\n");
    }

    viewHeight += 5.f * rl::GetMouseWheelMove();

    // Check for network events
}

void Game::render() {
    rl::BeginDrawing();
    rl::ClearBackground(rl::BLACK);

    {
        const auto pos = screenCenter();
        const float r = playerRadius * hpx();
        rl::DrawCircleV(screenCenter(), r, rl::WHITE);
        rl::DrawText(std::format("({:.1f}, {:.1f})", player.pos.x, player.pos.y).c_str(), pos.x + r, pos.y - r, 12, rl::WHITE);
    }

    for(auto& enemy : enemies) {
        const auto pos = worldPosToScreenCoord(enemy.pos);
        const float r = enemyRadius * hpx();
        rl::DrawCircleV(pos, r, rl::WHITE);
        rl::DrawText(std::format("({:.1f}, {:.1f})", enemy.pos.x, enemy.pos.y).c_str(), pos.x + r, pos.y - r, 12, rl::WHITE);
    }

    {
        const auto pos = rl::GetMousePosition();
        const auto wpos = screenCoordToWorldPos(pos);
        const float w = 20.0f;
        rl::DrawLine(pos.x - w/2, pos.y, pos.x + w/2, pos.y, rl::GREEN);
        rl::DrawLine(pos.x, pos.y - w/2, pos.x, pos.y + w/2, rl::GREEN);
        rl::DrawText(std::format("({:.1f}, {:.1f})", wpos.x, wpos.y).c_str(), pos.x + w, pos.y - w, 12, rl::WHITE);
    }

    rl::EndDrawing();
}

void Game::run() {

    init();

    while (!rl::WindowShouldClose()) {
        update();
        render();
    }

    rl::CloseWindow();
}
