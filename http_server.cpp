#include "http_server.h"
#include <crow.h>
#include <iostream>

void RaftHTTPServer::start(int port) {
    // crow::SimpleApp app;
    //
    // CROW_ROUTE(app, "/put").methods(crow::HTTPMethod::Post)([this](const crow::request &req) {
    //     auto body = crow::json::load(req.body);
    //     if (!body || !body.has("key") || !body.has("value")) {
    //         return crow::response(400, crow::json::wvalue({{"error", "Missing key or value"}}));
    //     }
    //     return crow::response(200, handlePut(body["key"].s(), body["value"].s()));
    // });
    //
    // CROW_ROUTE(app, "/get/<string>").methods(crow::HTTPMethod::Get)([this](const crow::request &, std::string key) {
    //     return crow::response(200, handleGet(key));
    // });
    //
    // std::cout << "Crow HTTP Server running on port " << port << std::endl;
    // app.port(port + 100).multithreaded().run();
}
