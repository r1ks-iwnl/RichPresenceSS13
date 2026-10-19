#ifndef SERVER_H
#define SERVER_H

#include <string>
#include <chrono>
#include <cstdint>
#include <unordered_map>
#include <sstream>
#include <vector>
#include <cctype>
#include <nlohmann/json.hpp>

class Server {
private:
    std::string ip;
    uint16_t port = 0;
    std::string name;
    std::string map_name;
    std::string gamemode;
    std::string version;
    int population = 0;
    int round_duration_sec = 0;
    int round_id = 0;
    std::chrono::time_point<std::chrono::system_clock> current_round_time;

public:
    Server() = default;
    Server(const Server&) = default;
    Server& operator=(const Server&) = default;
    Server(Server&&) noexcept = default;
    Server& operator=(Server&&) noexcept = default;

    // Checks if a string looks like a 40-char or 7-char Git commit hash (SHA)
    static bool is_git_hash(const std::string& str) {
        if (str.length() != 40 && str.length() != 7) return false;
        for (char c : str) {
            if (!std::isxdigit(static_cast<unsigned char>(c))) return false;
        }
        return true;
    }

    // Parses duration in seconds from either a raw integer string or "HH:MM:SS" / "MM:SS" format
    static int parse_duration(const std::string& str) {
        if (str.empty()) return 0;
        if (str.find(':') != std::string::npos) {
            std::stringstream ss(str);
            std::string token;
            std::vector<int> parts;
            while (std::getline(ss, token, ':')) {
                try { parts.push_back(std::stoi(token)); } catch (...) { return 0; }
            }
            if (parts.size() == 3) {
                return parts[0] * 3600 + parts[1] * 60 + parts[2];
            } else if (parts.size() == 2) {
                return parts[0] * 60 + parts[1];
            }
            return 0;
        }
        try { return std::stoi(str); } catch (...) { return 0; }
    }

    // Selects the cleanest human-readable server name, ignoring git hashes
    static std::string determine_server_name(
        const std::string& station_name,
        const std::string& identifier,
        const std::string& version,
        const std::string& fallback)
    {
        if (!station_name.empty() && !is_git_hash(station_name)) {
            return station_name;
        }
        if (!identifier.empty()) {
            if (!version.empty() && !is_git_hash(version)) {
                return version + " (" + identifier + ")";
            }
            return identifier;
        }
        if (!version.empty() && !is_git_hash(version)) {
            return version;
        }
        return fallback;
    }

    // Checks whether this server instance holds valid, parsed game info
    // (returns false if the server returned an error or gave no recognizable game fields)
    bool is_valid() const {
        return (!version.empty() && !is_git_hash(version)) ||
               !map_name.empty() ||
               !gamemode.empty() ||
               round_duration_sec > 0;
    }

    // Getters
    const std::string& get_ip() const { return ip; }
    uint16_t get_port() const { return port; }
    const std::string& get_name() const { return name; }
    const std::string& get_map_name() const { return map_name; }
    const std::string& get_gamemode() const { return gamemode; }
    const std::string& get_version() const { return version; }
    int get_population() const { return population; }
    int get_round_duration_sec() const { return round_duration_sec; }
    int get_round_id() const { return round_id; }
    std::chrono::time_point<std::chrono::system_clock> get_current_round_time() const { return current_round_time; }

    // Setters
    void set_ip(const std::string& val) { ip = val; }
    void set_port(uint16_t val) { port = val; }
    void set_name(const std::string& val) { name = val; }
    void set_population(int val) { population = val; }

    // Helper: URL-decodes a string (e.g., "%2f" -> "/", "+" -> " ")
    static std::string url_decode(const std::string& in) {
        std::string out;
        out.reserve(in.size());
        for (size_t i = 0; i < in.size(); ++i) {
            if (in[i] == '+') {
                out += ' ';
            } else if (in[i] == '%' && i + 2 < in.size()) {
                char hex[3] = { in[i + 1], in[i + 2], '\0' };
                char* end = nullptr;
                long val = std::strtol(hex, &end, 16);
                if (end == hex + 2) {
                    out += static_cast<char>(val);
                    i += 2;
                } else {
                    out += in[i];
                }
            } else {
                out += in[i];
            }
        }
        return out;
    }

    // Helper: Parses URL-encoded query string ("key1=val1&key2=val2") into key-value map
    static std::unordered_map<std::string, std::string> parse_query_string(const std::string& query) {
        std::unordered_map<std::string, std::string> params;
        std::stringstream ss(query);
        std::string item;
        while (std::getline(ss, item, '&')) {
            if (item.empty()) continue;
            size_t eq_pos = item.find('=');
            if (eq_pos != std::string::npos) {
                std::string key = url_decode(item.substr(0, eq_pos));
                std::string val = url_decode(item.substr(eq_pos + 1));
                params[key] = val;
            } else {
                params[url_decode(item)] = "";
            }
        }
        return params;
    }

    // Constructs a Server instance by parsing the raw response returned by BYOND's ?status.
    // Supports both standard URL-encoded strings (version=...&players=...) and JSON responses.
    static Server from_topic_response(const std::string& raw_response, const std::string& server_ip, uint16_t server_port) {
        Server server;
        server.ip = server_ip;
        server.port = server_port;

        if (raw_response.empty()) {
            return server;
        }

        // Check if response is formatted as JSON
        if (raw_response.front() == '{') {
            try {
                auto j = nlohmann::json::parse(raw_response);

                // Detect error responses from restricted or API-only topic endpoints
                // e.g. {"statuscode":400,"response":"Bad Request - Invalid JSON format"}
                if (j.contains("statuscode")) {
                    int code = 0;
                    if (j["statuscode"].is_number()) code = j["statuscode"].get<int>();
                    else if (j["statuscode"].is_string()) code = std::stoi(j["statuscode"].get<std::string>());
                    if (code >= 400) {
                        return server; // Invalid / rejected query
                    }
                }
                if (j.contains("error")) {
                    return server;
                }

                // Parse JSON game fields
                if (j.contains("version") && j["version"].is_string()) server.version = j["version"].get<std::string>();
                if (j.contains("map_name") && j["map_name"].is_string()) server.map_name = j["map_name"].get<std::string>();
                if (j.contains("mode") && j["mode"].is_string()) server.gamemode = j["mode"].get<std::string>();
                else if (j.contains("gamemode") && j["gamemode"].is_string()) server.gamemode = j["gamemode"].get<std::string>();

                std::string station_name = (j.contains("station_name") && j["station_name"].is_string()) ? j["station_name"].get<std::string>() : "";
                std::string identifier = (j.contains("identifier") && j["identifier"].is_string()) ? j["identifier"].get<std::string>() : "";
                server.name = determine_server_name(station_name, identifier, server.version, server_ip + ":" + std::to_string(server_port));

                if (j.contains("players")) {
                    if (j["players"].is_number()) server.population = j["players"].get<int>();
                    else if (j["players"].is_string()) {
                        try { server.population = std::stoi(j["players"].get<std::string>()); } catch (...) {}
                    }
                }

                // Parse round duration: supports int seconds or formatted "HH:MM:SS" (e.g. "roundtime": "00:46:49")
                std::string dur_key;
                if (j.contains("round_duration")) dur_key = "round_duration";
                else if (j.contains("roundtime")) dur_key = "roundtime";
                else if (j.contains("round_time")) dur_key = "round_time";

                if (!dur_key.empty()) {
                    if (j[dur_key].is_number()) {
                        server.round_duration_sec = j[dur_key].get<int>();
                    } else if (j[dur_key].is_string()) {
                        server.round_duration_sec = parse_duration(j[dur_key].get<std::string>());
                    }
                }

                if (j.contains("round_id")) {
                    if (j["round_id"].is_number()) server.round_id = j["round_id"].get<int>();
                    else if (j["round_id"].is_string()) {
                        try { server.round_id = std::stoi(j["round_id"].get<std::string>()); } catch (...) {}
                    }
                }

                server.current_round_time = std::chrono::system_clock::now() - std::chrono::seconds(server.round_duration_sec);
                return server;
            } catch (...) {
                // If JSON parsing fails, fall through to URL-encoded query string parsing
            }
        }

        // Standard BYOND URL-encoded format (version=...&players=...)
        auto params = parse_query_string(raw_response);

        if (params.find("version") != params.end()) {
            server.version = params["version"];
        }
        if (params.find("map_name") != params.end()) {
            server.map_name = params["map_name"];
        }
        if (params.find("mode") != params.end()) {
            server.gamemode = params["mode"];
        } else if (params.find("gamemode") != params.end()) {
            server.gamemode = params["gamemode"];
        }

        std::string station_name = (params.find("station_name") != params.end()) ? params["station_name"] : "";
        std::string identifier = (params.find("identifier") != params.end()) ? params["identifier"] : "";
        server.name = determine_server_name(station_name, identifier, server.version, server_ip + ":" + std::to_string(server_port));

        if (params.find("players") != params.end()) {
            try {
                server.population = std::stoi(params["players"]);
            } catch (...) {
                server.population = 0;
            }
        }

        // Parse round duration: supports int seconds or formatted "HH:MM:SS"
        if (params.find("round_duration") != params.end()) {
            server.round_duration_sec = parse_duration(params["round_duration"]);
        } else if (params.find("roundtime") != params.end()) {
            server.round_duration_sec = parse_duration(params["roundtime"]);
        } else if (params.find("round_time") != params.end()) {
            server.round_duration_sec = parse_duration(params["round_time"]);
        }

        if (params.find("round_id") != params.end()) {
            try {
                server.round_id = std::stoi(params["round_id"]);
            } catch (...) {
                server.round_id = 0;
            }
        }

        server.current_round_time = std::chrono::system_clock::now() - std::chrono::seconds(server.round_duration_sec);
        return server;
    }
};

#endif // SERVER_H