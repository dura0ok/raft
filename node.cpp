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

    Logger::log("Node " + std::to_string(config_.getId()) + " is attempting to start election for term " +
                std::to_string(current_term_));

    setState(NodeState::CANDIDATE);
    ++current_term_;
    setVotedFor(config_.getId());

    int votes = 1;
    auto majority = node_configs_.size() / 2 + 1;

    Logger::log("Node " + std::to_string(config_.getId()) + " has become CANDIDATE for term " +
                std::to_string(current_term_));

    for (const auto &item : node_configs_)
    {
        if (config_.getId() == item.getId())
        {
            Logger::log("Skipping voting request for self (Node ID: " + std::to_string(config_.getId()) + ")");
            continue;
        }

        Logger::log("Node " + std::to_string(config_.getId()) + " requesting vote from Node " +
                    std::to_string(item.getId()) + " for term " + std::to_string(current_term_));

        raft_protocol::RequestVoteRequest request;
        request.set_term(current_term_);
        request.set_candidateid(std::to_string(config_.getId()));

        raft_protocol::RequestVoteResponse response;
        grpc::ClientContext context;
        context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(1));

        auto stub = raft_protocol::RaftService::NewStub(grpc::CreateChannel(
            item.getAddress() + ":" + std::to_string(item.getPort()), grpc::InsecureChannelCredentials()));

        Logger::log("Sending vote request to Node " + std::to_string(item.getId()));
        grpc::Status status = stub->RequestForVote(&context, request, &response);

        if (status.ok())
        {
            if (response.votegranted())
            {
                Logger::log("Node " + std::to_string(item.getId()) + " granted vote to Node " +
                            std::to_string(config_.getId()) + " for term " + std::to_string(current_term_));
                votes++;
            }
            else
            {
                Logger::log("Node " + std::to_string(item.getId()) + " did not grant vote to Node " +
                            std::to_string(config_.getId()) + " for term " + std::to_string(current_term_));
            }
        }
        else
        {
            Logger::log("Failed to send vote request to Node " + std::to_string(item.getId()) + " Error: " +
                        status.error_message());
        }
    }

    Logger::log("Node " + std::to_string(config_.getId()) + " has received " + std::to_string(votes) +
                " votes in total for term " + std::to_string(current_term_));

    if (votes >= majority)
    {
        setState(NodeState::LEADER);
        Logger::log("Node " + std::to_string(config_.getId()) + " has won the election and is now the LEADER for term " +
                    std::to_string(current_term_));
        sendHeartbeats();
    }
    else
    {
        Logger::log("Node " + std::to_string(config_.getId()) + " failed to win election for term " +
                    std::to_string(current_term_));
    }
}


void RaftNode::resetElectionTimer()
{
    Logger::log("Resetting election timer");
    ticker_.reset();
}
