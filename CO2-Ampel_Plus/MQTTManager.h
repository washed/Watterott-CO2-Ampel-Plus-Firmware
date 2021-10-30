#ifndef MQTTManager_H
#define MQTTManager_H
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <WiFi101.h>
#include "LED.h"
#include "NetworkManager.h"

bool mqtt_connect();
void mqtt_send_value(int co2, float temp, int hum, int lux);
bool mqtt_broker_connected();
void mqtt_loop();
void mqtt_message_received(char* topic, byte* payload, unsigned int length);

#endif
