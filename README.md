# Space Station 13 Discord Rich Presence (RPSS13)

A lightweight, native C++ Discord Rich Presence client for **Space Station 13**. It automatically detects running BYOND (`dreamseeker.exe`) instances, inspects active TCP socket connections to locate the game server, queries the server's live game statistics via the BYOND Topic protocol, and broadcasts rich live presence to Discord.

---

## Features

- **Process & Connection Detection**: Detects `dreamseeker.exe` processes (`CreateToolhelp32Snapshot`) and scans the TCP connection table (`GetExtendedTcpTable`) to discover active SS13 server endpoints without requiring game modifications.
- **BYOND Topic Querying**: Communicates with the remote SS13 server via the native BYOND `0x83` Topic protocol to query `?status` for:
  - Station name / server version / identifier
  - Current map rotation (e.g. `Kilo Station`, `Box Station`, `Delta Station`)
  - Active game mode (e.g. `secret`, `extended`, `traitor`)
  - Connected player count (`players`)
  - Round duration elapsed time
- **Dynamic Elapsed Time**: Converts `round_duration` seconds into a live counting timer in Discord (`XX:XX elapsed`).
- **Discord IPC Management**:
  - Implements clean RAII pipe connection and disconnection.
  - Automatically scans and connects to Discord IPC pipes `\\.\pipe\discord-ipc-0` through `9`.
  - Sends Discord `SET_ACTIVITY` commands with JSON framing.
  - Automatically clears presence when the game or DreamSeeker exits.
- **Configurable**: Generates and loads `config.json` for custom Discord Application Client IDs, asset keys, and polling intervals.

---

## Architecture Overview

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

### Module Breakdown
- [`server.h`](file:///d:/Code/C++/RichPresenceSS13/server.h): Data structure storing server state, with URL decoding and `?status` response parser.
- [`src/game_detector.h`](file:///d:/Code/C++/RichPresenceSS13/src/game_detector.h) / [`src/game_detector.cpp`](file:///d:/Code/C++/RichPresenceSS13/src/game_detector.cpp): Process enumeration and TCP connection table inspection via `GetExtendedTcpTable`.
- [`src/byond_topic.h`](file:///d:/Code/C++/RichPresenceSS13/src/byond_topic.h) / [`src/byond_topic.cpp`](file:///d:/Code/C++/RichPresenceSS13/src/byond_topic.cpp): Implementation of the BYOND binary `0x83` query protocol with timeout handling.
- [`src/discord_rpc.h`](file:///d:/Code/C++/RichPresenceSS13/src/discord_rpc.h) / [`src/discord_rpc.cpp`](file:///d:/Code/C++/RichPresenceSS13/src/discord_rpc.cpp): Named pipe communication (`\\.\pipe\discord-ipc-0`), handshake, and `SET_ACTIVITY` serialization.
- [`src/config.h`](file:///d:/Code/C++/RichPresenceSS13/src/config.h): Configuration loader and generator (`config.json`).
- [`src/main.cpp`](file:///d:/Code/C++/RichPresenceSS13/src/main.cpp): Daemon loop, console logging, and graceful signal handling (Ctrl+C).

---

## Configuration (`config.json`)

Upon first run, the program creates a default `config.json` next to the executable:

```json
{
    "client_id": "1545495325366550630",
    "large_image_key": "ss13",
    "large_image_text": "Space Station 13",
    "small_image_key": "",
    "small_image_text": "",
    "poll_interval_seconds": 5,
    "topic_timeout_ms": 3000
}
```

### Discord Application Setup
1. Go to the [Discord Developer Portal](https://discord.com/developers/applications).
2. Create an application (or use your existing one).
3. Under **Rich Presence -> Art Assets**, upload your icon (e.g. named `ss13`).
4. Set `"client_id"` and `"large_image_key"` in `config.json`.

---

## Building and Running

### Prerequisites
- CMake 3.30+
- MinGW-w64 (GCC 14+) or MSVC with C++17 support
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
Or override client ID on the command line:
```powershell
.\build\RPSS13.exe 1545495325366550630
```