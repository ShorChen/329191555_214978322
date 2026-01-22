#include <stdlib.h>
#include "../include/ConnectionHandler.h"
#include "../include/StompProtocol.h"
#include "../include/event.h"
#include <thread>
#include <iostream>
#include <fstream>
#include <sstream>
#include <map>
#include <vector>
#include <mutex>
#include <algorithm>

volatile bool isConnected = false;
std::map<std::string, int> gameToSubId;
std::map<int, std::string> subIdToGame;
int subIdCounter = 0;
int receiptCounter = 0;
std::map<std::string, std::map<std::string, std::vector<Event>>> gameEvents;
volatile int disconnectReceiptId = -1;
std::mutex gameEventsMutex;

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
                std::string username = "unknown";
                if (event.get_game_updates().count("user"))
                    username = event.get_game_updates().at("user");
                { 
                    std::lock_guard<std::mutex> lock(gameEventsMutex);
                    gameEvents[gameName][username].push_back(event);
                }
            }
        } 
        else if (answer.find("RECEIPT") == 0) {
            std::cout << "Action completed successfully (Receipt received)" << std::endl;             
             size_t pos = answer.find("receipt-id:");
             if (pos != std::string::npos) {
                 size_t start = pos + 11;
                 size_t end = answer.find('\n', start);
                 if (end != std::string::npos) {
                     try {
                         std::string idStr = answer.substr(start, end - start);
                         size_t last = idStr.find_last_not_of(" \t\r");
                         if (last != std::string::npos) idStr = idStr.substr(0, last+1);
                         int id = std::stoi(idStr);
                         if (id == disconnectReceiptId) {
                             isConnected = false;
                             connectionHandler->close();
                             break; 
                         }
                     } catch (const std::invalid_argument& e) {
                        std::cerr << "Error: Receipt ID is not a number. " << e.what() << std::endl;
                    } catch (const std::out_of_range& e) {
                         std::cerr << "Error: Receipt ID is too large. " << e.what() << std::endl;
                    }
                 }
             }
        }
        else if (answer.find("ERROR") == 0) {
             std::cout << "ERROR frame received:" << std::endl;
             std::cout << answer << std::endl;
             isConnected = false;
             connectionHandler->close();
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
        if (frame == "" && line != "logout") continue;
        std::stringstream ss(line);
        std::string command;
        ss >> command;
        if (command == "login") {
            if (isConnected) {
                std::cout << "The client is already logged in, log out before trying again" << std::endl;
                continue;
            }
            if (socketThread.joinable())
                socketThread.join();
            std::string hostPort, user, pass;
            ss >> hostPort >> user >> pass;
            currentUser = user; 
            std::string host = hostPort.substr(0, hostPort.find(':'));
            short port = (short)stoi(hostPort.substr(hostPort.find(':') + 1));
            if (connectionHandler != nullptr) delete connectionHandler;
            connectionHandler = new ConnectionHandler(host, port);
            if (!connectionHandler->connect()) {
                std::cout << "Could not connect" << std::endl;
                delete connectionHandler;
                connectionHandler = nullptr;
                continue;
            }
            isConnected = true;
            disconnectReceiptId = -1;
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
                 disconnectReceiptId = receipt;
                 size_t recPos = frame.find("{receipt}");
                 if (recPos != std::string::npos) frame.replace(recPos, 9, std::to_string(receipt));
                 connectionHandler->sendFrameAscii(frame, '\0');
                 if (socketThread.joinable()) socketThread.join();
                 isConnected = false;
                 delete connectionHandler;
                 connectionHandler = nullptr;
                 std::cout << "Logged out" << std::endl;
             }
        }
        else if (frame.find("REPORT") == 0) {
            if (!isConnected) { std::cout << "Not connected" << std::endl; continue; }
            std::string file = frame.substr(7);
            try {
                names_and_events data = parseEventsFile(file);
                std::string gameName = data.team_a_name + "_" + data.team_b_name;
                bool firstEvent = true;
                for (const Event& event : data.events) {
                    std::string sendFrame = "SEND\n";
                    sendFrame += "destination:/" + gameName + "\n";
                    if (firstEvent) {
                         sendFrame += "file:" + file + "\n";
                         firstEvent = false;
                     }
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
        else if (command == "summary") {
            std::string gameName, user, file;
            ss >> gameName >> user >> file;            
            std::vector<Event> eventsCopy;
            bool found = false;
            {
                std::lock_guard<std::mutex> lock(gameEventsMutex);
                if (gameEvents.find(gameName) != gameEvents.end() && 
                    gameEvents[gameName].find(user) != gameEvents[gameName].end()) {
                    eventsCopy = gameEvents[gameName][user];
                    found = true;
                }
            }
            if (found && !eventsCopy.empty()) {
                std::sort(eventsCopy.begin(), eventsCopy.end(), [](const Event& a, const Event& b) {
                    bool a_first_half = true;
                    bool b_first_half = true;
                    if (a.get_game_updates().count("before halftime"))
                        a_first_half = (a.get_game_updates().at("before halftime") == "true");
                    if (b.get_game_updates().count("before halftime"))
                        b_first_half = (b.get_game_updates().at("before halftime") == "true");
                    if (a_first_half != b_first_half)
                        return a_first_half;
                    return a.get_time() < b.get_time();
                });
                std::ofstream outFile(file);
                std::string teamA = eventsCopy[0].get_team_a_name();
                std::string teamB = eventsCopy[0].get_team_b_name();
                outFile << teamA << " vs " << teamB << "\n";
                outFile << "Game stats:\n";
                outFile << "General stats:\n";
                outFile << "Game event reports:\n";
                for (const auto& ev : eventsCopy) {
                    outFile << ev.get_time() << " - " << ev.get_name() << ":\n\n";
                    outFile << ev.get_discription() << "\n\n\n";
                }
                outFile.close();
                std::cout << "Summary created in " << file << std::endl;
            } else std::cout << "No reports found for " << gameName << " from " << user << std::endl;
        }
    }    
    if (socketThread.joinable()) socketThread.join();
    if (connectionHandler != nullptr) delete connectionHandler;
    return 0;
}