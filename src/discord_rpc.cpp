#include "discord_rpc.h"
#include <iostream>
#include <vector>
#include <cstring>

DiscordRpcClient::DiscordRpcClient(std::string client_id)
    : client_id_(std::move(client_id)), pipe_handle_(INVALID_HANDLE_VALUE) {}

DiscordRpcClient::~DiscordRpcClient() {
    disconnect();
}

void DiscordRpcClient::disconnect() {
    if (pipe_handle_ != INVALID_HANDLE_VALUE) {
        CloseHandle(pipe_handle_);
        pipe_handle_ = INVALID_HANDLE_VALUE;
    }
}

bool DiscordRpcClient::connect() {
    if (is_connected()) {
        return true;
    }

    // Try pipes discord-ipc-0 through discord-ipc-9
    for (int i = 0; i < 10; ++i) {
        std::string pipe_path = R"(\\.\pipe\discord-ipc-)" + std::to_string(i);

        HANDLE handle = CreateFileA(
            pipe_path.c_str(),
            GENERIC_READ | GENERIC_WRITE,
            0,
            nullptr,
            OPEN_EXISTING,
            0,
            nullptr
        );

        if (handle == INVALID_HANDLE_VALUE) {
            DWORD err = GetLastError();
            if (err == ERROR_PIPE_BUSY) {
                // Pipe is busy; wait up to 100ms before trying again
                WaitNamedPipeA(pipe_path.c_str(), 100);
            }
            continue;
        }

        pipe_handle_ = handle;

        // Perform Opcode 0 (Handshake)
        nlohmann::json handshake = {
            {"v", 1},
            {"client_id", client_id_}
        };

        if (!write_frame(0, handshake.dump())) {
            disconnect();
            continue;
        }

        // Discord replies with Opcode 1 (READY event)
        uint32_t resp_opcode = 0;
        std::string resp_json;
        if (!read_frame(resp_opcode, resp_json)) {
            disconnect();
            continue;
        }

        try {
            auto j = nlohmann::json::parse(resp_json);
            if (j.value("evt", "") == "READY") {
                return true;
            }
        } catch (...) {
            disconnect();
            continue;
        }
    }

    return false;
}

bool DiscordRpcClient::write_frame(uint32_t opcode, const std::string& json_payload) {
    if (!is_connected()) {
        return false;
    }

    uint32_t payload_len = static_cast<uint32_t>(json_payload.size());
    std::vector<uint8_t> frame(8 + payload_len);

    std::memcpy(frame.data(), &opcode, sizeof(opcode));
    std::memcpy(frame.data() + 4, &payload_len, sizeof(payload_len));
    std::memcpy(frame.data() + 8, json_payload.data(), payload_len);

    DWORD bytes_written = 0;
    DWORD total_written = 0;
    DWORD total_to_write = static_cast<DWORD>(frame.size());

    while (total_written < total_to_write) {
        BOOL ok = WriteFile(
            pipe_handle_,
            frame.data() + total_written,
            total_to_write - total_written,
            &bytes_written,
            nullptr
        );

        if (!ok || bytes_written == 0) {
            disconnect();
            return false;
        }
        total_written += bytes_written;
    }

    return true;
}

bool DiscordRpcClient::read_frame(uint32_t& out_opcode, std::string& out_json_payload) {
    if (!is_connected()) {
        return false;
    }

    uint32_t opcode = 0;
    uint32_t length = 0;
    DWORD bytes_read = 0;

    // Read 4-byte opcode
    BOOL ok = ReadFile(pipe_handle_, &opcode, sizeof(opcode), &bytes_read, nullptr);
    if (!ok || bytes_read != sizeof(opcode)) {
        disconnect();
        return false;
    }

    // Read 4-byte length
    ok = ReadFile(pipe_handle_, &length, sizeof(length), &bytes_read, nullptr);
    if (!ok || bytes_read != sizeof(length)) {
        disconnect();
        return false;
    }

    // Read JSON payload
    std::vector<char> buffer(length);
    DWORD total_read = 0;

    while (total_read < length) {
        ok = ReadFile(pipe_handle_, buffer.data() + total_read, length - total_read, &bytes_read, nullptr);
        if (!ok || bytes_read == 0) {
            disconnect();
            return false;
        }
        total_read += bytes_read;
    }

    out_opcode = opcode;
    out_json_payload.assign(buffer.data(), length);
    return true;
}

bool DiscordRpcClient::set_activity(const DiscordActivity& activity) {
    if (!is_connected()) {
        if (!connect()) {
            return false;
        }
    }

    nlohmann::json act = nlohmann::json::object();

    if (!activity.details.empty()) {
        act["details"] = activity.details;
    }
    if (!activity.state.empty()) {
        act["state"] = activity.state;
    }
    if (activity.start_timestamp > 0) {
        act["timestamps"] = {
            {"start", activity.start_timestamp}
        };
    }

    nlohmann::json assets = nlohmann::json::object();
    if (!activity.large_image.empty()) {
        assets["large_image"] = activity.large_image;
    }
    if (!activity.large_text.empty()) {
        assets["large_text"] = activity.large_text;
    }
    if (!activity.small_image.empty()) {
        assets["small_image"] = activity.small_image;
    }
    if (!activity.small_text.empty()) {
        assets["small_text"] = activity.small_text;
    }

    if (!assets.empty()) {
        act["assets"] = assets;
    }

    std::string nonce = std::to_string(++nonce_counter_);

    nlohmann::json payload = {
        {"cmd", "SET_ACTIVITY"},
        {"args", {
            {"pid", static_cast<int64_t>(GetCurrentProcessId())},
            {"activity", act}
        }},
        {"nonce", nonce}
    };

    if (!write_frame(1, payload.dump())) {
        return false;
    }

    // Read Discord confirmation response
    uint32_t resp_opcode = 0;
    std::string resp_json;
    if (!read_frame(resp_opcode, resp_json)) {
        return false;
    }

    return true;
}

bool DiscordRpcClient::clear_activity() {
    if (!is_connected()) {
        return true;
    }

    std::string nonce = std::to_string(++nonce_counter_);

    nlohmann::json payload = {
        {"cmd", "SET_ACTIVITY"},
        {"args", {
            {"pid", static_cast<int64_t>(GetCurrentProcessId())},
            {"activity", nullptr}
        }},
        {"nonce", nonce}
    };

    if (!write_frame(1, payload.dump())) {
        return false;
    }

    uint32_t resp_opcode = 0;
    std::string resp_json;
    read_frame(resp_opcode, resp_json);
    return true;
}

