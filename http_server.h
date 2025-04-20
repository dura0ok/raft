#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include "node.h"
#include "proto/raft.grpc.pb.h"
#include <crow/json.h>
#include <memory>
#include <string>

class RaftHTTPServer
{
  public:
    explicit RaftHTTPServer(const std::shared_ptr<grpc::ChannelInterface> &channel, const RaftNode &raft_node)
        : stub_(raft_protocol::RaftService::NewStub(channel)), raft_node_(raft_node)
    {
    }

    void start(int port);

  private:
    std::unique_ptr<raft_protocol::RaftService::Stub> stub_;
    const RaftNode &raft_node_;
};

#endif // HTTP_SERVER_H