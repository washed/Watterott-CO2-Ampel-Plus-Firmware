#ifndef NETWORK_H
#define NETWORK_H
#include <SPI.h>
#include <WiFi101.h>
#include "CO2Sensor.h"
#include "MQTTManager.h"
#include "scheduler.h"

bool wifi_is_connected();
wl_status_t get_wifi_status();
void init_wifi_connect(Scheduler& scheduler);
void init_wifi_wpa_monitor(Scheduler& scheduler);

/*
// TODO: AP mode disabled for now
void wifi_ap_create();
bool ap_is_active();
void wifi_handle_ap_html();
*/

void print_wifi_status();

extern Task task_wifi_connect;

#endif
