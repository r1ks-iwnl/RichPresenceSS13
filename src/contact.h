#ifndef CONTACT_H
#define CONTACT_H
#include <vector>
#include <windows.h>
#include <iostream>
#include <cstdint>

class Contact{
    private: 
        //HANDLE pipe;
        std::string pipeName;
        WINBOOL instance;

        //relevant to write
        std::string payload; //SECRET
        std::uint32_t payload_size;
        std::uint32_t opcode;
        std::vector<uint32_t> frame; //call constructor

        //relevant to read
        std::uint32_t respone_opcode;
        std::uint32_t response_size;
        DWORD bytes_read;
        std::vector<char> buffer;

    public:
        HANDLE pipe;
        Contact(std::string pipeName);
        void write_pipe();
        void read_pipe();

};

#endif