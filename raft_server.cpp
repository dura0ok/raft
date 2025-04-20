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

    Logger::log("Request for vote lock guard try");
    std::lock_guard lock(node_.getMutex());
    Logger::log("Request for vote lock guard after");
    Logger::log("ЗАХОЖУ В IF ");
    if (request->term() > node_.getCurrentTerm())
    {
        Logger::log("RequestForVote: Updating current term from " + std::to_string(node_.getCurrentTerm()) + " to " +
                    std::to_string(request->term()));
        node_.setCurrentTerm(request->term());
        node_.setState(NodeState::FOLLOWER);
        node_.setVotedFor("");
        Logger::log("Node is now FOLLOWER with term " + std::to_string(node_.getCurrentTerm()));
    }

    const auto& request_candidate_id = request->candidateid();
    Logger::log("ЗАХОЖУ В IF 2");
    if ((node_.getVotedFor().empty() || node_.getVotedFor() == request_candidate_id))
    {
        Logger::log("Granting vote to candidate " + request_candidate_id);
        node_.setVotedFor(request_candidate_id);
        response->set_votegranted(true);
        node_.resetElectionTimer();
    }
    else
    {
        Logger::log("Vote not granted. Already voted for " + node_.getVotedFor());
        response->set_votegranted(false);
    }

    response->set_term(node_.getCurrentTerm());
    Logger::log("Responding with term " + std::to_string(node_.getCurrentTerm()) +
                " and vote granted status: " + (response->votegranted() ? "true" : "false"));
    Logger::log("Lock guard Request for vote finished");

    return grpc::Status::OK;
}

grpc::Status RaftServiceImpl::AppendEntries(grpc::ServerContext *context,
                                            const raft_protocol::AppendEntriesRequest *request,
                                            raft_protocol::AppendEntriesResponse *response)
{
    Logger::log("Received AppendEntries: term = " + std::to_string(request->term()) +
                ", leaderId = " + request->leaderid());

    Logger::log("AppendEntries lock guard try");
    std::lock_guard lock(node_.getMutex());
    Logger::log("AppendEntries lock guard after");

    if (request->term() >= node_.getCurrentTerm())
    {
        node_.setLeaderId(request->leaderid());

        Logger::log("AppendEntries: Updating current term from " + std::to_string(node_.getCurrentTerm()) + " to " +
                    std::to_string(request->term()));
        node_.setCurrentTerm(request->term());
        node_.setState(NodeState::FOLLOWER);
        node_.setVotedFor(request->leaderid());
        node_.resetElectionTimer();
        Logger::log("Node is now FOLLOWER, voted for leader " + request->leaderid() +
                    ", reset election timer.");
    }

    response->set_term(node_.getCurrentTerm());
    response->set_success(true);
    Logger::log("Responding to AppendEntries with term " + std::to_string(node_.getCurrentTerm()) +
                " and success status: true");
    Logger::log("Lock guard AppendEntries finished");
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

    std::thread httpThread([server_address, it, &cur_node]() {
        RaftHTTPServer httpServer(grpc::CreateChannel(server_address, grpc::InsecureChannelCredentials()), cur_node);
        httpServer.start(it->getHttpPort());
    });
    httpThread.join();
    server_thread.join();
}