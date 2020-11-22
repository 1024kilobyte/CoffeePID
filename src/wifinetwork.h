#ifndef WiFiNetwork_h
#define WiFiNetwork_h

#include "Arduino.h"

class WiFiNetwork {
    public:
        WiFiNetwork();
        WiFiNetwork(String SSID, int RSSI, int Channel, byte encryptionType);
        
        String SSID;
        int RSSI;
        int Channel;
        String Encryption;
        String Reception;
};

#endif
