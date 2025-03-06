#include "config_parser.h"
#include <yaml-cpp/yaml.h>

RaftConfig parseConfig(const std::string& path) {
    RaftConfig config;
    YAML::Node yamlConfig = YAML::LoadFile(path);

    for (const auto& node : yamlConfig["nodes"]) {
        config.nodes.push_back({
            node["id"].as<int>(),
            node["address"].as<std::string>(),
            node["port"].as<int>()
        });
    }

    config.timeout = yamlConfig["election_timeout"].as<int>();
    return config;
}