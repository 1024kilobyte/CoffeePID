/*   _____       __  __          _____ _____ _____  
 *  / ____|     / _|/ _|        |  __ \_   _|  __ \ 
 * | |     ___ | |_| |_ ___  ___| |__) || | | |  | |
 * | |    / _ \|  _|  _/ _ \/ _ \  ___/ | | | |  | |
 * | |___| (_) | | | ||  __/  __/ |    _| |_| |__| |
 *  \_____\___/|_| |_| \___|\___|_|   |_____|_____/ 
 * 
 * A user-friendly IoT temperature regulator for your
 * coffee machine. This setup is based on:
 * - an ESP8266 microcontroller board
 * - MAX31865 with a PT1000 thermo element
 * - an SSR-25DA relay (pin variable "relayPin")
 * - an 3,3 V power supply for the esp
 * 
 * Author   : @1024kilobyte
 * Version  : 0.1
 */

// wifi stuff
#include <ESP8266WiFi.h>
#include "wifinetwork.h"

// webserver component
#include <ESP8266WebServer.h>

// mdns component
#include <ESP8266mDNS.h>

// JSON library
#include <ArduinoJson.h>

// LittleFS
#include <LittleFS.h>
// MAX31865 library
#include <Adafruit_MAX31865.h>

// constants
#define PT1000_RNOM     1000.0
#define PT1000_RREF     4300.0

// declare webserver instance
ESP8266WebServer server(80);

// hardware pinout
int relayPin = 0; // D3

int maxCLK = 14; // D5 
int maxSDO = 12; // D6
int maxSDI = 13; // D7
int maxCS = 4;   // D2

// declare max31865 instance
Adafruit_MAX31865 ptThermo = Adafruit_MAX31865(maxCS, maxSDI, maxSDO, maxCLK);

// internal program variables
bool isHeating = false;
int loopCounter = 0;

// configuration values
String global_preferred_wifi_mode;
String global_wifi_client_ssid, global_wifi_client_password, global_wifi_ap_ssid, global_wifi_ap_password;
float global_target_temp, global_current_temp;

float temp_history[5];
int current_history_index = 0;

bool initial_heat_up = false;
float heating_start_delta = 0;
double correction_value = 0;
unsigned long correction_value_timestamp = 0;

unsigned long next_action_time = 0;

// *********
// * SETUP *
// *********
void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial.println();

  // set hostname
  WiFi.hostname("CoffeePID");

  // read config from file system to global variables
  readConfig();

  Serial.println("* Configuration");
  Serial.println("  Preferred Wifi Mode: " + global_preferred_wifi_mode);
  Serial.println("  WiFi AP | SSID: " + global_wifi_ap_ssid + " | Length: " + global_wifi_ap_ssid.length());
  Serial.println("  Password: " + global_wifi_ap_password + " | Length: " + global_wifi_ap_password.length());
  Serial.println("  WiFi Client | SSID: " + global_wifi_client_ssid + " | Length: " + global_wifi_client_ssid.length());
  Serial.println("  Password: " + global_wifi_client_password + " | Length: " + global_wifi_client_password.length());
  Serial.println("  Brewing Temp: " + String(global_target_temp));

  if (global_preferred_wifi_mode == "client") {
    if (global_wifi_client_ssid.length() > 0 && global_wifi_client_password.length() > 0) {
      // Wifi client mode
      WiFi.mode(WIFI_STA);
      WiFi.begin(global_wifi_client_ssid, global_wifi_client_password);
      Serial.print("* Connecting to \"" + global_wifi_client_ssid + "\" ");
  
      int waitCounter = 0;
  
      while (WiFi.status() != WL_CONNECTED) {
        // ~20 seconds later -> maybe the credentials are wrong, lets delete them
        if (waitCounter == 40) {
          Serial.println("X Could not connect using given credentials");
          Serial.println("X Resetting values and restarting in accesspoint mode...");

          // reset wifi client config
          global_preferred_wifi_mode = "ap";
          global_wifi_client_ssid = "";
          global_wifi_client_password = "";
          writeConfig();
          
          // be sure everything is written and now reboot
          delay(500);
          ESP.restart();
        }
  
        // this yields for 500ms, more than enough for the wifi to work
        delay(500);
        
        Serial.print(".");
        waitCounter++;
      }
    
      Serial.println(" Connected (IP: " + WiFi.localIP().toString() + ")");
    } else {
      // reset wifi mode
      Serial.println("X Invalid credentials");
      Serial.println("X Resetting values and restarting in accesspoint mode...");
      
      global_wifi_client_ssid = "";
      global_wifi_client_password = "";
      global_preferred_wifi_mode = "ap";
      writeConfig();

      // be sure everything is written and now reboot
      delay(500);
      ESP.restart();
    }
  } else {
    // set default values
    if (global_preferred_wifi_mode != "ap") {
      global_preferred_wifi_mode = "ap";
      writeConfig();
    }

    if (global_wifi_ap_ssid.length() == 0) {
      global_wifi_ap_ssid = "CoffeePID";
      global_wifi_ap_password = "coffeepid";
      writeConfig();
    }
    
    // access point mode
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(global_wifi_ap_ssid, global_wifi_ap_password);
    Serial.println("* Started accesspoint \"" + global_wifi_ap_ssid +"\" with password \"" + global_wifi_ap_password + "\"");
  }

  // setup mDNS to be reachable by name
  if (!MDNS.begin("coffeepid")) {
    Serial.println("X Error setting up mDNS responder!");
  } else {
    Serial.println("* mDNS responder started");
  }

  // configure and start webserver
  // register root
  server.on("/", handleRoot);

  // register ajax endpoints
  server.on("/ajax_get_temp", getTemp);
  server.on("/ajax_get_settings", getConfig);
  server.on("/ajax_set_settings", setConfig);

  server.on("/ajax_get_wifis", getWifis);

  // all other routes and ressources
  server.onNotFound(handleWebRequests);
  
  server.begin();

  Serial.println("* webserver started");

  // add service to mDNS service discovery
  MDNS.addService("http", "tcp", 80);

  // init relay pin
  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, LOW);

  // init MAX31865 controller
  ptThermo.begin(MAX31865_2WIRE);

  // finished
  Serial.println("********************************");
  Serial.println("* CoffeePID is up and running! *");
  Serial.println("********************************");

  // boot time - this should be the last line in setup
  unsigned long bootTime = millis();
  Serial.println("* boot completed after " + String(bootTime / 1000.0) + " seconds");
}

// *************
// * MAIN LOOP *
// *************
void loop() {
  // *******
  // STANDBY - after 30 minutes esp runtime
  // *******
  if (millis() > 1800000) {
    if (isHeating) {
        isHeating = false;
        digitalWrite(relayPin, LOW);

        Serial.println("Maximum power on timer elapsed, machine going into standby.");
    }

    // blink the heater light to signal standby mode
    if (loopCounter % 10000000) {
        Serial.println("blink");
        digitalWrite(relayPin, HIGH);
        delay(150);
        digitalWrite(relayPin, LOW);
        delay(150);
        digitalWrite(relayPin, HIGH);
        delay(150);
        digitalWrite(relayPin, LOW);
    }

    // reset loop counter
    loopCounter = 0;
  }
  // *********
  // SLOW LOOP - as the heating is relatively slow, we don't need to measure with every loop cycle
  // *********
  else if (loopCounter % 10000 == 0) {
    // check and print any faults
    uint8_t fault = ptThermo.readFault();
    if (fault) {
      Serial.print("Fault 0x"); Serial.println(fault, HEX);
      if (fault & MAX31865_FAULT_HIGHTHRESH) {
        Serial.println("RTD High Threshold"); 
      }
      if (fault & MAX31865_FAULT_LOWTHRESH) {
        Serial.println("RTD Low Threshold"); 
      }
      if (fault & MAX31865_FAULT_REFINLOW) {
        Serial.println("REFIN- > 0.85 x Bias"); 
      }
      if (fault & MAX31865_FAULT_REFINHIGH) {
        Serial.println("REFIN- < 0.85 x Bias - FORCE- open"); 
      }
      if (fault & MAX31865_FAULT_RTDINLOW) {
        Serial.println("RTDIN- < 0.85 x Bias - FORCE- open"); 
      }
      if (fault & MAX31865_FAULT_OVUV) {
        Serial.println("Under/Over voltage"); 
      }
      ptThermo.clearFault();
    }
    
    // check temperature
    global_current_temp = ptThermo.temperature(PT1000_RNOM, PT1000_RREF);

    temp_history[current_history_index] = global_current_temp;
    current_history_index = current_history_index + 1;
    current_history_index = current_history_index % 5;
    
    Serial.println(String(global_current_temp));

    // fast exit for overheating
    if ((global_current_temp > 130 || global_current_temp > global_target_temp) && isHeating) {
      isHeating = false;
      digitalWrite(relayPin, LOW);

      Serial.println("Over temperature protection active");
      next_action_time = millis() + 15000;
    } else {
      if (millis() > next_action_time) {
        // get incline of the temp curve
        float oldest_history_value, latest_history_value; 

        if (current_history_index == 0) {
          latest_history_value = temp_history[4];
        } else {
          latest_history_value = temp_history[current_history_index - 1];
        }

        oldest_history_value = temp_history[current_history_index];

        float delta_temp = latest_history_value - oldest_history_value;

        if (isHeating) {
          isHeating = false;
          digitalWrite(relayPin, LOW);

          // Serial.println(String(global_current_temp) + "°C, switching off");

          // lets wait for the system to relax (5s)
          next_action_time = millis() + 5000;
        } else if (!isHeating) {
          // let the system relax to get a better value
          if (correction_value == 0 && initial_heat_up) {
            if (delta_temp <= 0) {
              // the times two part is totally based on testing 
              correction_value = 1 + ((global_target_temp - global_current_temp) / heating_start_delta) * 5;
              correction_value_timestamp = millis();

              Serial.println(String(correction_value));

              if (correction_value < 1) {
                correction_value = 1;  
              }
            }
          } else if (global_current_temp < global_target_temp) {
            heating_start_delta = global_target_temp - global_current_temp;

            double tmp_correction_value = 1;

            if (correction_value != 0 && correction_value != 1) {
              // reduce the correction value depending on time
              double corrected_correction_value = correction_value - ((millis() - correction_value_timestamp) * 0.000005);
              if (corrected_correction_value < 1) {
                // tmp value is already 1
                Serial.println("reached correction of 1");
                correction_value = 1;
              } else {
                tmp_correction_value = corrected_correction_value;  
              }
            }

            double heating_duration = tmp_correction_value * (heating_start_delta * 700 + delta_temp * -8600);

            // skip heating if it is to short
            if (heating_duration > 100) {
              if (!initial_heat_up) {
                initial_heat_up = true;  
              }
              
              isHeating = true;
              digitalWrite(relayPin, HIGH);
              
              next_action_time = millis() + heating_duration; 
            } else {
              // Serial.println("skipping short heating due to relaxation time");  
            }
          }
        }
      }
    }

    // reset loop counter
    loopCounter = 0;
  }

  // *********
  // FAST LOOP - execute with every loop cycle
  // *********
  // help the esp to do its other tasks (wifi connections, aso.)
  delay(0);
  
  // update mdns
  MDNS.update();
  
  // handle webserver requests
  server.handleClient();

  // incrementing late makes the program take a temp read on loop 0
  loopCounter++;
}

// ******************
// * HTTP ENDPOINTS *
// ******************
void handleRoot() {
  server.sendHeader("Location", "/app", true);
  server.send(302, "text/plain", "");
}

void handleWebRequests() {
  // check for route
  String ressourceRequest = server.uri();

  LittleFS.begin(); // Filesystem mounten

  String dataFileName;
  String dataType = "text/plain";

  // to have custom routes, we need our own little router
  if (ressourceRequest == "/app") {
    dataFileName = "/www/app.html";
  } else {
    dataFileName = "/www" + ressourceRequest;
  }

  // get file type
  if (dataFileName.endsWith(".svg")) dataType = "image/svg+xml";
  else if (dataFileName.endsWith(".js")) dataType = "application/javascript";
  else if (dataFileName.endsWith(".css")) dataType = "text/css";
  else if (dataFileName.endsWith(".html")) dataType = "text/html";
  else if (dataFileName.endsWith(".jpg") || dataFileName.endsWith(".jpeg")) dataType = "image/jpeg";
  else if (dataFileName.endsWith(".ico")) dataType = "image/x-icon";
  else if (dataFileName.endsWith(".png")) dataType = "image/png";

  // load compressed version off the file if exists - the stream file method will set the content encoding header
  // this should be safe when we ensure that all files are gzipped
  dataFileName = dataFileName + ".gz";

  // set cache header for all files
  server.sendHeader("Cache-Control"," max-age=31536000");

  File dataFile = LittleFS.open(dataFileName.c_str(), "r"); // Datei zum lesen öffnen
    
  if (!dataFile) {
    server.send(404, "text/html", "<html><h1>File not found!</h1></html>");
    return;
  }
  
  if (server.streamFile(dataFile, dataType) != dataFile.size()) {
    // nothing we can really do about this...
  } 
    
  dataFile.close();
}

// ******************
// * AJAX ENDPOINTS *
// ******************
void getTemp() {
  server.send(200, "text/plain", String(global_target_temp) + "|" + String(global_current_temp) + "|" + String(isHeating));
}

void getConfig() {
  server.send(200, "text/json", "{ \"preferred_wifi_mode\": \"" + String(global_preferred_wifi_mode) + "\", \"wifi_ap_ssid\": \"" + String(global_wifi_ap_ssid) + "\", \"wifi_ap_password\": \"" + String(global_wifi_ap_password) + "\", \"wifi_client_ssid\": \"" + String(global_wifi_client_ssid) + "\", \"target_temp\": \"" + String(global_target_temp) + "\" }");
}

void setConfig() {
  // check for reboot
  if (server.arg("reboot").length() > 0) {
    bool reboot = server.arg("reboot") == "true";

    if (reboot) {
      Serial.println("reboot");
      server.send(202);

      // wait for the response to be received, else the client 
      // doesn't receive our response and get a disconnect error
      delay(800);
      
      ESP.restart();
    }
  }
  
  // check for changed wifi credentials
  String preferred_wifi_mode = server.arg("preferred_wifi_mode");
  if (preferred_wifi_mode.length() > 0 && preferred_wifi_mode != "null") {
    global_preferred_wifi_mode = preferred_wifi_mode;
  }
  
  String wifi_ap_ssid = server.arg("wifi_ap_ssid");
  String wifi_ap_password = server.arg("wifi_ap_password");
  if (wifi_ap_ssid.length() > 0) {
    global_wifi_ap_ssid = wifi_ap_ssid;
    global_wifi_ap_password = wifi_ap_password;
  }
  
  String wifi_client_ssid = server.arg("wifi_client_ssid");
  String wifi_client_password = server.arg("wifi_client_password");
  if (wifi_client_ssid.length() > 0) {
    global_wifi_client_ssid = wifi_client_ssid;
    global_wifi_client_password = wifi_client_password;
  }

  String target_temp = server.arg("target_temp");
  if (target_temp.length() > 0) {
    global_target_temp = target_temp.toFloat();
  }

  writeConfig();

  server.send(202);
}

void getWifis() {
  byte numSsid = WiFi.scanNetworks();
  WiFiNetwork networks[numSsid];

  // print the network number and name for each network found:
  for (int wifiCounter = 0; wifiCounter < numSsid; wifiCounter++) {
    networks[wifiCounter] = WiFiNetwork(WiFi.SSID(wifiCounter), WiFi.RSSI(wifiCounter), WiFi.channel(wifiCounter), WiFi.encryptionType(wifiCounter));

    Serial.println(WiFi.SSID(wifiCounter) + " " + WiFi.encryptionType(wifiCounter));
  }

  // "quick" sorting - as its around 1 ms in bad conditions, this is just fine
  for (int sortRepeat = 0; sortRepeat < numSsid; sortRepeat++) {
    for (int sortCounter = 0; sortCounter < numSsid-1; sortCounter++) {
      if (networks[sortCounter].RSSI < networks[sortCounter+1].RSSI) {
        WiFiNetwork tmpNetwork = networks[sortCounter];
        networks[sortCounter] = networks[sortCounter+1];
        networks[sortCounter+1] = tmpNetwork;
      }
    } 
  }

  String jsonPayload = "{ \"wifis\":[";

  for (int index = 0; index < numSsid; index++) {
    jsonPayload += "{ \"ssid\": \"" + networks[index].SSID + "\", \"channel\": " + String(networks[index].Channel) + ", \"rssi\": " + String(networks[index].RSSI) + ", \"reception\": \"" + networks[index].Reception + "\", \"encryption\": \"" + networks[index].Encryption + "\"}";
  
    if (index != numSsid-1) {
      jsonPayload += ", ";  
    }
  }

  jsonPayload += "]}";

  server.send(200, "text/json", jsonPayload);
}

// **********
// * HELPER *
// **********
void readConfig() {
  LittleFS.begin();

  if (LittleFS.exists("/config.json")) {
    File config_file = LittleFS.open("/config.json", "r");

    DynamicJsonDocument doc(512);
    deserializeJson(doc, config_file.readString());
    
    config_file.close();

    global_preferred_wifi_mode = (const char *) doc["preferred_wifi_mode"];
    global_wifi_client_ssid = (const char *) doc["wifi_client_ssid"];
    global_wifi_client_password = (const char *) doc["wifi_client_password"];
    global_wifi_ap_ssid = (const char *) doc["wifi_ap_ssid"];
    global_wifi_ap_password = (const char *) doc["wifi_ap_password"];
    global_target_temp = (float) doc["target_temp"];
  }
}

void writeConfig() {
  DynamicJsonDocument doc(512);
  doc["preferred_wifi_mode"] = global_preferred_wifi_mode;
  doc["wifi_client_ssid"] = global_wifi_client_ssid;
  doc["wifi_client_password"] = global_wifi_client_password;
  doc["wifi_ap_ssid"] = global_wifi_ap_ssid;
  doc["wifi_ap_password"] = global_wifi_ap_password;
  doc["target_temp"] = global_target_temp;

  LittleFS.begin();

  File config_file = LittleFS.open("/config.json", "w+"); // Datei zum schreiben öffnen;
  serializeJson(doc, config_file);
  config_file.close();
}
