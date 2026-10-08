#include "database.h"
#include <iostream>
#include <ctime>

Database::Database(const std::string& db_path) {
    if (sqlite3_open(db_path.c_str(), &db) != SQLITE_OK) {
        std::cerr << "Cannot open database: " << sqlite3_errmsg(db) << std::endl;
        db = nullptr;
    }
}

Database::~Database() {
    if (db) sqlite3_close(db);
}

bool Database::init() {
    if (!db) return false;
    const char* sql =
        "CREATE TABLE IF NOT EXISTS devices ("
        "  id TEXT PRIMARY KEY,"
        "  name TEXT NOT NULL,"
        "  status TEXT NOT NULL,"
        "  last_seen INTEGER NOT NULL);"

        "CREATE TABLE IF NOT EXISTS media ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  filename TEXT NOT NULL,"
        "  original_name TEXT NOT NULL,"
        "  type TEXT NOT NULL,"
        "  uploaded_at INTEGER NOT NULL);"

        "CREATE TABLE IF NOT EXISTS device_media ("
        "  device_id TEXT NOT NULL,"
        "  media_id INTEGER NOT NULL,"
        "  position INTEGER NOT NULL,"
        "  PRIMARY KEY (device_id, media_id),"
        "  FOREIGN KEY (device_id) REFERENCES devices(id),"
        "  FOREIGN KEY (media_id) REFERENCES media(id));";

    char* errMsg = nullptr;
    if (sqlite3_exec(db, sql, nullptr, nullptr, &errMsg) != SQLITE_OK) {
        std::cerr << "SQL error: " << errMsg << std::endl;
        sqlite3_free(errMsg);
        return false;
    }
    return true;
}

bool Database::deviceExists(const std::string& device_id) {
    std::string sql = "SELECT 1 FROM devices WHERE id = ? LIMIT 1;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_text(stmt, 1, device_id.c_str(), -1, SQLITE_STATIC);
    bool exists = sqlite3_step(stmt) == SQLITE_ROW;
    sqlite3_finalize(stmt);
    return exists;
}

bool Database::registerDevice(const std::string& device_id) {
    if (deviceExists(device_id)) return true;  // already registered

    std::string default_name = "Device " + device_id;
    std::string sql = "INSERT INTO devices (id, name, status, last_seen) VALUES (?, ?, 'online', ?);";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) return false;

    long long now = std::time(nullptr);
    sqlite3_bind_text(stmt, 1, device_id.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, default_name.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 3, now);

    bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok;
}

bool Database::updateDeviceName(const std::string& device_id, const std::string& name) {
    std::string sql = "UPDATE devices SET name = ? WHERE id = ?;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, device_id.c_str(), -1, SQLITE_STATIC);
    bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok;
}

bool Database::updateHeartbeat(const std::string& device_id) {
    std::string sql = "UPDATE devices SET status = 'online', last_seen = ? WHERE id = ?;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) return false;
    long long now = std::time(nullptr);
    sqlite3_bind_int64(stmt, 1, now);
    sqlite3_bind_text(stmt, 2, device_id.c_str(), -1, SQLITE_STATIC);
    bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok;
}

std::vector<Device> Database::getAllDevices() {
    std::vector<Device> devices;
    std::string sql = "SELECT id, name, status, last_seen FROM devices;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) return devices;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Device d;
        d.id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        d.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        d.status = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        d.last_seen = sqlite3_column_int64(stmt, 3);
        devices.push_back(d);
    }
    sqlite3_finalize(stmt);
    return devices;
}

Device Database::getDevice(const std::string& device_id) {
    Device d;
    std::string sql = "SELECT id, name, status, last_seen FROM devices WHERE id = ?;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) return d;
    sqlite3_bind_text(stmt, 1, device_id.c_str(), -1, SQLITE_STATIC);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        d.id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        d.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        d.status = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        d.last_seen = sqlite3_column_int64(stmt, 3);
    }
    sqlite3_finalize(stmt);
    return d;
}
// ---------- Media ----------

int Database::addMedia(const std::string& filename,
                       const std::string& original_name,
                       const std::string& type) {
    const char* sql =
        "INSERT INTO media (filename, original_name, type, uploaded_at) "
        "VALUES (?, ?, ?, ?);";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return -1;

    long long now = std::time(nullptr);
    sqlite3_bind_text(stmt, 1, filename.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, original_name.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, type.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 4, now);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) return -1;
    return static_cast<int>(sqlite3_last_insert_rowid(db));
}

Media Database::getMedia(int media_id) {
    Media m;
    const char* sql = "SELECT id, filename, original_name, type, uploaded_at "
                      "FROM media WHERE id = ?;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return m;
    sqlite3_bind_int(stmt, 1, media_id);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        m.id            = sqlite3_column_int(stmt, 0);
        m.filename      = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        m.original_name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        m.type          = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        m.uploaded_at   = sqlite3_column_int64(stmt, 4);
    }
    sqlite3_finalize(stmt);
    return m;
}

std::vector<Media> Database::getAllMedia() {
    std::vector<Media> list;
    const char* sql = "SELECT id, filename, original_name, type, uploaded_at "
                      "FROM media ORDER BY id DESC;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return list;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Media m;
        m.id            = sqlite3_column_int(stmt, 0);
        m.filename      = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        m.original_name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        m.type          = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        m.uploaded_at   = sqlite3_column_int64(stmt, 4);
        list.push_back(m);
    }
    sqlite3_finalize(stmt);
    return list;
}

// ---------- Device <-> Media ----------

bool Database::assignMediaToDevice(const std::string& device_id, int media_id) {
    // If already assigned, do nothing
    const char* check = "SELECT 1 FROM device_media WHERE device_id = ? AND media_id = ?;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, check, -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_text(stmt, 1, device_id.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, media_id);
    bool exists = sqlite3_step(stmt) == SQLITE_ROW;
    sqlite3_finalize(stmt);
    if (exists) return true;

    // Determine next position
    const char* pos_sql = "SELECT COALESCE(MAX(position), 0) + 1 FROM device_media WHERE device_id = ?;";
    int next_pos = 1;
    if (sqlite3_prepare_v2(db, pos_sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, device_id.c_str(), -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) == SQLITE_ROW) next_pos = sqlite3_column_int(stmt, 0);
        sqlite3_finalize(stmt);
    }

    const char* ins = "INSERT INTO device_media (device_id, media_id, position) VALUES (?, ?, ?);";
    if (sqlite3_prepare_v2(db, ins, -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_text(stmt, 1, device_id.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, media_id);
    sqlite3_bind_int(stmt, 3, next_pos);
    bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok;
}

bool Database::removeMediaFromDevice(const std::string& device_id, int media_id) {
    const char* sql = "DELETE FROM device_media WHERE device_id = ? AND media_id = ?;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_text(stmt, 1, device_id.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, media_id);
    bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok;
}

std::vector<Media> Database::getDeviceMedia(const std::string& device_id) {
    std::vector<Media> list;
    const char* sql =
        "SELECT m.id, m.filename, m.original_name, m.type, m.uploaded_at "
        "FROM media m "
        "JOIN device_media dm ON dm.media_id = m.id "
        "WHERE dm.device_id = ? "
        "ORDER BY dm.position ASC;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return list;
    sqlite3_bind_text(stmt, 1, device_id.c_str(), -1, SQLITE_STATIC);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Media m;
        m.id            = sqlite3_column_int(stmt, 0);
        m.filename      = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        m.original_name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        m.type          = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        m.uploaded_at   = sqlite3_column_int64(stmt, 4);
        list.push_back(m);
    }
    sqlite3_finalize(stmt);
    return list;
}