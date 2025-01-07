#include "game.h"
#include <chrono>
#include <cstring>
#include "util.h"
#include <format>
#include "protocol.h"

using namespace std::chrono_literals;


int renderWidth = 1280;
int renderHeight = 960;
float viewHeight = 50.f;


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
                player.stats = p.stats;
                player.target = p.target;
            } else {
                enemies.push_back(p);
            }
        }

        bullets.clear();
        for (int i = 0; i < state.numBullets; ++i) {
            bullets.push_back(state.bullets[i]);
        }
    });
}

void Game::init() {
    rl::InitWindow(renderWidth, renderHeight, "Verilandia");
    rl::SetTargetFPS(144);
    rl::HideCursor();

    moveAnimation = std::make_unique<Animation>("assets/Top_Down_Survivor/rifle/move", 1000ms);

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
    const auto dt = now - prevUpdate;
    const float dtf = std::chrono::duration_cast<std::chrono::duration<float>>(dt).count();
    prevUpdate = now;

    const auto prevVelo = player.velo;

    if (rl::IsKeyPressed(rl::KEY_A)) {
        player.velo.x = -proto::playerSpeed;
    } else if (rl::IsKeyReleased(rl::KEY_A)) {
        if (rl::IsKeyDown(rl::KEY_D)) {
            player.velo.x = proto::playerSpeed;
        } else {
            player.velo.x = 0;
        }
    }

    if (rl::IsKeyPressed(rl::KEY_D)) {
        player.velo.x = proto::playerSpeed;
    } else if (rl::IsKeyReleased(rl::KEY_D)) {
        if (rl::IsKeyDown(rl::KEY_A)) {
            player.velo.x = -proto::playerSpeed;
        } else {
            player.velo.x = 0;
        }

    }

    if (rl::IsKeyPressed(rl::KEY_W)) {
        player.velo.y = -proto::playerSpeed;
    } else if (rl::IsKeyReleased(rl::KEY_W)) {
        if (rl::IsKeyDown(rl::KEY_S)) {
            player.velo.y = proto::playerSpeed;
        } else {
            player.velo.y = 0;
        }

    }

    if (rl::IsKeyPressed(rl::KEY_S)) {
        player.velo.y = proto::playerSpeed;
    } else if (rl::IsKeyReleased(rl::KEY_S)) {
        if (rl::IsKeyDown(rl::KEY_W)) {
            player.velo.y = -proto::playerSpeed;
        } else {
            player.velo.y = 0;
        }
    }

    if (rl::IsKeyDown(rl::KEY_TAB)) {
        viewStats = true;
    } else {
        viewStats = false;
    }

    if (rl::IsMouseButtonPressed(rl::MOUSE_BUTTON_LEFT)) {
        const auto target = rl::GetMousePosition();
        const auto direction = {target - player.pos};
        eventShoot();
    }

    {
        const auto pos = rl::GetMousePosition();
        const auto wpos = screenCoordToWorldPos(pos);
        if (wpos.x != player.target.x || wpos.y != player.target.y) {
            player.target = wpos;
            static Clock::time_point prevEvent{};
            if (Clock::now() - prevEvent > 30ms) {
                eventMouseMove();
            }
        }

    }

    if (player.velo.x != prevVelo.x || player.velo.y != prevVelo.y) {
        eventMove();
    }

//    viewHeight += 5.f * rl::GetMouseWheelMove();

    player.pos = player.pos + dtf * player.velo;

    for (auto& enemy : enemies) {
        enemy.pos = enemy.pos + dtf * enemy.velo;
    }

    for (auto& bullet : bullets) {
        bullet.pos = bullet.pos + dtf * bullet.velo;
    }

//    moveAnimation->update(dt);
    con.poll();
}

void Game::renderPlayer(const proto::Player& player, Animation& animation) {
        const auto pos = (player.id == this->player.id ? screenCenter() : worldPosToScreenCoord(player.pos));
        auto& frame = animation.currentFrame();
        const auto diff = player.target - player.pos;
        const float rotation =  RAD2DEG * std::atan2(diff.y, diff.x);
        const rl::Vector2 origin{3 * frame.width / 8.f, 5 * frame.height / 8.f};

        rl::DrawTexturePro(frame, 
                           {0, 0, (float)frame.width, (float)frame.height},
                           {pos.x, pos.y, (float)frame.width, (float)frame.height}, 
                           origin,
                           rotation, 
                           rl::WHITE);

        rl::DrawText(std::format("ID: {} AT ({:.1f}, {:.1f})", player.id, player.pos.x, player.pos.y).c_str(), pos.x + origin.x, pos.y - origin.y, 14, rl::WHITE);
}


void Game::render() {
    rl::BeginDrawing();
    rl::ClearBackground(rl::BLACK);

    renderPlayer(player, *moveAnimation);

    for(auto& enemy : enemies) {
        renderPlayer(enemy, *moveAnimation);
    }

    {
        const auto pos = rl::GetMousePosition();
        const auto wpos = screenCoordToWorldPos(pos);
        const float w = 20.0f;
        rl::DrawLine(pos.x - w/2, pos.y, pos.x + w/2, pos.y, rl::GREEN);
        rl::DrawLine(pos.x, pos.y - w/2, pos.x, pos.y + w/2, rl::GREEN);
        rl::DrawText(std::format("({:.1f}, {:.1f})", wpos.x, wpos.y).c_str(), pos.x + w, pos.y - w, 12, rl::WHITE);
    }

    {
        for(auto& bullet : bullets) {
            const auto pos = worldPosToScreenCoord(bullet.pos);
            const float r = proto::bulletRadius * hpx();
            rl::DrawCircleV(pos, r, rl::GRAY);
            rl::DrawText(std::format("{}", bullet.shooterID).c_str(), pos.x + r, pos.y - r, 12, rl::WHITE);
        }
    }

    {
        const float ping = std::chrono::duration_cast<std::chrono::duration<float, std::milli>>(con.getPing()).count();
        rl::DrawText(std::format("ping {:.3f}ms", ping).c_str(), 10, 10, 16, rl::WHITE);
    }

    if (viewStats) {
        std::vector<proto::Player> players{enemies.begin(), enemies.end()};
        players.push_back(player);
        std::sort(players.begin(), players.end(), [](const proto::Player& a, const proto::Player& b) {
            return (a.stats.kills - a.stats.deaths) > (b.stats.kills - b.stats.kills);
        });
        std::stringstream ss;
        for (const auto& p : players) {
                ss << std::format("ID: {:<6}\tKILLS: {:<6}\tDEATHS: {:<6}\tK/D: {:<6.1f}",
                              p.id, p.stats.kills, p.stats.deaths, 1.0f * p.stats.kills / p.stats.deaths);

            if (p.id == player.id) {
                ss << " <--- YOU";
            }

            ss << '\n';
        }

        rl::DrawText(ss.str().c_str(), 10, 50, 18, rl::GREEN);
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

    con.write(proto::moveChannel, bufOut, n, [bufOut](auto, auto) {
        delete[] bufOut;
    });
}

void Game::eventMouseMove() {
    proto::MouseMove mouseMove{player.target};
    proto::Header h{player.id, sizeof mouseMove};
    auto [bufOut, n] = proto::makeMessage(h, &mouseMove);

    con.write(proto::mouseMoveChannel, bufOut, n, [bufOut](auto, auto) {
        delete[] bufOut;
    });

}


void Game::eventShoot() {
    
    auto diff = (player.target - player.pos); 
    if (diff.x == 0 && diff.y == 0) {
        fprintf(stderr, "ERROR\t can't shoot yourself\n");
        return;
    }

    diff = (diff / length(diff));

    const proto::Bullet bullet(
        player.pos + proto::playerRadius * diff, 
        proto::bulletSpeed * diff,
        player.id
    );

    bullets.push_back(bullet);

    proto::Shoot shoot{bullet};
    auto [bufOut, n] = proto::makeMessage(
        {player.id, sizeof shoot}, 
        &shoot
    );

    con.write(proto::shootChannel, bufOut, n, [bufOut](auto, auto) {
        delete[] bufOut;
    });
}
