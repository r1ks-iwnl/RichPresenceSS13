#include "game_detector.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>
#include <tlhelp32.h>
#include <algorithm>
#include <unordered_set>
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

    std::unordered_set<DWORD> pid_set(pids.begin(), pids.end());

    DWORD size = 0;
    DWORD res = GetExtendedTcpTable(nullptr, &size, TRUE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0);
    std::vector<BYTE> buffer;
    PMIB_TCPTABLE_OWNER_PID table = nullptr;

    // Retry loop in case new connections are added between size calculation and retrieval
    for (int attempts = 0; attempts < 3 && (res == ERROR_INSUFFICIENT_BUFFER || size > buffer.size()); ++attempts) {
        buffer.resize(size);
        table = reinterpret_cast<PMIB_TCPTABLE_OWNER_PID>(buffer.data());
        res = GetExtendedTcpTable(table, &size, TRUE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0);
        if (res == NO_ERROR) {
            break;
        }
    }

    if (res != NO_ERROR || !table) {
        return external_conns;
    }

    for (DWORD i = 0; i < table->dwNumEntries; ++i) {
        const auto& row = table->table[i];

        // Check if connection is owned by one of the dreamseeker processes (O(1))
        if (pid_set.find(row.dwOwningPid) == pid_set.end()) {
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

            // Handle loopback connections
            if (ip_str.rfind("127.", 0) == 0 || ip_str == "0.0.0.0") {
                // Ignore BYOND pager/hub IPC sockets (ports 50987, 50988) or ephemeral range
                if (remote_port == 50987 || remote_port == 50988 || remote_port >= 49152) {
                    continue;
                }
                loopback_conns.push_back({ip_str, remote_port, row.dwOwningPid});
            } else {
                external_conns.push_back({ip_str, remote_port, row.dwOwningPid});
            }
        }
    }

    // External connections are real internet game servers and always take priority
    if (!external_conns.empty()) {
        return external_conns;
    }

    // Only return loopback if no external connections exist (e.g. self-hosting on localhost)
    return loopback_conns;
}

struct WindowSearchData {
    DWORD pid;
    std::string title;
};

static BOOL CALLBACK EnumWindowsCallback(HWND hwnd, LPARAM lParam) {
    auto* data = reinterpret_cast<WindowSearchData*>(lParam);
    DWORD window_pid = 0;
    GetWindowThreadProcessId(hwnd, &window_pid);

    if (window_pid == data->pid && IsWindowVisible(hwnd)) {
        wchar_t buf[256] = {0};
        int len = GetWindowTextW(hwnd, buf, 255);
        if (len > 0) {
            int utf8_len = WideCharToMultiByte(CP_UTF8, 0, buf, len, nullptr, 0, nullptr, nullptr);
            if (utf8_len > 0) {
                std::string title(utf8_len, '\0');
                WideCharToMultiByte(CP_UTF8, 0, buf, len, &title[0], utf8_len, nullptr, nullptr);
                // Exclude generic "Dream Seeker" or BYOND title wrappers
                if (title != "Dream Seeker" && title.find("BYOND") == std::string::npos) {
                    data->title = title;
                    return FALSE; // Found a specific window title, stop enumeration
                }
            }
        }
    }
    return TRUE;
}

std::string GameDetector::get_dreamseeker_window_title(DWORD pid) {
    WindowSearchData data{pid, ""};
    EnumWindows(EnumWindowsCallback, reinterpret_cast<LPARAM>(&data));
    return data.title;
}

