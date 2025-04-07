#include "raft_server.h"

#include "http_server.h"

#include "logger.h"
#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/grpcpp.h>

grpc::Status RaftServiceImpl::RequestForVote(grpc::ServerContext *context,
                                             const raft_protocol::RequestVoteRequest *request,
                                             raft_protocol::RequestVoteResponse *response)
{
    Logger::log("Received RequestForVote: term = " + std::to_string(request->term()) +
                ", candidateId = " + request->candidateid());

    std::lock_guard lock(node_.getMutex());

    if (request->term() > node_.getCurrentTerm())
    {
        Logger::log("RequestForVote: Updating current term from " + std::to_string(node_.getCurrentTerm()) + " to " +
                    std::to_string(request->term()));
        node_.setCurrentTerm(request->term());
        node_.setState(NodeState::FOLLOWER);
        node_.setVotedFor(-1);
        Logger::log("Node is now FOLLOWER with term " + std::to_string(node_.getCurrentTerm()));
    }

    const auto request_candidate_id = std::stoi(request->candidateid());

    if ((node_.getVotedFor() == -1 || node_.getVotedFor() == request_candidate_id))
    {
        Logger::log("Granting vote to candidate " + std::to_string(request_candidate_id));
        node_.setVotedFor(request_candidate_id);
        response->set_votegranted(true);
    }
    else
    {
        Logger::log("Vote not granted. Already voted for " + std::to_string(node_.getVotedFor()));
        response->set_votegranted(false);
    }

    response->set_term(node_.getCurrentTerm());
    Logger::log("Responding with term " + std::to_string(node_.getCurrentTerm()) +
                " and vote granted status: " + (response->votegranted() ? "true" : "false"));

    return grpc::Status::OK;
}

grpc::Status RaftServiceImpl::AppendEntries(grpc::ServerContext *context,
                                            const raft_protocol::AppendEntriesRequest *request,
                                            raft_protocol::AppendEntriesResponse *response)
{
    Logger::log("Received AppendEntries: term = " + std::to_string(request->term()) +
                ", leaderId = " + request->leaderid());

    std::lock_guard lock(node_.getMutex());

    if (request->term() >= node_.getCurrentTerm())
    {
        Logger::log("AppendEntries: Updating current term from " + std::to_string(node_.getCurrentTerm()) + " to " +
                    std::to_string(request->term()));
        node_.setCurrentTerm(request->term());
        node_.setState(NodeState::FOLLOWER);
        node_.setVotedFor(std::stoi(request->leaderid()));
        node_.resetElectionTimer();
        Logger::log("Node is now FOLLOWER, voted for leader " + request->leaderid() +
                    ", reset election timer.");
    }

    response->set_term(node_.getCurrentTerm());
    response->set_success(true);
    Logger::log("Responding to AppendEntries with term " + std::to_string(node_.getCurrentTerm()) +
                " and success status: true");

    return grpc::Status::OK;
}

void RunServer(const RaftConfig &config)
{
    auto it = std::find_if(config.nodes.begin(), config.nodes.end(),
                           [config](const auto &node) { return node.getId() == config.current_node_id; });

    if (it == config.nodes.end())
    {
        std::cerr << "Error: Node ID " << config.current_node_id << " not found in config nodes.\n";
        std::exit(EXIT_FAILURE);
    }

    auto cur_node = RaftNode(config.nodes, *it, config.election_timeout, config.hearbeat_timeout);

    std::string server_address = it->getAddress() + ":" + std::to_string(it->getPort());
    RaftServiceImpl service(config, cur_node);

    grpc::EnableDefaultHealthCheckService(true);
    grpc::reflection::InitProtoReflectionServerBuilderPlugin();

    grpc::ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);

    std::unique_ptr server(builder.BuildAndStart());

    Logger::log("Server listening on " + server_address);

    std::thread server_thread([&server]() { server->Wait(); });

    std::thread httpThread([server_address, it]()
                          {
                              RaftHTTPServer httpServer(grpc::CreateChannel(server_address, grpc::InsecureChannelCredentials()));
                              httpServer.start( it->getPort());
                          });
    httpThread.join();
    server_thread.join();
}

grpc::Status RaftServiceImpl::Put(grpc::ServerContext *context,
                 const raft_protocol::PutRequest *request,
                 raft_protocol::PutResponse *response)
{
    std::lock_guard lock(node_.getMutex());
    store_[request->key()] = request->value();
    Logger::log("Put: key = " + request->key() + ", value = " + request->value());

    response->set_success(true);
    return grpc::Status::OK;
}

grpc::Status RaftServiceImpl::Get(grpc::ServerContext *context,
                 const raft_protocol::GetRequest *request,
                 raft_protocol::GetResponse *response)
{
    std::lock_guard lock(node_.getMutex());
    auto it = store_.find(request->key());

    if (it != store_.end())
    {
        response->set_value(it->second);
        response->set_found(true);
        Logger::log("Get: key = " + request->key() + " -> " + it->second);
    }
    else
    {
        response->set_found(false);
        Logger::log("Get: key = " + request->key() + " not found");
    }

    return grpc::Status::OK;
}