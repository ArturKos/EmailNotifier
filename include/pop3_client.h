/**
 * @file pop3_client.h
 * @brief Minimal POP3 protocol client over raw BSD sockets.
 *
 * Implements a small subset of RFC 1939: USER, PASS, UIDL, NOP, QUIT.
 * The client is connection-oriented - the caller opens a socket, authenticates,
 * issues commands, then disconnects. No TLS, no APOP, no pipelining.
 */

#ifndef EMAILNOTIFIER_POP3_CLIENT_H
#define EMAILNOTIFIER_POP3_CLIENT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Default POP3 port (cleartext). */
#define POP3_DEFAULT_PORT 110

/** Maximum bytes read from the server in a single recv call. */
#define POP3_RECV_BUFFER_SIZE 1024

/**
 * @brief Resolve @p host and open a TCP connection on the given port.
 *
 * @param host Hostname or dotted-quad IPv4 address.
 * @param port TCP port (use POP3_DEFAULT_PORT for standard cleartext POP3).
 * @return Connected socket descriptor on success, -1 on resolution or connect failure.
 */
int pop3_client_connect(const char *host, unsigned short port);

/**
 * @brief Authenticate against the POP3 server using USER / PASS.
 *
 * @param socket_fd Connected socket returned by pop3_client_connect().
 * @param username POP3 account name, NUL-terminated.
 * @param password POP3 account password, NUL-terminated.
 * @return true when both commands receive a "+OK" reply; false otherwise.
 */
bool pop3_client_login(int socket_fd, const char *username, const char *password);

/**
 * @brief Send the UIDL command and write the raw response to @p out.
 *
 * The response is streamed to @p out verbatim (including the POP3 framing).
 * Use uidl_store_extract_ids() to normalize it into a bare-ID list.
 *
 * @param socket_fd Authenticated POP3 session socket.
 * @param out Open-for-write FILE* stream; must be writable.
 * @return 0 on success, -1 on socket error.
 */
int pop3_client_fetch_uidl(int socket_fd, FILE *out);

/**
 * @brief Send a QUIT command, closing the POP3 session gracefully.
 *
 * @param socket_fd Active POP3 session socket.
 * @return true when the server answers "+OK", false otherwise.
 */
bool pop3_client_logout(int socket_fd);

/**
 * @brief Send a NOP (no-op) keepalive and read the server reply.
 *
 * @param socket_fd Active POP3 session socket.
 * @return true when the server answers "+OK", false otherwise.
 */
bool pop3_client_send_nop(int socket_fd);

/**
 * @brief Close the underlying TCP socket.
 *
 * @param socket_fd Socket descriptor to close.
 */
void pop3_client_disconnect(int socket_fd);

#ifdef __cplusplus
}
#endif

#endif /* EMAILNOTIFIER_POP3_CLIENT_H */
