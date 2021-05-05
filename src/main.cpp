#include <Arduino.h>
#include <FS.h>
#include "BluetoothSerial.h" 
#include "AceButton.h"
#include "MD_REncoder.h"
// Spark*.* is a portion from Paul Hamshere https://github.com/paulhamsh/
#include "Spark.h"
#include "SparkIO.h"
#include "SparkComms.h"

#ifdef SSD1306WIRE //these global def's are in platformio.ini
  #include "SSD1306Wire.h"
#endif
#ifdef SH1106WIRE
  #include "SH1106Wire.h"
#endif
#include "OLEDDisplayUi.h"
#include "images.h"
#include "fonts.h"

#define FORMAT_LITTLEFS_IF_FAILED true
#include "LITTLEFS.h"

#ifdef DEBUG_ENABLE
#define DEBUG(x) Serial.println((String)(millis()/1000) + "s. " + x)
#else
#define DEBUG(x)
#endif

/* GENERAL AND GLOBALS ======================================================================= */
enum e_mode {MODE_CONNECT, MODE_EFFECTS, MODE_PRESETS, MODE_ABOUT, MODE_LEVEL}; // these numbers also correspond to frame numbers of the UI
e_mode mode = MODE_CONNECT;
e_mode returnFrame = MODE_EFFECTS;
const unsigned long FRAME_TIMEOUT = 2000; //(ms) to return to main ui  
const char* DEVICE_NAME = "Easy Spark";
const char* VERSION = "0.1a"; 
// const char* SPARK_BT_NAME = "Spark 40 Audio";
const uint8_t TOTAL_PRESETS = 100; // number of stored on board presets
const uint8_t MAX_LEVEL = 100; // maximum level of effect, actual value in UI is level divided by 10
unsigned long countBT = 0;
unsigned long countUI = 0;
unsigned long countBlink = 0;
bool tempUI = false;
bool btConnected = false;
uint8_t localPreset;
int level = 0;
unsigned long timeToGoBack;

uint8_t selected_preset;

uint8_t b;
uint8_t midi_in;

int i, j, p;

/* Forward declarations ====================================================================== */
void tempFrame(e_mode tempFrame, e_mode returnFrame, const unsigned long msTimeout) ;
void handleEvent(ace_button::AceButton*, uint8_t, uint8_t);
void btConnect();
void btInit();
void returnToMainUI();
void dump_preset(SparkPreset);
bool blinkOn() {if(round(millis()/400)*400 != round(millis()/300)*300 ) return true; else return false;}


/* SPARKIE ================================================================================== */
SparkIO spark_io(false); // do NOT do passthru as only one device here, no serial to the app
SparkComms spark_comms;

unsigned int cmdsub;
SparkMessage msg;
SparkPreset preset;
SparkPreset presets[6];

unsigned long last_millis;
int my_state;
int scr_line;
char str[50];


/* BUTTONS Init ============================================================================== */
struct s_buttons {
  const uint8_t pin;
  const String efxLabel; //don't like String here, but further GUI functiions require Strings :-/
  const String actLabel;
  const uint8_t ledAddr; //future needs for addressable RGB LEDs
  uint32_t ledState; //data type may change, as i read the docs, enum of selected rgb colors maybe
};

const uint8_t BUTTONS_NUM = 6;
const uint8_t PEDALS_NUM = 4; //  first N buttons which are not encoders 

s_buttons BUTTONS[BUTTONS_NUM] = {
  {25, "DRV", "DLO", 0, 0},
  {26, "MOD", "SAV", 0, 0},
  {27, "DLY", "PRV", 0, 0},
  {14, "RVB", "NXT", 0, 0},
  {19, "VOL", "MAS", 0, 0}, //encoder 1 (may or may not be here)
  {17, "PRS", "PRS", 0, 0}, //encoder 2 (may or may not be here)
};

ace_button::AceButton buttons[BUTTONS_NUM];


/* ENCODERS Init ============================================================================= */
MD_REncoder Encoder1 = MD_REncoder(18, 5);
MD_REncoder Encoder2 = MD_REncoder(16, 4);


/* DISPLAY Init ============================================================================== */
#ifdef SSD1306WIRE
  SSD1306Wire  display(0x3c, 21 , 22); //in my case GPIO's are SDA=21 , SCL=22  
#endif
#ifdef SF1106WIRE
  SH1106Wire display(0x3c, 21, 22);
#endif
OLEDDisplayUi ui( &display );

void screenOverlay(OLEDDisplay *display, OLEDDisplayUiState* state) {
  if (btConnected) {
    display->drawXbm(display->width()-7, 0, small_bt_logo_width, small_bt_logo_height, small_bt_logo_bits);
  } 
}

void frameBtConnect(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  display->drawXbm((display->width()-BT_Logo_width)/2 + x, 16, BT_Logo_width, BT_Logo_height, BT_Logo_bits);
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_10);
  display->drawString(display->width()/2 + x,  y, "CONNECTING...");
}

void frameEffects(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  display->setFont(Roboto_Mono_Medium_52);
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->drawString(64 + x, 11 + y, String(localPreset) );
  
  display->setFont(ArialMT_Plain_10);
  int pxPerLabel = (display->width() - 8) / PEDALS_NUM;
  int boxWidth = display->getStringWidth("WWW")+1;
  for (int i=0 ; i<PEDALS_NUM; i++) {
    display->fillRect(x+(i*pxPerLabel+(pxPerLabel-boxWidth)/2),y,boxWidth,13);
    display->drawString(x+((i+0.5)*pxPerLabel),y,(BUTTONS[i].efxLabel));
  }
}

void framePresets(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  display->setFont(ArialMT_Plain_10);
  int pxPerLabel = (display->width() - 8) / PEDALS_NUM;
  int boxWidth = display->getStringWidth("WWW")+1;
  for (int i=0 ; i<PEDALS_NUM; i++) {
    display->fillRect(x+(i*pxPerLabel+(pxPerLabel-boxWidth)/2),y,boxWidth,13);
    display->drawString(x+((i+0.5)*pxPerLabel),y,(BUTTONS[i].actLabel));
  }
  display->setFont(Roboto_Mono_Medium_52);
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->drawString(64 + x, 17 + y, String(localPreset) );
}

void frameAbout(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  display->setFont(ArialMT_Plain_16);
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->drawString(display->width()/2 + x, 14 + y, DEVICE_NAME);
  display->drawString(display->width()/2 + x, 36 + y, VERSION);
}

void frameLevel(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  display->setFont(ArialMT_Plain_10);
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->drawString(display->width()/2 + x,  y, "MASTER");
  
  display->setFont(Roboto_Mono_Medium_52);
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->drawString(64 + x, 11 + y, String(level) );
} 

// array of frame drawing functions
FrameCallback frames[] = { frameBtConnect, frameEffects, framePresets, frameAbout, frameLevel };

// number of frames in UI
int frameCount = 5;

// Overlays are statically drawn on top of a frame eg. a clock
OverlayCallback overlays[] = { screenOverlay };
int overlaysCount = 1;


/* BLUETOOTH Init ============================================================================ */
#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run `make menuconfig` to and enable it
#endif


/* SETUP() WWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWW */
void setup() {  
#ifdef DEBUG_ENABLE
  Serial.begin(115200);
  DEBUG(F("Serial started"));
#endif  
  localPreset = 1;
  ui.setTargetFPS(30);
  ui.disableAllIndicators();
  ui.setFrameAnimation(SLIDE_LEFT);
  ui.setFrames(frames, frameCount);
  ui.setTimePerTransition(200);
  ui.setOverlays(overlays, overlaysCount);
  ui.disableAutoTransition();
  // Initialising the UI will init the display too.
  ui.init();
  display.flipScreenVertically();
  display.setColor(INVERSE);
  ui.switchToFrame(MODE_ABOUT);// show welcome screen
  ui.update();
  delay(1000);
  ui.switchToFrame(MODE_CONNECT);
  ui.update();
  spark_io.comms = &spark_comms;
  spark_comms.start_bt();

  for (uint8_t i = 0; i < BUTTONS_NUM; i++) {
    pinMode(BUTTONS[i].pin, INPUT_PULLUP);
    buttons[i].init(BUTTONS[i].pin, HIGH, i); //init AceButtons
    //leds code follows
    //..........
  }

  //Start rotary encoders
  Encoder1.begin();
  Encoder2.begin();

  // Configure the ButtonConfig with the event handler, and enable all higher level events.
  ace_button::ButtonConfig* buttonConfig = ace_button::ButtonConfig::getSystemButtonConfig();
  buttonConfig->setEventHandler(handleEvent);
  buttonConfig->setFeature(ace_button::ButtonConfig::kFeatureClick);
  buttonConfig->setFeature(ace_button::ButtonConfig::kFeatureDoubleClick);
  buttonConfig->setFeature(ace_button::ButtonConfig::kFeatureLongPress);
  buttonConfig->setFeature(ace_button::ButtonConfig::kFeatureSuppressAfterLongPress);
  buttonConfig->setFeature(ace_button::ButtonConfig::kFeatureSuppressAfterDoubleClick);
  buttonConfig->setFeature(ace_button::ButtonConfig::kFeatureSuppressAfterClick);

  // btInit();
  DEBUG("Setup(): done");
}

/* LOOP() WWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWW */
void loop() {
// Check BT Connection and (RE-)connect if needed
  if (!btConnected) {
    btConnect();
  } else {
/* SSSSSSSSSSSSSSSSSS-PPPPPPPPPPPPPPPPP-AAAAAAAAAAAAAAAAA-RRRRRRRRRRRRRRRR-KKKKKKKKKKKKKKKKKKK */
    spark_io.process();
    if (spark_io.get_message(&cmdsub, &msg, &preset)) { //there is something there
      sprintf(str, "< %4.4x", cmdsub);
      DEBUG("From Spark: "  + str);
      
      if (cmdsub == 0x0301) {
        p = preset.preset_num;
        j = preset.curr_preset;
        if (p == 0x7f)       
          p = 4;
        if (j == 0x01)
          p = 5;
        presets[p] = preset;
        dump_preset(preset);
      }

      if (cmdsub == 0x0306) {
        strcpy(presets[5].effects[3].EffectName, msg.str2);
        DEBUG("Change to amp model ");
        DEBUG(presets[5].effects[3].EffectName);
      }
      if (cmdsub == 0x0363) {
        DEBUG("Tap Tempo " + msg.val);
      }
      if (cmdsub == 0x0337) {
        DEBUG("Change model parameter ");
        DEBUG(msg.str1 +" " + msg.param1+ " " + msg.val);
      }
      
      if (cmdsub == 0x0338) {
        selected_preset = msg.param2;
        presets[5] = presets[selected_preset];
        DEBUG("Change to preset: " + selected_preset);
      }      
      
      if (cmdsub == 0x0327) {
        selected_preset = msg.param2;
        if (selected_preset == 0x7f) 
          selected_preset=4;
        presets[selected_preset] = presets[5];
        DEBUG("Store in preset: " + selected_preset);
      }

      if (cmdsub == 0x0310) {
        selected_preset = msg.param2;
        j = msg.param1;
        if (selected_preset == 0x7f) 
          selected_preset = 4;
        if (j == 0x01) 
          selected_preset = 5;
        presets[5] = presets[selected_preset];
        DEBUG("Hadware preset is: " + selected_preset);
      }
    }


    if (millis() > countUI ) {
      countUI = ui.update() + millis();
    }
    for (uint8_t i = 0; i < BUTTONS_NUM; i++) {
      buttons[i].check();
    }

    uint8_t x ;
    uint16_t s;
    x = Encoder1.read();
    s = Encoder1.speed();
    if (x) {
      s = s/7 + 1;
      //DEBUG((String)s);
      tempFrame(MODE_LEVEL,mode,FRAME_TIMEOUT);
      if (x == DIR_CW) {
        level = level + s;
        if (level>MAX_LEVEL) level = MAX_LEVEL;
      } else {
        level = level - s;
        if (level<0) level=0;
      }
    }
    x = Encoder2.read();
    if (x) {
      returnToMainUI();
      if (x == DIR_CW) {
        localPreset++;
        if (localPreset>TOTAL_PRESETS) localPreset = 1;
      } else {
        localPreset--;
        if (localPreset<1) localPreset=TOTAL_PRESETS;
      }
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
  if ((millis() > timeToGoBack) && tempUI) {
    returnToMainUI();
  }
  
}

/* CUSTOM FUNCTIONS =============================================================================== */
void handleEvent(ace_button::AceButton* button, uint8_t eventType, uint8_t buttonState) {
  uint8_t id = button->getId();
  if (id != 4) {returnToMainUI();}
  DEBUG("handleEvent(): id: " + (String)id + " eventType: " + (String)eventType + "; buttonState: " + (String)buttonState );

//  uint8_t ledPin = BUTTONS[id].ledAddr;
  if (mode==MODE_EFFECTS) {
    switch (eventType) {
      case ace_button::AceButton::kEventPressed:
        if (id==4) {
          tempFrame(MODE_LEVEL,mode,FRAME_TIMEOUT);
        }
        break;
      case ace_button::AceButton::kEventLongPressed:
        if (id==3) {
          mode = MODE_PRESETS;
          ui.transitionToFrame(mode);
        }
        break;
      case ace_button::AceButton::kEventClicked:
        break;
      case ace_button::AceButton::kEventDoubleClicked:
        break;
    }
  }
  if (mode==MODE_PRESETS){
    switch (eventType) {
      case ace_button::AceButton::kEventPressed:
        break;
      case ace_button::AceButton::kEventLongPressed:
        if (id==3) {
          mode = MODE_EFFECTS;
          ui.transitionToFrame(mode);
        }
        break;
      case ace_button::AceButton::kEventClicked:
        break;
      case ace_button::AceButton::kEventDoubleClicked:
        break;
    }
  }
}

/*
//on Bluetooth event
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
    display.clear();
    display.setFont(ArialMT_Plain_16);
    display.setTextAlignment(TEXT_ALIGN_CENTER);
    display.drawString(display.width()/2, 13, "Bluetooth");
    display.drawString(display.width()/2, 36, "Not Available!");
    display.display();
    // Loop infinitely until device shutdown/restart
    while (true) {};
  }
}
*/
// Conect To Spark Amp
void btConnect() {
  // Loop until device establishes connection with amp
  while (!btConnected) {
    ui.switchToFrame(MODE_CONNECT);
    ui.update();
    delay(50); //let cores breathe
    ui.update(); // kinda workaround for not-switching to this frame
    DEBUG("Connecting...");
    btConnected =  spark_comms.connect_to_spark();;
    // If BT connection with amp is successful
    if (btConnected && spark_comms.bt->hasClient()) {
      mode = MODE_EFFECTS;
      ui.switchToFrame(mode);
      ui.update();
      DEBUG("BT Connected");
    } else {
      ui.switchToFrame(MODE_CONNECT);
      ui.update();
      // in case of some connection loss
      btConnected = false;
      DEBUG("BT NOT Connected");
    }
  }
}

void tempFrame(e_mode tempFrame, e_mode retFrame, const unsigned long msTimeout) {
  if (!tempUI) {
    mode = tempFrame;
    ui.switchToFrame(mode);
    returnFrame = retFrame;
    tempUI = true;
  }
  timeToGoBack = millis() + msTimeout; 
}

void returnToMainUI() {
  mode = returnFrame;
  ui.switchToFrame(mode);
  timeToGoBack = millis();
  tempUI = false;
}

void dump_preset(SparkPreset preset) {
  int i,j;

  DEBUG(preset.curr_preset); DEBUG(" ");
  DEBUG(preset.preset_num); DEBUG(" ");
  DEBUG(preset.Name); DEBUG(" ");

  DEBUG(preset.Description);

  for (j=0; j<7; j++) { 
    DEBUG(preset.effects[j].EffectName) ;
    if (preset.effects[j].OnOff == true) DEBUG(" On "); else DEBUG (" Off ");
    for (i = 0; i < preset.effects[j].NumParameters; i++) {
      DEBUG(preset.effects[j].Parameters[i]) ;
    } 
  }
  DEBUG(preset.chksum); 
}

