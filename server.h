#ifndef DOCUMENT_H
#define DOCUMENT_H

#include <string>
#include <chrono>

//using namespace std::literals; // enables literal suffixes, e.g. 24h, 1ms, 1s.
class Server {
    private:
        std::string name;
        int population;
        std::chrono::time_point<std::chrono::system_clock> current_round_time;
};

#endif