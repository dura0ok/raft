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
            crow::response res(307);
            res.add_header("Location", "http://" + raft_node_.getLeaderAddress() + "/put");
            return res;
        }

        auto body = crow::json::load(req.body);
        if (!body || !body.has("key") || !body.has("value"))
        {
            return crow::response(400, crow::json::wvalue{{"error", "Missing key or value"}});
        }
        // Здесь должна быть логика обработки PUT запроса
        return crow::response(200, crow::json::wvalue{{"result", "ok"}});
    });

    CROW_ROUTE(app, "/get/<string>")
        .methods(crow::HTTPMethod::Get)([this](const crow::request & /*req*/, const std::string &key) {
            // Здесь должна быть логика обработки GET запроса
            return crow::response(200, crow::json::wvalue{{"value", this->raft_node_.getLeaderAddress()}});
        });

    std::cout << "Crow HTTP Server running on port " << port << std::endl;
    app.port(port).multithreaded().run();
}