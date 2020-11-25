// Callback: receiving any WebSocket message
void onWebSocketEvent(uint8_t client_num,
                      WStype_t type,
                      uint8_t * payload,
                      size_t length) {

  StaticJsonDocument<512> doc;
  
  // Figure out the type of WebSocket event
  switch(type) {

    // Client has disconnected
    case WStype_DISCONNECTED:
      Serial.printf("[%u] Disconnected!\n\r", client_num);
      break;

    // New client has connected
    case WStype_CONNECTED:
      {
        IPAddress ip = webSocket.remoteIP(client_num);
        Serial.printf("[%u] Connection from %s\n\r", client_num, ip.toString().c_str());
      }
      break;

    // Handle text messages from client
    case WStype_TEXT:
      // Print out raw message
      Serial.printf("[%u] Received text: %s\n\r", client_num, payload);

      deserializeJson(doc, payload);
      if(doc["get"]["history"]) {  
          TemperatureHistory.mkSendTask(client_num, doc["get"]["history"]["from"].as<unsigned long>(),
                                                    doc["get"]["history"]["to"].as<unsigned long>());
      }
      break;

    // For everything else: do nothing
    case WStype_BIN:
    case WStype_ERROR:
    case WStype_FRAGMENT_TEXT_START:
    case WStype_FRAGMENT_BIN_START:
    case WStype_FRAGMENT:
    case WStype_FRAGMENT_FIN:
    default:
      break;
  }
}


#define HEATER_IDLE_SEND_INTERVAL 5000
#define STATUS_SEND_INTERVAL 5000

void sendState() {
  static unsigned long nextHeaterStatusSendTime = 0;
  static unsigned long nextTemperatureSendTime = 0;
  static unsigned long nextSendStatus = 0;
  static float lastTemp = 0;
  static bool lastHeatingState = false;
  
  StaticJsonDocument<512> doc;
  unsigned long time = millis();
  
  if (time >= nextHeaterStatusSendTime || isHeating != lastHeatingState) { //send heater state and power
    JsonObject heater = doc.createNestedObject("heater");
    heater["on"] = isHeating;
    float power;
    if (heaterPowerLog.getStdDev() > 0.05) power = heaterPowerLog.back();
    else power = heaterPowerLog.getMean();
    power *= 100;
    if (power < 0.01) power = 0;
    if (power > 99.99) power = 100;
    heater["power"] = power;
    nextHeaterStatusSendTime = time + HEATER_IDLE_SEND_INTERVAL;
    lastHeatingState = isHeating;
  }

  static unsigned long lastSendTime = 0;
  TemperaturePoint lastPoint = TemperatureHistory.back();
  if (lastPoint.time != lastSendTime) {  //send temperature state and power
    doc["temperature"] = lastPoint.temperature;
    doc["power"] = lastPoint.power;
    doc["meanPower"] = lastPoint.meanPower;
    doc["time"] = lastPoint.time;
    lastSendTime = lastPoint.time;
  }
  
  if(time >= nextSendStatus) {
    doc["millis"] = time;

    if (globalFaultCode) doc["fault"] = globalFaultCode;
    if (globalErrString != "") doc["error"] = globalErrString;
    if(isStandby) doc["standby"] = isStandby;
 
    doc["tuning"] = pidTunerStateStr;
    doc["target"] = pidSetpoint;
    doc["PWM"] = heater.getPWM();
    doc["FreeHeap"] = ESP.getFreeHeap();

    nextSendStatus = time + STATUS_SEND_INTERVAL;
  }

  if(!doc.isNull()) {
    String jsonPayload;
    serializeJson(doc, jsonPayload);
    webSocket.broadcastTXT(jsonPayload);
  }      
}
