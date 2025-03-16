#include "node.h"
#include "logger.h"
#include "util.h"
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/security/credentials.h>
#include <proto/raft.grpc.pb.h>
#include <proto/raft.pb.h>
#include <random>
#include <thread>

void RaftNode::sendHeartbeats() const
{
    while (getState() == NodeState::LEADER)
    {
        for (const auto &item : node_configs_)
        {
            if (config_.getId() == item.getId())
                continue;
            Logger::log("Start sending to " + std::to_string(item.getId()) + " " + std::to_string(config_.getId()));
            raft_protocol::AppendEntriesRequest request;
            request.set_term(getCurrentTerm());
            request.set_leaderid(std::to_string(config_.getId()));

            raft_protocol::AppendEntriesResponse response;
            grpc::ClientContext context;
            context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(1));

            auto stub = raft_protocol::RaftService::NewStub(grpc::CreateChannel(
                item.getAddress() + ":" + std::to_string(item.getPort()), grpc::InsecureChannelCredentials()));
            stub->AppendEntries(&context, request, &response);
            Logger::log("Sended to " + std::to_string(item.getId()));
        }
        std::this_thread::sleep_for(
            std::chrono::milliseconds(util::get_random_interval(heartbeat_timeout_, heartbeat_timeout_ + 100)));
    }
}

void RaftNode::startElection()
{
    std::lock_guard lock(mutex_);
    setState(NodeState::CANDIDATE);
    ++current_term_;
    setVotedFor(config_.getId());

    int votes = 1;
    auto majority = node_configs_.size() / 2 + 1;

    Logger::log("Node " + std::to_string(config_.getId()) + " starting election for term " +
                std::to_string(getCurrentTerm()));

    for (const auto &item : node_configs_)
    {
        if (config_.getId() == item.getId())
            continue;

        raft_protocol::RequestVoteRequest request;
        request.set_term(current_term_);
        request.set_candidateid(std::to_string(config_.getId()));

        raft_protocol::RequestVoteResponse response;
        grpc::ClientContext context;
        context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(1));
        auto stub = raft_protocol::RaftService::NewStub(grpc::CreateChannel(
            item.getAddress() + ":" + std::to_string(item.getPort()), grpc::InsecureChannelCredentials()));
        grpc::Status status = stub->RequestForVote(&context, request, &response);

        if (status.ok() && response.votegranted())
        {
            votes++;
        }
    }

    if (votes >= majority)
    {
        setState(NodeState::LEADER);
        Logger::log("Node " + std::to_string(config_.getId()) + " won election for term " +
                    std::to_string(current_term_));
        sendHeartbeats();
    }
}

void RaftNode::resetElectionTimer()
{
    Logger::log("Resetting election timer");
    ticker_.reset();
}
