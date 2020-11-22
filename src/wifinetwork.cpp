#include "Arduino.h"
#include "wifinetwork.h"

WiFiNetwork::WiFiNetwork() {}

WiFiNetwork::WiFiNetwork(String ssid, int rssi, int channel, byte encryptionType) {
  SSID = ssid;
  RSSI = rssi;
  Channel = channel;

  switch (encryptionType) {
    case 2:
    case 4:
      Encryption = "WPA";
      break;
    case 5:
      Encryption = "WEP";
      break;
    case 7:
      Encryption = "None";
      break;
    default:
      Encryption = "Unknown";
  }

  if (rssi < -73) {
    Reception = "bad";
  } else if (rssi < -65) {
    Reception = "ok";
  } else {
    Reception = "good";
  }
}
