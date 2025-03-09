#ifndef RAFTSERVER_H
#define RAFTSERVER_H

#include "proto/raft.grpc.pb.h"
#include "proto/raft.pb.h"
#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/grpcpp.h>

#include "config_parser.h"

class RaftServiceImpl final : public raft_protocol::RaftService::Service
{
  public:
    grpc::Status RequestForVote(grpc::ServerContext *context, const raft_protocol::RequestVoteRequest *request,
                                raft_protocol::RequestVoteResponse *response) override;
    grpc::Status AppendEntries(grpc::ServerContext *context, const raft_protocol::AppendEntriesRequest *request,
                               raft_protocol::AppendEntriesResponse *response) override;
};

inline void RunServer(const RaftConfig &config)
{
    auto it = std::find_if(config.nodes.begin(), config.nodes.end(),
                            [config](const auto &node) { return node.id == config.current_node_id; });

    if (it == config.nodes.end())
    {
        std::cerr << "Error: Node ID " << config.current_node_id << " not found in config nodes.\n";
        std::exit(EXIT_FAILURE);
    }

    std::string server_address = it->address + ":" + std::to_string(it->port);
    RaftServiceImpl service;

    grpc::EnableDefaultHealthCheckService(true);
    grpc::reflection::InitProtoReflectionServerBuilderPlugin();

    grpc::ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);

    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());

    std::cout << "Server listening on " << server_address << std::endl;
    server->Wait();
}

#endif // RAFTSERVER_H
