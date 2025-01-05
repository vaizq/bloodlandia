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
std::pair<proto::Header, T> parseMessage(const Message& msg) {

        if (msg.payload.size() < sizeof (proto::Header)) {
            throw std::runtime_error("ERROR\t payloadsize smaller than header size");
        }

        proto::Header h;
        std::memcpy(&h, msg.payload.data(), sizeof h);

        if (!playerIDs.contains(msg.peer)) {
            printf("INFO\t new player connected\n");
            const proto::ID id = nextID();
            playerIDs[msg.peer] = id;
            h.playerId = id;
            state.players[state.numPlayers++] = proto::Player{h.playerId};
        }

        if (const auto id = playerIDs[msg.peer]; id != h.playerId) {
            throw std::runtime_error(std::format("ERROR\t invalid id {} should be {}", h.playerId, id));
        }

        if (h.payloadSize != msg.payload.size() - sizeof h) {
            throw std::runtime_error("ERROR\t invalid message size");
        }

        if (h.payloadSize != sizeof (T)) {
            throw std::runtime_error("ERROR\t invalid payload size");
        }

        T t;
        std::memcpy(&t, msg.payload.data() + sizeof h, sizeof t);
        return {h, t};
}

void sendUpdate(Server& server) {
    proto::Header h{0, sizeof state};
    Message msg{{}, {(const char*) &state, ((const char*)&state) + sizeof state}};

    for (const auto& [ep, id] : playerIDs) {
        msg.peer = ep;
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

    server.listen(proto::moveChannel, [](Message msg) {
        try {
            const auto [h, move] = parseMessage<proto::Move>(msg);
            proto::Player& p = findPlayer(h.playerId);
            p.velo = move.velo;
        } catch(const std::exception& e) {
            fprintf(stderr, "%s\n", e.what());
        }
    });

    server.listen(proto::shootChannel, [](Message msg) {
        try {
            const auto [h, shoot] = parseMessage<proto::Shoot>(msg);
            proto::Player& p = findPlayer(h.playerId);
            printf("player %d shoot (%.2f, %.2f)\n", p.id, shoot.direction.x, shoot.direction.y);
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

        while (Clock::now() - t0 < dt) {
            server.poll();
        }
    }


    return 0;
}
