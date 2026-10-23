# Space Station 13 Discord Rich Presence (RPSS13)

A lightweight, zero-configuration Discord Rich Presence client for **Space Station 13**. It automatically detects your running BYOND game, queries live round info from the server, and displays rich presence on your Discord profile.

---

## Quick Start

1. **Download or build** `RPSS13.exe`.
2. **Run `RPSS13.exe`** (you can start it before or after launching BYOND).
3. **Join any Space Station 13 server** — your presence updates automatically!

> **Works out of the box**: A default Discord Application ID with Space Station 13 icons is included, so no initial setup is required.

---

## What It Shows

```
Playing Space Station 13
NSS Cyberiad
Box Station (45 players)
00:46 elapsed
```

- **Server / Station Name**: Cleanly displays the station name or codebase title (e.g. *NSS Cyberiad*, */tg/Station 13*, *Paradise Station*).
- **Map & Player Count**: Shows the current station map and active player count.
- **Live Elapsed Round Time**: Seamlessly ticks the round elapsed time directly in Discord.
- **Menu / Connecting State**: Displays *"In Menus / Connecting"* when DreamSeeker is open but not yet connected to a server.
- **Automatic Cleanup**: Instantly clears your Rich Presence when you close the game.

---

## Configuration (`config.json`)

A `config.json` file is automatically generated next to the executable on first run:

```json
{
    "client_id": "1545495325366550630",
    "large_image_key": "ss13",
    "large_image_text": "Space Station 13",
    "small_image_key": "",
    "small_image_text": "",
    "local_poll_interval_seconds": 3,
    "server_refresh_seconds": 60,
    "failed_query_cooldown_seconds": 180,
    "topic_timeout_ms": 3000
}
```

### Using Your Own Discord Application & Assets
1. Go to the [Discord Developer Portal](https://discord.com/developers/applications) and create an application.
2. Under **Rich Presence -> Art Assets**, upload your custom icons.
3. Replace `"client_id"` and `"large_image_key"` in `config.json` (or pass your Client ID via command line: `RPSS13.exe <client_id>`).

---

## Building from Source

### Prerequisites
- CMake 3.30+
- C++17 compatible compiler (MinGW-w64 GCC 14+ or MSVC)
- Ninja or Make

### Build
```powershell
cmake -B build -G Ninja
cmake --build build
```

### Run
```powershell
.\build\RPSS13.exe
```

### Run Tests
```powershell
ctest --test-dir build --output-on-failure
```

---

<details>
<summary><b>Technical Details & Architecture (Click to expand)</b></summary>

### How It Works

```
                      +-------------------+
                      |   DreamSeeker     |
                      | (dreamseeker.exe) |
                      +---------+---------+
                                | (TCP socket)
                                v
+-------------+        +-------------------+
|   Discord   | <----- |  RPSS13 (Daemon)  | -----> [ BYOND SS13 Server ]
| Named Pipe  | (IPC)  |                   | (TCP)   (Topic: ?status)
+-------------+        +-------------------+
```

- **Non-Invasive Game Detection**: Inspects running `dreamseeker.exe` processes and the local Windows TCP connection table to locate active remote game endpoints. Requires no game files, BYOND modifications, or hooks.
- **Host-Friendly Topic Queries**: Queries the BYOND server's `?status` topic once per 60 seconds (rounds last 40-90+ minutes; Discord ticks elapsed time locally, so high-frequency polling is never needed).
- **Graceful Failure Backoff**: If a server blocks or disables topic queries, RPSS13 falls back to window-title detection and backs off for 3 minutes to avoid triggering host firewalls or rate limits.
- **Native Named-Pipe IPC**: Directly speaks Discord's local IPC protocol over Windows named pipes with zero third-party Discord SDK bloat.

### Source Structure
- `src/main.cpp` — Application loop, presence state machine, and signal handling.
- `src/server.h` / `src/server.cpp` — Server data model, URL decoding, and topic parsing (JSON & URL-encoded).
- `src/game_detector.h` / `src/game_detector.cpp` — Process scanning and TCP connection table inspection.
- `src/byond_topic.h` / `src/byond_topic.cpp` — Native BYOND binary `0x83` Topic client.
- `src/discord_rpc.h` / `src/discord_rpc.cpp` — Discord named-pipe IPC client.
- `src/config.h` — Configuration loader and JSON serializer.
- `tests/test_main.cpp` — CTest unit and integration test suite.

</details>