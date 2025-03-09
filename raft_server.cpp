#include "raft_server.h"

grpc::Status RaftServiceImpl::RequestForVote(grpc::ServerContext *context,
                                             const raft_protocol::RequestVoteRequest *request,
                                             raft_protocol::RequestVoteResponse *response)
{
    std::cout << "Received RequestForVote: term = " << request->term() << ", candidateId = " << request->candidateid()
              << std::endl;

    response->set_term(1);
    response->set_votegranted(true);

    return grpc::Status::OK;
}

grpc::Status RaftServiceImpl::AppendEntries(grpc::ServerContext *context,
                                            const raft_protocol::AppendEntriesRequest *request,
                                            raft_protocol::AppendEntriesResponse *response)
{
    std::cout << "Received AppendEntries: term = " << request->term() << ", leaderId = " << request->leaderid()
              << std::endl;

    response->set_term(1);
    response->set_success(true);

    return grpc::Status::OK;
}