#include "Spark.h"
#include "SparkComms.h"
#include "BluetoothSerial.h"

SparkComms::SparkComms() {
}

SparkComms::~SparkComms() {
}

void SparkComms::start_ser() {
  uint8_t b;
  
  ser = new HardwareSerial(2); 
  // 5 is rx, 18 is tx
  ser->begin(HW_BAUD, SERIAL_8N1, 5, 18);

  while (ser->available())
    b = ser->read(); 
}

void SparkComms::start_bt() {
  bt = new BluetoothSerial();
  
  if (!bt->begin (MY_NAME, true)) {
    DEBUG("Bluetooth init fail");
    while (true);
  }    
}

bool SparkComms::connect_to_spark() {
  uint8_t b;
  bool connected = bt->connect(SPARK_NAME);
  if (!(connected && bt->hasClient())) {
    connected = false;
    delay(100);
  }
  // flush anything read from Spark - just in case
  while (bt->available())
    b = bt->read(); 
}
