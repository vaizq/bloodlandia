#include "game.h"
#include <chrono>
#include "raylib.h"
#include "util.h"
#include <cstring>


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

Game::Game(unsigned short port, unsigned short peerPort, bool judge)
: port{port}, peerPort{peerPort}, judge{judge}
{}

void Game::init() {
    InitWindow(screenWidth, screenHeight, "PongOnline");
    SetTargetFPS(60);

    restart();

    prevUpdate = Clock::now();
}

void Game::restart() {
    state.ballPosX = screenWidth/2;
    state.ballPosY = screenHeight/2;
    state.ballVeloX = RandFloat(0.3*ballSpeed) + 0.5*ballSpeed * RandSign();
    state.ballVeloY = sqrt(ballSpeed*ballSpeed-state.ballVeloX*state.ballVeloX * RandSign());
    state.playerPos = screenHeight/2;
    state.enemyPos = screenHeight/2;
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

void Game::render() {
    BeginDrawing();
    ClearBackground(BLACK);
    drawScore();
    DrawRectangle(0, state.enemyPos, paddleWidth, paddleHeight, WHITE);
    DrawRectangle(screenWidth - paddleWidth, state.playerPos, paddleWidth, paddleHeight, WHITE);
    DrawCircle(state.ballPosX, state.ballPosY, ballRadius, WHITE);
    EndDrawing();
}

void Game::run() {
    mnet = std::make_unique<net<GameState>>(port, peerPort);
    init();

    while (!WindowShouldClose()) {
        mnet->poll();
        mnet->send(state);

        if (mnet->hasPeer())
        {
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
        }

        render();
    }

    CloseWindow();
}
