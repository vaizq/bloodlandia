#include "protocol.h"
#include "server.h"
#include <chrono>
#include "util.h"
#include <set>


using namespace std::chrono_literals;

static proto::ID nextID() {
    static proto::ID id{1};
    return id++;
}

constexpr auto bulletLiveDuration = 1000ms;

std::map<udp::endpoint, proto::ID> playerIDs;
std::vector<proto::Bullet> bullets;
std::vector<proto::Player> players;
proto::GameState state;
int tickrate;

proto::Player& findPlayer(proto::ID id) {
        auto it = std::find_if(players.begin(), players.end(), [id](const proto::Player& p) {
                return p.id == id;
            }
        );

        if (it == players.end()) {
            throw std::runtime_error(std::format("ERROR\t unable to find player {}", id));
        }

        return *it;
}

rl::Vector2 spawnPos() {
    return {RandFloat(100)-50, RandFloat(100)-50};
}

template <typename T>
std::pair<proto::Header, T> parseMessage(const udp::endpoint& ep, const char* data, size_t n) {
        if (n < sizeof (proto::Header)) {
            throw std::runtime_error("ERROR\t payloadsize smaller than header size");
        }

        proto::Header h;
        std::memcpy(&h, data, sizeof h);

        if (!playerIDs.contains(ep)) {
            const proto::ID id = nextID();
            playerIDs[ep] = id;
            players.push_back(proto::Player{id, spawnPos()});
            h.playerId = id;
            printf("INFO\t new player connected %s:%d id %d\n", ep.address().to_string().c_str(), ep.port(), id);
        }

        if (const auto id = playerIDs[ep]; id != h.playerId) {
            throw std::runtime_error(std::format("ERROR\t invalid id {} should be {}", h.playerId, id));
        }

        if (h.payloadSize != n - sizeof h) {
            throw std::runtime_error("ERROR\t invalid message size");
        }

        if (h.payloadSize != sizeof (T)) {
            throw std::runtime_error("ERROR\t invalid payload size");
        }

        T t;
        std::memcpy(&t, data + sizeof h, sizeof t);
        return {h, t};
}

void sendUpdate(Server& server) {

    for (const auto& [ep, id] : playerIDs) {
        // TODO: Serialization
        const proto::Header h{id, sizeof state};
        constexpr size_t n = sizeof h + sizeof state;
        char buf[n];
        std::memcpy(buf, &h, sizeof h);
        std::memcpy(buf + sizeof h, &state, sizeof state);

        server.write(proto::updateChannel, ep, buf, n);
    }
}

int main(int argc, char** argv) 
{
    if (argc != 3) {
        printf("usage: app port tickrate\n");
        return -1;
    }

    unsigned short port = std::atoi(argv[1]);
    tickrate = std::atoi(argv[2]);

    printf("server listening on port %d with tickrate %d\n", port, tickrate);

    Server server{port};

    server.listen(proto::moveChannel, [](const udp::endpoint& ep, char* data, size_t n) {
        try {
            const auto [h, move] = parseMessage<proto::Move>(ep, data, n);
            proto::Player& p = findPlayer(h.playerId);
            float velo = length(move.velo);
            if (velo > 0) {
                p.velo = proto::playerSpeed * move.velo / length(move.velo);
            } else {
                p.velo = move.velo;
            }
        } catch(const std::exception& e) {
            fprintf(stderr, "%s\n", e.what());
        }
    });

    server.listen(proto::mouseMoveChannel, [](const udp::endpoint& ep, char* data, size_t n) {
        try {
            const auto [h, move] = parseMessage<proto::MouseMove>(ep, data, n);
            proto::Player& p = findPlayer(h.playerId);
            p.target = move.pos;
        } catch(const std::exception& e) {
            fprintf(stderr, "%s\n", e.what());
        }
    });


    server.listen(proto::shootChannel, [](const udp::endpoint& ep, char* data, size_t n) {
        try {
            const auto [h, shoot] = parseMessage<proto::Shoot>(ep, data, n);
            proto::Player& p = findPlayer(h.playerId);
            bullets.push_back(shoot.bullet);
            bullets.back().createdAt = Clock::now();
        } catch(const std::exception& e) {
            fprintf(stderr, "%s\n", e.what());
        }
    });

    auto prevUpdate = Clock::now();
    const std::chrono::duration<float> dt(1.0f/tickrate);
    for(;;) {
        const auto t0 = Clock::now();

        {
            std::set<proto::ID> killed;
            for (const auto& b : bullets) {
                for (auto& p : players) {
                    const float dist = distance(p.pos, b.pos);
                    if (dist < proto::playerRadius && !killed.contains(p.id) && p.id != b.shooterID) {
                        printf("player %d killed player %d\n", b.shooterID, p.id);
                        p.stats.deaths++;
                        p.pos = spawnPos();
                        p.velo = rl::Vector2{0, 0};
                        killed.insert(p.id);
                        findPlayer(b.shooterID).stats.kills++;
                    }
                }
            }
        }

        bullets.erase(std::remove_if(bullets.begin(), bullets.end(), [](const auto& bullet) {
            return Clock::now() - bullet.createdAt > bulletLiveDuration;
        }), bullets.end());

        {
            std::set<proto::ID> timedOutPlayers;
            for (auto it = playerIDs.begin(); it != playerIDs.end();) {
                const auto ping = server.getPing(it->first);
                if (ping > 500ms) {
                    printf("player %d timed out\n", it->second);
                    timedOutPlayers.insert(it->second);
                    it = playerIDs.erase(it);
                } else {
                    ++it;
                }
            }

            players.erase(std::remove_if(players.begin(), players.end(), [&timedOutPlayers](const auto& player) {
                const auto age = Clock::now() - player.createdAt;
                return age > 1000ms && timedOutPlayers.contains(player.id);
            }), players.end());
        }

        {
            // Handle collisions
            for (auto ita = players.begin(); ita != players.end(); ++ita) {
                for (auto itb = ita + 1; itb != players.end(); ++itb) {
                    const auto diff = ita->pos - itb->pos;
                    const float dist = length(diff);
                    if (dist < 2 * proto::playerRadius) {
                        const auto move = (2 * proto::playerRadius - dist) * diff / dist;
                        ita->pos = ita->pos + move;
                        itb->pos = itb->pos - move;
                        printf("collision handled!\n");
                    }
                }
            }
        }

        state.numPlayers = 0;
        for (auto& p : players) {
            p.pos = p.pos + dt.count() * p.velo;
            state.players[state.numPlayers++] = p;
        }

        state.numBullets = 0;
        for (auto& b : bullets) {
            b.pos = b.pos + dt.count() * b.velo;
            state.bullets[state.numBullets++] = b;
        }

        sendUpdate(server);

        server.poll();
        while (Clock::now() - t0 < dt) {
            server.poll();
        }
    }

    return 0;
}
