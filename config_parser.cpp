#include "config_parser.h"

#include <yaml-cpp/yaml.h>

RaftConfig initConfig(const std::string &path, int current_node_id)
{
    RaftConfig config;
    YAML::Node yamlConfig = YAML::LoadFile(path);

    for (const auto &node : yamlConfig["nodes"])
    {
        config.nodes.push_back({node["id"].as<int>(), node["address"].as<std::string>(), node["port"].as<int>()});
    }

    config.election_timeout = yamlConfig["election_timeout"].as<int>();
    config.hearbeat_timeout = yamlConfig["heartbeat_timeout"].as<int>();
    config.current_node_id = current_node_id;
    return config;
}