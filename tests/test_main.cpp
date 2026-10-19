#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iostream>
#include <string>
#include <vector>
#include <cassert>
#include <cmath>
#include <chrono>

#include "server.h"
#include "config.h"
#include "discord_rpc.h"
#include "byond_topic.h"

// Lightweight test framework macros
static int g_tests_run = 0;
static int g_tests_passed = 0;
static int g_tests_failed = 0;

#define TEST_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cerr << "  [FAIL] Assertion failed: " << msg << " (" << __FILE__ << ":" << __LINE__ << ")\n"; \
            return false; \
        } \
    } while (0)

#define RUN_TEST(fn) \
    do { \
        ++g_tests_run; \
        std::cout << "[RUN] " << #fn << "...\n"; \
        if (fn()) { \
            ++g_tests_passed; \
            std::cout << "  [PASS] " << #fn << "\n"; \
        } else { \
            ++g_tests_failed; \
        } \
    } while (0)

// -------------------------------------------------------------
// Test 1: URL Decoding
// -------------------------------------------------------------
bool test_url_decode() {
    TEST_ASSERT(Server::url_decode("hello+world") == "hello world", "plus sign should decode to space");
    TEST_ASSERT(Server::url_decode("%2ftg%2fStation+13") == "/tg/Station 13", "hex %2f should decode to '/'");
    TEST_ASSERT(Server::url_decode("port%3a3336") == "port:3336", "%3a should decode to ':'");
    TEST_ASSERT(Server::url_decode("plus%2btest") == "plus+test", "%2b should decode to '+'");
    TEST_ASSERT(Server::url_decode("no_encoding") == "no_encoding", "plain string should remain identical");
    TEST_ASSERT(Server::url_decode("") == "", "empty string should remain empty");
    // Invalid / partial hex should not crash
    TEST_ASSERT(Server::url_decode("invalid%2") == "invalid%2", "partial escape should not crash");
    TEST_ASSERT(Server::url_decode("invalid%ZZ") == "invalid%ZZ", "invalid hex should not crash");
    return true;
}

// -------------------------------------------------------------
// Test 2: Query String Parsing
// -------------------------------------------------------------
bool test_parse_query_string() {
    std::string query = "version=%2ftg%2fStation+13&players=42&mode=secret&empty_val=&flag";
    auto params = Server::parse_query_string(query);

    TEST_ASSERT(params["version"] == "/tg/Station 13", "version should be parsed and decoded");
    TEST_ASSERT(params["players"] == "42", "players should be parsed");
    TEST_ASSERT(params["mode"] == "secret", "mode should be parsed");
    TEST_ASSERT(params["empty_val"] == "", "empty value should be empty string");
    TEST_ASSERT(params.find("flag") != params.end(), "key without equals should still be present");

    auto empty_params = Server::parse_query_string("");
    TEST_ASSERT(empty_params.empty(), "empty query string should result in empty map");
    return true;
}

// -------------------------------------------------------------
// Test 3: Server Model from Topic Response
// -------------------------------------------------------------
bool test_server_from_topic_response() {
    // Standard /tg/station style response
    std::string tg_raw = "version=%2ftg%2fStation+13&round_id=264396&players=15&identifier=terry"
                         "&map_name=Kilo+Station&round_duration=1800&mode=secret";

    Server tg_server = Server::from_topic_response(tg_raw, "185.107.96.12", 3336);

    TEST_ASSERT(tg_server.get_name() == "/tg/Station 13 (terry)", "name should combine version and identifier");
    TEST_ASSERT(tg_server.get_map_name() == "Kilo Station", "map name should be Kilo Station");
    TEST_ASSERT(tg_server.get_gamemode() == "secret", "gamemode should be secret");
    TEST_ASSERT(tg_server.get_population() == 15, "population should be 15");
    TEST_ASSERT(tg_server.get_round_duration_sec() == 1800, "round duration should be 1800s");
    TEST_ASSERT(tg_server.get_round_id() == 264396, "round id should be 264396");
    TEST_ASSERT(tg_server.get_ip() == "185.107.96.12", "ip should match");
    TEST_ASSERT(tg_server.get_port() == 3336, "port should match");

    // Paradise / Goon style with station_name
    std::string paradise_raw = "station_name=NSS+Cyberiad&players=64&gamemode=traitor&round_duration=3600";
    Server paradise_server = Server::from_topic_response(paradise_raw, "1.2.3.4", 5000);

    TEST_ASSERT(paradise_server.get_name() == "NSS Cyberiad", "station_name takes precedence for display name");
    TEST_ASSERT(paradise_server.get_population() == 64, "population should be 64");
    TEST_ASSERT(paradise_server.get_gamemode() == "traitor", "gamemode should be parsed");

    // Minimal fallback response (no known name fields)
    std::string fallback_raw = "players=5";
    Server fallback_server = Server::from_topic_response(fallback_raw, "10.0.0.1", 1337);
    TEST_ASSERT(fallback_server.get_name() == "10.0.0.1:1337", "fallback name should be ip:port");
    TEST_ASSERT(fallback_server.get_population() == 5, "population should be 5");

    // Robustness: invalid numbers in integer fields should not crash
    std::string malformed_raw = "players=not_a_number&round_duration=xyz&round_id=bad";
    Server malformed_server = Server::from_topic_response(malformed_raw, "10.0.0.1", 1337);
    TEST_ASSERT(malformed_server.get_population() == 0, "malformed player count should fallback to 0");
    TEST_ASSERT(malformed_server.get_round_duration_sec() == 0, "malformed duration should fallback to 0");
    return true;
}

// -------------------------------------------------------------
// Test 4: AppConfig Defaults and JSON Serialization
// -------------------------------------------------------------
bool test_app_config() {
    AppConfig config;
    TEST_ASSERT(config.client_id == "1545495325366550630", "default client_id should match");
    TEST_ASSERT(config.large_image_key == "ss13", "default large_image_key should be ss13");
    TEST_ASSERT(config.poll_interval_seconds == 5, "default poll interval should be 5");
    TEST_ASSERT(config.topic_timeout_ms == 3000, "default topic timeout should be 3000ms");

    // Save and load test
    std::string temp_config = "test_temp_config.json";
    config.client_id = "999888777666555444";
    config.large_image_key = "custom_icon";
    config.poll_interval_seconds = 10;
    config.save_to_file(temp_config);

    AppConfig loaded = AppConfig::load_or_create(temp_config);
    TEST_ASSERT(loaded.client_id == "999888777666555444", "loaded client_id should match saved");
    TEST_ASSERT(loaded.large_image_key == "custom_icon", "loaded large_image_key should match saved");
    TEST_ASSERT(loaded.poll_interval_seconds == 10, "loaded poll_interval should match saved");

    DeleteFileA(temp_config.c_str());
    return true;
}

// -------------------------------------------------------------
// Test 5: Discord Activity JSON Formatting
// -------------------------------------------------------------
bool test_discord_activity_json() {
    DiscordActivity activity;
    activity.details = "/tg/Station 13 (terry)";
    activity.state = "Kilo Station (15 players)";
    activity.start_timestamp = 1700000000;
    activity.large_image = "ss13";
    activity.large_text = "Space Station 13";

    nlohmann::json act = nlohmann::json::object();
    act["details"] = activity.details;
    act["state"] = activity.state;
    act["timestamps"] = {{"start", activity.start_timestamp}};
    act["assets"] = {
        {"large_image", activity.large_image},
        {"large_text", activity.large_text}
    };

    nlohmann::json payload = {
        {"cmd", "SET_ACTIVITY"},
        {"args", {
            {"pid", 1234},
            {"activity", act}
        }},
        {"nonce", "1"}
    };

    TEST_ASSERT(payload["cmd"] == "SET_ACTIVITY", "cmd should be SET_ACTIVITY");
    TEST_ASSERT(payload["args"]["pid"] == 1234, "pid should be 1234");
    TEST_ASSERT(payload["args"]["activity"]["details"] == "/tg/Station 13 (terry)", "details should match");
    TEST_ASSERT(payload["args"]["activity"]["timestamps"]["start"] == 1700000000, "start timestamp should match");
    TEST_ASSERT(payload["args"]["activity"]["assets"]["large_image"] == "ss13", "large_image should match");
    return true;
}

// -------------------------------------------------------------
// Test 6: BYOND Topic Packet Construction Layout
// -------------------------------------------------------------
bool test_byond_topic_packet_framing() {
    std::string query = "?status";
    uint16_t packet_len = static_cast<uint16_t>(query.size() + 6);

    std::vector<uint8_t> packet;
    packet.push_back(0x00);
    packet.push_back(0x83);
    packet.push_back(static_cast<uint8_t>((packet_len >> 8) & 0xFF));
    packet.push_back(static_cast<uint8_t>(packet_len & 0xFF));
    for (int i = 0; i < 5; ++i) {
        packet.push_back(0x00);
    }
    packet.insert(packet.end(), query.begin(), query.end());
    packet.push_back(0x00);

    TEST_ASSERT(packet[0] == 0x00 && packet[1] == 0x83, "header must start with 0x00 0x83");
    uint16_t decoded_len = (static_cast<uint16_t>(packet[2]) << 8) | packet[3];
    TEST_ASSERT(decoded_len == (query.size() + 6), "big-endian length must match query.size() + 6");
    TEST_ASSERT(packet[8] == 0x00, "byte 8 must be null byte");
    TEST_ASSERT(packet.back() == 0x00, "packet must terminate with null byte");
    TEST_ASSERT(packet.size() == (4 + decoded_len), "total packet size must equal 4 + decoded length");
    return true;
}

int main() {
    std::cout << "====================================================\n";
    std::cout << "             RPSS13 Unit & Integration Tests        \n";
    std::cout << "====================================================\n";

    RUN_TEST(test_url_decode);
    RUN_TEST(test_parse_query_string);
    RUN_TEST(test_server_from_topic_response);
    RUN_TEST(test_app_config);
    RUN_TEST(test_discord_activity_json);
    RUN_TEST(test_byond_topic_packet_framing);

    std::cout << "----------------------------------------------------\n";
    std::cout << "Test Summary: " << g_tests_passed << "/" << g_tests_run << " passed";
    if (g_tests_failed > 0) {
        std::cout << " (" << g_tests_failed << " FAILED)";
    }
    std::cout << "\n====================================================\n";

    return (g_tests_failed == 0) ? 0 : 1;
}

