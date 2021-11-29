#include "NetworkManager.h"
#include <ArduinoJson.h>
#include <array>
#include "Config.h"
#include "DeviceConfig.h"
#include "LEDPatterns.h"
#include "RequestParser.h"
#include "Sensor.h"

char mdnsName[] = "wifi101"; // the MDNS name that the board will respond to
                             // after WiFi settings have been provisioned
// Note that the actual MDNS name will have '.local' after
// the name above, so "wifi101" will be accessible on
// the MDNS name "wifi101.local".

WiFiServer server(80);

// Create a MDNS responder to listen and respond to MDNS name requests.
// WiFiMDNSResponder mdnsResponder;

device_config_t cfg = config_get_values();
int wifi_status = WL_IDLE_STATUS;
byte wifi_mac[6];
bool ap_mode_activated = false;

bool wifi_is_connected()
{
  return WiFi.status() == WL_CONNECTED;
}

void wifi_ap_create()
{
#if DEBUG_LOG > 0
  Serial.println("Create access point for configuration");
#endif

  ap_mode_activated = true;

  // led_set_color(LED_COLOR_WIFI_MANAGER);
  // led_set_brightness();
  // led_update();

  if (wifi_status == WL_CONNECTED)
  {
    WiFi.end();
  }

  if (WiFi.status() == WL_NO_SHIELD)
  {
    Serial.println("WiFi shield not present");
    // don't continue
    while (true)
    {
      // TODO: Think of a better way to handle final errors
      // led_failure(LED_COLOR_WIFI_FAILURE);
    }
  }
  WiFi.macAddress(wifi_mac);

  char ap_ssid[20];

  sprintf(ap_ssid, "%s %02X:%02X", WIFI_AP_SSID, wifi_mac[4], wifi_mac[5]);
  wifi_status = WiFi.beginAP(ap_ssid, cfg.ap_password);
  if (wifi_status != WL_AP_LISTENING)
  {
    Serial.println("Creating access point failed");
    while (true)
    {
      // TODO: Think of a better way to handle final errors
      // led_failure(LED_COLOR_WIFI_FAILURE);
    }
  }
  delay(5000);

  // TODO: print_wifi_status();
  server.begin();

  while (true)
  {
    wifi_handle_client();
  }
}

bool ap_is_active()
{
  return ap_mode_activated;
}

enum WIFI_CONNECT_STATES
{
  INIT,
  CONNECTING,
  CONNECTED,
  FAILURE,
  TIMEOUT
};
WIFI_CONNECT_STATES wifi_connect_state = WIFI_CONNECT_STATES::INIT;

void wifi_wpa_connect();

Task task_wifi_connect(500 * TASK_MILLISECOND, -1, &wifi_wpa_connect, &ts);
Task task_wifi_handle_client(10 * TASK_MILLISECOND,
                             -1,
                             &wifi_handle_client,
                             &ts);

void wifi_wpa_connect()
{
  wifi_status = WiFi.status();
  static int started_connecting_run_count;
  int connect_try_count = 0;
  switch (wifi_connect_state)
  {
  default:
  case WIFI_CONNECT_STATES::INIT:
    if (wifi_status == WL_AP_CONNECTED)
    {
      WiFi.end();
      ap_mode_activated = false;
    }
    // check for the presence of the shield:
    if (WiFi.status() == WL_NO_SHIELD)
    {
      Serial.println("WiFi shield not present");
      wifi_connect_state = WIFI_CONNECT_STATES::FAILURE;
      led_wifi_failure();
    }

    cfg = config_get_values();
    if (strlen(cfg.wifi_ssid) == 0 || strlen(cfg.wifi_password) == 0)
    {
      Serial.println("Wifi SSID and/or password not set!");
      wifi_connect_state = WIFI_CONNECT_STATES::INIT;
      task_wifi_connect.restartDelayed(1000);
      break;
    }

    WiFi.begin(cfg.wifi_ssid, cfg.wifi_password);
    started_connecting_run_count = task_wifi_connect.getRunCounter();
    led_wifi_connecting();
    wifi_connect_state = WIFI_CONNECT_STATES::CONNECTING;
    break;

  case WIFI_CONNECT_STATES::CONNECTING:
    connect_try_count =
        task_wifi_connect.getRunCounter() - started_connecting_run_count;
    if (wifi_status == WL_CONNECTED)
    {
      led_queue_flush();
      led_wifi_connected();
      wifi_connect_state = WIFI_CONNECT_STATES::CONNECTED;
      // TODO: move server and mqtt stuff to their own tasks to separate
      server.begin();
      mqtt_connect();
      task_wifi_handle_client.enable();
    }
    else if (connect_try_count >= 10)
    {
      wifi_connect_state = WIFI_CONNECT_STATES::TIMEOUT;
    }
    break;

  case WIFI_CONNECT_STATES::TIMEOUT:
    Serial.print("Timeout connecting to wifi!");
    Serial.println(cfg.wifi_ssid);
    wifi_connect_state = WIFI_CONNECT_STATES::INIT;
    task_wifi_connect.restartDelayed(1000);
    break;

  case WIFI_CONNECT_STATES::CONNECTED:
    print_wifi_status();
    break;

  case WIFI_CONNECT_STATES::FAILURE:
    break;
  }
}

void print_wifi_status()
{
  // print the SSID of the network you're attached to:
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // print your WiFi shield's IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  // print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print("Signal strength (RSSI): ");
  Serial.print(rssi);
  Serial.println(" dBm");
}

void print_mac_address(byte mac[])
{
  for (int i = 5; i >= 0; i--)
  {
    if (mac[i] < 16)
    {
      Serial.print("0");
    }
    Serial.print(mac[i], HEX);
    if (i > 0)
    {
      Serial.print(":");
    }
  }
  Serial.println();
}

template <size_t N>
void tokenize(std::array<String, N> &targets, String source, String delimiter)
{
  int start = 0;
  int end = 0;
  for (auto &target : targets)
  {
    end = source.indexOf(delimiter, start);
    if (end != -1)
      target = source.substring(start, end);
    else
    {
      // If we find no match, the delimiter is either not present or it is the
      // part of the string after the last delimiter occurence
      target = source.substring(start, source.length());
      break;
    }
    start = end + 1;
  }
}

void wifi_handle_client()
{
  bool reboot = false;
  bool respond = false;
  // compare the previous status to the current status
  if (wifi_status != WiFi.status())
  {
    // it has changed update the variable
    wifi_status = WiFi.status();

    if (wifi_status == WL_AP_CONNECTED)
    {
      // a device has connected to the AP
      Serial.println(F("Device connected to AP"));
    }
    else
    {
      // a device has disconnected from the AP, and we are back in listening
      // mode
      Serial.println(F("Device disconnected from AP"));
    }
  }

  WiFiClient client = server.available(); // listen for incoming clients

  if (client)
  {
    // make a String to hold incoming data from the client
    static String request_string = "";

    // TODO: extend the parser to get request body for POST endpoints!
    std::array<String, 2> request_words;
    std::array<String, 2> url_parts;

    String &method = request_words[0];
    String &url = url_parts[0];
    String &params = url_parts[1];

    if (client.connected())
    {
      // loop while the client's connected
      auto avl = client.available();
      Serial.println(avl);
      while (avl)
      {
        // if there's bytes to read from the client,
        char c = client.read(); // read a byte, then
        request_string += String(c);

        if (request_string.endsWith("\r\n\r\n"))
        {
          tokenize(request_words, request_string, " ");
          tokenize(url_parts, request_words[1], "?");
          method = request_words[0];
          url = url_parts[0];
          params = url_parts[1];

          Serial.println(method);
          Serial.println(url);
          Serial.println(params);

          respond = true;
          break;
        }
      }

      if (respond == true)
      {
        /**
         * WPA Connection Routes
         */
        if (wifi_status == WL_CONNECTED)
        {
          if (method == F("GET"))
          {
            if (url == F("/api/sensor"))
            {
              client.println("HTTP/1.1 200 OK");
              client.println("Content-type:application/json");
              client.println();

              DynamicJsonDocument doc(256);
              doc["co2"] = get_co2();
              doc["temperature"] = get_temperature();
              doc["humidity"] = get_humidity();
              doc["brightness"] = get_brightness();

              serializeJson(doc, client);
            }
            else if (url == F("/"))
            {
              cfg = config_get_values();
              client.println("HTTP/1.1 200 OK");
              client.println("Content-type:text/html");
              client.println();
              client.print(wpa_root_html_header);
              client.print(cssampel);
              client.print(wpa_root_html_middle);
              client.print("<div class=\"box\"><h1>CO2 Ampel Status</h1>");
              client.print("<span class=\"css-ampel");
              int ampel = get_co2();
              if (ampel < START_YELLOW)
              {
                client.print(" ampelgruen");
              }
              else if (ampel < START_RED)
              {
                client.print(" ampelgelb");
              }
              else if (ampel < START_RED_BLINK)
              {
                client.print(" ampelrot");
              }
              else
              { // rot blinken
                client.print(" ampelrotblinkend");
              }
              client.print("\"><span class=\"cssampelspan\"></span></span>");
              client.print("<br><br>");
              client.print("Co2: ");
              client.print(get_co2());
              client.print(" ppm<br>Temperatur: ");
              client.print(get_temperature());
              client.print(" &ordm;C<br>Luftfeuchtigkeit: ");
              client.print(get_humidity());
              client.print(" %<br>Helligkeit: ");
              int brgt = get_brightness();
              if (brgt == 1024)
              {
                client.print("--");
              }
              else
              {
                client.print(brgt);
              }

              client.print("<br><br>");
              client.print("MQTT Broker is ");
              if (!mqtt_broker_connected())
              {
                client.print("not ");
              }
              client.print("connected.");
              client.print("<br><br>");
              client.print("Firmware: ");
              client.println(VERSION);
              client.print("<br>");
              client.print("</div>");
              client.print(wpa_root_html_footer);
              client.println();
            }
          }
          if (method == F("POST"))
          {
            // TODO: Start of the config API
            if (url == F("/save"))
            {
              client.println("HTTP/1.1 200 OK");
              client.println("Content-type:text/html");
              client.println();
              client.print(ap_save_html);
              client.println();
              config_set_values(cfg);

              reboot = true;
            }
          }
        }

        /**
         * Access Point Routes
         */
        if (wifi_status == WL_AP_CONNECTED)
        {
          if (method == F("GET"))
          {
            if (url == F("/"))
            {
              client.println("HTTP/1.1 200 OK");
              client.println("Content-type:text/html");
              client.println();
              client.print(ap_root_html_header);
              client.print(
                  "<form class=\"box\" action=\"/save\" "
                  "method=\"POST\" "
                  "name=\"loginForm\"><h1>Settings</h1>");
              client.print("<label for=broker>MQTT Broker IP</label>");
              client.print(
                  "<input name=broker placeholder='127.0.0.1' value='");
              client.print(cfg.mqtt_broker_address);
              client.print("'>");

              client.print("<label for=port>MQTT Broker Port</label>");
              client.print("<input name=port placeholder='1883' value='");
              client.print(cfg.mqtt_broker_port);
              client.print("'>");

              client.print("<label for=port>MQTT Base Topic</label>");
              client.print("<input name=topic placeholder='sensors' value='");
              client.print(cfg.mqtt_topic);
              client.print("'>");

              client.print("<label for=port>MQTT Username</label>");
              client.print(
                  "<input name=mqttuser placeholder='username' value='");
              client.print(cfg.mqtt_username);
              client.print("'>");

              client.print("<label for=port>MQTT Password</label>");
              client.print(
                  "<input type=password name=mqttpass placeholder='password' "
                  "value='");
              client.print(cfg.mqtt_password);
              client.print("'>");

              client.print("<label for=ampel>Ampel Name</label>");
              client.print("<input name=ampel placeholder='Ampel_1' value='");
              client.print(cfg.ampel_name);
              client.print("'>");

              client.print("<label for=broker>SSID</label>");
              client.print("<input name=ssid placeholder='SSID' value='");
              client.print(cfg.wifi_ssid);
              client.print("'>");

              client.print("<label for=pwd>Password</label>");
              client.print(
                  "<input type=password name=pwd placeholder='Passwort' "
                  "value='");
              client.print(cfg.wifi_password);
              client.print("'>");

              client.print("<label for=ap_pwd>Access Point Passwort</label>");
              client.print(
                  "<input type=password name=ap_pwd placeholder='Passwort' "
                  "value='");
              client.print(cfg.ap_password);
              client.print("'>");

              client.print("<label for=buzzer>Buzzer</label>");
              client.print("<select id=buzzer name=buzzer size=2>");
              if (cfg.buzzer_enabled)
              {
                client.print(
                    "<option value=\"true\" selected > Enabled</ option> ");
                client.print(" < option value =\"false\">Disabled</option>");
              }
              else
              {
                client.print("<option value=\"true\">Enabled</option>");
                client.print(
                    "<option value=\"false\" selected>Disabled</option>");
              };
              client.print("</select>");
              client.print("<br><br>");

              client.print("<label for=led>LEDs</label>");
              client.print("<select id=led name=led size=2>");
              if (cfg.light_enabled)
              {
                client.print(
                    "<option value=\"true\" selected > Enabled</ option> ");
                client.print(" < option value =\"false\">Disabled</option>");
              }
              else
              {
                client.print("<option value=\"true\">Enabled</option>");
                client.print(
                    "<option value=\"false\" selected>Disabled</option>");
              };
              client.print("</select>");
              client.print("<br><br>");

              client.print("<label for=format>Format</label>");
              client.print("<select id=format name=format size=2>");
              if (cfg.mqtt_format == 0)
              {
                client.print("<option value=\"0\" selected > JSON</ option> ");
                client.print(" < option value =\"1\">Influx</option>");
              }
              else
              {
                client.print("<option value=\"0\">JSON</option>");
                client.print("<option value=\"1\" selected>Influx</option>");
              };
              client.print("</select>");

              // client.print("<div class=\"btnbox\"><button
              // onclick=\"window.location.href='/selftest'\"
              // class=\"btn\">Selftest</button></div>");
              client.print(
                  "<div class=\"btnbox\"><button "
                  "onclick=\"window.location.href='/calibrate'\" "
                  "class=\"btn\">Calibration</button></div>");
              client.print(
                  "<input type=submit class=btn value=\"Save and reboot\">");
              client.print("<br><br>");
              client.print("Firmware: ");
              client.println(VERSION);
              client.print("</form>");

              client.print(ap_root_html_footer);
              client.println();
            }
          }
          else if (method == F("POST"))
          {
            // API should be the same for AP and WPA mode?
            if (url == F("/save"))
            {
              client.println("HTTP/1.1 200 OK");
              client.println("Content-type:text/html");
              client.println();
              client.print(ap_save_html);
              client.println();
              reboot = true;
            }
          }
        }
        // close the connection:
        client.stop();
        request_string = "";
      }
    }
  }
}

/*
       TODO: Implement config as REST API
           requestParser.grabPayload();
           if (requestParser.getPayload().length() > 0) {
             Serial.print("Broker ist ");
             Serial.println(requestParser.getField("broker"));

             if ((requestParser.getField("ssid").length() > 0)) {
               requestParser.getField("ssid").toCharArray(cfg.wifi_ssid, 40);
             }

             if ((requestParser.getField("pwd").length() > 0)) {
               requestParser.getField("pwd").toCharArray(cfg.wifi_password,
                                                         40);
             }

             if ((requestParser.getField("ap_pwd").length() > 0)) {
               requestParser.getField("ap_pwd").toCharArray(cfg.ap_password,
                                                            40);
             }

             if ((requestParser.getField("broker").length() > 0)) {
               requestParser.getField("broker").toCharArray(
                   cfg.mqtt_broker_address, 20);
             }

             if ((requestParser.getField("port").length() > 0)) {
               cfg.mqtt_broker_port = requestParser.getField("port").toInt();
             }

             if ((requestParser.getField("topic").length() > 0)) {
               requestParser.getField("topic").toCharArray(cfg.mqtt_topic, 20);
             }

             if ((requestParser.getField("mqttuser").length() > 0)) {
               requestParser.getField("mqttuser")
                   .toCharArray(cfg.mqtt_username, 20);
             }

             if ((requestParser.getField("mqttpass").length() > 0)) {
               requestParser.getField("mqttpass")
                   .toCharArray(cfg.mqtt_password, 20);
             }

             if ((requestParser.getField("ampel").length() > 0)) {
               requestParser.getField("ampel").toCharArray(cfg.ampel_name, 40);
             }

             if ((requestParser.getField("buzzer").length() > 0)) {
               if (requestParser.getField("buzzer") == "false") {
                 cfg.buzzer_enabled = false;
               } else {
                 cfg.buzzer_enabled = true;
               }
             }

             if ((requestParser.getField("led").length() > 0)) {
               if (requestParser.getField("led") == "false") {
                 cfg.light_enabled = false;
               } else {
                 cfg.light_enabled = true;
               }
             }

             if ((requestParser.getField("format").length() > 0)) {
               cfg.mqtt_format = requestParser.getField("format").toInt();
             }
             if (reboot) {
               config_set_values(cfg);
               client.stop();
               NVIC_SystemReset();
             }
             cfg = config_get_values();
       */
