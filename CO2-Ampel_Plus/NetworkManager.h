#ifndef NETWORK_H
#define NETWORK_H
#include <SPI.h>
#include <WiFi101.h>
#include "Buzzer.h"
#include "HTMLAPMode.h"
#include "HTMLWPAMode.h"
#include "LED.h"
#include "MQTTManager.h"
#include "Sensor.h"
#include "scheduler.h"

bool wifi_is_connected();

void wifi_ap_create();

bool ap_is_active();

void wifi_handle_ap_html();

void wifi_wpa_connect();

void wpa_listen_handler();

void print_wifi_status();

void print_wifi_status();

void print_mac_address(byte mac[]);
void wifi_handle_client();

extern Task task_wifi_connect;

#endif
