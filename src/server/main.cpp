#include <asio/co_spawn.hpp>
#include <asio/detached.hpp>
#include <asio/high_resolution_timer.hpp>
#include <asio/use_awaitable.hpp>
#include <memory>
#include <stdio.h>
#include <asio.hpp>
#include <queue>
#include "protocol.h"

// Put players in a queue and match them

using tcp = asio::ip::tcp;

struct Player
{
    virtual std::string address() = 0;
    virtual asio::awaitable<void> send(proto::ConnectInfo info) = 0;
};

using PlayerPtr = std::shared_ptr<Player>;


class MatchQueue
{
public:
    MatchQueue() {
        for (uint32_t i = 7000; i < 12000; ++i) {
            freePorts.push_back(i);
        }
    }
    asio::awaitable<void> join(PlayerPtr player)
    {
        auto executor = co_await asio::this_coro::executor;

        if (players.empty())
        {
            players.push(player);
        } 
        else
        {
            auto peer = players.front(); players.pop();

            uint32_t portA = freePorts.back(); freePorts.pop_back();
            uint32_t portB = freePorts.back(); freePorts.pop_back();

            try {
                proto::ConnectInfo info{portA, player->address(), portB, true};
                co_await peer->send(info);
            } catch (const std::exception& e) {
                fprintf(stderr, "Failed to send connect info to peer: %s\n", e.what());
                players.push(player);
                co_return;
            }

            try {
                proto::ConnectInfo info{portB, peer->address(), portA};
                co_await player->send(info);
            } catch (const std::exception& e) {
                fprintf(stderr, "Failed to send connect info to player: %s\n", e.what());
                players.push(peer);
                co_return;
            }
        }
    }
private:
    std::queue<PlayerPtr> players;
    std::vector<uint32_t> freePorts;
};

class PlayerSession
: public Player, 
  public std::enable_shared_from_this<PlayerSession>
{
public:
    PlayerSession(tcp::socket socket, MatchQueue& mq)
    :   socket{std::move(socket)}, 
        mq{mq}
    {
    }

    void start()
    {
        co_spawn(socket.get_executor(),
                 [self = shared_from_this()]{ return self->mq.join(self); },
                 asio::detached);
    }

    std::string address() override
    {
        return socket.remote_endpoint().address().to_string();
    }

    asio::awaitable<void> send(proto::ConnectInfo info) override
    {
        proto::Header h{proto::PayloadType::ConnectInfo, sizeof info};
        char buf[sizeof h + sizeof info];
        std::memcpy(buf, &h, sizeof h);
        std::memcpy(buf + sizeof h, &info, sizeof info);
        co_await asio::async_write(socket, asio::buffer(buf, sizeof buf), asio::use_awaitable);
    }

private:
    tcp::socket socket;
    MatchQueue& mq;
};


asio::awaitable<void> listener(unsigned short port)
{
    MatchQueue mq;
    auto executor = co_await asio::this_coro::executor;
    tcp::acceptor acceptor{executor, {tcp::v4(), port}};
    printf("listening at %s:%d\n", 
           acceptor.local_endpoint().address().to_string().c_str(),
           acceptor.local_endpoint().port()
    );
    for (;;)
    {
        std::make_shared<PlayerSession>(
            co_await acceptor.async_accept(asio::use_awaitable), 
            mq
        )->start();
    }
}

int main(int argc, char** argv) 
{
    if (argc != 2) {
        printf("usage: app port");
        return -1;
    }

    try
    {
        asio::io_context io_context(1);

        asio::signal_set signals(io_context, SIGINT, SIGTERM);
        signals.async_wait([&](auto, auto){ io_context.stop(); });

        const unsigned short port = atoi(argv[1]);
        co_spawn(io_context, listener(port), asio::detached);

        io_context.run();
    }
    catch (const std::exception& e)
    {
        std::printf("Exception: %s\n", e.what());
    }

    return 0;
}
