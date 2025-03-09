#ifndef CONFIG_PARSER_H
#define CONFIG_PARSER_H

#include <vector>

#include "node.h"

struct RaftConfig
{
    std::vector<NodeConfig> nodes;
    int current_node_id;
    int timeout;
};

RaftConfig initConfig(const std::string &filename, int current_node_id);

#endif