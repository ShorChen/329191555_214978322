#include <stdlib.h>
#include "../include/ConnectionHandler.h"
#include "../include/StompProtocol.h"
#include "../include/event.h"
#include <thread>
#include <iostream>
#include <sstream>
#include <map>
#include <vector>

volatile bool isConnected = false;
std::map<std::string, int> gameToSubId;
std::map<int, std::string> subIdToGame;
int subIdCounter = 0;
int receiptCounter = 0;
std::map<std::string, std::map<std::string, std::vector<Event>>> gameEvents; 

void readSocketTask(ConnectionHandler* connectionHandler) {
    while (isConnected) {
        std::string answer;
        if (!connectionHandler->getFrameAscii(answer, '\0')) {
            if (isConnected) {
                std::cout << "Disconnected from server." << std::endl;
                isConnected = false;
            }
            break;
        }
        if (answer.find("CONNECTED") == 0)
            std::cout << "Login successful" << std::endl;
        else if (answer.find("MESSAGE") == 0) {
            size_t bodyPos = answer.find("\n\n");
            if (bodyPos != std::string::npos) {
                std::string body = answer.substr(bodyPos + 2);
                Event event(body);
                std::string gameName = event.get_team_a_name() + "_" + event.get_team_b_name();                
                std::cout << "Received event: " << event.get_name() << " in game " << gameName << std::endl;                
            }
        } else if (answer.find("RECEIPT") == 0)
             std::cout << "Action completed successfully (Receipt received)" << std::endl;
        else if (answer.find("ERROR") == 0) {
             std::cout << answer << std::endl;
             isConnected = false;
        }
    }
}

int main(int argc, char *argv[]) {
    ConnectionHandler* connectionHandler = nullptr;
    std::thread socketThread;
    std::string currentUser;
    while (1) {
        const short bufsize = 1024;
        char buf[bufsize];
        std::cin.getline(buf, bufsize);
        std::string line(buf);
        std::string frame = StompProtocol::commandToFrame(line);
        if (frame == "") continue;
        std::stringstream ss(line);
        std::string command;
        ss >> command;
        if (command == "login") {
            if (isConnected) {
                std::cout << "The client is already logged in" << std::endl;
                continue;
            }
            std::string hostPort, user, pass;
            ss >> hostPort >> user >> pass;
            currentUser = user; 
            std::string host = hostPort.substr(0, hostPort.find(':'));
            short port = (short)stoi(hostPort.substr(hostPort.find(':') + 1));
            connectionHandler = new ConnectionHandler(host, port);
            if (!connectionHandler->connect()) {
                std::cout << "Could not connect" << std::endl;
                delete connectionHandler;
                continue;
            }
            isConnected = true;
            connectionHandler->sendFrameAscii(frame, '\0');
            socketThread = std::thread(readSocketTask, connectionHandler);
        } 
        else if (command == "join") {
             if (!isConnected) { std::cout << "Not connected" << std::endl; continue; }
             std::string gameName;
             ss >> gameName;
             int id = subIdCounter++;
             int receipt = receiptCounter++;
             gameToSubId[gameName] = id;
             subIdToGame[id] = gameName;
             size_t idPos = frame.find("{id}");
             if (idPos != std::string::npos) frame.replace(idPos, 4, std::to_string(id));
             size_t recPos = frame.find("{receipt}");
             if (recPos != std::string::npos) frame.replace(recPos, 9, std::to_string(receipt));
             connectionHandler->sendFrameAscii(frame, '\0');
             std::cout << "Joined channel " << gameName << std::endl;
        }
        else if (command == "exit") {
             if (!isConnected) { std::cout << "Not connected" << std::endl; continue; }
             std::string gameName;
             ss >> gameName;
             if (gameToSubId.find(gameName) == gameToSubId.end()) {
                 std::cout << "Not subscribed to " << gameName << std::endl;
                 continue;
             }
             int id = gameToSubId[gameName];
             int receipt = receiptCounter++;
             size_t idPos = frame.find("{id}");
             if (idPos != std::string::npos) frame.replace(idPos, 4, std::to_string(id));
             size_t recPos = frame.find("{receipt}");
             if (recPos != std::string::npos) frame.replace(recPos, 9, std::to_string(receipt));
             connectionHandler->sendFrameAscii(frame, '\0');
             gameToSubId.erase(gameName);
             subIdToGame.erase(id);
             std::cout << "Exited channel " << gameName << std::endl;
        }
        else if (command == "logout") {
             if (isConnected) {
                 int receipt = receiptCounter++;
                 size_t recPos = frame.find("{receipt}");
                 if (recPos != std::string::npos) frame.replace(recPos, 9, std::to_string(receipt));
                 connectionHandler->sendFrameAscii(frame, '\0');
                 if (socketThread.joinable()) socketThread.join();
                 isConnected = false;
                 delete connectionHandler;
                 std::cout << "Logged out" << std::endl;
             }
        }
        else if (frame.find("REPORT") == 0) {
            std::string file = frame.substr(7);
            try {
                names_and_events data = parseEventsFile(file);
                std::string gameName = data.team_a_name + "_" + data.team_b_name;
                for (const Event& event : data.events) {
                    std::string sendFrame = "SEND\n";
                    sendFrame += "destination:/" + gameName + "\n";
                    sendFrame += "\n";
                    sendFrame += "user:" + currentUser + "\n";
                    sendFrame += "team a:" + data.team_a_name + "\n";
                    sendFrame += "team b:" + data.team_b_name + "\n";
                    sendFrame += "event name:" + event.get_name() + "\n";
                    sendFrame += "time:" + std::to_string(event.get_time()) + "\n";
                    sendFrame += "general game updates:\n";
                    for (auto const& [key, val] : event.get_game_updates())
                        sendFrame += "\t" + key + ":" + val + "\n";
                    sendFrame += "team a updates:\n";
                    for (auto const& [key, val] : event.get_team_a_updates())
                        sendFrame += "\t" + key + ":" + val + "\n";
                    sendFrame += "team b updates:\n";
                    for (auto const& [key, val] : event.get_team_b_updates())
                        sendFrame += "\t" + key + ":" + val + "\n";
                    sendFrame += "description:" + event.get_discription() + "\n";
                    connectionHandler->sendFrameAscii(sendFrame, '\0');
                    std::cout << "Sent event: " << event.get_name() << std::endl;
                }
                std::cout << "Report finished." << std::endl;
            } catch (std::exception& e) {
                std::cout << "ERROR: Failed to parse JSON file. Reason: " << e.what() << std::endl;
            }
        }
    }
    return 0;
}