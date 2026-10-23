#include "byond_topic.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <vector>
#include <iostream>

bool ByondTopic::query(const std::string& ip, uint16_t port, const std::string& query_string, std::string& out_response, int timeout_ms) {
    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    addrinfo* result = nullptr;
    std::string port_str = std::to_string(port);

    struct AddrInfoGuard {
        addrinfo* ptr = nullptr;
        ~AddrInfoGuard() { if (ptr) freeaddrinfo(ptr); }
    } guard;

    if (getaddrinfo(ip.c_str(), port_str.c_str(), &hints, &result) != 0 || !result) {
        return false;
    }
    guard.ptr = result;

    SOCKET s = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (s == INVALID_SOCKET) {
        return false;
    }

    // Set non-blocking to allow timeout on connect()
    u_long non_blocking = 1;
    ioctlsocket(s, FIONBIO, &non_blocking);

    int connect_res = connect(s, result->ai_addr, static_cast<int>(result->ai_addrlen));

    if (connect_res == SOCKET_ERROR) {
        int err = WSAGetLastError();
        if (err == WSAEWOULDBLOCK) {
            fd_set write_fds, err_fds;
            FD_ZERO(&write_fds);
            FD_ZERO(&err_fds);
            FD_SET(s, &write_fds);
            FD_SET(s, &err_fds);

            timeval tv{};
            tv.tv_sec = timeout_ms / 1000;
            tv.tv_usec = (timeout_ms % 1000) * 1000;

            int select_res = select(0, nullptr, &write_fds, &err_fds, &tv);
            if (select_res <= 0 || FD_ISSET(s, &err_fds)) {
                closesocket(s);
                return false;
            }

            // Verify whether connect succeeded
            int so_error = 0;
            int len = sizeof(so_error);
            getsockopt(s, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&so_error), &len);
            if (so_error != 0) {
                closesocket(s);
                return false;
            }
        } else {
            closesocket(s);
            return false;
        }
    }

    // Return to blocking mode for sends and receives
    non_blocking = 0;
    ioctlsocket(s, FIONBIO, &non_blocking);

    DWORD socket_timeout = static_cast<DWORD>(timeout_ms);
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&socket_timeout), sizeof(socket_timeout));
    setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&socket_timeout), sizeof(socket_timeout));

    // Construct the BYOND 0x83 Topic Packet:
    // [0x00, 0x83, len_high, len_low, 0x00, 0x00, 0x00, 0x00, 0x00, <query_string>, 0x00]
    // where len = query_string.size() + 6 (accounting for 5 bytes of flags/padding + null terminator)
    uint16_t packet_len = static_cast<uint16_t>(query_string.size() + 6);

    std::vector<uint8_t> packet;
    packet.push_back(0x00);
    packet.push_back(0x83);
    packet.push_back(static_cast<uint8_t>((packet_len >> 8) & 0xFF));
    packet.push_back(static_cast<uint8_t>(packet_len & 0xFF));
    packet.insert(packet.end(), 5, 0x00);
    packet.insert(packet.end(), query_string.begin(), query_string.end());
    packet.push_back(0x00); // Null terminator

    // Send packet
    int bytes_sent = send(s, reinterpret_cast<const char*>(packet.data()), static_cast<int>(packet.size()), 0);
    if (bytes_sent != static_cast<int>(packet.size())) {
        closesocket(s);
        return false;
    }

    // Read response header: 5 bytes
    // [0x00, 0x83, len_high, len_low, type_byte]
    uint8_t header[5] = {0};
    int header_read = 0;
    while (header_read < 5) {
        int r = recv(s, reinterpret_cast<char*>(header + header_read), 5 - header_read, 0);
        if (r <= 0) {
            closesocket(s);
            return false;
        }
        header_read += r;
    }

    if (header[0] != 0x00 || header[1] != 0x83) {
        closesocket(s);
        return false;
    }

    uint16_t response_len = (static_cast<uint16_t>(header[2]) << 8) | static_cast<uint16_t>(header[3]);
    uint8_t response_type = header[4];

    // 0x06 indicates string response
    if (response_type != 0x06 || response_len <= 1) {
        closesocket(s);
        return false;
    }

    // Payload size is (response_len - 1) bytes because the type byte was already read
    int payload_size = response_len - 1;
    std::vector<char> buffer(payload_size);
    int total_read = 0;

    while (total_read < payload_size) {
        int r = recv(s, buffer.data() + total_read, payload_size - total_read, 0);
        if (r <= 0) {
            break;
        }
        total_read += r;
    }

    closesocket(s);

    if (total_read == 0) {
        return false;
    }

    // Strip trailing null character if present
    if (total_read > 0 && buffer[total_read - 1] == '\0') {
        --total_read;
    }

    out_response.assign(buffer.data(), total_read);
    return true;
}

