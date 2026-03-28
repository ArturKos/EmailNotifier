# EmailNotifier

A terminal-based **POP3 email notifier** written in C that polls a mail server every 60 seconds over raw BSD sockets, tracks message UIDs to detect new arrivals, and displays status through an ncurses interface.

![C](https://img.shields.io/badge/C-99-A8B9CC?logo=c)
![Linux](https://img.shields.io/badge/Linux-BSD_Sockets-FCC624?logo=linux)
![ncurses](https://img.shields.io/badge/ncurses-Terminal_UI-4E9A06)
![POP3](https://img.shields.io/badge/POP3-Port_110-005AF0)

## Features

- **Raw POP3 protocol implementation** -- USER, PASS, UIDL, QUIT, and NOP commands sent over TCP sockets without external mail libraries
- **UIDL-based new mail detection** -- unique message IDs are stored locally and compared on each poll to identify only genuinely new messages
- **60-second polling interval** with non-blocking keyboard input so the user can exit at any time
- **ncurses terminal UI** with automatic screen clearing, status messages, and new mail count display
- **Persistent UID database** -- message IDs are saved to `baza.uidl` and updated atomically (write to `baza.uidlnew`, then rename) to survive restarts
- **DNS resolution** via `gethostbyname()` for server address lookup

## Dependencies

| Component | Version | Purpose |
|-----------|---------|---------|
| GCC | any | C compiler |
| ncurses (libncurses) | any | Terminal UI rendering |
| POSIX / Linux | any | BSD sockets, DNS resolution, `sleep()` |

### Installing dependencies (Ubuntu / Debian)

```bash
sudo apt-get install gcc libncurses5-dev
```

### Installing dependencies (Arch Linux)

```bash
sudo pacman -S gcc ncurses
```

## Building

```bash
gcc notifier.c -o notifier -lncurses
```

## Usage

```bash
./notifier <pop3_server> <username> <password>
```

| Argument | Description |
|----------|-------------|
| `pop3_server` | Hostname or IP of the POP3 mail server |
| `username` | POP3 account username |
| `password` | POP3 account password |

### Example

```bash
./notifier pop3.example.com myuser mypassword
```

The notifier will:

1. Connect to the POP3 server on port 110.
2. Authenticate with the provided credentials.
3. Retrieve the UIDL list and compare against the local database.
4. Display the count of new messages (or "No new messages").
5. Log out, close the connection, and sleep for 60 seconds before repeating.
6. Press any key to exit.

## How It Works

```
  [init()]         -- resolve hostname, create TCP socket, connect to port 110
      |
      v
  [logowanie()]    -- send USER + PASS commands, verify +OK responses
      |
      v
  [GetUIDL()]      -- send UIDL command, parse response into baza.uidlnew
      |
      v
  [CompareFiles()] -- diff baza.uidl vs baza.uidlnew, count new UIDs
      |
      v
  New mail? -----> rename baza.uidlnew to baza.uidl (atomic update)
      |
      v
  [wyloguj()]      -- send QUIT command
      |
      v
  [finito()]       -- close socket
      |
      v
  sleep(60)        -- wait, then repeat (non-blocking key check for exit)
```

## Project Structure

```
EmailNotifier/
├── notifier.c      # Main loop: ncurses UI, 60s polling, POP3 session management
├── notifier.h      # POP3 protocol: socket init, login, UIDL retrieval, UID comparison
├── baza.uidl       # Persistent UID database (known message IDs)
├── baza.uidlnew    # Temporary UID file (current poll, compared then promoted)
└── README.md
```

## License

This project is provided as-is for educational purposes.
