#include "game.h"
#include <chrono>
#include "util.h"
#include <cstring>

namespace rl {
    #include "raylib.h"
}


using namespace std::chrono_literals;


constexpr int screenWidth = 800;
constexpr int screenHeight = 400;
constexpr int paddleHeight = 100;
constexpr int paddleWidth = 20;
constexpr int ballRadius = 20;
constexpr int paddleSpeed = 400;
constexpr int ballSpeed = 500;


Game::Game(const char* serverAddr)
: serverAddr{serverAddr}
{}

void Game::init() {
    rl::InitWindow(screenWidth, screenHeight, "PongOnline");
    rl::SetTargetFPS(144);
}

void Game::restart() {
    const float minvx = ballSpeed / std::sqrt(2);
    state.ballPosX = screenWidth/2;
    state.ballPosY = screenHeight/2;
    state.ballVeloX = (minvx + RandFloat(0.8 * (ballSpeed - minvx))) * RandSign();
    state.ballVeloY = std::sqrt(ballSpeed*ballSpeed-state.ballVeloX*state.ballVeloX) * RandSign();
    state.playerPos = screenHeight/2;
    state.enemyPos = screenHeight/2;
    prevUpdate = Clock::now();
}

void Game::update() {
    const auto now = Clock::now();
    const float dt = std::chrono::duration_cast<std::chrono::duration<float, std::chrono::seconds::period>>(now - prevUpdate).count();
    prevUpdate = now;

    if (rl::IsKeyDown(rl::KEY_UP)) {
        state.playerPos -= dt * paddleSpeed;
    }
    if (rl::IsKeyDown(rl::KEY_DOWN)) {
        state.playerPos += dt * paddleSpeed;
    }


    // collisions with paddles
    if (state.ballPosX - ballRadius <= paddleWidth 
        && state.ballVeloX < 0
        && state.ballPosY > state.enemyPos
        && state.ballPosY < (state.enemyPos + paddleHeight)
    ) {
        state.ballPosX = paddleWidth + ballRadius;
        state.ballVeloX *= -1;
    } else if (state.ballPosX + ballRadius >= screenWidth - paddleWidth
        && state.ballVeloX > 0
        && state.ballPosY > state.playerPos
        && state.ballPosY < (state.playerPos + paddleHeight)
    ) {
        state.ballPosX = screenWidth - paddleWidth - ballRadius;
        state.ballVeloX *= -1;
    }

    // collisions with ceiling and floor
    if (state.ballPosY - ballRadius <= 0 && state.ballVeloY < 0) {
        state.ballVeloY *= -1.0f;
        state.ballPosY = ballRadius;
    } else if (state.ballPosY + ballRadius >= screenHeight && state.ballVeloY > 0) {
        state.ballVeloY *= -1.0f;
        state.ballPosY = screenHeight - ballRadius;
    }

    // goals
    if (state.ballPosX - ballRadius < paddleWidth) {
        state.playerScore += 1;
        if (judge) {
            restart();
        }
    } else if (state.ballPosX + ballRadius >= screenWidth) {
        state.enemyScore += 1;
        if (judge) {
            restart();
        }
    }

    state.ballPosX += state.ballVeloX * dt;
    state.ballPosY += state.ballVeloY * dt;
}

void Game::drawScore() {
    char s[64];
    std::snprintf(s, sizeof s, "%d : %d", state.enemyScore, state.playerScore);
    rl::DrawText(s, screenWidth/2 -18, 10, 24, rl::WHITE); 
}

void Game::drawPing() {
    char s[64];
    std::snprintf(s, sizeof s, "ping %.3fms", std::chrono::duration_cast<std::chrono::duration<float, std::milli>>(mnet->getPing()).count());
    rl::DrawText(s, 10, 10, 16, rl::WHITE); 
}

void renderQueue() {
    rl::BeginDrawing();
    rl::ClearBackground(rl::BLACK);
    rl::DrawText("You are in a queue waiting for opponent...", 50, screenHeight/2, 24, rl::WHITE); 
    rl::EndDrawing();
}

void Game::renderWaiting() {
    const auto text = isReady ? "Waiting for opponent to be ready..." : "Press ENTER to start";
    rl::BeginDrawing();
    rl::ClearBackground(rl::BLACK);
    rl::DrawText(text, 50, screenHeight/2, 24, rl::WHITE); 
    rl::EndDrawing();
}

void Game::renderGame() {
    rl::BeginDrawing();
    rl::ClearBackground(rl::BLACK);
    drawPing();
    drawScore();
    rl::DrawRectangle(0, state.enemyPos, paddleWidth, paddleHeight, rl::WHITE);
    rl::DrawRectangle(screenWidth - paddleWidth, state.playerPos, paddleWidth, paddleHeight, rl::WHITE);
    rl::DrawCircle(state.ballPosX, state.ballPosY, ballRadius, rl::WHITE);
    rl::EndDrawing();
}

void Game::run() {

    init();
    while (!rl::WindowShouldClose()) {
        switch (status) {
        case GameStatus::Pending:
        {
                rl::BeginDrawing();
                rl::ClearBackground(rl::BLACK);
                rl::EndDrawing();
                qc = std::make_unique<QueueClient>(serverAddr, "6969");
                qc->start();
                printf("client started\n");
                status = GameStatus::Queue;
                break;
        }
        case GameStatus::Queue:
        {
                qc->poll();
                if (qc->hasPeer()) {
                    printf("Found peer at %s:%d\nGame starts%s\n", 
                           qc->getPeer().address().to_string().c_str(), 
                           qc->getPeer().port(),
                           (qc->isJudge() ? " and your the judge!" : "!"));
                    mnet = std::make_unique<net<GameState>>(qc->getBindPort(), qc->getPeer());
                    status = GameStatus::Waiting;
                    judge = qc->isJudge();
                    if (judge)
                            printf("I'm a judge!\n");
                    isReady = false;
                    qc.reset();
                    break;
                }
                renderQueue();
                break;
        }
        case GameStatus::Waiting:
        {
                mnet->poll();

                if (mnet->getPing() > 1000ms) {
                    printf("Lost connection to peer!\n");
                    status = GameStatus::Pending;
                    break;
                }

                if (!isReady && rl::IsKeyPressed(rl::KEY_ENTER)) {
                    isReady = true;
                    mnet->setReady();
                }

                if (isReady && mnet->isPeerReady()) {
                    status = GameStatus::Playing;
                    restart();
                }

                renderWaiting();
                break;
        }
        case GameStatus::Playing:
        {
                mnet->poll();

                mnet->send(state);

                if (mnet->getPing() > 500ms) {
                    printf("Lost connection to peer!\n");
                    status = GameStatus::Pending;
                    break;
                }

                update();

                auto& mail = mnet->getMailbox();
                if (mail.size() > 0) {
                    while (mail.size() > 1)
                        mail.pop();

                    const auto& latest = mail.front();
                    mail.pop();
                    state.enemyPos = latest.playerPos;
                    if (!judge) {
                        state.ballPosX = screenWidth - latest.ballPosX;
                        state.ballPosY = latest.ballPosY;
                        state.ballVeloX = -latest.ballVeloX;
                        state.ballVeloY = latest.ballVeloY;
                        state.playerScore = latest.enemyScore;
                        state.enemyScore = latest.playerScore;
                    }
                }
                renderGame();
                break;
        }
        default:
                printf("default\n");
        }
    }

    printf("closing, bye!\n");

    rl::CloseWindow();
}
