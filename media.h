#ifndef MEDIA_H
#define MEDIA_H

#include <string>

struct Media {
    int id = 0;
    std::string filename;       // stored filename on disk
    std::string original_name;  // original uploaded name
    std::string type;           // "image" / "video" / "unknown"
    long long uploaded_at = 0;
};

#endif