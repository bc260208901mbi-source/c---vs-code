#include "httplib.h"
#include "database.h"
#include "routes.h"
#include <iostream>

int main() {
    Database db("signage.db");
    if (!db.init()) {
        std::cerr << "Failed to init database" << std::endl;
        return 1;
    }

    httplib::Server svr;

    // CORS for easy frontend testing
    svr.set_pre_routing_handler([](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type");
        if (req.method == "OPTIONS") {
            res.status = 200;
            return httplib::Server::HandlerResponse::Handled;
        }
        return httplib::Server::HandlerResponse::Unhandled;
    });

    registerDeviceRoutes(svr, db);
    registerMediaRoutes(svr, db, "media_storage", "/api/media");

    std::cout << "Server starting on port 8080..." << std::endl;
    svr.listen("0.0.0.0", 8080);
    return 0;
}