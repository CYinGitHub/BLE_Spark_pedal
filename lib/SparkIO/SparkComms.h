#ifndef SparkComms_h
#define SparkComms_h

#include <Arduino.h>

#include "NimBLEDevice.h"
#define  SPARK_NAME  "Spark 40 BLE"
#define  MY_NAME     "BLE_Pedal"

#ifdef DEBUG_ENABLE
#define DEBUG(x) Serial.println((String)(millis()/1000) + "s. " + x)
#else
#define DEBUG(x)
#endif

class SparkComms {
  public:
    SparkComms();
    ~SparkComms();

    void startBLE();
    bool connect_to_spark();
    bool connected() {return _btConnected;} ;
    // bluetooth communications

    NimBLEDevice *bt;
    NimBLEClient *pClient;
    NimBLERemoteService *pService;
    NimBLERemoteCharacteristic *pSender;
    NimBLERemoteCharacteristic *pReceiver;
    static bool _btConnected; 
};

#endif
