#include <windows.h>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

int main(){
    HANDLE pipe;
    std::string message = "Poggers";
    std::string pipePrefix = R"(\\?\pipe\discord-ipc-)";
    std::string pipeName = pipePrefix + std::to_string(0);
    bool success;

    //Try to open pipe, wait if needed(busy)
    while(1){
        pipe = CreateFileA(
            pipeName.c_str(),
            GENERIC_READ | GENERIC_WRITE, //read and write access
            0,  //no sharing
            NULL,
            OPEN_EXISTING, //opens existing pipe
            0, //default attributes
            NULL //no template file
            );
        
            if(pipe != INVALID_HANDLE_VALUE){
                break;
            }

            if(GetLastError() != ERROR_PIPE_BUSY){
                std::cerr << "Pipe connection error: " << GetLastError() << "\n";
                return 1;
            }
    }

    std::string payload = "{\"v\": 1, \"client_id\": \"1545495325366550630\"}";

    constexpr std::uint32_t opcode_handshake = 0;
    std::uint32_t payload_size = static_cast<std::uint32_t>(payload.size());
    std::vector<std::uint8_t> frame(8 + payload.size());

    // 4B + 4B + variable payload
    std::memcpy(frame.data(), &opcode_handshake, sizeof(opcode_handshake));
    std::memcpy(frame.data() + sizeof(opcode_handshake), &payload_size, sizeof(payload_size));
    std::memcpy(frame.data() + 8, payload.data(), payload.size());

    DWORD bytes_written = 0;

    //send our payload
    success = WriteFile(
        pipe, //our pipe
        frame.data(), //our framed payload
        static_cast<DWORD>(frame.size()), //the length of the frame
        &bytes_written, //how many bytes we wrote
        nullptr //not overlapped/not synchronous
    );

    if(!success || bytes_written != frame.size()){
        std::cerr << "Pipe write failed. Error: " << GetLastError() << "\n";
        return 1;
    }

    //read the response header first, then its payload
    std::uint32_t response_opcode = 0;
    std::uint32_t response_size = 0;
    DWORD bytes_read = 0;

    //opcode
    success = ReadFile(
        pipe,
        &response_opcode,
        sizeof(response_opcode),
        &bytes_read,
        nullptr
    );

    if (!success || bytes_read != sizeof(response_opcode)) {
        std::cerr << "Pipe read failed while reading opcode. Error: " << GetLastError() << "\n";
        return 1;
    }

    //coming response size
    success = ReadFile(
        pipe,
        &response_size,
        sizeof(response_size),
        &bytes_read,
        nullptr
    );

    if (!success || bytes_read != sizeof(response_size)) {
        std::cerr << "Pipe read failed while reading payload size. Error: " << GetLastError() << "\n";
        return 1;
    }

    std::vector<char> buffer(response_size);
    
    success = ReadFile(
        pipe, //our pipe
        buffer.data(), //buffer to read into
        response_size, //buffer size
        &bytes_read, //how many bytes we read
        nullptr //not overlapped/synch
    );

    if(!success){
        std::cerr << "Pipe read failed. Error: " << GetLastError() << "\n";
    } else {
        std::string response(buffer.data(), bytes_read);
        std::cout << "Received opcode " << response_opcode << " ("
                  << bytes_read << " bytes): " << response << "\n";
    }
};
