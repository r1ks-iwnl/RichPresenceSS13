#ifndef GAME_DETECTOR_H
#define GAME_DETECTOR_H

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <windows.h>
#include <string>
#include <vector>
#include <cstdint>

struct ServerEndpoint {
    std::string ip;
    uint16_t port;
    DWORD pid;
};

class GameDetector {
public:
    // Finds all running instances of dreamseeker.exe and returns their PIDs
    static std::vector<DWORD> find_dreamseeker_pids();

    // Inspects established TCP connections owned by the given dreamseeker PIDs
    // Filters out HTTP/HTTPS web ports to find candidate SS13 game server connections
    static std::vector<ServerEndpoint> find_server_connections(const std::vector<DWORD>& pids);

    // Retrieves the visible main window title of DreamSeeker if available
    static std::string get_dreamseeker_window_title(DWORD pid);
};

#endif // GAME_DETECTOR_H
