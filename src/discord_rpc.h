#ifndef DISCORD_RPC_H
#define DISCORD_RPC_H

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <string>
#include <cstdint>
#include <chrono>
#include <nlohmann/json.hpp>

struct DiscordActivity {
    std::string details;
    std::string state;
    int64_t start_timestamp = 0; // Unix timestamp in seconds (0 = none)
    std::string large_image;
    std::string large_text;
    std::string small_image;
    std::string small_text;
};

// Manages named-pipe communication with the local Discord client using RAII.
// Handles automatic pipe discovery (discord-ipc-0..9), framing, handshaking,
// and activity dispatch.
class DiscordRpcClient {
public:
    explicit DiscordRpcClient(std::string client_id);
    ~DiscordRpcClient();

    // Prevent copying because this class owns a Windows HANDLE resource
    DiscordRpcClient(const DiscordRpcClient&) = delete;
    DiscordRpcClient& operator=(const DiscordRpcClient&) = delete;

    // Discovers and connects to the active Discord IPC pipe, then performs the handshake
    bool connect();

    // Closes pipe handle and resets state
    void disconnect();

    // Checks if the pipe handle is currently open and valid
    bool is_connected() const { return pipe_handle_ != INVALID_HANDLE_VALUE; }

    // Sends SET_ACTIVITY command to update Discord presence
    bool set_activity(const DiscordActivity& activity);

    // Clears the user's Discord presence
    bool clear_activity();

private:
    std::string client_id_;
    HANDLE pipe_handle_ = INVALID_HANDLE_VALUE;
    uint32_t nonce_counter_ = 0;
    std::chrono::steady_clock::time_point last_connect_attempt_{};

    // Frame layout: [uint32 opcode (LE)] [uint32 length (LE)] [JSON string bytes]
    bool write_frame(uint32_t opcode, const std::string& json_payload);
    bool read_frame(uint32_t& out_opcode, std::string& out_json_payload, DWORD timeout_ms = 2000);
    bool read_bytes_with_timeout(void* buffer, DWORD bytes_to_read, DWORD timeout_ms);
};

#endif // DISCORD_RPC_H
