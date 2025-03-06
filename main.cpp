#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <argparse/argparse.hpp>
#include "config_parser.h"
#include "dbg.h"

int main(const int argc, char* argv[]) {
    argparse::ArgumentParser program("raft_node");

    program.add_argument("--config")
        .default_value(std::string("config.yaml"))
        .help("Path to the YAML configuration file");

    program.add_argument("--id")
        .required()
        .help("ID of the current node");

    try {
        program.parse_args(argc, argv);
        const std::string& config_path = program.get<std::string>("--config");
        const int node_id = std::stoi(program.get<std::string>("--id"));

        const auto config = parseConfig(config_path);

        dbg(config.timeout);

        if (std::none_of(config.nodes.begin(), config.nodes.end(),
                      [node_id](const auto& node) { return node.id == node_id; })) {
            std::cerr << "Error: Node ID " << node_id << " not found in config nodes.\n";
            return EXIT_FAILURE;
                      }

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
