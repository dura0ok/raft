#ifndef CONFIG_PARSER_H
#define CONFIG_PARSER_H

#include <vector>
#include "node.h"

struct RaftConfig
{
    std::vector<NodeConfig> nodes;
    std::string current_node_id;
    int election_timeout;
    int hearbeat_timeout;
};

RaftConfig initConfig(const std::string &filename, const std::string &current_node_id);

#endif