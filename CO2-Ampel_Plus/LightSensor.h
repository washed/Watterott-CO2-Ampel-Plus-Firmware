#ifndef LIGHTSENSOR_H
#define LIGHTSENSOR_H

#include <Arduino.h>
#include "scheduler.h"

extern Task task_trigger_read_light_sensor;
uint16_t get_brightness();

enum LIGHT_SENSOR_STATES {
  PRE_BLANK,
  MEASURING,
  POST_BLANK,
};

constexpr uint32_t LIGHT_SENSOR_TASK_PERIOD_MS = 1;
constexpr uint32_t LIGHT_SENSOR_MEASUREMENT_PERIOD_MS = 10000;
constexpr uint32_t LIGHT_SENSOR_READ_PERIOD_MS = 5;
constexpr uint32_t LIGHT_SENSOR_MEASUREMENT_COUNT = 2;
constexpr uint32_t LIGHT_SENSOR_PRE_BLANKING_MS = 5;
constexpr uint32_t LIGHT_SENSOR_POST_BLANKING_MS = 5;

#endif
