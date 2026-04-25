#include "pop3_client.h"
#include "uidl_store.h"

#include <arpa/inet.h>
#include <errno.h>
#include <stdbool.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

static const char POP3_OK_PREFIX[] = "+OK";
static const size_t POP3_OK_PREFIX_LEN = sizeof(POP3_OK_PREFIX) - 1;

static const char POP3_RESPONSE_TERMINATOR[] = "\r\n.\r\n";
static const size_t POP3_RESPONSE_TERMINATOR_LEN = sizeof(POP3_RESPONSE_TERMINATOR) - 1;

/**
 * @brief Send a complete command to the server using repeated send() calls.
 *
 * Returns 0 on success, -1 on short-write or send error.
 */
static int send_all(int socket_fd, const char *data, size_t length) {
    size_t total_sent = 0;
    while (total_sent < length) {
        ssize_t sent_now = send(socket_fd, data + total_sent, length - total_sent, 0);
        if (sent_now <= 0) {
            return -1;
        }
        total_sent += (size_t)sent_now;
    }
    return 0;
}

/**
 * @brief Read a single-line response into @p buffer and return its length.
 *
 * The buffer is NUL-terminated on success. Returns -1 on recv failure.
 */
static ssize_t recv_line(int socket_fd, char *buffer, size_t buffer_size) {
    if (buffer_size == 0) {
        return -1;
    }
    ssize_t received = recv(socket_fd, buffer, buffer_size - 1, 0);
    if (received < 0) {
        return -1;
    }
    buffer[received] = '\0';
    return received;
}

static bool response_is_ok(const char *buffer, ssize_t length) {
    return length >= (ssize_t)POP3_OK_PREFIX_LEN &&
           memcmp(buffer, POP3_OK_PREFIX, POP3_OK_PREFIX_LEN) == 0;
}

int pop3_client_connect(const char *host, unsigned short port) {
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    char port_string[6];
    snprintf(port_string, sizeof(port_string), "%u", port);

    struct addrinfo *resolved = NULL;
    if (getaddrinfo(host, port_string, &hints, &resolved) != 0 || resolved == NULL) {
        return -1;
    }

    int socket_fd = socket(resolved->ai_family, resolved->ai_socktype, resolved->ai_protocol);
    if (socket_fd < 0) {
        freeaddrinfo(resolved);
        return -1;
    }

    if (connect(socket_fd, resolved->ai_addr, resolved->ai_addrlen) < 0) {
        close(socket_fd);
        freeaddrinfo(resolved);
        return -1;
    }

    freeaddrinfo(resolved);

    char greeting[POP3_RECV_BUFFER_SIZE];
    ssize_t greeting_length = recv_line(socket_fd, greeting, sizeof(greeting));
    if (!response_is_ok(greeting, greeting_length)) {
        close(socket_fd);
        return -1;
    }

    return socket_fd;
}

bool pop3_client_login(int socket_fd, const char *username, const char *password) {
    char command[POP3_RECV_BUFFER_SIZE];
    char response[POP3_RECV_BUFFER_SIZE];

    int written = snprintf(command, sizeof(command), "USER %s\r\n", username);
    if (written < 0 || (size_t)written >= sizeof(command)) {
        return false;
    }
    if (send_all(socket_fd, command, (size_t)written) != 0) {
        return false;
    }
    if (!response_is_ok(response, recv_line(socket_fd, response, sizeof(response)))) {
        return false;
    }

    written = snprintf(command, sizeof(command), "PASS %s\r\n", password);
    if (written < 0 || (size_t)written >= sizeof(command)) {
        return false;
    }
    if (send_all(socket_fd, command, (size_t)written) != 0) {
        return false;
    }
    return response_is_ok(response, recv_line(socket_fd, response, sizeof(response)));
}

int pop3_client_fetch_uidl(int socket_fd, FILE *out) {
    static const char command[] = "UIDL\r\n";
    if (send_all(socket_fd, command, sizeof(command) - 1) != 0) {
        return -1;
    }

    char buffer[POP3_RECV_BUFFER_SIZE];
    for (;;) {
        ssize_t received = recv(socket_fd, buffer, sizeof(buffer), 0);
        if (received < 0) {
            return -1;
        }
        if (received == 0) {
            break;
        }
        uidl_store_extract_ids(buffer, (size_t)received, out);
        if ((size_t)received >= POP3_RESPONSE_TERMINATOR_LEN &&
            memcmp(buffer + received - POP3_RESPONSE_TERMINATOR_LEN,
                   POP3_RESPONSE_TERMINATOR,
                   POP3_RESPONSE_TERMINATOR_LEN) == 0) {
            break;
        }
    }
    return 0;
}

bool pop3_client_logout(int socket_fd) {
    static const char command[] = "QUIT\r\n";
    if (send_all(socket_fd, command, sizeof(command) - 1) != 0) {
        return false;
    }
    char response[POP3_RECV_BUFFER_SIZE];
    return response_is_ok(response, recv_line(socket_fd, response, sizeof(response)));
}

bool pop3_client_send_nop(int socket_fd) {
    static const char command[] = "NOOP\r\n";
    if (send_all(socket_fd, command, sizeof(command) - 1) != 0) {
        return false;
    }
    char response[POP3_RECV_BUFFER_SIZE];
    return response_is_ok(response, recv_line(socket_fd, response, sizeof(response)));
}

void pop3_client_disconnect(int socket_fd) {
    if (socket_fd >= 0) {
        close(socket_fd);
    }
}
