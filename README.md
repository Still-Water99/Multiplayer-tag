# Multiplayer Tag

A real-time multiplayer tag game built with C++ (SFML + UDP sockets) and a Python stats visualizer.

---

## Requirements

### Server & Client
- C++17 compiler (`g++` or MSVC)
- **Windows:** Winsock2 (included with Windows SDK)
- **Linux/macOS:** POSIX sockets (standard)
- [SFML 2.x](https://www.sfml-dev.org/download.php) (client only)

### Stats Visualizer
- Python 3.8+
- `pandas`, `matplotlib`

```bash
pip install pandas matplotlib
```

---

## Building

### Server

**Windows:**
```bash
g++ server.cpp -o server -lws2_32
```

**Linux/macOS:**
```bash
g++ server.cpp -o server
```

### Client

**Windows:**
```bash
g++ client.cpp -o client -lws2_32 -lsfml-graphics -lsfml-window -lsfml-system
```

**Linux/macOS:**
```bash
g++ client.cpp -o client -lsfml-graphics -lsfml-window -lsfml-system
```

> Make sure SFML headers and libraries are on your include/library paths. If SFML is installed to a custom location, add `-I/path/to/sfml/include -L/path/to/sfml/lib`.

---

## Configuration

Before building the client, set the server IP in `client.cpp`:

```cpp
#define SERVER_IP "169.254.18.240"   // ← change to your server's IP
```

The server prints its IP on startup. Other constants you may want to adjust:

| Constant | File | Default | Description |
|---|---|---|---|
| `SERVER_PORT` | both | `9999` | UDP port |
| `MAX_PLAYERS` | both | `8` | Max simultaneous players |
| `MIN_PLAYERS` | server | `2` | Players needed to start |
| `GAME_DURATION` | server | `120` | Round length in seconds |

The font file `GoogleSans.ttf` must be present in the **same directory as the client executable**. The client will render without text if the font is missing.

---

## Running

### 1. Start the server

```bash
./server
```

The server will print its IP address and begin listening on port `9999`. It writes a `stats.csv` log to the working directory.

### 2. Launch clients

Run the client on each player's machine:

```bash
./client
```

Click **START** on the title screen to join the game. The game begins automatically once at least 2 players have joined.

### Gameplay

| Color | Meaning |
|---|---|
| 🟡 Yellow | You (not "it") |
| 🟣 Magenta | You (you are "it") |
| 🟢 Green | Other player (not "it") |
| 🔴 Red | Other player (is "it") |
| 🔵 Cyan dots | Directional markers to other players (only visible when you are "it") |

- Move with **W A S D**.
- The player who is "it" moves faster; run them down to tag others.
- A tagged player cannot immediately re-tag (1-second grace period).
- The round timer counts down from 120 seconds. Survive without being "it" when time runs out to win.
- After a round ends there is a 15-second intermission before the next round starts.
- Players who stop sending packets for more than 2 seconds are removed automatically.
- Close the window to disconnect cleanly.

---

## Stats Visualizer

After a session, a `stats.csv` file is written by the server. Run the visualizer to plot network and gameplay statistics:

```bash
python stats.py
```

This generates `stats.png` and opens an interactive window with four plots:

- **Packet Loss %** over time per player
- **Cumulative packets received** per player
- **Player paths** (X/Y movement traces)
- **Who was "it"** over time

---

## Troubleshooting

**Port already in use**
Change `SERVER_PORT` in both files and rebuild.

**Client can't connect**
- Confirm `SERVER_IP` in `client.cpp` matches the address printed by the server.
- Check that port `9999` (UDP) is not blocked by a firewall.

**SFML not found at compile time**
Install SFML via your package manager or download from [sfml-dev.org](https://www.sfml-dev.org/download.php) and pass the correct `-I` / `-L` flags.

**Font not rendering**
Place `GoogleSans.ttf` in the same directory as the client binary. The game runs without it but text will not display.

**Windows: `WSAStartup failed`**
Ensure you are linking with `-lws2_32` and that the Windows SDK is installed.
