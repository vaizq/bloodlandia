#include "protocol.h"
#include "server.h"
#include <chrono>
#include <optional>
#include "util.h"


using Clock = std::chrono::high_resolution_clock;


static proto::ID nextID() {
    static proto::ID id{1};
    return id++;
}

std::map<udp::endpoint, proto::ID> playerIDs;
proto::GameState state;
int tickrate;

proto::Player& findPlayer(proto::ID id) {
        const auto end = state.players + state.numPlayers * sizeof (proto::Player);
        auto it = std::find_if(state.players, end, 
            [id](const proto::Player& p) {
                return p.id == id;
            }
        );

        if (it == end) {
            throw std::runtime_error(std::format("ERROR\t unable to find player {}", id));
        }

        return *it;
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
            h.playerId = id;
            state.players[state.numPlayers++] = proto::Player{id};

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
        proto::Header h{id, sizeof state};
        Message msg{ep, {(const char*)&h, ((const char*)&h) + sizeof h}};
        msg.payload.insert(msg.payload.end(), (const char*)&state, ((const char*)&state) + sizeof state);

        server.write(proto::updateChannel, msg);
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


    Server server{port};

    server.listen(proto::moveChannel, [](const udp::endpoint& ep, char* data, size_t n) {
        try {
            const auto [h, move] = parseMessage<proto::Move>(ep, data, n);
            proto::Player& p = findPlayer(h.playerId);
            p.velo = move.velo;
        } catch(const std::exception& e) {
            fprintf(stderr, "%s\n", e.what());
        }
    });

    server.listen(proto::shootChannel, [](const udp::endpoint& ep, char* data, size_t n) {
        try {
            const auto [h, shoot] = parseMessage<proto::Shoot>(ep, data, n);
            proto::Player& p = findPlayer(h.playerId);
            printf("player %d shoot target is (%.1f, %.1f)\n", p.id, shoot.target.x, shoot.target.y);
        } catch(const std::exception& e) {
            fprintf(stderr, "%s\n", e.what());
        }
    });

    auto prevUpdate = Clock::now();
    const std::chrono::duration<float> dt(1.0f/tickrate);
    for(;;) {
        const auto t0 = Clock::now();

        // Update world
        for (int i = 0; i < state.numPlayers; ++i) {
            proto::Player& p = state.players[i];
            p.pos = p.pos + dt.count() * p.velo;
        }

        sendUpdate(server);

        server.poll();
        while (Clock::now() - t0 < dt) {
            server.poll();
        }
    }


    return 0;
}
