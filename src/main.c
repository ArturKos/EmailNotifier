/**
 * @file main.c
 * @brief Entry point and ncurses-driven polling loop for the POP3 notifier.
 */

#include "pop3_client.h"
#include "uidl_store.h"

#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define SECONDS_BETWEEN_POLLS 60
#define KNOWN_IDS_PATH "known_uids.txt"
#define PENDING_IDS_PATH "pending_uids.txt"

static void show_banner(void) {
    clear();
    printw("Email notifier - watches a POP3 mailbox for new messages.\n");
    printw("Press any key to exit.\n");
}

static int count_new_messages(const char *pending_path) {
    FILE *known = fopen(KNOWN_IDS_PATH, "rb");
    FILE *pending = fopen(pending_path, "rb");
    if (pending == NULL) {
        if (known != NULL) {
            fclose(known);
        }
        return 0;
    }

    int new_count = uidl_store_count_new(known, pending);

    if (known != NULL) {
        fclose(known);
    }
    fclose(pending);
    return new_count;
}

/**
 * @brief Run one full poll: connect, login, fetch UIDL, diff, promote.
 *
 * The function is self-contained - on any failure it tears down whatever
 * resources it opened and returns without leaving state around.
 */
static void poll_mailbox(const char *host, const char *username, const char *password) {
    int socket_fd = pop3_client_connect(host, POP3_DEFAULT_PORT);
    if (socket_fd < 0) {
        printw("Could not connect to server: %s\n", host);
        return;
    }

    if (!pop3_client_login(socket_fd, username, password)) {
        printw("Login failed - check username and password.\n");
        pop3_client_disconnect(socket_fd);
        return;
    }

    FILE *pending = fopen(PENDING_IDS_PATH, "w");
    if (pending == NULL) {
        printw("Could not open %s for writing.\n", PENDING_IDS_PATH);
        pop3_client_logout(socket_fd);
        pop3_client_disconnect(socket_fd);
        return;
    }

    if (pop3_client_fetch_uidl(socket_fd, pending) != 0) {
        printw("Failed to retrieve message list.\n");
        fclose(pending);
        pop3_client_logout(socket_fd);
        pop3_client_disconnect(socket_fd);
        return;
    }
    fclose(pending);

    const int new_messages = count_new_messages(PENDING_IDS_PATH);
    if (new_messages > 0) {
        printw("Received %d new message(s)!\n", new_messages);
        uidl_store_promote(KNOWN_IDS_PATH, PENDING_IDS_PATH);
    } else {
        printw("No new messages.\n");
    }

    pop3_client_logout(socket_fd);
    pop3_client_disconnect(socket_fd);
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <pop3_server> <username> <password>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *host = argv[1];
    const char *username = argv[2];
    const char *password = argv[3];

    initscr();
    noecho();
    nodelay(stdscr, TRUE);

    int seconds_since_last_poll = SECONDS_BETWEEN_POLLS;
    while (getch() == ERR) {
        if (seconds_since_last_poll >= SECONDS_BETWEEN_POLLS) {
            show_banner();
            printw("Checking %s ...\n", host);
            refresh();
            poll_mailbox(host, username, password);
            refresh();
            seconds_since_last_poll = 0;
        }
        sleep(1);
        ++seconds_since_last_poll;
    }

    endwin();
    return EXIT_SUCCESS;
}
