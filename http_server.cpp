#include "http_server.h"
#include <crow.h>
#include <iostream>

crow::json::wvalue RaftHTTPServer::handlePut(const std::string &key, const std::string &value) {
    raft_protocol::PutRequest request;
    request.set_key(key);
    request.set_value(value);
    raft_protocol::PutResponse response;
    grpc::ClientContext context;

    grpc::Status status = stub_->Put(&context, request, &response);
    crow::json::wvalue result;
    if (status.ok()) {
        result["success"] = true;
    } else {
        result["error"] = "RPC failed";
    }
    return result;
}

crow::json::wvalue RaftHTTPServer::handleGet(const std::string &key) {
    raft_protocol::GetRequest request;
    request.set_key(key);
    raft_protocol::GetResponse response;
    grpc::ClientContext context;

    grpc::Status status = stub_->Get(&context, request, &response);
    crow::json::wvalue result;
    if (status.ok() && response.found()) {
        result["value"] = response.value();
        result["found"] = true;
    } else {
        result["found"] = false;
    }
    return result;
}

void RaftHTTPServer::start(int port) {
    crow::SimpleApp app;

    CROW_ROUTE(app, "/put").methods(crow::HTTPMethod::Post)([this](const crow::request &req) {
        auto body = crow::json::load(req.body);
        if (!body || !body.has("key") || !body.has("value")) {
            return crow::response(400, crow::json::wvalue({{"error", "Missing key or value"}}));
        }
        return crow::response(200, handlePut(body["key"].s(), body["value"].s()));
    });

    CROW_ROUTE(app, "/get/<string>").methods(crow::HTTPMethod::Get)([this](const crow::request &, std::string key) {
        return crow::response(200, handleGet(key));
    });

    std::cout << "Crow HTTP Server running on port " << port << std::endl;
    app.port(port + 100).multithreaded().run();
}
