#ifndef HTTP_ROUTES_H
#define HTTP_ROUTES_H

#include "WiFiClient.h"

void get_root(WiFiClient& client);
void get_api_sensor(WiFiClient& client);
void get_calibrate(WiFiClient& client);
void get_calibrate_ok_count(WiFiClient& client);

#endif
