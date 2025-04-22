#ifndef NODE_H
#define NODE_H

#include "log_entry.h"
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
    NodeConfig(const std::string &id, std::string address, const int port, const int http_port_)
        : id_(id), address_(std::move(address)), port_(port), http_port_(http_port_)
    {
    }

    [[nodiscard]] std::string getId() const
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

    [[nodiscard]] int getHttpPort() const
    {
        return http_port_;
    }

  private:
    std::string id_;
    std::string address_;
    int port_;
    int http_port_;
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
    void sendHeartbeats();
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
    [[nodiscard]] std::string getVotedFor() const
    {
        return voted_for_;
    }
    void setVotedFor(const std::string& candidateId)
    {
        voted_for_ = candidateId;
    }
    [[nodiscard]] NodeState getState() const
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

    int getCommitIndex() const
    {
        return commit_index_.load();
    }

    void setCommitIndex(int commit_index)
    {
        commit_index_ = commit_index;
    }

    void setLeaderId(const std::string &leader_id);
    [[nodiscard]] std::string getLeaderAddress() const;
    bool isLeader() const;
    void applyLogs();
    Log log_;

    std::string getValue(const std::string& key) const
    {
        auto it = kv_store_.find(key);
        if (it != kv_store_.end()) {
            return it->second;
        }
        return "";
    }

    void setValue(const std::string& key, const std::string& value)
    {
        kv_store_[key] = value;
    }

    void deleteKey(const std::string& key)
    {
        kv_store_.erase(key);
    }

    int getLastApplied() const
    {
        return last_applied_.load();
    }

    void setLastApplied(int last_applied)
    {
        last_applied_ = last_applied;
    }

    std::unordered_map<std::string, std::string> kv_store_;

  private:

    bool tryToBecameLeader();
    std::mutex mutex_;
    NodeConfig config_;
    std::atomic<int> current_term_{0};
    std::string voted_for_{};
    std::atomic<NodeState> state_{NodeState::FOLLOWER};
    const std::vector<NodeConfig> &node_configs_;
    Ticker ticker_;
    int heartbeat_timeout_;
    std::string leader_id_{};
    std::atomic<int> commit_index_{-1};
    std::atomic<int> last_applied_{-1};

    std::unordered_map<std::string, int> matchIndex;
    std::unordered_map<std::string, int> nextIndex;
};

#endif