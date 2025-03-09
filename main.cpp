#include <argparse/argparse.hpp>
#include <fstream>
#include <iostream>
#include <string>

#include "config_parser.h"
#include "dbg.h"
#include "raft_server.h"

int main(const int argc, char *argv[])
{
    argparse::ArgumentParser program("raft_node");

    program.add_argument("--config")
        .default_value(std::string("config.yaml"))
        .help("Path to the YAML configuration file");

    program.add_argument("--id").required().help("ID of the current node");

    try
    {
        program.parse_args(argc, argv);
        const auto &config_path = program.get<std::string>("--config");
        const auto current_node_id = std::stoi(program.get<std::string>("--id"));

        const auto config = initConfig(config_path, current_node_id);

        RunServer(config);
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error: " << e.what() << "\n";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
