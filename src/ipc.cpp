#include <windows.h>
#include <iostream>
#include <string>
#include <vector>

int main(){//get debugger going
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
    DWORD bytes_written = 0;

    //send our payload
    success = WriteFile(
        pipe, //our pipe
        payload.data(), //our payload as a pointer to where the data starts
        static_cast<DWORD>(payload.size()), //the length of the payload
        &bytes_written, //how many bytes we wrote
        nullptr //not overlapped/not synchronous
    );

    if(!success){
        std::cerr << "Pipe write failed. Error: " << GetLastError() << "\n";
    }

    //read our payload
    std::vector<char> buffer(4096);
    DWORD bytes_read = 0;

    success = ReadFile(
        pipe, //our pipe
        buffer.data(), //buffer to read into
        static_cast<DWORD>(buffer.size()), //buffer size
        &bytes_read, //how many bytes we read
        nullptr //not overlapped/synch
    );

    if(!success){
        std::cerr << "Pipe read failed. Error: " << GetLastError() << "\n";
    } else {
        std::string response(buffer.data(), bytes_read); //wtf does this return
        std::cout << "Received (" << bytes_read << " bytes): " << response << "\n";
    }
};
