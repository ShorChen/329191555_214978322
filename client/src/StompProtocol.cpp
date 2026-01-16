#include "../include/StompProtocol.h"
#include "../include/event.h"
#include <sstream>
#include <iostream>
#include <fstream>

bool fileExists(const std::string& name) {
    std::ifstream f(name.c_str());
    return f.good();
}

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
    else if (command == "join") {
        std::string gameName;
        if (ss >> gameName) {
            return "SUBSCRIBE\n"
                   "destination:/" + gameName + "\n"
                   "id:{id}\n"
                   "receipt:{receipt}\n"
                   "\n";
        } else {
             std::cerr << "Usage: join {game_name}" << std::endl;
             return "";
        }
    }
    else if (command == "exit") {
        std::string gameName;
        if (ss >> gameName) {
            return "UNSUBSCRIBE\n"
                   "id:{id}\n"
                   "receipt:{receipt}\n" 
                   "\n";
        } else {
             std::cerr << "Usage: exit {game_name}" << std::endl;
             return "";
        }
    }
    else if (command == "logout") {
        return "DISCONNECT\n"
               "receipt:{receipt}\n"
               "\n";
    }
    else if (command == "report") {
        std::string jsonFile;
        if (ss >> jsonFile) {
            if (!fileExists(jsonFile)) {
                 std::cerr << "File not found: " << jsonFile << std::endl;
                 return "";
            }
            return "REPORT " + jsonFile; 
        } else {
             std::cerr << "Usage: report {file}" << std::endl;
             return "";
        }
    }
    else if (command == "summary") {
        std::string gameName, user, file;
        if (ss >> gameName >> user >> file)
            return "SUMMARY " + gameName + " " + user + " " + file;
        else {
             std::cerr << "Usage: summary {game_name} {user} {file}" << std::endl;
             return "";
        }
    }
    return "";
}