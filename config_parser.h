#ifndef CONFIG_PARSER_H
#define CONFIG_PARSER_H

#include <vector>

#include "node.h"

struct RaftConfig
{
    std::vector<NodeConfig> nodes;
    int timeout;
};

RaftConfig parseConfig(const std::string &filename);

#endif