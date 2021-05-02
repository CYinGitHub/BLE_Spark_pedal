#include <Arduino.h>
#include <FS.h>
#include "BluetoothSerial.h" 
#include "AceButton.h" 
#include "SSD1306Wire.h"
//#include "SH1106Wire.h"
#include "OLEDDisplay.h"

#define FORMAT_LITTLEFS_IF_FAILED true
#include "LITTLEFS.h"

#ifdef DEBUG_ENABLE
#define DEBUG(x) Serial.println(x)
#else
#define DEBUG(x)
#endif


const uint8_t BUTTONS_NUM = 4;
unsigned long count = 0;

struct s_buttons {
  const uint8_t pin;
  const char* efxShortName[3];
  const uint8_t ledAddr; //future needs for addressable RGB LEDs
  uint32_t ledState; //data type may change, as i read the docs, enum of selected rgb colors maybe
};

s_buttons BUTTONS[BUTTONS_NUM] = {
  {25, "DRV", 0, 0},
  {26, "MOD", 0, 0},
  {27, "DLY", 0, 0},
  {14, "RVB", 0, 0}
};

ace_button::AceButton buttons[BUTTONS_NUM];
void handleEvent(ace_button::AceButton*, uint8_t, uint8_t); //forward ref

void setup() {  
#ifdef DEBUG_ENABLE
  Serial.begin(115200);
  DEBUG(F("Serial started"));
#endif  
  for (uint8_t i = 0; i < BUTTONS_NUM; i++) {
    pinMode(BUTTONS[i].pin, INPUT_PULLUP);
    buttons[i].init(BUTTONS[i].pin, HIGH, i); //init AceButtons
    //leds code follows
    //..........
  }

  // Configure the ButtonConfig with the event handler, and enable all higher
  // level events.
  ace_button::ButtonConfig* buttonConfig = ace_button::ButtonConfig::getSystemButtonConfig();
  // just two events for now
  buttonConfig->setEventHandler(handleEvent);
  buttonConfig->setFeature(ace_button::ButtonConfig::kFeatureClick);
  buttonConfig->setFeature(ace_button::ButtonConfig::kFeatureLongPress);

  DEBUG("Setup(): done");
}

void loop() {
  for (uint8_t i = 0; i < BUTTONS_NUM; i++) {
    buttons[i].check();
  }
  count = millis();
  //keepalive for Spark amp BT connection
/*  if (millis() - count > 10000) {
    // request serial number and read returned bytes and discard - keep-alive link to Spark
    count = millis();
    SerialBT.write(get_serial, 23);
    flush_in();
  }
  */
}
 
void handleEvent(ace_button::AceButton* button, uint8_t eventType, uint8_t buttonState) {
  uint8_t id = button->getId();
  DEBUG("handleEvent(): id: " + (String)id + " eventType: " + (String)eventType + "; buttonState: " + (String)buttonState );

/*  uint8_t ledPin = BUTTONS[id].ledPin;

  // Control the LED only for the Pressed and Released events.
  // Notice that if the MCU is rebooted while the button is pressed down, no
  // event is triggered and the LED remains off.
  switch (eventType) {
    case ace_button::AceButton::kEventPressed:
      digitalWrite(ledPin, LED_ON);
      BUTTONS[id].ledState = LED_ON;
      break;
    case AceButton::kEventReleased:
      digitalWrite(ledPin, LED_OFF);
      INFOS[id].ledState = LED_OFF;
      break;
  }
  */
}