#ifndef RAFTSERVER_H
#define RAFTSERVER_H

#include "proto/raft.grpc.pb.h"
#include "proto/raft.pb.h"
#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/grpcpp.h>

class RaftServiceImpl final : public raft_protocol::RaftService::Service
{
  public:
    grpc::Status RequestForVote(grpc::ServerContext *context, const raft_protocol::RequestVoteRequest *request,
                                raft_protocol::RequestVoteResponse *response) override;
    grpc::Status AppendEntries(grpc::ServerContext *context, const raft_protocol::AppendEntriesRequest *request,
                               raft_protocol::AppendEntriesResponse *response) override;
};

inline void RunServer()
{
    std::string server_address("0.0.0.0:50051");
    RaftServiceImpl service;

    grpc::EnableDefaultHealthCheckService(true);
    grpc::reflection::InitProtoReflectionServerBuilderPlugin();

    grpc::ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);

    grpc::Server *server = builder.BuildAndStart().release();

    std::cout << "Server listening on " << server_address << std::endl;
    server->Wait();
}

#endif // RAFTSERVER_H
