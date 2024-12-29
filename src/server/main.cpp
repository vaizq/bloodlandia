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
    virtual asio::awaitable<void> send(ConnectInfo info) = 0;
};

using PlayerPtr = std::shared_ptr<Player>;

class MatchQueue
{
public:
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
            ConnectInfo playerInfo{9692, peer->address(), 9420, true};
            ConnectInfo peerInfo{9420, player->address(), 9692};

            co_spawn(executor, player->send(playerInfo), asio::detached);
            co_spawn(executor, peer->send(peerInfo), asio::detached); 
            printf("Matched two players\n");
        }
    }
private:
    std::queue<PlayerPtr> players;
};

class PlayerSession
: public Player, 
  public std::enable_shared_from_this<PlayerSession>
{
public:
    PlayerSession(tcp::socket socket, MatchQueue& mq)
    :   socket{std::move(socket)}, 
        mq{mq},
        timer{socket.get_executor()}
    {
        timer.expires_after(std::chrono::seconds::max());
        timer.async_wait([](auto){});
    }

    void start()
    {
        printf("starting player session\n");
        co_spawn(socket.get_executor(),
                 [self = shared_from_this()]{ return self->mq.join(self); },
                 asio::detached);
    }

    std::string address() override
    {
        return socket.remote_endpoint().address().to_string();
    }

    asio::awaitable<void> send(ConnectInfo info) override
    {
        Header h{PayloadType::ConnectInfo, sizeof info};
        char buf[sizeof h + sizeof info];
        std::memcpy(buf, &h, sizeof h);
        std::memcpy(buf + sizeof h, &info, sizeof info);
        co_await asio::async_write(socket, asio::buffer(buf, sizeof buf), asio::use_awaitable);
        timer.cancel();
    }

private:
    tcp::socket socket;
    MatchQueue& mq;
    asio::high_resolution_timer timer;
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
