#ifndef CONFIG_H
#define CONFIG_H

#include <string>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

// Application configuration
struct AppConfig {
    // Discord Application Client ID
    // You can create your own Discord Application at https://discord.com/developers/applications
    std::string client_id = "1545495325366550630";

    // Discord Rich Presence Asset Keys
    // These must match the asset names uploaded to your Discord Developer Portal application
    std::string large_image_key = "ss13";
    std::string large_image_text = "Space Station 13";
    std::string small_image_key = "";
    std::string small_image_text = "";

    // Polling interval in seconds (how often to check for dreamseeker.exe and update presence)
    int poll_interval_seconds = 5;

    // Timeout in milliseconds when querying BYOND topic status
    int topic_timeout_ms = 3000;

    // Load configuration from config.json if present; otherwise save defaults
    static AppConfig load_or_create(const std::string& path = "config.json") {
        AppConfig config;
        std::ifstream file(path);
        if (file.is_open()) {
            try {
                nlohmann::json j;
                file >> j;
                if (j.contains("client_id")) config.client_id = j["client_id"].get<std::string>();
                if (j.contains("large_image_key")) config.large_image_key = j["large_image_key"].get<std::string>();
                if (j.contains("large_image_text")) config.large_image_text = j["large_image_text"].get<std::string>();
                if (j.contains("small_image_key")) config.small_image_key = j["small_image_key"].get<std::string>();
                if (j.contains("small_image_text")) config.small_image_text = j["small_image_text"].get<std::string>();
                if (j.contains("poll_interval_seconds")) config.poll_interval_seconds = j["poll_interval_seconds"].get<int>();
                if (j.contains("topic_timeout_ms")) config.topic_timeout_ms = j["topic_timeout_ms"].get<int>();
                return config;
            } catch (const std::exception& e) {
                std::cerr << "[Config] Warning: Failed to parse " << path << " (" << e.what() << "). Using defaults.\n";
            }
        }

        // Save defaults if file does not exist
        config.save_to_file(path);
        return config;
    }

    void save_to_file(const std::string& path = "config.json") const {
        std::ofstream file(path);
        if (file.is_open()) {
            nlohmann::json j = {
                {"client_id", client_id},
                {"large_image_key", large_image_key},
                {"large_image_text", large_image_text},
                {"small_image_key", small_image_key},
                {"small_image_text", small_image_text},
                {"poll_interval_seconds", poll_interval_seconds},
                {"topic_timeout_ms", topic_timeout_ms}
            };
            file << j.dump(4) << "\n";
        }
    }
};

#endif // CONFIG_H

