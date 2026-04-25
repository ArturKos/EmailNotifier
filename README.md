# EmailNotifier

A terminal **POP3 new-mail notifier** written in C. It polls a mailbox every 60 seconds over raw BSD sockets, tracks message UIDs to detect genuinely new arrivals, and displays status through an ncurses interface.

![C](https://img.shields.io/badge/C-11-A8B9CC?logo=c)
![CMake](https://img.shields.io/badge/build-CMake-064F8C?logo=cmake)
![GoogleTest](https://img.shields.io/badge/tests-GoogleTest-4285F4)
![Doxygen](https://img.shields.io/badge/docs-Doxygen-2C4AA8)
![Linux](https://img.shields.io/badge/Linux-BSD_Sockets-FCC624?logo=linux)

## Highlights

- **Raw POP3 protocol** (`USER`, `PASS`, `UIDL`, `NOOP`, `QUIT`) over TCP - no external mail library.
- **Clean separation of concerns**: protocol (`pop3_client`), persistence/diff (`uidl_store`), and UI (`main`) live in distinct translation units, each with a narrow public API.
- **Safe string handling**: all buffer writes use sized `snprintf` - no `strcat` overflow risk.
- **Modern DNS**: uses `getaddrinfo()` (IPv4-capable, thread-safe) rather than deprecated `gethostbyname()`.
- **Unit + integration tests** with GoogleTest. Network tests spin up a scripted mock POP3 server on an ephemeral loopback port - no real servers, no flakiness.
- **Doxygen-documented public headers**; HTML docs built via `cmake --build build --target docs`.

## Building

Requires GCC or Clang, CMake ≥ 3.14, ncurses development headers, and (for docs) Doxygen. Network access is needed on the first build so CMake can fetch GoogleTest.

```bash
# Ubuntu / Debian
sudo apt-get install build-essential cmake libncurses-dev doxygen

# Arch
sudo pacman -S base-devel cmake ncurses doxygen
```

Build the executable and tests:

```bash
cmake -S . -B build
cmake --build build -j
```

## Running

```bash
./build/notifier <pop3_server> <username> <password>
```

Example:

```bash
./build/notifier pop3.example.com alice hunter2
```

The notifier connects, authenticates, fetches the UIDL list, compares it against `known_uids.txt` in the working directory, and prints the count of new messages. It repeats every 60 seconds. Press any key to exit.

> **Security note:** POP3 on port 110 is **cleartext** - credentials are sent in the clear. This codebase is intended for trusted networks (home LAN, education) only. Do not point it at a production mailbox over the public internet without an encrypted tunnel.

## Testing

```bash
ctest --test-dir build --output-on-failure
```

Tests cover:

- **`uidl_store`** - UIDL response parsing, new-ID counting, atomic file promotion (pure functions against `mkstemp`-backed temp files).
- **`pop3_client`** - full `USER`/`PASS`/`UIDL`/`QUIT`/`NOOP` exchanges against a threaded loopback mock server that listens on an ephemeral port.

## Documentation

```bash
cmake --build build --target docs
xdg-open docs/build/html/index.html
```

or run Doxygen directly:

```bash
doxygen docs/Doxyfile
```

## Project layout

```
EmailNotifier/
├── CMakeLists.txt               # Build + test + docs targets
├── include/
│   ├── pop3_client.h            # Public POP3 client API
│   └── uidl_store.h             # Public UIDL parse/diff/persist API
├── src/
│   ├── pop3_client.c            # POP3 protocol mechanics
│   ├── uidl_store.c             # UIDL parsing and diff logic
│   └── main.c                   # ncurses loop and 60s poll cadence
├── tests/
│   ├── test_pop3_client.cpp     # Integration tests with loopback mock server
│   └── test_uidl_store.cpp      # Unit tests for parse / diff / promote
├── docs/
│   └── Doxyfile                 # Doxygen configuration
├── .gitignore
└── README.md
```

Files written at runtime (gitignored):

- `known_uids.txt` - persistent list of seen message UIDs.
- `pending_uids.txt` - current poll's UIDL response, promoted atomically when new mail is detected.

## Protocol flow

1. `pop3_client_connect()` resolves the hostname and opens a TCP session, verifying the server greeting.
2. `pop3_client_login()` sends `USER` and `PASS`, verifying a `+OK` on each.
3. `pop3_client_fetch_uidl()` streams the UIDL response into a pending file via `uidl_store_extract_ids()`.
4. `uidl_store_count_new()` diffs the pending file against the known-IDs file.
5. If new IDs are found, `uidl_store_promote()` atomically renames the pending file over the known-IDs file (POSIX `rename` guarantee).
6. `pop3_client_logout()` sends `QUIT`; `pop3_client_disconnect()` closes the socket.

## License

Provided as-is for educational use.
