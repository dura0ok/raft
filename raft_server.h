#ifndef RAFTSERVER_H
#define RAFTSERVER_H

#include "proto/raft.grpc.pb.h"
#include "proto/raft.pb.h"
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
    std::unordered_map<std::string, std::string> store_;
    const RaftConfig &config_;
    RaftNode &node_;
};

void RunServer(const RaftConfig &config);

#endif // RAFTSERVER_H
