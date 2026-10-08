#include "routes.h"
#include "json.hpp"
#include <ctime>

using json = nlohmann::json;

void registerDeviceRoutes(httplib::Server& svr, Database& db) {

    // POST /api/devices/register
    svr.Post("/api/devices/register", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            auto j = json::parse(req.body);
            if (!j.contains("device_id") || !j["device_id"].is_string()) {
                res.status = 400;
                res.set_content("{\"error\":\"Missing device_id\"}", "application/json");
                return;
            }
            std::string device_id = j["device_id"];
            if (db.registerDevice(device_id)) {
                Device d = db.getDevice(device_id);
                json response = {{"id", d.id}, {"name", d.name},
                                 {"status", d.status}, {"last_seen", d.last_seen}};
                res.status = 200;
                res.set_content(response.dump(), "application/json");
            } else {
                res.status = 500;
                res.set_content("{\"error\":\"Failed to register\"}", "application/json");
            }
        } catch (...) {
            res.status = 400;
            res.set_content("{\"error\":\"Invalid JSON\"}", "application/json");
        }
    });

    // GET /api/devices
    svr.Get("/api/devices", [&](const httplib::Request&, httplib::Response& res) {
        auto devices = db.getAllDevices();
        json arr = json::array();
        long long now = std::time(nullptr);
        for (const auto& d : devices) {
            bool online = (now - d.last_seen) <= 30;
            arr.push_back({{"id", d.id}, {"name", d.name},
                           {"status", online ? "online" : "offline"},
                           {"last_seen", d.last_seen}});
        }
        res.status = 200;
        res.set_content(arr.dump(), "application/json");
    });

    // GET /api/devices/{id}
    svr.Get(R"(/api/devices/([^/]+))", [&](const httplib::Request& req, httplib::Response& res) {
        std::string id = req.matches[1];
        Device d = db.getDevice(id);
        if (d.id.empty()) {
            res.status = 404;
            res.set_content("{\"error\":\"Device not found\"}", "application/json");
            return;
        }
        long long now = std::time(nullptr);
        bool online = (now - d.last_seen) <= 30;
        json dev = {{"id", d.id}, {"name", d.name},
                    {"status", online ? "online" : "offline"},
                    {"last_seen", d.last_seen}};
        res.status = 200;
        res.set_content(dev.dump(), "application/json");
    });

    // PUT /api/devices/{id}/name
    svr.Put(R"(/api/devices/([^/]+)/name)", [&](const httplib::Request& req, httplib::Response& res) {
        std::string id = req.matches[1];
        try {
            auto j = json::parse(req.body);
            if (!j.contains("name") || !j["name"].is_string()) {
                res.status = 400;
                res.set_content("{\"error\":\"Missing name\"}", "application/json");
                return;
            }
            if (db.updateDeviceName(id, j["name"])) {
                res.status = 200;
                res.set_content("{\"message\":\"Name updated\"}", "application/json");
            } else {
                res.status = 404;
                res.set_content("{\"error\":\"Device not found\"}", "application/json");
            }
        } catch (...) {
            res.status = 400;
            res.set_content("{\"error\":\"Invalid JSON\"}", "application/json");
        }
    });

    // POST /api/devices/{id}/heartbeat
    svr.Post(R"(/api/devices/([^/]+)/heartbeat)", [&](const httplib::Request& req, httplib::Response& res) {
        std::string id = req.matches[1];
        if (db.updateHeartbeat(id)) {
            res.status = 200;
            res.set_content("{\"message\":\"Heartbeat received\"}", "application/json");
        } else {
            res.status = 404;
            res.set_content("{\"error\":\"Device not found\"}", "application/json");
        }
    });
}