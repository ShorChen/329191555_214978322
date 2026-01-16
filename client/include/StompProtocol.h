#pragma once

#include <string>
#include <vector>
#include "../include/ConnectionHandler.h"

class StompProtocol {
public:
    static std::string commandToFrame(const std::string& commandLine);
};