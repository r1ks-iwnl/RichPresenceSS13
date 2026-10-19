#ifndef BYOND_TOPIC_H
#define BYOND_TOPIC_H

#include <string>
#include <cstdint>

class ByondTopic {
public:
    // Queries a BYOND server using the standard 0x83 topic protocol (e.g., "?status")
    // Returns true and writes the decoded response string on success
    static bool query(const std::string& ip, uint16_t port, const std::string& query_string, std::string& out_response, int timeout_ms = 3000);
};

#endif // BYOND_TOPIC_H

