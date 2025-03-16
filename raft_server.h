#ifndef RAFTSERVER_H
#define RAFTSERVER_H

#include <thread>

#include "proto/raft.grpc.pb.h"
#include "proto/raft.pb.h"
#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/grpcpp.h>

#include "config_parser.h"

class RaftServiceImpl final : public raft_protocol::RaftService::Service
{
  public:
    RaftServiceImpl(const RaftConfig &config, RaftNode &node) : config_(config), node_(node)
    {
    }
    grpc::Status RequestForVote(grpc::ServerContext *context, const raft_protocol::RequestVoteRequest *request,
                                raft_protocol::RequestVoteResponse *response) override;
    grpc::Status AppendEntries(grpc::ServerContext *context, const raft_protocol::AppendEntriesRequest *request,
                               raft_protocol::AppendEntriesResponse *response) override;

  private:
    const RaftConfig &config_;
    RaftNode &node_;
};

void RunServer(const RaftConfig &config);

#endif // RAFTSERVER_H
