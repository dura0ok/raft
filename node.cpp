#include "node.h"
#include "logger.h"
#include "util.h"
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/security/credentials.h>
#include <grpc++/channel.h>
#include <proto/raft.grpc.pb.h>
#include <proto/raft.pb.h>
#include <random>
#include <thread>

void RaftNode::sendHeartbeats()
{
    while (getState() == NodeState::LEADER)
    {
        Logger::log("Sending heartbeats lock");
       mutex_.lock();
        for (const auto &item : node_configs_)
        {
            if (config_.getId() == item.getId())
                continue;
            // Logger::log("Start sending to " + item.getId() + " " + config_.getId());
            raft_protocol::AppendEntriesRequest request;
            request.set_term(getCurrentTerm());
            request.set_leaderid(config_.getId());
            int prevLogIndex = log_.getLastIndex();
            request.set_prevlogindex(prevLogIndex);
            request.set_prevlogterm(log_.getLastTerm());

            for (const auto &logEntry : log_.getEntriesAfter(prevLogIndex))
            {
                raft_protocol::LogEntry *log_entry = request.add_entries();
                log_entry->set_term(logEntry.term);
                log_entry->set_command(static_cast<raft_protocol::CommandType>(logEntry.command));
                log_entry->set_index(logEntry.index);
                log_entry->set_key(logEntry.key);
                log_entry->set_value(logEntry.value);
            }

            raft_protocol::AppendEntriesResponse response;
            grpc::ClientContext context;
            context.set_deadline(std::chrono::system_clock::now() + std::chrono::milliseconds(100));

            grpc::ChannelArguments channel_args;
            channel_args.SetInt(GRPC_ARG_ENABLE_RETRIES, 0);

            auto channel = grpc::CreateCustomChannel(
                item.getAddress() + ":" + std::to_string(item.getPort()),
                grpc::InsecureChannelCredentials(),
                channel_args
            );

            auto stub = raft_protocol::RaftService::NewStub(channel);

            grpc::Status status = stub->AppendEntries(&context, request, &response);
            if (status.ok())
            {
                Logger::log("Sent success to " + item.getId());

                if (getCurrentTerm() < response.term())
                {
                    setCurrentTerm(response.term());
                    setState(NodeState::FOLLOWER);
                    break;
                }
            }else
            {
                Logger::log("Sent failed " + item.getId());
            }

        }
        Logger::log("Sending heartbeats unlock");
        mutex_.unlock();
        std::this_thread::sleep_for(
            std::chrono::milliseconds(heartbeat_timeout_));
    }
}

bool RaftNode::tryToBecameLeader()
{
    Logger::log("tryToBecameLeader lock guard");
     std::lock_guard lock(mutex_);
    Logger::log("tryToBecameLeader lock guard after");

    Logger::log("Node " + config_.getId() + " is attempting to start election for term " +
                std::to_string(current_term_));

    setState(NodeState::CANDIDATE);
    ++current_term_;
    setVotedFor(config_.getId());

    int votes = 1;
    auto majority = node_configs_.size() / 2 + 1;

    Logger::log("Node " + config_.getId() + " has become CANDIDATE for term " +
                std::to_string(current_term_));

    for (const auto &item : node_configs_)
    {
        if (config_.getId() == item.getId())
        {
            Logger::log("Skipping voting request for self (Node ID: " + config_.getId() + ")");
            continue;
        }

        Logger::log("Node " + config_.getId() + " requesting vote from Node " +
                    item.getId() + " for term " + std::to_string(current_term_));

        raft_protocol::RequestVoteRequest request;
        request.set_term(current_term_);
        request.set_candidateid(config_.getId());

        raft_protocol::RequestVoteResponse response;
        grpc::ClientContext context;
        context.set_deadline(std::chrono::system_clock::now() + std::chrono::milliseconds(100));

        grpc::ChannelArguments channel_args;

        auto channel = grpc::CreateCustomChannel(
            item.getAddress() + ":" + std::to_string(item.getPort()),
            grpc::InsecureChannelCredentials(),
            channel_args
        );
        channel_args.SetInt(GRPC_ARG_ENABLE_RETRIES, 0);

        auto stub = raft_protocol::RaftService::NewStub(channel);


        Logger::log("Sending vote request to Node " + item.getId());
        grpc::Status status = stub->RequestForVote(&context, request, &response);

        if (getCurrentTerm() < response.term())
        {
            setCurrentTerm(response.term());
            setState(NodeState::FOLLOWER);
        }

        if (status.ok())
        {
            if (response.votegranted())
            {
                Logger::log("Node " + item.getId() + " granted vote to Node " +
                            config_.getId() + " for term " + std::to_string(current_term_));
                votes++;
            }
            else
            {
                Logger::log("Node " + item.getId() + " did not grant vote to Node " +
                            config_.getId() + " for term " + std::to_string(current_term_));
            }
        }else
        {
            Logger::log("Status sending vote not ok node " + item.getId());
        }
    }

    Logger::log("Node " + config_.getId() + " has received " + std::to_string(votes) +
                " votes in total for term " + std::to_string(current_term_));

    if (votes >= majority)
    {
        setState(NodeState::LEADER);
        Logger::log("Node " + config_.getId() + " has won the election and is now the LEADER for term " +
                    std::to_string(current_term_));
        setLeaderId(config_.getId());

        return true;
    }
    else
    {
        Logger::log("Node " + config_.getId() + " failed to win election for term " +
                    std::to_string(current_term_));
    }
    return false;
}


void RaftNode::startElection()
{
    setLeaderId("");
    const auto res = tryToBecameLeader();
    Logger::log("Successfully log guard tryToBecameLeader");
    if (res)
    {
        sendHeartbeats();
    }
}


void RaftNode::resetElectionTimer()
{
    Logger::log("Resetting election timer");
    ticker_.reset();
}

void RaftNode::setLeaderId(const std::string &leader_id)
{
    leader_id_ = leader_id;
}

std::string RaftNode::getLeaderAddress() const
{
    for (const auto &node : node_configs_)
    {
        if (node.getId() == leader_id_)
        {
            return node.getAddress() + ":" + std::to_string(node.getHttpPort());
        }
    }
    return "";
}

bool RaftNode::isLeader() const
{
    return this->config_.getId() == leader_id_;
}
