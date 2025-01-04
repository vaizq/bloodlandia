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
:   socket{ioc, udp::v4()},
    server{udp::resolver{ioc}.resolve(serverAddr, "6969")->endpoint()}
{
    printf("server: %s:%d\n", server.address().to_string().c_str(), server.port());
    printf("local: %s:%d\n", 
           socket.local_endpoint().address().to_string().c_str(), 
           socket.local_endpoint().port());
    startReceive();
}

void Game::startReceive() {
    socket.async_receive_from(
        asio::buffer(buf, sizeof buf),
        peer,
        [this](std::error_code ec, size_t n) {
            if (ec) {
                fprintf(stderr, "ERROR: failed to receive_from: %s\n", ec.message().c_str());
                startReceive();
                return;
            }

            if (n < sizeof (proto::Header)) {
                fprintf(stderr, "ERROR: received less bytes than header\n");
                startReceive();
                return;
            }

            proto::Header h;
            std::memcpy(&h, buf, sizeof h);
            player.id = h.playerId;

            if(sizeof h + h.payloadSize != n) {
                fprintf(stderr, "ERROR: received invalid number of bytes %ld when should have been %ld. PayloadSize: %ld\n",
                        n, sizeof h + h.payloadSize, h.payloadSize);
                startReceive();
                return;
            }

            switch (h.type) {
                case proto::MessageType::Reliable:
                {
                    // send confirmation back
                    proto::Header confh{h.event, h.playerId, 0, h.messageId, proto::MessageType::Confirmation};
                    char* buf = new char[sizeof confh];
                    std::memcpy(buf, &confh, sizeof confh);
                    socket.async_send_to(
                        asio::buffer(buf, sizeof confh),
                        peer,
                        [buf](std::error_code ec, size_t n) {
                                if (ec) {
                                    fprintf(stderr, "ERROR: failed to send confirmation: %s\n", ec.message().c_str());
                                }
                                delete[] buf;
                        });
                    break;
                }
                case proto::MessageType::Confirmation:
                    if (waitingForConfirmation.erase(h.messageId) != 0) {
                        printf("INFO: Got confirmation for %d\n", h.messageId);
                    }
                    startReceive();
                    return;
                case proto::MessageType::Unreliable:
                    break;
            }

            switch (h.event) {
                case proto::Event::Update:
                {
                    if (sizeof (proto::GameState) != h.payloadSize) {
                        fprintf(stderr, "ERROR: invalid payload size for Update\n");
                        return;
                    }

                    proto::GameState state;
                    std::memcpy(&state, buf + sizeof (proto::Header), h.payloadSize);

                    enemies.clear();
                    for (int i = 0; i < state.numPlayers; ++i) {
                        const auto& p = state.players[i];
                        if (p.id != player.id) {
                            enemies.emplace_back(p.pos, p.velo);
                        } else {
                            player.pos = p.pos;
                            player.velo = p.velo;
                        }
                    }
                    prevServerUpdate = Clock::now();
                    break;
                }
                default:
                    fprintf(stderr, "ERROR: invalid event %d\n", static_cast<int>(h.event));
            }

            startReceive();
        });
}

void Game::init() {
    rl::InitWindow(renderWidth, renderHeight, "Verilandia");
    rl::SetTargetFPS(144);
    rl::HideCursor();

    // connect()

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

    ioc.poll();
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

void Game::eventMove() {
    proto::Move move{player.velo};
    proto::Header h{proto::Event::Move, player.id, sizeof move, nextID(), proto::MessageType::Reliable};
    auto [bufOut, n] = proto::makeMessage(h, &move);

    waitingForConfirmation[h.messageId] = Message{h, {(char*)&move, (char*)&move + sizeof move}};

    socket.async_send_to(
            asio::buffer(bufOut, n),
            server,
            [bufOut](std::error_code ec, size_t n) {
                if (ec) {
                    fprintf(stderr, "ERROR: failed to send move event!: %s\n", ec.message().c_str());
                }
                delete[] bufOut;
            }
    );
}

void Game::eventShoot() {
    proto::Shoot shoot{player.target - player.pos};
    auto [bufOut, n] = proto::makeMessage(
        {proto::Event::Shoot, player.id, sizeof shoot}, 
        &shoot
    );

    socket.async_send_to(
            asio::buffer(bufOut, n),
            server,
            [bufOut](std::error_code ec, size_t n) {
                if (ec) {
                    fprintf(stderr, "ERROR: failed to send shoot event!: %s\n", ec.message().c_str());
                }
                delete[] bufOut;
            }
    );
}

proto::ID Game::nextID() {
    static proto::ID id{420};
    return id++;
}
