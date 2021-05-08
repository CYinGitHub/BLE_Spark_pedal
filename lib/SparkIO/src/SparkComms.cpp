#include "Spark.h"
#include "SparkComms.h"
#include "BluetoothSerial.h"


bool SparkComms::_btConnected=false;

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

void btEventCallback(esp_spp_cb_event_t event, esp_spp_cb_param_t *param){
  // On BT connection close
  if (event == ESP_SPP_CLOSE_EVT ){
    // TODO: Until the cause of connection instability (compared to Pi version) over long durations 
    // is resolved, this should keep your pedal and amp connected fairly well by forcing reconnection
    // in the main loop
    SparkComms::_btConnected = false;
  }

}

void SparkComms::start_bt() {
  bt = new BluetoothSerial();
  
   bt->register_callback(btEventCallback);
  if (!bt->begin (MY_NAME, true)) {
    DEBUG("Bluetooth init fail");
    while (true);
  }    
}


bool SparkComms::connect_to_spark() { //changed to boolean to be able to do it in a fancy way =)
  volatile uint8_t b;
  _btConnected = bt->connect(SPARK_NAME);
  if (!(_btConnected && bt->hasClient() )) {
    _btConnected = false;
  }
  // flush anything read from Spark - just in case
  while (bt->available())     b = bt->read(); 

  return _btConnected;
}
