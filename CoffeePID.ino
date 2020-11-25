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
 * Version  : 1.1
 */

// wifi stuff
#include <ESP8266WiFi.h>
#include "src/wifinetwork.h"
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

// webserver component
#include <ESP8266WebServer.h>

//!!!!!!!!!!!!!!!!!!!
//!!!! WebSocket library - https://github.com/Links2004/arduinoWebSockets
//!!!! IMPORTANT !!!! - in WebSockets.h - #define NETWORK_ESP8266 should be NETWORK_ESP8266_ASYNC
//!!!! otherwise the loop may be blocked when the client loses the connection
//!!!! this will require ESPAsyncTCP library - "https://github.com/me-no-dev/ESPAsyncTCP"
//!!!!!!!!!!!!!!!!!!!
#include <WebSocketsServer.h>

// mdns component
#include <ESP8266mDNS.h>

// JSON library
#include <ArduinoJson.h>

// LittleFS
#include <LittleFS.h>
// MAX31865 library with changes for autoConvert mode
#include "src/Adafruit_MAX31865_library/Adafruit_MAX31865.h"

#define PT1000 (1)
#define PT100  (2)
#define RTD PT1000
//#define USE_MAX_DRDY  //uncomment this to use autoConvert mode of MAX31865 with DRDY pin

#if(RTD == PT1000) 
// pt1000 constants
#define RTD_RNOM     1000.0
#define RTD_RREF     4300.0
#define NUM_WIRES    MAX31865_2WIRE
#else
// pt100 constants
#define RTD_RNOM     100.0
#define RTD_RREF     430.0
#define NUM_WIRES    MAX31865_3WIRE
#endif

#include "src/TemperatureHistory.h"
TemperatureHistoryBuffer TemperatureHistory(60 * 60);  //60 min
MeanBuffer<float> heaterPowerLog(30); //to calculate average output power for last 30*pwm_period sec

// upper limit for temperature
#define TEMPERATURE_UPPER_LIMIT 130

// standby after 30 minutes
#define STANDBY_TIMEOUT 1800000

// declare webserver instance
ESP8266WebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);
                     
// hardware pinout
int relayPin = 0; // D3

int maxCLK = 14; // D5 
int maxSDO = 12; // D6
int maxSDI = 13; // D7
int maxCS = 4;   // D2
int maxDRDY = 5; // D1

// declare max31865 instance
Adafruit_MAX31865 ptThermo = Adafruit_MAX31865(maxCS, maxSDI, maxSDO, maxCLK);

// Low Frequency PWM
// based on https://github.com/AlexGyver/GyverLibs - PWMrelay
#include "src/PWMrelay.h"
PWMrelay heater(relayPin, HIGH);

// PID
#define TUNE_IDLE   0
#define TUNE_START  1
#define TUNE_STOP   2
int pidTunerState = 0;
String pidTunerStateStr;

// using  https://github.com/br3ttb/Arduino-PID-Library.git with some modifications
// and https://github.com/br3ttb/Arduino-PID-AutoTune-Library.git  for autotune
#include "src/Arduino-PID-Library/PID_v1.h"
//Define Variables we'll be connecting to
double pidSetpoint, pidInput, pidOutput;
//Specify the links and initial tuning parameters
PID regulator(&pidInput, &pidOutput, &pidSetpoint, 0, 0, 0, DIRECT);

// internal state variables
bool isHeating = false;
bool isStandby = false;

// configuration values
String global_preferred_wifi_mode;
String global_wifi_client_ssid, global_wifi_client_password, global_wifi_ap_ssid, global_wifi_ap_password;
float global_target_temp;
float global_pid_kp, global_pid_ki, global_pid_kd; 
int16_t global_pid_dt;
int global_pwm_period;

float global_current_temp, global_current_temp_f;
Median3Buffer<float> median3Filter; //to calculate median for last 3 measurements of temperature;

uint8_t globalFaultCode = 0; 
String globalErrString = "";

// loop control timestamps
unsigned long measure_time = 0;
unsigned long last_measure_time = 0;

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
  
  Serial.printf("  PID: (%.1f, %.1f, %.1f, %d)\n\r", global_pid_kp, global_pid_ki, global_pid_kd, global_pid_dt);
  Serial.printf("  PWM period: %d\n\r", global_pwm_period);

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

  // setup OTA
  setupOTA();

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

  // Start WebSocket server and assign callback
  webSocket.begin();
  webSocket.onEvent(onWebSocketEvent);

  // init relay pin
  heater.setLevel(HIGH);  
  global_pwm_period = max(global_pwm_period, 1000);
  heater.setPeriod(global_pwm_period);
  heaterPowerLog.fill(0);
  
  regulator.SetTunings(global_pid_kp, global_pid_ki, global_pid_kd);
  regulator.SetSampleTime(max(global_pid_dt, (int16_t)100)); //sets the period, in Milliseconds
  regulator.SetOutputLimits(0, 100);  //for PWM
  pidSetpoint = global_target_temp;
  //turn the PID on
  regulator.SetMode(AUTOMATIC);

  // init MAX31865 controller
  pinMode(maxDRDY, INPUT);
  ptThermo.begin(NUM_WIRES);
  ptThermo.enable50Hz(true);
  global_current_temp = ptThermo.temperature(RTD_RNOM, RTD_RREF);
  global_current_temp_f = global_current_temp;
#ifdef USE_MAX_DRDY
  ptThermo.enableBias(true); //required for autoConvert
  ptThermo.autoConvert(true);
#endif
  median3Filter.fill(global_current_temp);

  TemperatureHistory.setNextTime();
  last_measure_time = millis();

  // finished
  Serial.println("********************************");
  Serial.println("* CoffeePID is up and running! *");
  Serial.println("********************************");

  // boot time - this should be the last line in setup
  unsigned long bootTime = millis();
  Serial.println("* boot completed after " + String(bootTime / 1000.0) + " seconds");
}

void setupOTA() {
  // Port defaults to 8266
  // ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  // ArduinoOTA.setHostname("myesp8266");

  // No authentication by default
  // ArduinoOTA.setPassword("admin");

  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_FS
      type = "filesystem";
    }

    // NOTE: if updating FS this would be the place to unmount FS using FS.end()
    LittleFS.end();
    goIntoStandBy("OTA started");
    
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });
  ArduinoOTA.begin();
  Serial.println("OTA Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

// *************
// * MAIN LOOP *
// *************
void loop() {
  unsigned long current_time = millis();

  // ************
  // TEMP READING - updates the temp value
  // ************

  if (current_time - last_measure_time > 5000) {
    goIntoStandBy("No temperature data for 5 sec");
  }

  // check and print any faults
  uint8_t fault = ptThermo.readFault();
  if (fault) {
    // it seems switching the pump during heating may trigger voltage fault
    if (fault & MAX31865_FAULT_OVUV) {
      Log("error", "Under/Over voltage");
    }
    else {
      globalFaultCode = fault;

      // deactivate on temp reading error
      if (!isStandby) {
        goIntoStandBy("Temperature reading error");
      }
      Log("error", "Fault 0x%02x", (int)fault);
      if (fault & MAX31865_FAULT_HIGHTHRESH) {
        Log("error", "RTD High Threshold");
      }
      if (fault & MAX31865_FAULT_LOWTHRESH) {
        Log("error", "RTD Low Threshold");
      }
      if (fault & MAX31865_FAULT_REFINLOW) {
        Log("error", "REFIN- > 0.85 x Bias");
      }
      if (fault & MAX31865_FAULT_REFINHIGH) {
        Log("error", "REFIN- < 0.85 x Bias - FORCE- open");
      }
      if (fault & MAX31865_FAULT_RTDINLOW) {
        Log("error", "RTDIN- < 0.85 x Bias - FORCE- open");
      }
    }
    ptThermo.clearFault();
  }

#ifdef USE_MAX_DRDY
  constexpr float FILTER_COEF = 0.02;
  if (digitalRead(maxDRDY) == LOW) {  //measurement result is ready	    
    global_current_temp = ptThermo.temperature_nowait(RTD_RNOM, RTD_RREF); //~ 20ms between measurements
#else
  constexpr float FILTER_COEF = 0.1;  
  if (current_time >= measure_time) {
    global_current_temp = ptThermo.temperature(RTD_RNOM, RTD_RREF);     //~ 75ms per one measurement, blocking
#endif

    //median filter then smoothing filter (EMA)
    median3Filter.push_back(global_current_temp);
    global_current_temp_f = median3Filter.getMedian() * FILTER_COEF + global_current_temp_f * (1 - FILTER_COEF);

    // *******
    // CONTROL
    // *******
    // set the heater according to the regulator
    pidInput = global_current_temp_f;
    regulator.Compute();
    if (!isStandby) heater.setPWM(pidOutput);
    //Log5000("regulator", "temp: %.2f, output:%.2f     %.2f,   %.2f,   %.2f   ", global_current_temp_f, pidOutput,
    //	regulator.GetProportional(), regulator.GetIntegral(), regulator.GetDerivative());

    // tune pid if started
    tune_pid_loop();

    last_measure_time = current_time;
#ifndef USE_MAX_DRDY
    measure_time = millis() + 75;
#endif
  }

  // **********************
  // OVERHEATING PROTECTION - don't heat past 130°C
  // **********************
  if (global_current_temp_f > TEMPERATURE_UPPER_LIMIT) {
    heater.setPWM(0);
    isHeating = false;
    Serial.println("Over temperature protection active");
  }

  // *******
  // STANDBY - after STANDBY_TIMEOUT esp runtime (in ms)
  // *******
  if (!isStandby && current_time > STANDBY_TIMEOUT) {
    goIntoStandBy("Maximum power on timer elapsed");
  }

  // *********
  // FAST LOOP - executed with every loop cycle
  // *********

  // let PWM do the job
  heater.tick();
  logPower(current_time);
  isHeating = heater.isActive();

  // broadcast state to all clients
  sendState();

  // help the esp to do its other tasks (wifi connections, aso.)
  delay(0);

  // update mdns
  MDNS.update();

  // Look for and handle WebSocket data
  webSocket.loop();

  // handle webserver requests
  server.handleClient();

  // handle OTA
  ArduinoOTA.handle();

  // save temperature history
  TemperatureHistory.loop();
}

void logPower(unsigned long current_time) {
  static bool isHeatingPrev = false;
  bool isHeatingNow = heater.isActive();
  static unsigned long lastOn = 0, lastOff = 0, lastWrite = 0;
  if (isHeatingNow != isHeatingPrev || (current_time - lastWrite) > global_pwm_period) {
    if (isHeatingNow == isHeatingPrev) {
      heaterPowerLog.push_back(isHeatingNow ? 1.0 : 0.0); //long stable state
      if (isHeatingNow) lastOn = current_time;
      else lastOff = lastOn; //to set power to 0 whith next change to On
    }
    else {
      if (isHeatingNow) {
        heaterPowerLog.push_back(static_cast<float>(lastOff - lastOn) / static_cast<float>(current_time - lastOn));
        lastOn = current_time;
      }
      else lastOff = current_time;
      isHeatingPrev = isHeatingNow;
    }
    lastWrite = current_time;
  }
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
  server.send(200, "text/plain", String(global_target_temp) + "|" + 
                                 String(global_current_temp_f) + "|" + 
                                 String(isHeating) + "|" + 
                                 String(millis()) + "|" + 
                                 String(isStandby));
}

void getConfig() {
  // Allocate the JsonDocument
  StaticJsonDocument<512> doc;
  
  doc["preferred_wifi_mode"] = global_preferred_wifi_mode;
  doc["wifi_client_ssid"] = global_wifi_client_ssid;
  doc["wifi_client_password"] = global_wifi_client_password;
  doc["wifi_ap_ssid"] = global_wifi_ap_ssid;
  doc["wifi_ap_password"] = global_wifi_ap_password;
  doc["target_temp"] = global_target_temp;
  doc["pid_kp"] = global_pid_kp;
  doc["pid_ki"] = global_pid_ki;
  doc["pid_kd"] = global_pid_kd;
  doc["pid_dt"] = global_pid_dt;
  doc["pwm_period"] = global_pwm_period; 

  //Serial.println("getConfig: ");
  //serializeJsonPretty(doc, Serial);

  String jsonPayload;
  serializeJson(doc, jsonPayload);
  server.send(200, "text/json", jsonPayload);
}

void setConfig() {
  //Log("setConfig: ", server.arg("plain").c_str());

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
    global_target_temp = constrain(target_temp.toFloat(), 0, TEMPERATURE_UPPER_LIMIT);
    pidSetpoint = global_target_temp;
  }

  String str = server.arg("pid_kp");
  if (str.length() > 0) {
    global_pid_kp = str.toFloat();
  }
  str = server.arg("pid_ki");
  if (str.length() > 0) {
    global_pid_ki = str.toFloat();
  }
  str = server.arg("pid_kd");
  if (str.length() > 0) {
    global_pid_kd = str.toFloat();
  }
  regulator.SetTunings(global_pid_kp, global_pid_ki, global_pid_kd);

  str = server.arg("pid_dt");
  if (str.length() > 0) {
    global_pid_dt = str.toInt();
    global_pid_dt = max(global_pid_dt, (int16_t)100);
    regulator.SetSampleTime(global_pid_dt);
  }
  str = server.arg("pwm_period");
  if (str.length() > 0) {
    global_pwm_period = str.toInt();
    heater.setPeriod(global_pwm_period);
  }
  
  str = server.arg("start_pid_tune");
  if (str.length() > 0) {  //toggle tuner
    pidTunerState = ((pidTunerState == TUNE_IDLE) ? TUNE_START : TUNE_STOP); 
  }
  else if(pidTunerState) pidTunerState = TUNE_STOP; //Stop pid tuner if running
 
  writeConfig();

  server.send(202);
}

void getWifis() {
  byte numSsid = WiFi.scanNetworks();
  if (numSsid == 255) numSsid = 0;
  WiFiNetwork networks[numSsid];
  int len = 0;

  // print the network number and name for each network found:
  for (int wifiCounter = 0; wifiCounter < numSsid; wifiCounter++) {
    networks[wifiCounter] = WiFiNetwork(WiFi.SSID(wifiCounter), WiFi.RSSI(wifiCounter), WiFi.channel(wifiCounter), WiFi.encryptionType(wifiCounter));
    len += networks[wifiCounter].SSID.length() + networks[wifiCounter].Reception.length() + networks[wifiCounter].Encryption.length();
    Serial.println(WiFi.SSID(wifiCounter) + " " + WiFi.encryptionType(wifiCounter));
  }

  //uint32_t StartSortingTime = micros();
  qsort(networks, sizeof(networks) / sizeof(networks[0]), sizeof(networks[0]),
      [](const void* cmp1, const void* cmp2) {return (static_cast<const WiFiNetwork*>(cmp2))->RSSI - (static_cast<const WiFiNetwork*>(cmp1))->RSSI; }
  );
  //uint32_t EndSortingTime = micros();
  //Log("qsort", "sorted at: %d microseconds", (int)(EndSortingTime - StartSortingTime));
 
  const size_t capacity = JSON_ARRAY_SIZE(numSsid) + JSON_OBJECT_SIZE(1) + numSsid*JSON_OBJECT_SIZE(5) + len + 4;
  DynamicJsonDocument doc(capacity);
  
  JsonArray wifis = doc.createNestedArray("wifis");
  for (int index = 0; index < numSsid; index++) {
    JsonObject wifi = wifis.createNestedObject();
    wifi["ssid"] = networks[index].SSID;
    wifi["channel"] = networks[index].Channel;
    wifi["rssi"] = networks[index].RSSI;
    wifi["reception"] = networks[index].Reception;
    wifi["encryption"] = networks[index].Encryption;
  }

  String jsonPayload;
  serializeJson(doc, jsonPayload);
  server.send(200, "text/json", jsonPayload);
}

// **********
// * HELPER *
// **********
void goIntoStandBy(String reason) {
  isStandby = true;
  heater.setPWM(0); 
  isHeating = false;
  if (pidTunerState) pidTunerState = TUNE_STOP; //Stop pid tuner if running
  globalErrString = reason;
  Log("error", "%s, %s", reason.c_str(), "machine going into standby.");
}

void readConfig() {
  LittleFS.begin();

  if (LittleFS.exists("/config.json")) {
    File config_file = LittleFS.open("/config.json", "r");

    StaticJsonDocument<512> doc;
    deserializeJson(doc, config_file.readString());
    //serializeJsonPretty(doc, Serial);
     
    config_file.close();

    global_preferred_wifi_mode = doc["preferred_wifi_mode"].as<const char*>();
    global_wifi_client_ssid = doc["wifi_client_ssid"].as<const char*>();
    global_wifi_client_password = doc["wifi_client_password"].as<const char*>();
    global_wifi_ap_ssid = doc["wifi_ap_ssid"].as<const char*>();
    global_wifi_ap_password = doc["wifi_ap_password"].as<const char*>();
    global_target_temp = doc["target_temp"].as<float>();
    global_pid_kp = doc["pid_kp"].as<float>();
    global_pid_ki = doc["pid_ki"].as<float>();
    global_pid_kd = doc["pid_kd"].as<float>();
    global_pid_dt = doc["pid_dt"].as<int16_t>();
    global_pwm_period = doc["pwm_period"].as<int>();   
  }
}

void writeConfig() {
  StaticJsonDocument<512> doc;
  doc["preferred_wifi_mode"] = global_preferred_wifi_mode;
  doc["wifi_client_ssid"] = global_wifi_client_ssid;
  doc["wifi_client_password"] = global_wifi_client_password;
  doc["wifi_ap_ssid"] = global_wifi_ap_ssid;
  doc["wifi_ap_password"] = global_wifi_ap_password;
  doc["target_temp"] = global_target_temp;
  doc["pid_kp"] = global_pid_kp;
  doc["pid_ki"] = global_pid_ki;
  doc["pid_kd"] = global_pid_kd;
  doc["pid_dt"] = global_pid_dt;
  doc["pwm_period"] = global_pwm_period; 

  LittleFS.begin();

  File config_file = LittleFS.open("/config.json", "w+"); // Datei zum schreiben öffnen;
  serializeJson(doc, config_file);
  
  config_file.close();
  webSocket.broadcastTXT("{\"new_config\":true}");
}

void vLog(char *type, const char* format, va_list va)
{
  char buffer[512];
  StaticJsonDocument<512> doc;
  String jsonPayload;
  
  vsnprintf(buffer, sizeof(buffer), format, va);
 
  doc["millis"] = millis();
  doc[type] = buffer;
  serializeJson(doc, jsonPayload);
  webSocket.broadcastTXT(jsonPayload);
  Serial.println(jsonPayload);
}

void Log(char *type, const char* format, ...)
{
  va_list va;
  va_start(va, format);
  vLog(type, format, va);
  va_end(va);
}

void Log1000(char *type, const char* format, ...)
{
  static unsigned long nextMsg = 0;
  if(millis() <= nextMsg) return;
  
  va_list va;
  va_start(va, format);
  vLog(type, format, va);
  va_end(va);
  nextMsg = millis() + 1000;
}

void Log5000(char *type, const char* format, ...)
{
  static unsigned long nextMsg = 0;
  if(millis() <= nextMsg) return;
  
  va_list va;
  va_start(va, format);
  vLog(type, format, va);
  va_end(va);
  nextMsg = millis() + 5000;
}
