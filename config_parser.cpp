#include "config_parser.h"

#include <yaml-cpp/yaml.h>

#include <utility>

RaftConfig initConfig(const std::string &path, const std::string &current_node_id)
{
    RaftConfig config;
    YAML::Node yamlConfig = YAML::LoadFile(path);

    for (const auto &node : yamlConfig["nodes"])
    {
        config.nodes.emplace_back(node["id"].as<std::string>(), node["address"].as<std::string>(),
                                  node["port"].as<int>(), node["http_port"].as<int>());
    }

    config.election_timeout = yamlConfig["election_timeout"].as<int>();
    config.hearbeat_timeout = yamlConfig["heartbeat_timeout"].as<int>();
    config.current_node_id = current_node_id;
    return config;
}