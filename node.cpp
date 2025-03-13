#include "node.h"

#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/security/credentials.h>
#include <iostream>
#include <proto/raft.grpc.pb.h>
#include <proto/raft.pb.h>
#include <random>
#include <thread>

#include "util.hpp"

void RaftNode::sendHeartbeats() const
{
    while (state == NodeState::LEADER)
    {
        for (const auto &item : node_configs)
        {
            if (config.id == item.id)
                continue;
            std::cout << "Start sending to " << item.id << " " << getCurrentMsTime() << " " << config.id << std::endl;
            raft_protocol::AppendEntriesRequest request;
            request.set_term(current_term);
            request.set_leaderid(std::to_string(config.id));

            raft_protocol::AppendEntriesResponse response;
            grpc::ClientContext context;
            context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(1));

            auto stub = raft_protocol::RaftService::NewStub(grpc::CreateChannel(
                item.address + ":" + std::to_string(item.port), grpc::InsecureChannelCredentials()));
            stub->AppendEntries(&context, request, &response);
            std::cout << "Sended to " << item.id << " " << getCurrentMsTime() << std::endl;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(hearbeat_timeout_));
    }
}

void RaftNode::startElection()
{
    std::lock_guard<std::mutex> lock(mutex);

    state = NodeState::CANDIDATE;
    current_term++;
    voted_for = config.id;

    int votes = 1;
    auto majority = node_configs.size() / 2 + 1;

    std::cout << "Node " << config.id << " starting election for term " << current_term << " " << getCurrentMsTime()
              << std::endl;

    for (const auto &item : node_configs)
    {
        if (config.id == item.id)
            continue;

        raft_protocol::RequestVoteRequest request;
        request.set_term(current_term);
        request.set_candidateid(std::to_string(config.id));

        raft_protocol::RequestVoteResponse response;
        grpc::ClientContext context;
        context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(1));
        auto stub = raft_protocol::RaftService::NewStub(
            grpc::CreateChannel(item.address + ":" + std::to_string(item.port), grpc::InsecureChannelCredentials()));
        grpc::Status status = stub->RequestForVote(&context, request, &response);

        if (status.ok() && response.votegranted())
        {
            votes++;
        }
    }

    if (votes >= majority)
    {
        state = NodeState::LEADER;
        std::cout << "Node " << config.id << " won election for term " << current_term << " " << getCurrentMsTime()
                  << std::endl;
        sendHeartbeats();
    }
}

void RaftNode::resetElectionTimer()
{
    electionCondVar.notify_one();
    if (electionThread.joinable())
    {
        electionThread.join();
    }

    electionThread = std::thread([this] {
        static std::mt19937 rng{std::random_device{}()};
        static std::uniform_int_distribution<int> dist(150, 299);
        const auto timeout = std::chrono::milliseconds(dist(rng));

        std::mutex dummyMutex;
        std::unique_lock<std::mutex> lock(dummyMutex);
        if (electionCondVar.wait_for(lock, timeout) == std::cv_status::no_timeout)
        {
            return;
        }

        startElection();
    });
}
