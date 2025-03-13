#ifndef NODE_H
#define NODE_H
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

struct NodeConfig
{
    int id;
    std::string address;
    int port;
};

struct RaftNode
{
    NodeConfig config;
    int current_term = 0;
    int voted_for = -1;
    std::mutex mutex;
    std::condition_variable electionCondVar;
    std::thread electionThread;
    NodeState state = NodeState::FOLLOWER;
    const std::vector<NodeConfig> &node_configs;
    int election_timeout_;
    int hearbeat_timeout_;
    explicit RaftNode(const std::vector<NodeConfig> &node_configs, NodeConfig config, int election_timeout,
                      int heartbeat_timeout)
        : config(std::move(config)), election_timeout_(election_timeout), hearbeat_timeout_(heartbeat_timeout),
          node_configs(node_configs)
    {
    }
    void startElection();

    void resetElectionTimer();

    void sendHeartbeats() const;
};

#endif