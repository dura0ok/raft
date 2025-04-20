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

    //Logger::log("Request for vote lock guard try");
    std::lock_guard lock(node_.getMutex());
    //Logger::log("Request for vote lock guard after");
    //Logger::log("ЗАХОЖУ В IF ");
    if (request->term() > node_.getCurrentTerm())
    {
       // Logger::log("RequestForVote: Updating current term from " + std::to_string(node_.getCurrentTerm()) + " to " +
        //            std::to_string(request->term()));
        node_.setCurrentTerm(request->term());
        node_.setState(NodeState::FOLLOWER);
        node_.setVotedFor("");
        Logger::log("Node is now FOLLOWER with term " + std::to_string(node_.getCurrentTerm()));
    }

    const auto& request_candidate_id = request->candidateid();
    //Logger::log("ЗАХОЖУ В IF 2");
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
    //Logger::log("Lock guard Request for vote finished");

    return grpc::Status::OK;
}

grpc::Status RaftServiceImpl::AppendEntries(grpc::ServerContext *context,
                                            const raft_protocol::AppendEntriesRequest *request,
                                            raft_protocol::AppendEntriesResponse *response)
{
    Logger::log("Received AppendEntries: term = " + std::to_string(request->term()) +
                ", leaderId = " + request->leaderid());

    std::lock_guard lock(node_.getMutex());

    if (request->term() < node_.getCurrentTerm())
    {
        response->set_term(node_.getCurrentTerm());
        response->set_success(false);
        Logger::log("AppendEntries rejected: term is less than current term");
        return grpc::Status::OK;
    }

    if (request->term() > node_.getCurrentTerm())
    {
        Logger::log("Updating term from " + std::to_string(node_.getCurrentTerm()) +
                    " to " + std::to_string(request->term()));
        node_.setCurrentTerm(request->term());
    }

    node_.setLeaderId(request->leaderid());
    node_.setState(NodeState::FOLLOWER);
    node_.setVotedFor(request->leaderid());
    node_.resetElectionTimer();

    const int prevLogIndex = request->prevlogindex();
    const int prevLogTerm = request->prevlogterm();

    if (prevLogIndex > 0 && prevLogIndex <= node_.log_.getLastIndex())
    {
        LogEntry prevEntry = node_.log_.getEntry(prevLogIndex);
        if (prevEntry.term != prevLogTerm)
        {
            Logger::log("AppendEntries failed: log inconsistency at prevLogIndex " +
                        std::to_string(prevLogIndex));
            response->set_term(node_.getCurrentTerm());
            response->set_success(false);
            return grpc::Status::OK;
        }
    }
    else if (prevLogIndex > node_.log_.getLastIndex())
    {
        Logger::log("AppendEntries failed: prevLogIndex " + std::to_string(prevLogIndex) + " not found");
        response->set_term(node_.getCurrentTerm());
        response->set_success(false);
        return grpc::Status::OK;
    }

    int lastLogIndex = node_.log_.getLastIndex();
    for (int i = 0; i < request->entries_size(); i++)
    {
        const auto& entry = request->entries(i);
        int entryIndex = entry.index();

        if (entryIndex <= lastLogIndex)
        {
            LogEntry existing = node_.log_.getEntry(entryIndex);
            if (existing.term != entry.term())
            {
                Logger::log("Conflict at index " + std::to_string(entryIndex) + ", deleting from here");
                node_.log_.deleteEntriesFrom(entryIndex);
                break;
            }
            else
            {
                Logger::log("Entry already exists at index " + std::to_string(entryIndex) + ", skipping");
                continue;
            }
        }

        CommandType cmdType;
        std::string key, value;
        if (entry.command().has_set())
        {
            cmdType = CommandType::PUT;
            key = entry.command().set().key();
            value = entry.command().set().value();
        }
        else if (entry.command().has_delete_())
        {
            cmdType = CommandType::DELETE;
            key = entry.command().delete_().key();
        }
        else
        {
            Logger::log("Unknown command in AppendEntries");
            continue;
        }

        LogEntry logEntry(entry.term(), cmdType, entryIndex, key, value);
        node_.log_.addEntry(logEntry);

        Logger::log("Appended log entry: term=" + std::to_string(entry.term()) +
                    ", index=" + std::to_string(entryIndex) +
                    ", command=" + (cmdType == CommandType::PUT ? "PUT" : "DELETE") +
                    ", key=" + key +
                    (cmdType == CommandType::PUT ? ", value=" + value : ""));
    }

    // Update commit index
    if (request->leadercommit() > node_.getCommitIndex())
    {
        auto newCommitIndex = std::min(
            request->leadercommit(),
            static_cast<uint64_t>(node_.log_.getLastIndex())
        );
        node_.setCommitIndex(newCommitIndex);
        Logger::log("Commit index updated to " + std::to_string(newCommitIndex));
        node_.applyLogs();
    }

    response->set_term(node_.getCurrentTerm());
    response->set_success(true);
    Logger::log("AppendEntries success response sent");

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