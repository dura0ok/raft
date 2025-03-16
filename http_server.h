#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include "proto/raft.grpc.pb.h"
#include <crow/json.h>
#include <memory>
#include <string>

class RaftHTTPServer
{
  public:
    explicit RaftHTTPServer(const std::shared_ptr<grpc::ChannelInterface> &channel)
        : stub_(raft_protocol::RaftService::NewStub(channel))
    {
    }

  crow::json::wvalue handlePut(const std::string &key, const std::string &value);
  crow::json::wvalue handleGet(const std::string &key);
  void start(int port);

  private:
    std::unique_ptr<raft_protocol::RaftService::Stub> stub_;
};

#endif // HTTP_SERVER_H