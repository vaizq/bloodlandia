#include "game.h"
#include <chrono>
#include <cstring>
#include <raylib.h>
#include "util.h"
#include <format>
#include "protocol.h"

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
    : con{udp::endpoint{asio::ip::make_address(serverAddr), 6969}}
{
    con.listen(proto::updateChannel, [this](char* data, size_t n) {
        proto::Header h;
        proto::GameState state;

        if (n != sizeof h + sizeof state) {
            fprintf(stderr, "ERROR\t invalid datalen\n");
            return;
        }

        std::memcpy(&h, data, sizeof h);
        if (player.id != h.playerId) {
            printf("INFO\t different player id (my: %d header: %d)\n", player.id, h.playerId);
            player.id = h.playerId;
        }

        if (h.payloadSize != sizeof state) {
            fprintf(stderr, "ERROR\t, invalid payloadsize\n");
            return;
        }

        std::memcpy(&state, data + sizeof h, sizeof state);

        enemies.clear();

        for (int i = 0; i < state.numPlayers; ++i) {
            proto::Player& p = state.players[i];
            if (p.id == player.id) {
                player.pos = p.pos;
                player.velo = p.velo;
            } else {
                enemies.push_back(Enemy{p.pos, p.velo});
            }
        }
    });
}

void Game::init() {
    rl::InitWindow(renderWidth, renderHeight, "Verilandia");
    rl::SetTargetFPS(144);
    rl::HideCursor();

    prevUpdate = Clock::now();
    player.pos.x = 0;
    player.pos.y = 0;
    player.velo.x = 0;
    player.velo.y = 0;
}

void Game::update() {
    renderWidth = rl::GetRenderWidth();
    renderHeight = rl::GetRenderHeight();

    const auto now = Clock::now();
    const float dt = std::chrono::duration_cast<std::chrono::duration<float, std::chrono::seconds::period>>(now - prevUpdate).count();
    prevUpdate = now;

    const auto prevVelo = player.velo;

    if (rl::IsKeyPressed(rl::KEY_A)) {
        player.velo.x = -playerSpeed;
    } else if (rl::IsKeyReleased(rl::KEY_A)) {
        if (rl::IsKeyDown(rl::KEY_D)) {
            player.velo.x = playerSpeed;
        } else {
            player.velo.x = 0;
        }
    }

    if (rl::IsKeyPressed(rl::KEY_D)) {
        player.velo.x = playerSpeed;
    } else if (rl::IsKeyReleased(rl::KEY_D)) {
        if (rl::IsKeyDown(rl::KEY_A)) {
            player.velo.x = -playerSpeed;
        } else {
            player.velo.x = 0;
        }

    }

    if (rl::IsKeyPressed(rl::KEY_W)) {
        player.velo.y = -playerSpeed;
    } else if (rl::IsKeyReleased(rl::KEY_W)) {
        if (rl::IsKeyDown(rl::KEY_S)) {
            player.velo.y = playerSpeed;
        } else {
            player.velo.y = 0;
        }

    }

    if (rl::IsKeyPressed(rl::KEY_S)) {
        player.velo.y = playerSpeed;
    } else if (rl::IsKeyReleased(rl::KEY_S)) {
        if (rl::IsKeyDown(rl::KEY_W)) {
            player.velo.y = -playerSpeed;
        } else {
            player.velo.y = 0;
        }
    }

    if (rl::IsMouseButtonPressed(rl::MOUSE_BUTTON_LEFT)) {
        const auto target = rl::GetMousePosition();
        const auto direction = {target - player.pos};
        eventShoot();
    }

    if (player.velo.x != prevVelo.x || player.velo.y != prevVelo.y) {
        eventMove();
    }

    viewHeight += 5.f * rl::GetMouseWheelMove();

    player.pos = player.pos + dt * player.velo;

    for (auto& enemy : enemies) {
        enemy.pos = enemy.pos + dt * enemy.velo;
    }

    con.poll();
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
        player.target = wpos;
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

void Game::eventMove() {
    proto::Move move{player.velo};
    proto::Header h{player.id, sizeof move};
    auto [bufOut, n] = proto::makeMessage(h, &move);

    printf("send move event with id %d\n", player.id);
    con.write(proto::moveChannel, bufOut, n, [bufOut](auto, auto) {
        delete[] bufOut;
    });
}

void Game::eventShoot() {
    proto::Shoot shoot{player.target};
    auto [bufOut, n] = proto::makeMessage(
        {player.id, sizeof shoot}, 
        &shoot
    );

    con.write(proto::shootChannel, bufOut, n, [bufOut](auto, auto) {
        delete[] bufOut;
    });
}








