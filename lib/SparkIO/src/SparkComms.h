#ifndef SparkComms_h
#define SparkComms_h

#include <Arduino.h>
#define HW_BAUD 1000000

#include "BluetoothSerial.h"

// Bluetooth vars
#define  SPARK_NAME  "Spark 40 Audio"
#define  MY_NAME     "Heltec"

class SparkComms {
  public:
    SparkComms();
    ~SparkComms();

    void start_ser();
    void start_bt();
    bool connect_to_spark();

    // bluetooth communications

    BluetoothSerial *bt;
    HardwareSerial *ser;
    
};


#endif
