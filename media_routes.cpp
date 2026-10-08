#include "routes.h"
#include "json.hpp"
#include <filesystem>
#include <fstream>
#include <ctime>
#include <cstdlib>
#include <iostream>

using json = nlohmann::json;
namespace fs = std::filesystem;

// Simple extension -> type mapping
static std::string detectType(const std::string& filename) {
    auto dot = filename.find_last_of('.');
    if (dot == std::string::npos) return "unknown";
    std::string ext = filename.substr(dot);
    for (auto& c : ext) c = std::tolower(c);

    if (ext == ".jpg" || ext == ".jpeg" || ext == ".png" ||
        ext == ".gif" || ext == ".webp") return "image";
    if (ext == ".mp4" || ext == ".webm" || ext == ".mov" ||
        ext == ".mkv") return "video";
    if (ext == ".txt" || ext == ".json") return "text";
    return "unknown";
}

static std::string mimeFor(const std::string& type, const std::string& filename) {
    auto dot = filename.find_last_of('.');
    std::string ext = (dot == std::string::npos) ? "" : filename.substr(dot);
    for (auto& c : ext) c = std::tolower(c);

    if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
    if (ext == ".png")  return "image/png";
    if (ext == ".gif")  return "image/gif";
    if (ext == ".webp") return "image/webp";
    if (ext == ".mp4")  return "video/mp4";
    if (ext == ".webm") return "video/webm";
    if (ext == ".txt")  return "text/plain";
    if (ext == ".json") return "application/json";
    return "application/octet-stream";
}

void registerMediaRoutes(httplib::Server& svr, Database& db,
                         const std::string& storage_dir,
                         const std::string& public_prefix) {

    // Make sure storage dir exists
    fs::create_directories(storage_dir);

    // ---------------------------------------------------------------
    // POST /api/media/upload/{filename}
    // The file bytes are sent as the raw request body.
    // ---------------------------------------------------------------
    svr.Post(R"(/api/media/upload/([^/]+))",
        [&db, storage_dir, public_prefix](const httplib::Request& req, httplib::Response& res) {
        try {
            std::string original = req.matches[1];
            std::string type = detectType(original);

            std::string ext;
            auto dot = original.find_last_of('.');
            if (dot != std::string::npos) ext = original.substr(dot);

            std::string stored = std::to_string(std::time(nullptr)) + "_" +
                                 std::to_string(std::rand()) + ext;

            fs::path full = fs::path(storage_dir) / stored;

            std::ofstream ofs(full, std::ios::binary);
            if (!ofs) {
                res.status = 500;
                res.set_content("{\"error\":\"Cannot write file\"}", "application/json");
                return;
            }
            ofs.write(req.body.data(), static_cast<std::streamsize>(req.body.size()));
            ofs.close();

            int media_id = db.addMedia(stored, original, type);
            if (media_id < 0) {
                fs::remove(full);
                res.status = 500;
                res.set_content("{\"error\":\"DB insert failed\"}", "application/json");
                return;
            }

            json response = {
                {"id", media_id},
                {"filename", stored},
                {"original_name", original},
                {"type", type},
                {"url", public_prefix + "/" + std::to_string(media_id) + "/file"}
            };
            res.status = 201;
            res.set_content(response.dump(), "application/json");
        } catch (const std::exception& e) {
            std::cerr << "[upload error] " << e.what() << std::endl;
            res.status = 500;
            res.set_content(std::string("{\"error\":\"") + e.what() + "\"}", "application/json");
        }
    });

    // ---------------------------------------------------------------
    // GET /api/media  (list all uploaded media)
    // ---------------------------------------------------------------
    svr.Get("/api/media",
        [&db, storage_dir, public_prefix](const httplib::Request&, httplib::Response& res) {
        auto list = db.getAllMedia();
        json arr = json::array();
        for (const auto& m : list) {
            arr.push_back({
                {"id", m.id},
                {"filename", m.filename},
                {"original_name", m.original_name},
                {"type", m.type},
                {"uploaded_at", m.uploaded_at},
                {"url", public_prefix + "/" + std::to_string(m.id) + "/file"}
            });
        }
        res.status = 200;
        res.set_content(arr.dump(), "application/json");
    });

    // ---------------------------------------------------------------
    // GET /api/media/{id}/file  (serve the actual file)
    // ---------------------------------------------------------------
    svr.Get(R"(/api/media/(\d+)/file)",
        [&db, storage_dir, public_prefix](const httplib::Request& req, httplib::Response& res) {
        int id = std::stoi(req.matches[1]);
        Media m = db.getMedia(id);
        if (m.id == 0) {
            res.status = 404;
            res.set_content("{\"error\":\"Media not found\"}", "application/json");
            return;
        }
        fs::path full = fs::path(storage_dir) / m.filename;
        std::ifstream ifs(full, std::ios::binary);
        if (!ifs) {
            res.status = 404;
            res.set_content("{\"error\":\"File missing on disk\"}", "application/json");
            return;
        }
        std::string data((std::istreambuf_iterator<char>(ifs)),
                          std::istreambuf_iterator<char>());
        res.status = 200;
        res.set_content(data, mimeFor(m.type, m.filename).c_str());
    });

    // ---------------------------------------------------------------
    // POST /api/devices/{id}/media  body: {"media_id": 3}
    // ---------------------------------------------------------------
    svr.Post(R"(/api/devices/([^/]+)/media)",
        [&db, storage_dir, public_prefix](const httplib::Request& req, httplib::Response& res) {
        std::string device_id = req.matches[1];

        if (!db.deviceExists(device_id)) {
            res.status = 404;
            res.set_content("{\"error\":\"Device not found\"}", "application/json");
            return;
        }

        int media_id = -1;
        try {
            auto j = json::parse(req.body);
            if (!j.contains("media_id") || !j["media_id"].is_number_integer()) {
                res.status = 400;
                res.set_content("{\"error\":\"Missing media_id\"}", "application/json");
                return;
            }
            media_id = j["media_id"];
        } catch (...) {
            res.status = 400;
            res.set_content("{\"error\":\"Invalid JSON\"}", "application/json");
            return;
        }

        Media m = db.getMedia(media_id);
        if (m.id == 0) {
            res.status = 404;
            res.set_content("{\"error\":\"Media not found\"}", "application/json");
            return;
        }

        if (db.assignMediaToDevice(device_id, media_id)) {
            res.status = 200;
            res.set_content("{\"message\":\"Media assigned\"}", "application/json");
        } else {
            res.status = 500;
            res.set_content("{\"error\":\"Assign failed\"}", "application/json");
        }
    });

    // ---------------------------------------------------------------
    // DELETE /api/devices/{id}/media/{media_id}
    // ---------------------------------------------------------------
    svr.Delete(R"(/api/devices/([^/]+)/media/(\d+))",
        [&db, storage_dir, public_prefix](const httplib::Request& req, httplib::Response& res) {
        std::string device_id = req.matches[1];
        int media_id = std::stoi(req.matches[2]);
        if (db.removeMediaFromDevice(device_id, media_id)) {
            res.status = 200;
            res.set_content("{\"message\":\"Media unassigned\"}", "application/json");
        } else {
            res.status = 500;
            res.set_content("{\"error\":\"Unassign failed\"}", "application/json");
        }
    });

    // ---------------------------------------------------------------
    // GET /api/devices/{id}/media  (what the display fetches)
    // ---------------------------------------------------------------
    svr.Get(R"(/api/devices/([^/]+)/media)",
        [&db, storage_dir, public_prefix](const httplib::Request& req, httplib::Response& res) {
        std::string device_id = req.matches[1];
        if (!db.deviceExists(device_id)) {
            res.status = 404;
            res.set_content("{\"error\":\"Device not found\"}", "application/json");
            return;
        }
        auto list = db.getDeviceMedia(device_id);
        json arr = json::array();
        for (const auto& m : list) {
            arr.push_back({
                {"id", m.id},
                {"type", m.type},
                {"original_name", m.original_name},
                {"url", public_prefix + "/" + std::to_string(m.id) + "/file"}
            });
        }
        res.status = 200;
        res.set_content(arr.dump(), "application/json");
    });
}