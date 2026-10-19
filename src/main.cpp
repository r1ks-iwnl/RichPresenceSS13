#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iostream>
#include <chrono>
#include <thread>
#include <atomic>
#include <csignal>

#include "config.h"
#include "../server.h"
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
    std::cout << "Client ID     : " << config.client_id << "\n";
    std::cout << "Large Image   : " << config.large_image_key << " (" << config.large_image_text << ")\n";
    std::cout << "Poll Interval : " << config.poll_interval_seconds << "s\n";
    std::cout << "Config File   : config.json\n";
    std::cout << "----------------------------------------------------\n";
    std::cout << "Searching for dreamseeker.exe... (Press Ctrl+C to stop)\n\n";

    DiscordRpcClient discord(config.client_id);

    bool was_in_game = false;
    std::string last_server_ip;
    uint16_t last_server_port = 0;
    int last_round_id = -1;

    while (g_running) {
        auto dreamseeker_pids = GameDetector::find_dreamseeker_pids();

        if (dreamseeker_pids.empty()) {
            if (was_in_game) {
                std::cout << "[Status] DreamSeeker closed. Clearing Discord Rich Presence.\n";
                discord.clear_activity();
                was_in_game = false;
                last_server_ip.clear();
                last_server_port = 0;
                last_round_id = -1;
            }
        } else {
            // DreamSeeker is running; check for active game server connections
            auto connections = GameDetector::find_server_connections(dreamseeker_pids);

            if (connections.empty()) {
                if (!was_in_game) {
                    // DreamSeeker is open, but not yet connected to a server (e.g. in pager or lobby)
                    if (!discord.is_connected()) {
                        discord.connect();
                    }
                    DiscordActivity activity;
                    activity.details = "In Menus / Connecting";
                    activity.state = "Space Station 13";
                    activity.large_image = config.large_image_key;
                    activity.large_text = config.large_image_text;
                    discord.set_activity(activity);
                    was_in_game = true;
                    std::cout << "[Status] DreamSeeker running. Waiting for server connection...\n";
                }
            } else {
                // We have active candidate connections
                // Try querying ?status on candidates to find the active SS13 server
                bool found_server = false;
                Server active_server;

                for (const auto& conn : connections) {
                    std::string topic_response;
                    if (ByondTopic::query(conn.ip, conn.port, "?status", topic_response, config.topic_timeout_ms)) {
                        active_server = Server::from_topic_response(topic_response, conn.ip, conn.port);
                        found_server = true;
                        break;
                    }
                }

                // Ensure Discord IPC connection is active
                if (!discord.is_connected()) {
                    if (discord.connect()) {
                        std::cout << "[Discord] Connected to Discord IPC pipe.\n";
                    }
                }

                if (found_server) {
                    // Rich presence with live server statistics
                    DiscordActivity activity;
                    activity.details = active_server.get_name();

                    std::string state_text;
                    if (!active_server.get_map_name().empty()) {
                        state_text = active_server.get_map_name();
                    } else if (!active_server.get_gamemode().empty()) {
                        state_text = active_server.get_gamemode();
                    } else {
                        state_text = "Station";
                    }
                    state_text += " (" + std::to_string(active_server.get_population()) + " players)";
                    activity.state = state_text;

                    // Round start timestamp for elapsed time
                    if (active_server.get_round_duration_sec() > 0) {
                        auto now_sec = std::chrono::duration_cast<std::chrono::seconds>(
                            std::chrono::system_clock::now().time_since_epoch()
                        ).count();
                        activity.start_timestamp = now_sec - active_server.get_round_duration_sec();
                    }

                    activity.large_image = config.large_image_key;
                    activity.large_text = config.large_image_text;
                    activity.small_image = config.small_image_key;
                    activity.small_text = config.small_image_text;

                    discord.set_activity(activity);

                    // Print log on server change or new round
                    if (active_server.get_ip() != last_server_ip || 
                        active_server.get_port() != last_server_port ||
                        (active_server.get_round_id() != 0 && active_server.get_round_id() != last_round_id)) {
                        
                        std::cout << "[SS13] Connected: " << active_server.get_name()
                                  << " | Map: " << (active_server.get_map_name().empty() ? "Unknown" : active_server.get_map_name())
                                  << " | Players: " << active_server.get_population()
                                  << " | Round Duration: " << (active_server.get_round_duration_sec() / 60) << "m "
                                  << (active_server.get_round_duration_sec() % 60) << "s\n";

                        last_server_ip = active_server.get_ip();
                        last_server_port = active_server.get_port();
                        last_round_id = active_server.get_round_id();
                    }
                    was_in_game = true;
                } else {
                    // Fallback: connection detected but ?status did not respond (some servers restrict ?status)
                    const auto& primary = connections[0];
                    DiscordActivity activity;
                    activity.details = "Playing Space Station 13";
                    activity.state = "Server: " + primary.ip + ":" + std::to_string(primary.port);
                    activity.large_image = config.large_image_key;
                    activity.large_text = config.large_image_text;

                    discord.set_activity(activity);

                    if (primary.ip != last_server_ip || primary.port != last_server_port) {
                        std::cout << "[SS13] Connected to " << primary.ip << ":" << primary.port << " (Topic query unsupported or blocked)\n";
                        last_server_ip = primary.ip;
                        last_server_port = primary.port;
                    }
                    was_in_game = true;
                }
            }
        }

        // Sleep in short 250ms increments to stay responsive to Ctrl+C
        for (int i = 0; i < config.poll_interval_seconds * 4 && g_running; ++i) {
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

