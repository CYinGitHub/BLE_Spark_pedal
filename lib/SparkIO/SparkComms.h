#ifndef SparkComms_h
#define SparkComms_h

#include <Arduino.h>

#include "NimBLEDevice.h"
#define  SPARK_NAME  " Spark 40 BLE"
#define  MY_NAME     "BLE_Pedal"

#ifdef DEBUG_ENABLE
#define DEBUG(x) Serial.println((String)(millis()/1000) + "s. " + x)
#else
#define DEBUG(x)
#endif

#define RCV_BUFF_MAX 5000

class SparkComms {
  public:
    SparkComms();
    ~SparkComms();

    void startBLE();
    bool connect_to_spark();
    bool connected() {return _btConnected;} ;
    static void notifyCB(NimBLERemoteCharacteristic* pRemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) ;
    static void scanEndedCB(NimBLEScanResults results);

    static NimBLEDevice *bt;
    static NimBLEClient *pClient;
    static NimBLERemoteService *pService;
    static NimBLERemoteCharacteristic *pSender;
    static NimBLERemoteCharacteristic *pReceiver;
    static NimBLEAdvertisedDevice* advDevice;


    static uint32_t scanTime; // (s) 0 = scan forever
    static bool doConnect;
    static bool _btConnected;
    static size_t rcv_length; // read data length
    static size_t rcv_pos; //read position
    static uint8_t rcv_buffer[RCV_BUFF_MAX]; // buffer byte-array


    class AdvertisedDeviceCallbacks: public NimBLEAdvertisedDeviceCallbacks {
      void onResult(NimBLEAdvertisedDevice* advertisedDevice) ;
    };

    /*
    class ClientCallbacks : public NimBLEClientCallbacks {
      void onConnect(NimBLEClient* pClient) {
        DEBUG("Connected");

      //    pClient->updateConnParams(120,120,0,60);
      //    pClient->updateConnParams(36,36,0,60);
      };

      void onDisconnect(NimBLEClient* pClient) {
        DEBUG(pClient->getPeerAddress().toString().c_str());
        DEBUG(" Disconnected - Starting scan");
      //    NimBLEDevice::getScan()->start(scanTime, scanEndedCB);
      };

      bool onConnParamsUpdateRequest(NimBLEClient* pClient, const ble_gap_upd_params* params) {
        if(params->itvl_min < 24) { // 1.25ms units 
          return false;
        } else if(params->itvl_max > 48) { // 1.25ms units 
          return false;
        } else if(params->latency > 2) { // Number of intervals allowed to skip 
          return false;
        } else if(params->supervision_timeout > 100) { // 10ms units 
          return false;
        }
        return true;
      };
    };
    */

};


#endif
