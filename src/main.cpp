#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iostream>
#include <chrono>
#include <thread>
#include <atomic>
#include <algorithm>
#include <cstring>

#include "config.h"
#include "server.h"
#include "game_detector.h"
#include "byond_topic.h"
#include "discord_rpc.h"

static std::atomic<bool> g_running{true};

BOOL WINAPI ConsoleCtrlHandler(DWORD signal) {
    if (signal == CTRL_C_EVENT || signal == CTRL_BREAK_EVENT || signal == CTRL_CLOSE_EVENT) {
        std::cout << "\n[App] Shutdown signal received. Exiting...\n";
        g_running = false;
        return TRUE;
    }
    return FALSE;
}

static bool activity_equals(const DiscordActivity& a, const DiscordActivity& b) {
    return a.details == b.details &&
           a.state == b.state &&
           a.start_timestamp == b.start_timestamp &&
           a.large_image == b.large_image &&
           a.large_text == b.large_text &&
           a.small_image == b.small_image &&
           a.small_text == b.small_text;
}

int main(int argc, char* argv[]) {
    // Set console title
    SetConsoleTitleA("SS13 Discord Rich Presence");

    // Install Ctrl+C / close handler
    SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);

    // Initialize Winsock for network topic queries
    WSADATA wsa_data;
    int wsa_err = WSAStartup(MAKEWORD(2, 2), &wsa_data);
    if (wsa_err != 0) {
        std::cerr << "[Error] WSAStartup failed: " << wsa_err << "\n";
        return 1;
    }

    // Load configuration (creates config.json with defaults if not present)
    AppConfig config = AppConfig::load_or_create("config.json");

    // Allow overriding client_id via command line arguments if provided
    if (argc > 1 && argv[1] != nullptr && strlen(argv[1]) > 0) {
        config.client_id = argv[1];
    }

    std::cout << "====================================================\n";
    std::cout << "         Space Station 13 Discord Rich Presence     \n";
    std::cout << "====================================================\n";
    std::cout << "Client ID         : " << config.client_id << "\n";
    std::cout << "Large Image       : " << config.large_image_key << " (" << config.large_image_text << ")\n";
    std::cout << "Local Process Poll: " << config.local_poll_interval_seconds << "s (Zero network traffic)\n";
    std::cout << "Server Refresh    : " << config.server_refresh_seconds << "s (Rate-limited host queries)\n";
    std::cout << "Host Backoff      : " << config.failed_query_cooldown_seconds << "s (On topic timeout/blocked)\n";
    std::cout << "Config File       : config.json\n";
    std::cout << "----------------------------------------------------\n";
    std::cout << "Searching for dreamseeker.exe... (Press Ctrl+C to stop)\n\n";

    DiscordRpcClient discord(config.client_id);

    bool was_in_game = false;
    std::string active_server_ip;
    uint16_t active_server_port = 0;
    bool active_server_supports_topic = true;
    bool has_server_info = false;
    Server cached_server;
    DiscordActivity last_sent_activity;
    std::chrono::steady_clock::time_point last_topic_query_time{};

    while (g_running) {
        auto dreamseeker_pids = GameDetector::find_dreamseeker_pids();

        if (dreamseeker_pids.empty()) {
            if (was_in_game) {
                std::cout << "[Status] DreamSeeker closed. Clearing Discord Rich Presence.\n";
                discord.clear_activity();
                was_in_game = false;
                active_server_ip.clear();
                active_server_port = 0;
                active_server_supports_topic = true;
                has_server_info = false;
                last_topic_query_time = {};
                last_sent_activity = {};
            }
        } else {
            // DreamSeeker is running; inspect active TCP connections (external servers prioritized over loopback)
            auto connections = GameDetector::find_server_connections(dreamseeker_pids);

            if (connections.empty()) {
                // DreamSeeker is open, but has no active game server connection (e.g. in pager or lobby)
                if (!was_in_game || !active_server_ip.empty()) {
                    if (!discord.is_connected()) {
                        discord.connect();
                    }
                    DiscordActivity activity;
                    activity.details = "In Menus / Connecting";
                    activity.state = "Space Station 13";
                    activity.large_image = config.large_image_key;
                    activity.large_text = config.large_image_text;

                    if (!activity_equals(activity, last_sent_activity)) {
                        discord.set_activity(activity);
                        last_sent_activity = activity;
                        std::cout << "[Status] DreamSeeker in menus/lobby. Waiting for game connection...\n";
                    }

                    was_in_game = true;
                    active_server_ip.clear();
                    active_server_port = 0;
                    has_server_info = false;
                    last_topic_query_time = {};
                }
            } else {
                // Check if our currently active server is still in the connection list
                bool active_still_connected = false;
                if (!active_server_ip.empty()) {
                    for (const auto& conn : connections) {
                        if (conn.ip == active_server_ip && conn.port == active_server_port) {
                            active_still_connected = true;
                            break;
                        }
                    }
                }

                // If disconnected or not yet chosen, discover the active server among candidates
                if (!active_still_connected) {
                    bool found_responsive = false;

                    for (const auto& conn : connections) {
                        std::string topic_response;
                        if (ByondTopic::query(conn.ip, conn.port, "?status", topic_response, config.topic_timeout_ms)) {
                            Server parsed = Server::from_topic_response(topic_response, conn.ip, conn.port);
                            if (parsed.is_valid()) {
                                active_server_ip = conn.ip;
                                active_server_port = conn.port;
                                cached_server = parsed;
                                has_server_info = true;
                                active_server_supports_topic = true;
                                found_responsive = true;
                                last_topic_query_time = std::chrono::steady_clock::now();

                                std::cout << "[SS13] Connected to server: " << cached_server.get_name()
                                          << " (" << active_server_ip << ":" << active_server_port << ")\n"
                                          << "       Map: " << (cached_server.get_map_name().empty() ? "Station" : cached_server.get_map_name())
                                          << " | Players: " << cached_server.get_population()
                                          << " | Next refresh in " << config.server_refresh_seconds << "s\n";
                                break;
                            }
                        }
                    }

                    if (!found_responsive) {
                        // Check if there is an external (non-loopback) connection to fall back to
                        bool has_external = false;
                        for (const auto& conn : connections) {
                            if (conn.ip.rfind("127.", 0) != 0 && conn.ip != "0.0.0.0") {
                                active_server_ip = conn.ip;
                                active_server_port = conn.port;
                                has_external = true;
                                break;
                            }
                        }

                        if (has_external) {
                            active_server_supports_topic = false;
                            has_server_info = false;
                            last_topic_query_time = std::chrono::steady_clock::now();

                            std::cout << "[SS13] Connected to " << active_server_ip << ":" << active_server_port
                                      << " (?status blocked or unsupported). Backing off for "
                                      << config.failed_query_cooldown_seconds << "s.\n";
                        } else {
                            // No external server connection; loopback connections without ?status are just pager/menu IPC
                            active_server_ip.clear();
                            active_server_port = 0;
                            has_server_info = false;
                            last_topic_query_time = {};
                        }
                    }
                } else {
                    // Active server is already identified and still connected.
                    // Check if it's time for a periodic refresh
                    auto now = std::chrono::steady_clock::now();
                    int required_cooldown = active_server_supports_topic
                        ? config.server_refresh_seconds
                        : config.failed_query_cooldown_seconds;

                    auto elapsed_seconds = std::chrono::duration_cast<std::chrono::seconds>(now - last_topic_query_time).count();

                    if (elapsed_seconds >= required_cooldown) {
                        last_topic_query_time = now;
                        std::string topic_response;

                        if (ByondTopic::query(active_server_ip, active_server_port, "?status", topic_response, config.topic_timeout_ms)) {
                            Server parsed = Server::from_topic_response(topic_response, active_server_ip, active_server_port);
                            if (parsed.is_valid()) {
                                cached_server = parsed;
                                has_server_info = true;
                                active_server_supports_topic = true;
                            } else {
                                if (active_server_supports_topic) {
                                    std::cout << "[SS13] Notice: Server at " << active_server_ip << ":" << active_server_port
                                              << " returned unparseable or error response. Entering " << config.failed_query_cooldown_seconds
                                              << "s backoff.\n";
                                }
                                active_server_supports_topic = false;
                            }
                        } else {
                            if (active_server_supports_topic) {
                                std::cout << "[SS13] Notice: Server at " << active_server_ip << ":" << active_server_port
                                          << " did not respond to ?status. Entering " << config.failed_query_cooldown_seconds
                                          << "s backoff cooldown to avoid host strain.\n";
                            }
                            active_server_supports_topic = false;
                        }
                    }
                }

                // Ensure Discord IPC connection is active
                if (!discord.is_connected()) {
                    if (discord.connect()) {
                        std::cout << "[Discord] Connected to Discord IPC pipe.\n";
                    }
                }

                // Construct presence activity
                DiscordActivity activity;
                if (has_server_info) {
                    std::string display_name = cached_server.get_name();
                    std::string ip_port = active_server_ip + ":" + std::to_string(active_server_port);
                    if ((display_name == ip_port || display_name == "Space Station 13") && !connections.empty()) {
                        std::string win_title = GameDetector::get_dreamseeker_window_title(connections[0].pid);
                        if (!win_title.empty()) {
                            size_t pos = win_title.find(" - Space Station 13");
                            if (pos != std::string::npos) {
                                win_title = win_title.substr(0, pos);
                            }
                            display_name = win_title;
                        } else {
                            display_name = "Space Station 13";
                        }
                    }
                    activity.details = display_name;

                    std::string state_text;
                    if (!cached_server.get_map_name().empty()) {
                        state_text = cached_server.get_map_name();
                    } else if (!cached_server.get_gamemode().empty()) {
                        state_text = cached_server.get_gamemode();
                    } else {
                        state_text = "Station";
                    }
                    state_text += " (" + std::to_string(cached_server.get_population()) + " players)";
                    activity.state = state_text;

                    // Round start timestamp: Discord's UI automatically counts elapsed seconds upward
                    // from this timestamp without requiring repeated updates!
                    if (cached_server.get_round_duration_sec() > 0) {
                        auto now_sec = std::chrono::duration_cast<std::chrono::seconds>(
                            std::chrono::system_clock::now().time_since_epoch()
                        ).count();
                        activity.start_timestamp = now_sec - cached_server.get_round_duration_sec();
                    }

                    activity.large_image = config.large_image_key;
                    activity.large_text = config.large_image_text;
                    activity.small_image = config.small_image_key;
                    activity.small_text = config.small_image_text;
                } else {
                    // Fallback when ?status is disabled or firewalled by host
                    std::string title = "Playing Space Station 13";
                    if (!connections.empty()) {
                        std::string win_title = GameDetector::get_dreamseeker_window_title(connections[0].pid);
                        if (!win_title.empty()) {
                            title = win_title;
                        }
                    }
                    activity.details = title;
                    activity.state = "Server: " + active_server_ip + ":" + std::to_string(active_server_port);
                    activity.large_image = config.large_image_key;
                    activity.large_text = config.large_image_text;
                }

                // Only send update to Discord if the activity data actually changed
                if (!activity_equals(activity, last_sent_activity)) {
                    discord.set_activity(activity);
                    last_sent_activity = activity;
                }

                was_in_game = true;
            }
        }

        // Sleep for local poll interval in short 250ms increments to stay responsive to Ctrl+C
        for (int i = 0; i < config.local_poll_interval_seconds * 4 && g_running; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
        }
    }

    // Clean exit: clear rich presence and close handles
    std::cout << "[App] Cleaning up Rich Presence...\n";
    discord.clear_activity();
    discord.disconnect();
    WSACleanup();
    std::cout << "[App] Done. Goodbye!\n";
    return 0;
}
