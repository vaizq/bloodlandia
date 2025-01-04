#include <asio/detached.hpp>
#include <asio/use_awaitable.hpp>
#include <chrono>
#include <stdio.h>
#include <asio.hpp>
#include <map>
#include "protocol.h"
#include <algorithm>
#include "util.h"


using udp = asio::ip::udp;
using Clock = std::chrono::high_resolution_clock;

static proto::ID nextID() {
    static proto::ID id{69};
    return id++;
}

std::map<udp::endpoint, proto::ID> playerIds;
proto::GameState state;

void handleDatagram(proto::ID playerID, char* data, size_t n) {
    if (n < sizeof (proto::Header)) {
        fprintf(stderr, "ERROR: datagramsize < sizeof header\n");
        return;
    }

    proto::Header h;
    std::memcpy(&h, data, sizeof h);

    if (sizeof h + h.payloadSize != n) {
        fprintf(stderr, "ERROR: payload size is not correct\n");
        return;
    }

    const auto end = state.players + state.numPlayers * sizeof (proto::Player);
    auto it = std::find_if(state.players, end, [id=h.playerId](const proto::Player& p) {
        return id == p.id;
    });

    if (it == end) {
        fprintf(stderr, "ERROR: No player with ID=%d in state!\n", h.playerId);
        return;
    }

    proto::Player& player = *it;

    switch (h.event) {
        case proto::Event::Move:
        {
            if (sizeof (proto::Move) != h.payloadSize) {
                printf("ERROR: invalid payload size for Move\n");
                return;
            }

            printf("received move event\n");

            proto::Move move;
            std::memcpy(&move, data + sizeof h, h.payloadSize);
            player.velo = move.velo;
            break;
        }
        case proto::Event::Shoot:
        {
            if (sizeof (proto::Shoot) != h.payloadSize) {
                printf("ERROR: invalid payload size for Shoot\n");
                return;
            }

            printf("received shoot event\n");

            proto::Shoot shoot;
            std::memcpy(&shoot, data + sizeof h, h.payloadSize);
            printf("player %d shot!\n", player.id);
            break;
        }
        default:
            fprintf(stderr, "ERROR: unhandled event received %d\n", static_cast<int>(h.event));
    }
}

asio::awaitable<void> listen(udp::socket& socket) {
    udp::endpoint peer;
    char buf[2048];
    for(;;) {
        size_t n = co_await socket.async_receive_from(
            asio::buffer(buf, sizeof buf),
            peer,
            asio::use_awaitable
        );

        if (!playerIds.contains(peer)) {
            printf("New connection!\n");
            const auto id = nextID();
            playerIds[peer] = id;
            state.players[state.numPlayers++].id = id;
            handleDatagram(id, buf, n);
        } else {
            handleDatagram(playerIds[peer], buf, n);
        }
    }
}

asio::awaitable<void> sendUpdate(udp::socket& socket, const udp::endpoint& endpoint, proto::ID id) {
    constexpr size_t n = sizeof (proto::Header) + sizeof state;
    char buf[n];
    proto::Header h{proto::Event::Update, id, sizeof state};
    std::memcpy(buf, &h, sizeof h);
    std::memcpy(buf + sizeof h, &state, sizeof state);

    size_t sent = co_await socket.async_send_to(
                        asio::buffer(buf, n),
                        endpoint,
                        asio::use_awaitable);
    if (sent != n) {
        fprintf(stderr, "ERROR: invalid number of bytes sent (%ld when should been %ld)\n", sent, n);
    }
}

asio::awaitable<void> update(udp::socket& socket) {
    asio::high_resolution_timer timer{socket.get_executor()};
    constexpr std::chrono::milliseconds interval{100};
    for (;;) {
        co_await timer.async_wait(asio::use_awaitable);
        timer.expires_after(interval);

        const float dt = std::chrono::duration_cast<std::chrono::duration<float>>(interval).count();
        for (int i = 0; i < state.numPlayers; ++i) {
            proto::Player& player = state.players[i];
            player.pos = player.pos + dt * player.velo;
        }

        for (const auto& [ep, id] : playerIds) {
            co_await sendUpdate(socket, ep, id);
        }
    }
}

int main(int argc, char** argv) 
{
    if (argc != 2) {
        printf("usage: app port\n");
        return -1;
    }

    unsigned short port = std::atoi(argv[1]);

    asio::io_context ioc;
    udp::socket socket{ioc, udp::endpoint{udp::v4(), port}};

    asio::co_spawn(ioc, listen(socket), asio::detached);
    asio::co_spawn(ioc, update(socket), asio::detached);

    const auto ep = socket.local_endpoint();
    printf("listeing on %s:%d\n", ep.address().to_string().c_str(), ep.port());

    ioc.run();

    return 0;
}
