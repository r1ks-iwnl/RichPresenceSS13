#include "game_detector.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>
#include <tlhelp32.h>
#include <algorithm>
#include <iostream>

std::vector<DWORD> GameDetector::find_dreamseeker_pids() {
    std::vector<DWORD> pids;

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return pids;
    }

    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(PROCESSENTRY32W);

    if (Process32FirstW(snapshot, &entry)) {
        do {
            // Case-insensitive comparison for "dreamseeker.exe"
            if (_wcsicmp(entry.szExeFile, L"dreamseeker.exe") == 0) {
                pids.push_back(entry.th32ProcessID);
            }
        } while (Process32NextW(snapshot, &entry));
    }

    CloseHandle(snapshot);
    return pids;
}

std::vector<ServerEndpoint> GameDetector::find_server_connections(const std::vector<DWORD>& pids) {
    std::vector<ServerEndpoint> external_conns;
    std::vector<ServerEndpoint> loopback_conns;

    if (pids.empty()) {
        return external_conns;
    }

    DWORD size = 0;
    // First call to determine required buffer size
    GetExtendedTcpTable(nullptr, &size, TRUE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0);
    if (size == 0) {
        return external_conns;
    }

    std::vector<BYTE> buffer(size);
    PMIB_TCPTABLE_OWNER_PID table = reinterpret_cast<PMIB_TCPTABLE_OWNER_PID>(buffer.data());

    if (GetExtendedTcpTable(table, &size, TRUE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0) != NO_ERROR) {
        return external_conns;
    }

    for (DWORD i = 0; i < table->dwNumEntries; ++i) {
        const auto& row = table->table[i];

        // Check if connection is owned by one of the dreamseeker processes
        if (std::find(pids.begin(), pids.end(), row.dwOwningPid) == pids.end()) {
            continue;
        }

        // Only established connections
        if (row.dwState != MIB_TCP_STATE_ESTAB) {
            continue;
        }

        uint16_t remote_port = ntohs(static_cast<uint16_t>(row.dwRemotePort));

        // Skip well-known web ports (DreamSeeker uses HTTP/HTTPS for web assets, not game traffic)
        if (remote_port == 80 || remote_port == 443 || remote_port == 8080 || remote_port == 8443) {
            continue;
        }

        IN_ADDR addr{};
        addr.S_un.S_addr = row.dwRemoteAddr;
        char ip_buf[INET_ADDRSTRLEN] = {0};

        if (inet_ntop(AF_INET, &addr, ip_buf, sizeof(ip_buf))) {
            std::string ip_str = ip_buf;
            // Differentiate local loopback IPC (BYOND pager communication) from remote game servers
            if (ip_str.rfind("127.", 0) == 0 || ip_str == "0.0.0.0") {
                loopback_conns.push_back({ip_str, remote_port, row.dwOwningPid});
            } else {
                external_conns.push_back({ip_str, remote_port, row.dwOwningPid});
            }
        }
    }

    // If external connections exist, they are remote game servers (127.0.0.1 is just BYOND pager IPC)
    if (!external_conns.empty()) {
        return external_conns;
    }

    // Only return loopback if no external connections exist (e.g. user self-hosting DreamDaemon locally)
    return loopback_conns;
}
