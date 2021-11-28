#ifndef LED_H
#define LED_H
#include <Arduino.h>
#include <queue>
#include <vector>
#include "scheduler.h"

void led_init();
void led_update();

uint32_t led_get_color();
void led_blink(uint32_t, uint32_t);
void led_adjust_brightness(uint32_t);

constexpr uint32_t LED_MIN_PERIOD_MS = 10;
constexpr uint32_t LED_DEFAULT_PERIOD_MS = 100;

extern Task task_led;

typedef struct led_state_t {
  bool (*handler)(uint32_t, std::vector<uint32_t>&, uint32_t);
  uint32_t period_ms;
  int32_t runs;
  std::vector<uint32_t> colors;
} led_state_t;

extern std::queue<led_state_t> led_state_queue;

bool set_leds_on(uint32_t period_ms,
                 std::vector<uint32_t>& colors,
                 uint32_t run_count);
bool set_leds_blink(uint32_t period_ms,
                    std::vector<uint32_t>& colors,
                    uint32_t run_count);
bool set_leds_circle_cw(uint32_t period_ms,
                        std::vector<uint32_t>& colors,
                        uint32_t run_count);
bool set_leds_circle_ccw(uint32_t period_ms,
                         std::vector<uint32_t>& colors,
                         uint32_t run_count);
bool set_leds_failure(uint32_t period_ms,
                      std::vector<uint32_t>& colors,
                      uint32_t run_count);
bool set_leds_alternate(uint32_t period_ms,
                        std::vector<uint32_t>& colors,
                        uint32_t run_count);

void led_queue_flush();
void run_until_queue_size(uint32_t stop_queue_size);
void led_set_default(led_state_t led_state);

#endif
