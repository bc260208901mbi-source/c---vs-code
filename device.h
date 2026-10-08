#ifndef DEVICE_H
#define DEVICE_H

#include <string>

struct Device {
    std::string id;
    std::string name;
    std::string status;   // "online" / "offline"
    long long last_seen;  // Unix timestamp
};

#endif