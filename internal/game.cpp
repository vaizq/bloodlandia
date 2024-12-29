#include "game.h"
#include <chrono>
#include "raylib.h"
#include "util.h"
#include <cstring>


using namespace std::chrono_literals;


const int screenWidth = 800;
const int screenHeight = 450;
const int paddleHeight = 100;
const int paddleWidth = 20;
const int ballRadius = 20;
const int paddleSpeed = 400;
const int ballSpeed = 500;


GameState mirrorize(const GameState& state) {
    GameState mirror = state;
    std::swap(mirror.playerPos, mirror.enemyPos);
    mirror.ballPosX -= 2 * mirror.ballPosX - screenWidth;
    mirror.ballPosY -= 2 * mirror.ballPosY - screenHeight;
    mirror.ballVeloX *= -1;
    mirror.ballVeloY *= -1;
    return mirror;
}

Game::Game(const char* serverAddr)
: serverAddr{serverAddr}
{}

void Game::init() {
    InitWindow(screenWidth, screenHeight, "PongOnline");
    SetTargetFPS(144);
}

void Game::restart() {
    state.ballPosX = screenWidth/2;
    state.ballPosY = screenHeight/2;
    state.ballVeloX = RandFloat(0.3*ballSpeed) + 0.5*ballSpeed * RandSign();
    state.ballVeloY = sqrt(ballSpeed*ballSpeed-state.ballVeloX*state.ballVeloX * RandSign());
    state.playerPos = screenHeight/2;
    state.enemyPos = screenHeight/2;
    prevUpdate = Clock::now();
}

void Game::update() {
    const auto now = Clock::now();
    const float dt = std::chrono::duration_cast<std::chrono::duration<float, std::chrono::seconds::period>>(now - prevUpdate).count();
    prevUpdate = now;

    if (IsKeyDown(KEY_UP)) {
        state.playerPos -= dt * paddleSpeed;
    }
    if (IsKeyDown(KEY_DOWN)) {
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
        if (judge)
            restart();
    } else if (state.ballPosX + ballRadius >= screenWidth) {
        state.enemyScore += 1;
        if (judge)
            restart();
    }

    state.ballPosX += state.ballVeloX * dt;
    state.ballPosY += state.ballVeloY * dt;
}

void Game::drawScore() {
    char s[64];
    std::snprintf(s, sizeof s, "%d : %d", state.enemyScore, state.playerScore);
    DrawText(s, screenWidth/2 -18, 10, 24, WHITE); 
}

void Game::renderGame() {
    BeginDrawing();
    ClearBackground(BLACK);
    drawScore();
    DrawRectangle(0, state.enemyPos, paddleWidth, paddleHeight, WHITE);
    DrawRectangle(screenWidth - paddleWidth, state.playerPos, paddleWidth, paddleHeight, WHITE);
    DrawCircle(state.ballPosX, state.ballPosY, ballRadius, WHITE);
    EndDrawing();
}

void renderQueue() {
    BeginDrawing();
    ClearBackground(BLACK);
    DrawText("You are in a queue waiting for opponent...", 50, screenHeight/2, 24, WHITE); 
    EndDrawing();
}

void Game::run() {

    init();
    while (!WindowShouldClose()) {
        switch (status) {
        case GameStatus::Pending:
        {
                BeginDrawing();
                ClearBackground(BLACK);
                EndDrawing();
                qc = std::make_unique<QueueClient>(serverAddr, "6969");
                qc->start();
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
                    status = GameStatus::Playing;
                    judge = qc->isJudge();
                    qc.reset();
                    restart();
                    break;
                }
                renderQueue();
                break;
        }
        case GameStatus::Playing:
        {
                mnet->poll();

                if (Clock::now() - mnet->prevSendTime() > 10ms) {
                    mnet->send(state);
                }

                if (!mnet->hasPeer()) {
                    printf("Lost connection to peer!\n");
                    status = GameStatus::Pending;
                    break;
                }

                update();
                GameState mirror;

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

    CloseWindow();
}
