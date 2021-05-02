#include <Arduino.h>
#include <FS.h>
#include "BluetoothSerial.h" 
#include "AceButton.h" 
#ifdef SSD1306WIRE //these global def's are in platformio.ini
  #include "SSD1306Wire.h"
#endif
#ifdef SH1106WIRE
  #include "SH1106Wire.h"
#endif
#include "OLEDDisplayUi.h"
#include "images.h"

#define FORMAT_LITTLEFS_IF_FAILED true
#include "LITTLEFS.h"

#ifdef DEBUG_ENABLE
#define DEBUG(x) Serial.println(x)
#else
#define DEBUG(x)
#endif

/* GENERAL AND GLOBALS ======================================================================= */
const char* DEVICE_NAME = "Easy Spark"; 
const char* VERSION = "0.1a"; 
const char* SPARK_BT_NAME = "Spark 40 Audio";

/* BUTTONS Init ============================================================================== */
const uint8_t BUTTONS_NUM = 4;
unsigned long countBT = 0;
unsigned long countUI = 0;

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

/* DISPLAY Init ============================================================================== */
#ifdef SSD1306WIRE
  SSD1306Wire  display(0x3c, 21 , 22); //in my case GPIO's are SDA=21 , SCL=22  
#endif
#ifdef SF1106WIRE
  SH1106Wire display(0x3c, 21, 22);
#endif
OLEDDisplayUi ui( &display );

void msOverlay(OLEDDisplay *display, OLEDDisplayUiState* state) {
  display->setTextAlignment(TEXT_ALIGN_RIGHT);
  display->setFont(ArialMT_Plain_10);
  display->drawString(128, 0, String(millis()));
}

void drawFrame1(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  // draw an xbm image.
  display->drawXbm((display->width()-BT_Logo_width)/2 + x, (display->height()-BT_Logo_height)/2 + y, BT_Logo_width, BT_Logo_height, BT_Logo_bits);
}

void drawFrame2(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  // Demonstrates the 3 included default sizes. The fonts come from SSD1306Fonts.h file
  // Besides the default fonts there will be a program to convert TrueType fonts into this format
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->setFont(ArialMT_Plain_10);
  display->drawString(0 + x, 10 + y, "Arial 10");

  display->setFont(ArialMT_Plain_16);
  display->drawString(0 + x, 20 + y, "Arial 16");

  display->setFont(ArialMT_Plain_24);
  display->drawString(0 + x, 34 + y, "Arial 24");
}

void drawFrame3(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  // Text alignment demo
  display->setFont(ArialMT_Plain_10);

  // The coordinates define the left starting point of the text
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->drawString(0 + x, 11 + y, "Left aligned (0,10)");

  // The coordinates define the center of the text
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->drawString(64 + x, 22 + y, "Center aligned (64,22)");

  // The coordinates define the right end of the text
  display->setTextAlignment(TEXT_ALIGN_RIGHT);
  display->drawString(128 + x, 33 + y, "Right aligned (128,33)");
}

void drawFrame4(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  // Demo for drawStringMaxWidth:
  // with the third parameter you can define the width after which words will be wrapped.
  // Currently only spaces and "-" are allowed for wrapping
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->setFont(ArialMT_Plain_10);
  display->drawStringMaxWidth(0 + x, 10 + y, 128, "Lorem ipsum\n dolor sit amet, consetetur sadipscing elitr, sed diam nonumy eirmod tempor invidunt ut labore.");
}

void drawFrame5(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {

}

// This array keeps function pointers to all frames
// frames are the single views that slide in
FrameCallback frames[] = { drawFrame1, drawFrame2, drawFrame3, drawFrame4, drawFrame5 };

// how many frames are there?
int frameCount = 5;

// Overlays are statically drawn on top of a frame eg. a clock
OverlayCallback overlays[] = { msOverlay };
int overlaysCount = 1;


/* BLUETOOTH Init ============================================================================ */
#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run `make menuconfig` to and enable it
#endif
bool btConnected = false;
BluetoothSerial SerialBT;
void btConnect(); // Forward ref
void btInit(); // Forward ref


/* SETUP() WWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWW */
void setup() {  
#ifdef DEBUG_ENABLE
  Serial.begin(115200);
  DEBUG(F("Serial started"));
#endif  

  ui.setTargetFPS(30);

	// Customize the active and inactive symbol
  ui.setActiveSymbol(activeSymbol);
  ui.setInactiveSymbol(inactiveSymbol);

  // You can change this to
  // TOP, LEFT, BOTTOM, RIGHT
  ui.setIndicatorPosition(BOTTOM);

  // Defines where the first frame is located in the bar.
  ui.setIndicatorDirection(LEFT_RIGHT);

  // You can change the transition that is used
  // SLIDE_LEFT, SLIDE_RIGHT, SLIDE_UP, SLIDE_DOWN
  ui.setFrameAnimation(SLIDE_LEFT);

  // Add frames
  ui.setFrames(frames, frameCount);

  // Add overlays
  ui.setOverlays(overlays, overlaysCount);

  // Initialising the UI will init the display too.
  ui.init();
  display.flipScreenVertically();

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

  btInit();
  DEBUG("Setup(): done");
}

/* LOOP() WWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWW */
void loop() {
// Check BT Connection and (RE-)connect if needed
  if (!btConnected) {
    btConnect();
  } else {
    if (millis() > countUI ) {
      countUI = ui.update() + millis();
    }
    for (uint8_t i = 0; i < BUTTONS_NUM; i++) {
      buttons[i].check();
    }
    // Read  response data from amp, to clear BT message buffer
    if (SerialBT.available()) {
      SerialBT.read();
    }
    //keepalive for Spark amp BT connection
  /*  if (millis() - countBT > 10000) {
      // request serial number and read returned bytes and discard - keep-alive link to Spark
      countBT = millis();
      SerialBT.write(get_serial, 23);
      flush_in();
    }
  */
  }
  
}

/* CUSTOM FUNCTIONS =============================================================================== */
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

//Bluetooth Event Call Back
void btEventCallback(esp_spp_cb_event_t event, esp_spp_cb_param_t *param) {
  // On BT connection close
  if (event == ESP_SPP_CLOSE_EVT ) {
    // TODO: Until the cause of connection instability (compared to Pi version) over long durations
    // is resolved, this should keep your pedal and amp connected fairly well by forcing reconnection
    // in the main loop
    btConnected = false;
  }
}

//Initialize Bluetooth
void btInit() {
  // Register BT event callback method
  SerialBT.register_callback(btEventCallback);
  if (!SerialBT.begin(DEVICE_NAME, true)) { // Detect for BT failure on ESP32 chip
    // Show "BT Init Failed!" message on device screen
    display.clear();
    display.setFont(ArialMT_Plain_24);
    display.setTextAlignment(TEXT_ALIGN_CENTER);
    display.drawString(64, 6, "BT Init");
    display.drawString(64, 30, "Failed!");
    display.display();

    // Loop infinitely until device shutdown/restart
    while (true) {};
  }
}


//Conect To Spark Amp
void btConnect() {
  // Loop until device establishes connection with amp
  while (!btConnected) {
    DEBUG("Connecting");
    btConnected = SerialBT.connect(SPARK_BT_NAME);
    // If BT connection with amp is successful
    if (btConnected && SerialBT.hasClient()) {
      DEBUG("BT Connected");
    } else {
      // in case of some connection loss
      btConnected = false;
      DEBUG("BT NOT Connected");
    }
  }
}
