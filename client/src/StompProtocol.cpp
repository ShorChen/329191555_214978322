#include "../include/StompProtocol.h"
#include <sstream>
#include <iostream>

std::string StompProtocol::commandToFrame(const std::string& commandLine) {
    std::stringstream ss(commandLine);
    std::string command;
    ss >> command;
    if (command == "login") {
        std::string hostPort, username, password;
        if (ss >> hostPort >> username >> password) {
            return "CONNECT\n"
                   "accept-version:1.2\n"
                   "host:stomp.cs.bgu.ac.il\n"
                   "login:" + username + "\n"
                   "passcode:" + password + "\n"
                   "\n";
        } else {
            std::cerr << "Usage: login {host:port} {username} {password}" << std::endl;
            return "";
        }
    }    
    return "";
}