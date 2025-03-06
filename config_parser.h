#ifndef CONFIG_PARSER_H
#define CONFIG_PARSER_H

#include "node.h"
#include <vector>

struct RaftConfig {
    std::vector<NodeConfig> nodes;
    int timeout;
};

RaftConfig parseConfig(const std::string& filename);

#endif