#include <stdlib.h>
#include "../include/ConnectionHandler.h"
#include "../include/StompProtocol.h"
#include <thread>
#include <iostream>
#include <sstream>

volatile bool isConnected = false;

void readSocketTask(ConnectionHandler* connectionHandler) {
    while (isConnected) {
        std::string answer;        
        if (!connectionHandler->getFrameAscii(answer, '\0')) {
            std::cout << "Disconnected from server." << std::endl;
            isConnected = false;
            break;
        }
        std::cout << answer << std::endl;
        if (answer.find("CONNECTED") != std::string::npos)
            std::cout << "Login successful" << std::endl;
        else if (answer.find("ERROR") != std::string::npos)
             if (answer.find("User already logged in") != std::string::npos) {
                 std::cout << "User already logged in" << std::endl;
                 isConnected = false;
             } else if (answer.find("Wrong password") != std::string::npos) {
                 std::cout << "Wrong password" << std::endl;
                 isConnected = false;
             }
    }
}

int main(int argc, char *argv[]) {    
    ConnectionHandler* connectionHandler = nullptr;
    std::thread socketThread;
    while (1) {
        const short bufsize = 1024;
        char buf[bufsize];
        std::cin.getline(buf, bufsize);
        std::string line(buf);
        std::string command;
        std::stringstream ss(line);
        ss >> command;
        if (command == "login") {
            if (isConnected) {
                std::cout << "The client is already logged in, log out before trying again" << std::endl;
                continue;
            }
            std::string hostPort;
            ss >> hostPort;
            size_t colonPos = hostPort.find(':');
            if (colonPos == std::string::npos) {
                std::cout << "Invalid host:port format" << std::endl;
                continue;
            }
            std::string host = hostPort.substr(0, colonPos);
            short port = (short)stoi(hostPort.substr(colonPos + 1));
            connectionHandler = new ConnectionHandler(host, port);
            if (!connectionHandler->connect()) {
                std::cout << "Could not connect to server" << std::endl;
                delete connectionHandler;
                connectionHandler = nullptr;
                continue;
            }
            isConnected = true;
            std::string frame = StompProtocol::commandToFrame(line);
            if (!connectionHandler->sendFrameAscii(frame, '\0')) {
                 std::cout << "Failed to send connect frame" << std::endl;
                 isConnected = false;
                 delete connectionHandler;
                 connectionHandler = nullptr;
                 continue;
            }
            socketThread = std::thread(readSocketTask, connectionHandler);
        } else if (command == "logout") {
            if (isConnected) {
               isConnected = false;
               connectionHandler->close();
               if (socketThread.joinable()) socketThread.join();
               delete connectionHandler;
               connectionHandler = nullptr;
            }
            break;
        } else {
            if (!isConnected)
                std::cout << "Not connected. Please login first." << std::endl;
            else {
                // Generic send for Phase 3
                // std::string frame = StompProtocol::commandToFrame(line);
                // connectionHandler->sendFrameAscii(frame, '\0');
            }
        }
    }
    return 0;
}