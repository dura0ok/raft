#include "raft_server.h"
#include "util.hpp"

grpc::Status RaftServiceImpl::RequestForVote(grpc::ServerContext *context,
                                             const raft_protocol::RequestVoteRequest *request,
                                             raft_protocol::RequestVoteResponse *response)
{
    std::cout << "Received RequestForVote: term = " << request->term() << ", candidateId = " << request->candidateid()
              << " " << getCurrentMsTime() << std::endl;

    std::lock_guard lock(node.mutex);

    if (request->term() > node.current_term)
    {
        node.current_term = request->term();
        node.state = NodeState::FOLLOWER;
        node.voted_for = -1;
    }

    const auto request_candidate_id = std::stoi(request->candidateid());

    if (node.voted_for == -1 || node.voted_for == request_candidate_id)
    {
        node.voted_for = request_candidate_id;
        response->set_votegranted(true);
    }
    else
    {
        response->set_votegranted(false);
    }

    response->set_term(node.current_term);
    return grpc::Status::OK;
}

grpc::Status RaftServiceImpl::AppendEntries(grpc::ServerContext *context,
                                            const raft_protocol::AppendEntriesRequest *request,
                                            raft_protocol::AppendEntriesResponse *response)
{
    std::cout << "Received AppendEntries: term = " << request->term() << ", leaderId = " << request->leaderid() << " "
              << getCurrentMsTime() << std::endl;

    std::lock_guard lock(node.mutex);

    if (request->term() >= node.current_term)
    {
        node.current_term = request->term();
        node.state = NodeState::FOLLOWER;
        node.voted_for = std::stoi(request->leaderid());
        node.resetElectionTimer();
    }

    response->set_term(node.current_term);
    response->set_success(true);
    return grpc::Status::OK;
}