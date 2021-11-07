#include "SerialCommand.h"
#include <Arduino.h>
#include "DeviceConfig.h"
#include "core_cm0plus.h"
#include "errno.h"

constexpr size_t _SERIAL_BUFFER_SIZE = 128;
char serial_buffer[_SERIAL_BUFFER_SIZE + 1] = {};

constexpr size_t MAX_CMD_LENGTH = 8;

// Commands
constexpr char SET_COMMAND[] = "set";
constexpr size_t SET_COMMAND_LENGTH = sizeof(SET_COMMAND);
constexpr char GET_COMMAND[] = "get";
constexpr size_t GET_COMMAND_LENGTH = sizeof(GET_COMMAND);
constexpr char REBOOT_COMMAND[] = "reboot";
constexpr size_t REBOOT_COMMAND_LENGTH = sizeof(REBOOT_COMMAND);

// Variables
constexpr char VAR_BUZZER[] = "buzzer";
constexpr size_t VAR_BUZZER_LENGTH = sizeof(VAR_BUZZER);
constexpr char VAR_CHANGE_COUNT[] = "change_count";
constexpr size_t VAR_CHANGE_COUNT_LENGTH = sizeof(VAR_CHANGE_COUNT);
constexpr char VAR_WIFI_SSID[] = "wifi_ssid";
constexpr size_t VAR_WIFI_SSID_LENGTH = sizeof(VAR_WIFI_SSID);
constexpr char VAR_WIFI_PASSWORD[] = "wifi_password";
constexpr size_t VAR_WIFI_PASSWORD_LENGTH = sizeof(VAR_WIFI_PASSWORD);

enum COMMAND_STATES { NONE_CMD, SET, GET, REBOOT };
enum VAR_STATES { NONE_VAR, CHANGE_COUNT, BUZZER, WIFI_SSID, WIFI_PASSWORD };
enum VAL_STATES { NONE_VAL, VAL, ERR_VAL };

constexpr size_t COMMAND_VAL_MAX_STR_LEN = 70;

typedef struct {
  COMMAND_STATES command_state = COMMAND_STATES::NONE_CMD;
  VAR_STATES var_state = VAR_STATES::NONE_VAR;
  VAL_STATES val_state = VAL_STATES::NONE_VAL;
  int val_int = 0;
  bool val_bool = false;
  char val_string[COMMAND_VAL_MAX_STR_LEN] = "";
} serial_command_state_t;

void parse_bool(const char* token,
                serial_command_state_t& serial_command_state) {
  char* endptr;
  errno = 0;

  auto val = strtol(token, &endptr, 10);
  if (endptr == token || errno == ERANGE || (!(val == 0 || val == 1))) {
    Serial.print("Error parsing value string for bool: '");
    Serial.print(token);
    Serial.println("' valid values: [0, 1]");
    serial_command_state.val_state = VAL_STATES::ERR_VAL;
  } else {
    Serial.print("Bool parser: '");
    Serial.print(val);
    Serial.println("'");
    serial_command_state.val_bool = static_cast<bool>(val);
    serial_command_state.val_state = VAL_STATES::VAL;
  }
}

void parse_string(const char* token,
                  serial_command_state_t& serial_command_state) {
  if (strnlen(token, COMMAND_VAL_MAX_STR_LEN) >= COMMAND_VAL_MAX_STR_LEN) {
    Serial.print("String value too long: '");
    Serial.print(token);
    Serial.print("' max ");
    Serial.print(String(COMMAND_VAL_MAX_STR_LEN));
    Serial.print(" characters!");
    serial_command_state.val_state = VAL_STATES::ERR_VAL;
  } else {
    strncpy(serial_command_state.val_string, token, COMMAND_VAL_MAX_STR_LEN);
    serial_command_state.val_state = VAL_STATES::VAL;
  }
}

void print_var(const char* name, String value) {
  Serial.print(name);
  Serial.print(": '");
  Serial.print(value);
  Serial.println("'");
}

static void command_state_machine(
    const char* token,
    serial_command_state_t& serial_command_state) {
  device_config_t cfg;

  switch (serial_command_state.command_state) {
    case COMMAND_STATES::SET:
      cfg = config_get_values();
      switch (serial_command_state.var_state) {
        case VAR_STATES::BUZZER:
          switch (serial_command_state.val_state) {
            case VAL_STATES::NONE_VAL:
              parse_bool(token, serial_command_state);
              break;

            case VAL_STATES::VAL:
              cfg.buzzer_enabled = serial_command_state.val_bool;
              config_set_values(cfg);
              cfg = config_get_values();
              print_var("buzzer", String(cfg.buzzer_enabled));
              break;

            case VAL_STATES::ERR_VAL:
              Serial.println("Value error!");
              break;
          }
          break;

        case VAR_STATES::WIFI_SSID:
          switch (serial_command_state.val_state) {
            case VAL_STATES::NONE_VAL:
              parse_string(token, serial_command_state);
              break;

            case VAL_STATES::VAL:
              strncpy(cfg.wifi_ssid, serial_command_state.val_string,
                      sizeof(cfg.wifi_ssid));
              config_set_values(cfg);
              cfg = config_get_values();
              print_var("wifi_ssid", cfg.wifi_ssid);
              break;

            case VAL_STATES::ERR_VAL:
              Serial.println("Value error!");
              break;
          }
          break;

        case VAR_STATES::WIFI_PASSWORD:
          switch (serial_command_state.val_state) {
            case VAL_STATES::NONE_VAL:
              parse_string(token, serial_command_state);
              break;

            case VAL_STATES::VAL:
              strncpy(cfg.wifi_password, serial_command_state.val_string,
                      sizeof(cfg.wifi_password));
              config_set_values(cfg);
              cfg = config_get_values();
              print_var("wifi_password", cfg.wifi_password);
              break;

            case VAL_STATES::ERR_VAL:
              Serial.println("Value error!");
              break;
          }
          break;

        default:
        case VAR_STATES::NONE_VAR:
          if (strncmp(token, VAR_BUZZER, VAR_BUZZER_LENGTH) == 0)
            serial_command_state.var_state = VAR_STATES::BUZZER;

          if (strncmp(token, VAR_WIFI_SSID, VAR_WIFI_SSID_LENGTH) == 0)
            serial_command_state.var_state = VAR_STATES::WIFI_SSID;

          if (strncmp(token, VAR_WIFI_PASSWORD, VAR_WIFI_PASSWORD_LENGTH) == 0)
            serial_command_state.var_state = VAR_STATES::WIFI_PASSWORD;
          break;
      }
      break;
    case COMMAND_STATES::GET:
      cfg = config_get_values();
      switch (serial_command_state.var_state) {
        case VAR_STATES::BUZZER:
          print_var("buzzer", String(cfg.buzzer_enabled));
          break;
        case VAR_STATES::CHANGE_COUNT:
          print_var("change_count", String(cfg.change_count));
          break;
        case VAR_STATES::WIFI_SSID:
          print_var("wifi_ssid", String(cfg.wifi_ssid));
          break;
        case VAR_STATES::WIFI_PASSWORD:
          // TODO: Makes reading that out really easy...
          print_var("wifi_password", String(cfg.wifi_password));
          break;

        default:
        case VAR_STATES::NONE_VAR:
          if (strncmp(token, VAR_BUZZER, VAR_BUZZER_LENGTH) == 0)
            serial_command_state.var_state = VAR_STATES::BUZZER;

          if (strncmp(token, VAR_CHANGE_COUNT, VAR_CHANGE_COUNT_LENGTH) == 0)
            serial_command_state.var_state = VAR_STATES::CHANGE_COUNT;

          if (strncmp(token, VAR_WIFI_SSID, VAR_WIFI_SSID_LENGTH) == 0)
            serial_command_state.var_state = VAR_STATES::WIFI_SSID;

          if (strncmp(token, VAR_WIFI_PASSWORD, VAR_WIFI_PASSWORD_LENGTH) == 0)
            serial_command_state.var_state = VAR_STATES::WIFI_PASSWORD;
          break;
      }
      break;

    case COMMAND_STATES::REBOOT:
      Serial.println("Rebooting in 1 second!");
      delay(1000);
      NVIC_SystemReset();
      break;

    default:
    case COMMAND_STATES::NONE_CMD:
      if (strncmp(token, SET_COMMAND, SET_COMMAND_LENGTH) == 0)
        serial_command_state.command_state = COMMAND_STATES::SET;
      if (strncmp(token, GET_COMMAND, GET_COMMAND_LENGTH) == 0)
        serial_command_state.command_state = COMMAND_STATES::GET;
      if (strncmp(token, REBOOT_COMMAND, REBOOT_COMMAND_LENGTH) == 0)
        serial_command_state.command_state = COMMAND_STATES::REBOOT;
      break;
  }
}

void serial_handler() {
  static size_t offset = 0;

  if (Serial.available()) {
    if (offset > _SERIAL_BUFFER_SIZE) {
      Serial.println("Buffer full, resetting!");
      offset = 0;
    }
    serial_buffer[offset] = Serial.read();
    offset++;
  }

  if (serial_buffer[offset - 1] == ';') {
    serial_buffer[offset - 1] = 0;
    offset = 0;

    serial_command_state_t serial_command_state = {};
    char* token = strtok(serial_buffer, " ");
    while (token != NULL) {
      command_state_machine(token, serial_command_state);
      token = strtok(NULL, " ");
    }

    command_state_machine(token, serial_command_state);
  }
}