#include "http_server.h"
#include <crow.h>
#include <iostream>

struct JsonHeaderMiddleware
{
    struct context
    {
    };

    void before_handle(crow::request & /*req*/, crow::response &res, context & /*ctx*/)
    {
    }

    void after_handle(crow::request &req, crow::response &res, context &ctx)
    {
        res.add_header("Content-Type", "application/json");
    }
};

void RaftHTTPServer::start(int port)
{
    crow::App<JsonHeaderMiddleware> app;


    CROW_ROUTE(app, "/put").methods(crow::HTTPMethod::Post)([this](const crow::request &req) {
        if (!raft_node_.isLeader())
        {
            std::string leaderAddr = raft_node_.getLeaderAddress();
            if (leaderAddr.empty()) {
                return crow::response(503, crow::json::wvalue{{"error", "Leader not known"}});
            }
            crow::response res(307);
            res.add_header("Location", "http://" + leaderAddr + "/put");
            return res;
        }

        auto body = crow::json::load(req.body);
        if (!body || !body.has("key") || !body.has("value"))
        {
            return crow::response(400, crow::json::wvalue{{"error", "Missing key or value"}});
        }

        // Store the key-value pair in the in-memory store
        std::string key = body["key"].s();
        std::string value = body["value"].s();

        int term = raft_node_.getCurrentTerm();
        int index = raft_node_.log_.getLastIndex() + 1;

        LogEntry entry(term, CommandType::PUT, index, key, value);
        raft_node_.log_.addEntry(entry);


        return crow::response(200, crow::json::wvalue{{"result", "ok"}});
    });

    CROW_ROUTE(app, "/delete/<string>")
    .methods(crow::HTTPMethod::Delete)([this](const crow::request & /*req*/, const std::string &key) {
        if (!raft_node_.isLeader())
        {
            std::string leaderAddr = raft_node_.getLeaderAddress();
            if (leaderAddr.empty()) {
                return crow::response(503, crow::json::wvalue{{"error", "Leader not known"}});
            }
            crow::response res(307);
            res.add_header("Location", "http://" + leaderAddr + "/delete/" + key);
            return res;
        }

        // Check if key exists
        if (raft_node_.kv_store_.find(key) == raft_node_.kv_store_.end()) {
            return crow::response(404, crow::json::wvalue{{"error", "Key not found"}});
        }

        int term = raft_node_.getCurrentTerm();
        int index = raft_node_.log_.getLastIndex() + 1;

        LogEntry entry(term, CommandType::DELETE, index, key, "");
        raft_node_.log_.addEntry(entry);

        return crow::response(200, crow::json::wvalue{{"result", "ok"}});
    });




    CROW_ROUTE(app, "/get/<string>")
        .methods(crow::HTTPMethod::Get)([this](const crow::request & /*req*/, const std::string &key) {
            auto it = raft_node_.kv_store_.find(key);
            if (it == raft_node_.kv_store_.end())
            {
                return crow::response(404, crow::json::wvalue{{"error", "Key not found"}});
            }

            return crow::response(200, crow::json::wvalue{{"value", it->second}});
        });

    std::cout << "Crow HTTP Server running on port " << port << std::endl;
    app.port(port).multithreaded().run();
}