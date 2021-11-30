#ifndef SENSOR_H
#define SENSOR_H
#include <Arduino.h>
#include "MQTTManager.h"
#include "scheduler.h"

extern Task task_read_co2_sensor;
constexpr uint32_t CO2_SENSOR_TASK_PERIOD_MS = 1000;
constexpr uint32_t CO2_SENSOR_MEASUREMENT_COUNT = 2;

struct co2_sensor_measurement_t {
  uint16_t co2;
  float temperature;
  float humidity;

  co2_sensor_measurement_t& operator+=(const co2_sensor_measurement_t& rhs) {
    this->co2 += rhs.co2;
    this->temperature += rhs.temperature;
    this->humidity += rhs.humidity;
    return *this;
  }

  co2_sensor_measurement_t operator/(uint16_t divisor) {
    return {
        co2 / divisor,
        temperature / divisor,
        humidity / divisor,
    };
  };
};

void sensor_init();
void sensor_calibration();
void sensor_set_temperature_offset(float offset);

co2_sensor_measurement_t get_co2_sensor_measurement();
float get_temperature();
unsigned int get_co2();
float get_humidity();

#endif
