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
        Logger::log("💓 [Heartbeat] Acquiring lock to send heartbeats");
        mutex_.lock();

        auto selfId = config_.getId();
        int currentTerm = getCurrentTerm();
        int commitIndex = getCommitIndex();
        int quorum = node_configs_.size() / 2 + 1;

        Logger::log("💓 [Heartbeat] Leader: " + selfId +
                    ", Term: " + std::to_string(currentTerm) +
                    ", CommitIndex: " + std::to_string(commitIndex) +
                    ", Quorum: " + std::to_string(quorum));

        for (const auto &item : node_configs_)
        {
            std::string nodeId = item.getId();
            if (nodeId == selfId)
                continue;

            if (nextIndex.find(nodeId) == nextIndex.end())
                nextIndex[nodeId] = log_.getLastIndex() + 1;

            int nextIdx = nextIndex[nodeId];
            int prevIdx = nextIdx - 1;
            int prevTerm = prevIdx >= 0 ? log_.getEntry(prevIdx).term : 0;

            Logger::log("➡️  [Send] Preparing AppendEntries to " + nodeId +
                        " (nextIdx=" + std::to_string(nextIdx) +
                        ", prevIdx=" + std::to_string(prevIdx) +
                        ", prevTerm=" + std::to_string(prevTerm) + ")");

            raft_protocol::AppendEntriesRequest request;
            request.set_term(currentTerm);
            request.set_leaderid(selfId);
            request.set_leadercommit(commitIndex);
            request.set_prevlogindex(prevIdx);
            request.set_prevlogterm(prevTerm);

            if (nextIdx <= log_.getLastIndex())
            {
                LogEntry logEntry = log_.getEntry(nextIdx);
                auto *entry = request.add_entries();
                entry->set_term(logEntry.term);
                entry->set_index(logEntry.index);

                raft_protocol::Command *command = entry->mutable_command();
                if (logEntry.command == CommandType::PUT)
                {
                    auto *set_cmd = command->mutable_set();
                    set_cmd->set_key(logEntry.key);
                    set_cmd->set_value(logEntry.value);
                    Logger::log("📦 [Entry] PUT " + logEntry.key + " = " + logEntry.value);
                }
                else if (logEntry.command == CommandType::DELETE)
                {
                    auto *del_cmd = command->mutable_delete_();
                    del_cmd->set_key(logEntry.key);
                    Logger::log("📦 [Entry] DELETE " + logEntry.key);
                }
            }
            else
            {
                Logger::log("📭 [Heartbeat] No new entries for " + nodeId + ", sending empty heartbeat");
            }

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

            raft_protocol::AppendEntriesResponse response;
            grpc::Status status = stub->AppendEntries(&context, request, &response);

            if (status.ok())
            {
                Logger::log("✅ [RPC OK] Response from " + nodeId +
                            " | term=" + std::to_string(response.term()) +
                            " | success=" + std::to_string(response.success()));

                if (response.term() > currentTerm)
                {
                    Logger::log("⚠️  [Term Mismatch] Received higher term from " + nodeId +
                                ": " + std::to_string(response.term()) +
                                " > " + std::to_string(currentTerm));
                    setCurrentTerm(response.term());
                    setState(NodeState::FOLLOWER);
                    mutex_.unlock();
                    return;
                }

                if (getState() != NodeState::LEADER || getCurrentTerm() != currentTerm)
                {
                    Logger::log("🛑 [Abort] No longer leader or term changed");
                    mutex_.unlock();
                    return;
                }

                if (response.success())
                {
                    int lastSent = request.entries_size() > 0
                        ? request.entries(request.entries_size() - 1).index()
                        : prevIdx;

                    matchIndex[nodeId] = lastSent;
                    nextIndex[nodeId] = lastSent + 1;
                    Logger::log("✅ [Success] Updated matchIndex[" + nodeId + "] = " + std::to_string(lastSent));
                    Logger::log("➡️  [Update] nextIndex[" + nodeId + "] = " + std::to_string(lastSent + 1));
                }
                else
                {
                    nextIndex[nodeId] = std::max(1, nextIndex[nodeId] - 1);
                    Logger::log("❌ [Rejected] Append failed — backoff nextIndex[" + nodeId + "] = " +
                                std::to_string(nextIndex[nodeId]));
                }
            }
            else
            {
                Logger::log("🚫 [RPC FAIL] Failed to contact " + nodeId +
                            " | Error: " + status.error_message());
            }
        }
        int lastIndex = log_.getLastIndex();
        Logger::log("Starting commit index update loop. lastIndex: " + std::to_string(lastIndex) + ", commitIndex: " + std::to_string(commitIndex) + ", currentTerm: " + std::to_string(currentTerm));

        for (int N = lastIndex; N > commitIndex; --N)
        {
            Logger::log("Checking index N: " + std::to_string(N));

            if (log_.getEntry(N).term != currentTerm)
            {
                Logger::log("Skipping index N: " + std::to_string(N) + " because term " + std::to_string(log_.getEntry(N).term) + " does not match currentTerm " + std::to_string(currentTerm));
                continue;
            }

            int count = 1; // self
            Logger::log("Initializing count to 1 (self) for index N: " + std::to_string(N));

            for (const auto &[nodeId, matchedIdx] : matchIndex)
            {
                Logger::log("Checking node " + nodeId + " with matchedIdx: " + std::to_string(matchedIdx) + " against N: " + std::to_string(N));
                if (matchedIdx >= N)
                {
                    count++;
                    Logger::log("Incrementing count because matchedIdx " + std::to_string(matchedIdx) + " >= N " + std::to_string(N) + ". New count: " + std::to_string(count));
                }
            }

            Logger::log("Final count for index N: " + std::to_string(N) + " is " + std::to_string(count) + ", Quorum is " + std::to_string(quorum));

            if (count >= quorum)
            {
                Logger::log("🎯 [Commit] Advancing commit index to " + std::to_string(N));
                setCommitIndex(N);
                applyLogs();
                Logger::log("Commit index advanced to " + std::to_string(N) + ". Breaking the loop.");
                break;
            }
        }


        Logger::log("🔓 [Heartbeat] Releasing lock");
        mutex_.unlock();
        std::this_thread::sleep_for(std::chrono::milliseconds(heartbeat_timeout_));
    }

    Logger::log("🛑 [Heartbeat Thread] Exiting — no longer LEADER");
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

        nextIndex[item.getId()] = log_.getLastIndex() + 1;
        matchIndex[item.getId()] = -1;


        Logger::log("Node " + config_.getId() + " requesting vote from Node " +
                    item.getId() + " for term " + std::to_string(current_term_));

        raft_protocol::RequestVoteRequest request;
        request.set_term(current_term_);
        request.set_candidateid(config_.getId());
        request.set_lastlogindex(log_.getLastIndex());
        request.set_lastlogterm(log_.getLastTerm());

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

void RaftNode::applyLogs()
{
    while (getLastApplied() < getCommitIndex())
    {
        uint64_t indexToApply = getLastApplied() + 1;
        LogEntry logEntry = log_.getEntry(indexToApply);
        if (logEntry.command == CommandType::PUT)
        {
            setValue(logEntry.key, logEntry.value);
        }
        else if (logEntry.command == CommandType::DELETE)
        {
            deleteKey(logEntry.key);  // Apply the 'DELETE' command
        }

        setLastApplied(indexToApply);
        Logger::log("Applied log entry: term=" + std::to_string(logEntry.term) +
                    ", index=" + std::to_string(indexToApply) +
                    ", command=" + (logEntry.command == CommandType::PUT ? "PUT" : "DELETE") +
                    ", key=" + logEntry.key +
                    (logEntry.command == CommandType::PUT ? ", value=" + logEntry.value : ""));
    }
}
