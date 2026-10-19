#include "contact.h"
#include <errhandlingapi.h>
#include <handleapi.h>
#include <stdexcept>
#include <string>

//Resource Acquisition Is Initialization - ???
Contact::Contact(std::string pipeName){
    pipe = CreateFileA(
        pipeName.c_str(),
        GENERIC_READ | GENERIC_WRITE, //read and write access
        0,  //no sharing
        NULL,
        OPEN_EXISTING, //opens existing pipe
        0, //default attributes
        NULL //no template file
        );

    if(pipe == INVALID_HANDLE_VALUE){ //never destructed?
        throw std::runtime_error("Pipe connection error: " + std::to_string(GetLastError()));
    } //need pipe busy handling?
}