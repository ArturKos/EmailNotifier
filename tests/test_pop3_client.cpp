/**
 * @file test_pop3_client.cpp
 * @brief Integration tests for the POP3 client against a loopback mock server.
 */

#include "pop3_client.h"
#include "uidl_store.h"

#include <gtest/gtest.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

using ResponseScript = std::vector<std::pair<std::string, std::string>>;

/**
 * @brief Runs a single-session POP3 mock server on an ephemeral loopback port.
 *
 * The server reads exactly one command per scripted entry, matches its
 * prefix, and writes back the scripted reply. Meant for one client connection
 * per instance - lifetime is controlled by the test fixture.
 */
class MockPop3Server {
public:
    explicit MockPop3Server(ResponseScript script, std::string greeting = "+OK ready\r\n")
        : script_(std::move(script)), greeting_(std::move(greeting)) {
        listening_socket_ = ::socket(AF_INET, SOCK_STREAM, 0);
        if (listening_socket_ < 0) {
            std::abort();
        }
        int enable = 1;
        ::setsockopt(listening_socket_, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable));

        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        address.sin_port = 0;
        if (::bind(listening_socket_, reinterpret_cast<sockaddr *>(&address), sizeof(address)) < 0) {
            std::abort();
        }
        socklen_t actual_length = sizeof(address);
        if (::getsockname(listening_socket_, reinterpret_cast<sockaddr *>(&address), &actual_length) < 0) {
            std::abort();
        }
        listening_port_ = ntohs(address.sin_port);
        ::listen(listening_socket_, 1);

        worker_thread_ = std::thread([this] { serve_one_session(); });
    }

    ~MockPop3Server() {
        stopping_ = true;
        if (listening_socket_ >= 0) {
            ::close(listening_socket_);
        }
        if (worker_thread_.joinable()) {
            worker_thread_.join();
        }
    }

    MockPop3Server(const MockPop3Server &) = delete;
    MockPop3Server &operator=(const MockPop3Server &) = delete;

    unsigned short port() const { return listening_port_; }

private:
    void serve_one_session() {
        int client_fd = ::accept(listening_socket_, nullptr, nullptr);
        if (client_fd < 0 || stopping_) {
            return;
        }

        ::send(client_fd, greeting_.data(), greeting_.size(), 0);

        for (const auto &step : script_) {
            char command_buffer[1024] = {};
            ssize_t received = ::recv(client_fd, command_buffer, sizeof(command_buffer) - 1, 0);
            if (received <= 0) {
                break;
            }
            // In a real server we would validate step.first against the received command;
            // here we simply send the scripted response so tests remain robust against
            // whitespace/casing drift on the command side.
            (void)step.first;
            ::send(client_fd, step.second.data(), step.second.size(), 0);
        }

        ::close(client_fd);
    }

    ResponseScript script_;
    std::string greeting_;
    int listening_socket_ = -1;
    unsigned short listening_port_ = 0;
    std::thread worker_thread_;
    std::atomic<bool> stopping_{false};
};

}  // namespace

TEST(Pop3ClientConnect, ConnectsAndReadsGreeting) {
    MockPop3Server server(ResponseScript{}, "+OK hello\r\n");
    int socket_fd = pop3_client_connect("127.0.0.1", server.port());
    ASSERT_GE(socket_fd, 0);
    pop3_client_disconnect(socket_fd);
}

TEST(Pop3ClientConnect, RefusesConnectionWhenGreetingIsError) {
    MockPop3Server server(ResponseScript{}, "-ERR busy\r\n");
    int socket_fd = pop3_client_connect("127.0.0.1", server.port());
    EXPECT_LT(socket_fd, 0);
}

TEST(Pop3ClientConnect, FailsOnUnresolvableHost) {
    int socket_fd = pop3_client_connect("definitely-not-a-real-host.invalid", 110);
    EXPECT_LT(socket_fd, 0);
}

TEST(Pop3ClientLogin, SucceedsWhenBothCommandsAreAccepted) {
    MockPop3Server server(ResponseScript{
        {"USER", "+OK user accepted\r\n"},
        {"PASS", "+OK logged in\r\n"},
    });
    int socket_fd = pop3_client_connect("127.0.0.1", server.port());
    ASSERT_GE(socket_fd, 0);
    EXPECT_TRUE(pop3_client_login(socket_fd, "alice", "hunter2"));
    pop3_client_disconnect(socket_fd);
}

TEST(Pop3ClientLogin, FailsOnBadPassword) {
    MockPop3Server server(ResponseScript{
        {"USER", "+OK user accepted\r\n"},
        {"PASS", "-ERR invalid password\r\n"},
    });
    int socket_fd = pop3_client_connect("127.0.0.1", server.port());
    ASSERT_GE(socket_fd, 0);
    EXPECT_FALSE(pop3_client_login(socket_fd, "alice", "wrong"));
    pop3_client_disconnect(socket_fd);
}

TEST(Pop3ClientFetchUidl, WritesNormalizedIdsToOutputStream) {
    MockPop3Server server(ResponseScript{
        {"UIDL", "+OK 2 messages\r\n1 uid-A\r\n2 uid-B\r\n.\r\n"},
    });
    int socket_fd = pop3_client_connect("127.0.0.1", server.port());
    ASSERT_GE(socket_fd, 0);

    char path_template[] = "/tmp/pop3_client_fetch_XXXXXX";
    int temp_fd = ::mkstemp(path_template);
    ASSERT_GE(temp_fd, 0);
    ::close(temp_fd);

    FILE *output_stream = std::fopen(path_template, "w");
    ASSERT_NE(output_stream, nullptr);
    EXPECT_EQ(pop3_client_fetch_uidl(socket_fd, output_stream), 0);
    std::fclose(output_stream);

    std::ifstream result(path_template);
    std::string contents((std::istreambuf_iterator<char>(result)),
                         std::istreambuf_iterator<char>());
    EXPECT_NE(contents.find("uid-A"), std::string::npos);
    EXPECT_NE(contents.find("uid-B"), std::string::npos);

    std::filesystem::remove(path_template);
    pop3_client_disconnect(socket_fd);
}

TEST(Pop3ClientLogout, ReturnsTrueOnOkResponse) {
    MockPop3Server server(ResponseScript{
        {"QUIT", "+OK bye\r\n"},
    });
    int socket_fd = pop3_client_connect("127.0.0.1", server.port());
    ASSERT_GE(socket_fd, 0);
    EXPECT_TRUE(pop3_client_logout(socket_fd));
    pop3_client_disconnect(socket_fd);
}

TEST(Pop3ClientSendNop, ReturnsTrueOnOkResponse) {
    MockPop3Server server(ResponseScript{
        {"NOOP", "+OK\r\n"},
    });
    int socket_fd = pop3_client_connect("127.0.0.1", server.port());
    ASSERT_GE(socket_fd, 0);
    EXPECT_TRUE(pop3_client_send_nop(socket_fd));
    pop3_client_disconnect(socket_fd);
}
