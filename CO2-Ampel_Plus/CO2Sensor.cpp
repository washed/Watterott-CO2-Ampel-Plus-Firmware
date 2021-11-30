#include "CO2Sensor.h"
#include <CircularBuffer.h>
#include <JC_Button.h>
#include <SparkFun_SCD30_Arduino_Library.h>
#include <Wire.h>
#include "Buzzer.h"
#include "Config.h"
#include "DeviceConfig.h"
#include "LEDPatterns.h"
#include "LightSensor.h"
#include "NetworkManager.h"
#include "scheduler.h"
#if DISPLAY_OUTPUT > 0
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#endif

#if DISPLAY_OUTPUT > 0
Adafruit_SSD1306 display(128, 64);  // 128x64 Pixel
// Serial.println("Use SSD1306 display");
#endif

SCD30 co2_sensor;

co2_sensor_measurement_t co2_sensor_measurement = {
    STARTWERT,
    0,
    0,
};

co2_sensor_measurement_t get_co2_sensor_measurement() {
  return co2_sensor_measurement;
}

unsigned int get_co2() {
  return co2_sensor_measurement.co2;
}

float get_temperature() {
  return co2_sensor_measurement.temperature;
}

float get_humidity() {
  return co2_sensor_measurement.humidity;
}

// static int dunkel = 0;

void show_data(void)  // Daten anzeigen
{
#if SERIAL_OUTPUT > 0
  Serial.print("co2: ");
  Serial.println(get_co2());  // ppm
  Serial.print("temp: ");
  Serial.println(get_temperature(), 1);  //°C
  Serial.print("humidity: ");
  Serial.println(get_humidity(), 1);  //%
  Serial.print("light: ");
  Serial.println(get_ambient_brightness());
  if (wifi_is_connected()) {
    print_wifi_status();
  }
  Serial.println();
#endif

#if DISPLAY_OUTPUT > 0
  display.clearDisplay();
  display.setTextSize(5);
  display.setCursor(5, 5);
  display.println(co2);
  display.setTextSize(1);
  display.setCursor(5, 56);
  display.println("CO2 Level in ppm");
  display.display();
#endif

  return;
}

/*
void sensor_calibration() {
#if DEBUG_LOG > 0
  Serial.println("Start CO2 sensor calibration");
#endif
  unsigned int okay = 0, co2_last = 0;
  // led_test();

  co2_last = co2;
  for (okay = 0; okay < 60;) {  // mindestens 60 Messungen (ca. 2 Minuten)

    // led_one_by_one(LED_YELLOW, 100);
    // led_update();

    if (co2_sensor.dataAvailable())  // alle 2s
    {
      co2 = co2_sensor.getCO2();
      temp = co2_sensor.getTemperature();
      humi = co2_sensor.getHumidity();

      if ((co2 > 200) && (co2 < 600) && (co2 > (co2_last - 30)) &&
          (co2 < (co2_last + 30)))  //+/-30ppm Toleranz zum vorherigen Wert
      {
        okay++;
      } else {
        okay = 0;
      }

      co2_last = co2;

      // if (co2 < 500) {
      //   led_set_color(LED_GREEN);
      // } else if (co2 < 600) {
      //   led_set_color(LED_YELLOW);
      // } else {  // >=600
      //   led_set_color(LED_RED);
      // }
      led_update();

#if SERIAL_OUTPUT > 0
      Serial.print("ok: ");
      Serial.println(okay);
#endif

      // TODO: show_data();
    }

    if (okay >= 60) {
      co2_sensor.setForcedRecalibrationFactor(400);  // 400ppm = Frischluft
      led_off();
      delay(50);
      led_on_color(LED_GREEN);
      delay(100);
      led_off();
      led_update();
      buzzer_ack();
    }
  }
}
*/

void sensor_init() {
  // co2_sensor.setForcedRecalibrationFactor(1135); //400ppm = Frischluft
  // //400ppm = Frischluft
  // co2_sensor.setMeasurementInterval(INTERVAL); //setze Messinterval
  // setze Pins
#if DEBUG_LOG > 0
  Serial.println("Initialize CO2 sensor");
#endif
  pinMode(PIN_LSENSOR_PWR, OUTPUT);
  digitalWrite(PIN_LSENSOR_PWR, LOW);  // Lichtsensor aus
  pinMode(PIN_LSENSOR, INPUT);
  pinMode(PIN_SWITCH, INPUT_PULLUP);

  // Wire/I2C
  Wire.begin();
  Wire.setClock(50000);  // 50kHz, empfohlen fue SCD30

#if DISPLAY_OUTPUT > 0
  delay(500);  // 500ms warten
  display.begin(SSD1306_SWITCHCAPVCC, 0x3D);
#endif

  while (co2_sensor.begin(Wire, AUTO_CALIBRATION) == false) {
    digitalWrite(PIN_LED, HIGH);
    delay(500);
    digitalWrite(PIN_LED, LOW);
    delay(500);
#if SERIAL_OUTPUT > 0
    Serial.println("Error: CO2 sensor not found.");
    led_queue_flush();
    led_sensor_failure();
#endif
  }
  // co2_sensor.setForcedRecalibrationFactor(1135);
  co2_sensor.setMeasurementInterval(INTERVAL);  // setze Messinterval
  delay(INTERVAL * 1000);                       // Intervallsekunden warten
  co2_sensor.setTemperatureOffset(TEMPERATURE_OFFSET);
}

void sensor_set_temperature_offset(float offset) {
  co2_sensor.setTemperatureOffset(offset);
#if DEBUG_LOG > 0
  Serial.print("Set temperatur offset to ");
  Serial.println(offset);
#endif
}

void read_co2_sensor();
Task task_read_co2_sensor(  //
    CO2_SENSOR_TASK_PERIOD_MS* TASK_MILLISECOND,
    -1,
    read_co2_sensor,
    &ts);

void read_co2_sensor() {
  static CircularBuffer<co2_sensor_measurement_t,
                        LIGHT_SENSOR_MEASUREMENT_COUNT>
      measurements;

  if (co2_sensor.dataAvailable()) {
    co2_sensor.readMeasurement();
    if (!measurements.isFull()) {
      measurements.push({co2_sensor.getCO2(), co2_sensor.getTemperature(),
                         co2_sensor.getHumidity()});
    }
  }

  if (measurements.isFull()) {
    co2_sensor_measurement_t sum = {0, 0, 0};
    uint32_t measurement_count = 0;

    while (!measurements.isEmpty()) {
      sum += measurements.shift();
      measurement_count++;
    }
    co2_sensor_measurement = sum / measurement_count;
  }

  if (wifi_is_connected()) {
    Serial.println("ENABLE task_mqtt_send_value");
    task_mqtt_send_value.enable();
  }

  // TODO: This might go into a decoupled ampel task
  if (co2_sensor_measurement.co2 < START_GREEN) {
    led_default_on(LED_BLUE);
  } else if (co2_sensor_measurement.co2 < START_YELLOW) {
    led_default_on(LED_GREEN);
  } else if (co2_sensor_measurement.co2 < START_RED) {
    led_default_on(LED_YELLOW);
  } else if (co2_sensor_measurement.co2 < START_RED_BLINK) {
    led_default_on(LED_RED);
  } else if (co2_sensor_measurement.co2 < START_VIOLET) {
    led_default_blink(LED_RED);
  } else {
    led_default_blink(LED_VIOLET);
  }
}

/*
TODO: Do we even want to change the measurement interval if it's darker?
Waht's the user story behind this?
void light_something() {
  if (light < LIGHT_DARK && dunkel == 0) {
    dunkel = 1;
    co2_sensor.setMeasurementInterval(INTERVAL_DARK);
    led_adjust_brightness(255 / (100 / BRIGHTNESS_DARK));
  } else if (dunkel == 1) {
    dunkel = 0;
    co2_sensor.setMeasurementInterval(INTERVAL);
    led_adjust_brightness(255);  // 0...255
  }
}
*/
