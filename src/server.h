#ifndef SERVER_H
#define SERVER_H

#include <string>
#include <chrono>
#include <cstdint>
#include <unordered_map>
#include <vector>
#include <cctype>

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

    // Checks if a string looks like a 40-char Git commit hash (SHA)
    // Only checks 40-char hashes to avoid false positives on short version tags (e.g. "1a2b3c4")
    static bool is_git_hash(const std::string& str) {
        if (str.length() != 40) return false;
        for (char c : str) {
            if (!std::isxdigit(static_cast<unsigned char>(c))) return false;
        }
        return true;
    }

    // Checks whether this server instance holds valid, parsed game info
    // (returns false if the server returned an error or gave no recognizable game fields)
    bool is_valid() const {
        return (!version.empty() && !is_git_hash(version)) ||
               !map_name.empty() ||
               !gamemode.empty() ||
               round_duration_sec > 0 ||
               population > 0;
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

    // Parsing and helper functions (implemented in server.cpp)
    static std::string url_decode(const std::string& in);
    static std::unordered_map<std::string, std::string> parse_query_string(const std::string& query);
    static int parse_duration(const std::string& str);
    static std::string determine_server_name(
        const std::string& station_name,
        const std::string& identifier,
        const std::string& version,
        const std::string& fallback);
    static Server from_topic_response(const std::string& raw_response, const std::string& server_ip, uint16_t server_port);
};

#endif // SERVER_H