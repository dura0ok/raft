#ifndef NODE_H
#define NODE_H

#include "ticker.h"
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

enum class NodeState
{
    FOLLOWER,
    CANDIDATE,
    LEADER
};

class NodeConfig
{
  public:
    NodeConfig(const int id, std::string address, const int port) : id_(id), address_(std::move(address)), port_(port)
    {
    }

    [[nodiscard]] int getId() const
    {
        return id_;
    }
    [[nodiscard]] const std::string &getAddress() const
    {
        return address_;
    }
    [[nodiscard]] int getPort() const
    {
        return port_;
    }

  private:
    int id_;
    std::string address_;
    int port_;
};

class RaftNode
{
  public:
    explicit RaftNode(const std::vector<NodeConfig> &node_configs, NodeConfig config, int election_timeout,
                      int heartbeat_timeout)
        : config_(std::move(config)), node_configs_(node_configs),
          ticker_(election_timeout, [this] { startElection(); }), heartbeat_timeout_(heartbeat_timeout)
    {
    }

    void startElection();
    void resetElectionTimer();
    void sendHeartbeats() const;
    void requestVotes(std::atomic<int> &votes, int majority);
    void declareLeader();
    void run();

    int getCurrentTerm() const
    {
        return current_term_.load();
    }
    void setCurrentTerm(const int term)
    {
        current_term_ = term;
    }
    int getVotedFor() const
    {
        return voted_for_.load();
    }
    void setVotedFor(const int candidateId)
    {
        voted_for_ = candidateId;
    }
    NodeState getState() const
    {
        return state_.load();
    }
    void setState(const NodeState &state)
    {
        state_ = state;
    }
    std::mutex &getMutex()
    {
        return mutex_;
    }

  private:
    std::mutex mutex_;
    NodeConfig config_;
    std::atomic<int> current_term_{0};
    std::atomic<int> voted_for_{-1};
    std::atomic<NodeState> state_{NodeState::FOLLOWER};
    const std::vector<NodeConfig> &node_configs_;
    Ticker ticker_;
    int heartbeat_timeout_;
};

#endif