# Space Station 13 Discord Rich Presence (RPSS13)

#  !!!!!WIP!!!!! 

A lightweight Discord Rich Presence client for **Space Station 13**. Uses Discord's RPC over IPC server. It automatically detects your running BYOND game, queries live round info from the server, and displays rich presence on your Discord profile.

Inspects running `dreamseeker.exe` processes and the local Windows TCP connection table to locate active remote game endpoints.

---

## Quick Start

1. **Download or build** `RPSS13.exe`.
2. **Run `RPSS13.exe`** (you can start it before or after launching BYOND).
3. **Join any Space Station 13 server** — your presence updates automatically!

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
