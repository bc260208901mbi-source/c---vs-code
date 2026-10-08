#ifndef ROUTES_H
#define ROUTES_H

#include "httplib.h"
#include "database.h"
#include <string>

void registerDeviceRoutes(httplib::Server& svr, Database& db);
void registerMediaRoutes(httplib::Server& svr, Database& db,
                         const std::string& storage_dir,
                         const std::string& public_prefix);

#endif