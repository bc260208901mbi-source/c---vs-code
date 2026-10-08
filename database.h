#ifndef DATABASE_H
#define DATABASE_H

#include "device.h"
#include "media.h"
#include <string>
#include <vector>
#include <sqlite3.h>

class Database {
public:
    Database(const std::string& db_path);
    ~Database();

    bool init();

    // --- Devices ---
    bool registerDevice(const std::string& device_id);
    bool updateDeviceName(const std::string& device_id, const std::string& name);
    bool updateHeartbeat(const std::string& device_id);
    bool deviceExists(const std::string& device_id);
    std::vector<Device> getAllDevices();
    Device getDevice(const std::string& device_id);

    // --- Media ---
    int  addMedia(const std::string& filename,
                  const std::string& original_name,
                  const std::string& type);
    Media getMedia(int media_id);
    std::vector<Media> getAllMedia();

    // --- Device <-> Media ---
    bool assignMediaToDevice(const std::string& device_id, int media_id);
    bool removeMediaFromDevice(const std::string& device_id, int media_id);
    std::vector<Media> getDeviceMedia(const std::string& device_id);

private:
    sqlite3* db;
};

#endif