#include "LED.h"
#include <Adafruit_NeoPixel.h>
#include <queue>
#include <vector>
#include "Buzzer.h"
#include "Config.h"
#include "DeviceConfig.h"
#include "NetworkManager.h"

Adafruit_NeoPixel ws2812 = Adafruit_NeoPixel(NUMBER_OF_WS2312_PIXELS,
                                             PIN_WS2812,
                                             NEO_GRB + NEO_KHZ800);
byte led_brightness = BRIGHTNESS;
byte new_led_brightness = BRIGHTNESS;

std::vector<uint32_t> BLACK_VECT = {0};

std::queue<led_state_t> led_state_queue;

bool fill_all(std::vector<uint32_t>& colors) {
  bool update_required = false;
  for (int i = 0; i < NUMBER_OF_WS2312_PIXELS; i++) {
    auto color = colors[i % colors.size()];
    if (ws2812.getPixelColor(i) != color) {
      ws2812.setPixelColor(i, color);
      update_required = true;
    }
  }
  return update_required;
}

bool fill_circle(std::vector<uint32_t>& colors,
                 int run_count,
                 bool ccw = false) {
  bool update_required = false;
  auto color_index = (run_count / NUMBER_OF_WS2312_PIXELS) % colors.size();
  auto color = colors[color_index];
  auto pixel_index = run_count % NUMBER_OF_WS2312_PIXELS;

  if (ccw == true) {
    pixel_index = NUMBER_OF_WS2312_PIXELS - pixel_index - 1;
  }

  if (ws2812.getPixelColor(pixel_index) != color) {
    ws2812.setPixelColor(pixel_index, color);
    update_required = true;
  }
  return update_required;
}

std::vector<uint32_t> failure_colors(std::vector<uint32_t>& colors) {
  size_t const half_size = colors.size() / 2;
  auto failure_colors =
      std::vector<uint32_t>(colors.begin() + half_size, colors.end());
  failure_colors.insert(failure_colors.end(), colors.begin(),
                        colors.begin() + half_size);
  return failure_colors;
}

bool set_leds_off(uint32_t period_ms,
                  std::vector<uint32_t>& colors,
                  uint32_t run_count) {
  return fill_all(BLACK_VECT);
}

void set_led_period(uint32_t period_ms) {
  if (period_ms < LED_MIN_PERIOD_MS)
    period_ms = LED_MIN_PERIOD_MS;
  task_led.setInterval(period_ms * TASK_MILLISECOND);
}

bool set_leds_on(uint32_t period_ms,
                 std::vector<uint32_t>& colors,
                 uint32_t run_count) {
  set_led_period(period_ms);
  bool update_required = fill_all(colors);
  return update_required;
}

bool set_leds_blink(uint32_t period_ms,
                    std::vector<uint32_t>& colors,
                    uint32_t run_count) {
  bool update_required = false;
  if (run_count % 2 == 0)
    update_required = fill_all(colors);
  else {
    update_required = fill_all(BLACK_VECT);
  }
  return update_required;
}

bool set_leds_circle_cw(uint32_t period_ms,
                        std::vector<uint32_t>& colors,
                        uint32_t run_count) {
  set_led_period(period_ms);
  return fill_circle(colors, run_count, false);
}

bool set_leds_circle_ccw(uint32_t period_ms,
                         std::vector<uint32_t>& colors,
                         uint32_t run_count) {
  set_led_period(period_ms);
  return fill_circle(colors, run_count, true);
}

bool set_leds_alternate(uint32_t period_ms,
                        std::vector<uint32_t>& colors,
                        uint32_t run_count) {
  const size_t middle = colors.size() / 2;
  std::vector<uint32_t> colors_a;
  std::vector<uint32_t> colors_b;

  if (middle == 1) {
    colors_a = {colors[0]};
    colors_b = {colors[1]};
  } else {
    colors_a = std::vector<uint32_t>(colors.cbegin(), colors.cbegin() + middle);
    colors_b = std::vector<uint32_t>(colors.cbegin() + middle, colors.cend());
  }

  set_led_period(period_ms);

  bool update_required = false;
  if (run_count % 2 == 0)
    update_required = fill_all(colors_a);
  else {
    update_required = fill_all(colors_b);
  }
  return update_required;
}

void led();
Task task_led(LED_DEFAULT_PERIOD_MS* TASK_MILLISECOND, -1, led, &ts);

led_state_t led_default_state = {
    set_leds_off,
    LED_DEFAULT_PERIOD_MS,
    0,
    std::vector<uint32_t>{0},
};

bool led_queue_done() {
  return led_state_queue.empty();
}

led_state_t get_led_state() {
  if (!led_state_queue.empty()) {
    return led_state_queue.front();
  }
  // fall back to default state if nothing is queued
  return led_default_state;
}

void led_queue_flush() {
  std::queue<led_state_t>().swap(led_state_queue);
}

void led() {
  bool update_required = false;
  static int32_t current_state_run_count = 0;
  led_state_t led_state = get_led_state();

  current_state_run_count++;

  if (led_state.runs != -1 && current_state_run_count >= led_state.runs) {
    // Current state is done
    if (!led_state_queue.empty()) {
      led_state_queue.pop();
    }
    current_state_run_count = 0;
    led_state = get_led_state();
  }

  update_required = led_state.handler(led_state.period_ms, led_state.colors,
                                      current_state_run_count);

  auto cfg = config_get_values();
  if (cfg.light_enabled == false) {
    ws2812.clear();
    update_required = true;
  }

  if (new_led_brightness != led_brightness) {
    update_required = true;
  }

  if (update_required == true) {
    ws2812.setBrightness(led_brightness);
    ws2812.show();
  }
}

void run_until_queue_size(uint32_t stop_queue_size) {
  auto queue_size = led_state_queue.size();
  do {
    ts.execute();
    queue_size = led_state_queue.size();
  } while (queue_size > stop_queue_size);
}

void led_set_default(led_state_t led_state) {
  led_default_state = led_state;
}

void led_init() {
#if DEBUG_LOG > 0
  Serial.print("Initialise LEDs... ");
#endif
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, LOW);  // LED aus
  pinMode(PIN_WS2812, OUTPUT);
  digitalWrite(PIN_WS2812, LOW);
  ws2812.clear();
  ws2812.show();
#if DEBUG_LOG > 0
  Serial.println("done!");
#endif
}

// OLD STUBS
uint32_t led_get_color() {
  return 0;
}
void led_blink(uint32_t, uint32_t) {}
void led_adjust_brightness(uint32_t) {}
