#include <Arduino.h>
#include <FS.h>
#include "BluetoothSerial.h" 
#include "AceButton.h"
#include "MD_REncoder.h"
// Spark*.* is a portion from Paul Hamshere https://github.com/paulhamsh/
#include "Spark.h"
#include "SparkIO.h"
#include "SparkComms.h"
/*  
      Some explanations
This project is located here: https://github.com/copych/BT_Spark_pedal
Initial hardware build included:
  - DOIT ESP32 DevKit v1 : 1pcs
  - buttons : 4 pcs
  - rotary encoders : 2pcs
  - SSD1306 duo-color OLED display : 1pcs
 
presets[]
 0,1,2,3 : slots 0x000-0x0003 hardware presets, associated with the amp's buttons
 4 : slot 0x007f used by the app (and this program) to hold current preset
 5 : slot 0x01XX (current state) - current preset + all the unsaved editing on the amp
*/
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

// GENERAL AND GLOBALS ======================================================================= 
#define DISPLAY_SCL 22
#define DISPLAY_SDA 21
#define ENCODER2_CLK 4
#define ENCODER2_DT 16
#define ENCODER2_SW 17
#define ENCODER1_CLK 5
#define ENCODER1_DT 18
#define ENCODER1_SW 19
#define BUTTON1_PIN 25
#define BUTTON2_PIN 26
#define BUTTON3_PIN 27
#define BUTTON4_PIN 14
#define TRANSITION_TIME 200 //(ms)
#define FRAME_TIMEOUT 2000 //(ms) to return to main UI from temporary UI frame 
#define SMALL_FONT ArialMT_Plain_10
#define HUGE_FONT Roboto_Mono_Medium_52
enum e_amp_presets {HW_PRESET_0,HW_PRESET_1,HW_PRESET_2,HW_PRESET_3,TMP_PRESET,CUR_EDITING};
enum e_mode {MODE_CONNECT, MODE_EFFECTS, MODE_PRESETS, MODE_ABOUT, MODE_LEVEL}; // these numbers also correspond to frame numbers of the UI
e_mode mode = MODE_CONNECT;
e_mode returnFrame = MODE_EFFECTS;  
const char* DEVICE_NAME = "Easy Spark";
const char* VERSION = "0.1a"; 
// const char* SPARK_BT_NAME = "Spark 40 Audio";
const uint8_t TOTAL_PRESETS = 20; // number of stored on board presets
const uint8_t MAX_LEVEL = 100; // maximum level of effect, actual value in UI is level divided by 100
unsigned long countBT = 0;
unsigned long countUI = 0;
unsigned long countBlink = 0;
bool tempUI = false;
bool btConnected = false;
int localPreset;
int level = 0;
volatile unsigned long timeToGoBack;
String ampName="", serialNum="", firmwareVer="" ; //sorry for the Strings, I hope this won't crash the pedal =)
unsigned int waitSubcmd=0x0000;
volatile bool stillWaiting=false;
unsigned long waitTill;
String btCaption, fxCaption="master";

uint8_t remotePreset;

uint8_t b;

int i, j, p;

// Forward declarations ======================================================================
void tempFrame(e_mode tempFrame, e_mode returnFrame, const unsigned long msTimeout) ;
void returnToMainUI();
void handleButtonEvent(ace_button::AceButton*, uint8_t, uint8_t);
void btConnect();
void btInit();
void dump_preset(SparkPreset);
void greetings();
bool waitForResponse(unsigned int subcmd, unsigned long msTimeout);
void stopWaiting();
bool blinkOn() {if(round(millis()/400)*400 != round(millis()/300)*300 ) return true; else return false;}
void updateStatuses();

// SPARKIE ================================================================================== 
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


// BUTTONS Init ==============================================================================
struct s_buttons {
  const uint8_t pin;
  const String fxLabel; //don't like String here, but further GUI functiions require Strings, so I don't care :-/
  const String actLabel;
  const uint8_t ledAddr; //future needs for addressable RGB LEDs
  uint32_t ledState; //data type may change, as i read the docs, enum of selected rgb colors maybe
  uint8_t fxSlotNumber; // [0-6] number in fx chain
  bool fxState;
};

const uint8_t BUTTONS_NUM = 6;
const uint8_t PEDALS_NUM = 4; //  first N buttons which are not encoders 

s_buttons BUTTONS[BUTTONS_NUM] = {
  {BUTTON1_PIN, "DRV", "DLO", 0, 0, 2, false},
  {BUTTON2_PIN, "MOD", "SAV", 0, 0, 4, false},
  {BUTTON3_PIN, "DLY", "PRV", 0, 0, 5, false},
  {BUTTON4_PIN, "RVB", "NXT", 0, 0, 6, false},
  {ENCODER1_SW, "VOL", "MAS", 0, 0, 0, false}, //encoder 1 (may or may not be here)
  {ENCODER2_SW, "PRS", "PRS", 0, 0, 0, false}, //encoder 2 (may or may not be here)
};

ace_button::AceButton buttons[BUTTONS_NUM];


// ENCODERS Init ============================================================================= 
MD_REncoder Encoder1 = MD_REncoder(ENCODER1_DT, ENCODER1_CLK);
MD_REncoder Encoder2 = MD_REncoder(ENCODER2_DT, ENCODER2_CLK);


// DISPLAY Init ============================================================================== 
#ifdef SSD1306WIRE
  SSD1306Wire  display(0x3c, DISPLAY_SDA , DISPLAY_SCL); //in my case GPIO's are SDA=21 , SCL=22 , addr is 0x3c 
#endif
#ifdef SF1106WIRE
  SH1106Wire display(0x3c, DISPLAY_SDA, DISPLAY_SCL);
#endif
OLEDDisplayUi ui( &display );

void screenOverlay(OLEDDisplay *display, OLEDDisplayUiState* state) {
  if (btConnected) {
    display->drawXbm(display->width()-7, 0, small_bt_logo_width, small_bt_logo_height, small_bt_logo_bits);
  } 
}

void frameBtConnect(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  static String addon;
  display->drawXbm((display->width()-BT_Logo_width)/2 + x, 16, BT_Logo_width, BT_Logo_height, BT_Logo_bits);
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(SMALL_FONT);
  if (blinkOn()) addon="."; else addon=" ";
  display->drawString(display->width()/2 + x,  y, btCaption+addon);
}

void frameEffects(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  display->setFont(SMALL_FONT);
  int pxPerLabel = (display->width() - 8) / PEDALS_NUM;
  int boxWidth = display->getStringWidth("WWW");
  boxWidth = max(boxWidth,pxPerLabel-2);
  for (int i=0 ; i<PEDALS_NUM; i++) {
    if (BUTTONS[i].fxState) {
      display->fillRect(x+(i*pxPerLabel+(pxPerLabel-boxWidth)/2),y,boxWidth,14);
    }
    display->drawString(x+((i+0.5)*pxPerLabel),y,(BUTTONS[i].fxLabel));
  }
  if (localPreset<=4) {
    display->drawRect(x+((pxPerLabel-boxWidth)/2),y + 16,boxWidth,14);
    if (localPreset==4) {
      display->drawString(boxWidth/2 + x,y + 16 ,"TMP");
    } else {
      display->drawString(boxWidth/2 + x,y + 16 ,"HW");
    }
  }
  display->setFont(HUGE_FONT);
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->drawString(64 + x, 11 + y, String(localPreset + 1) ); // +1 for humans
}

void framePresets(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  display->setFont(SMALL_FONT);
  int pxPerLabel = (display->width() - 8) / PEDALS_NUM;
  int boxWidth = display->getStringWidth("WWW");
  boxWidth = max(boxWidth,pxPerLabel-2);
  for (int i=0 ; i<PEDALS_NUM; i++) {
    display->fillRect(x+(i*pxPerLabel+(pxPerLabel-boxWidth)/2),y,boxWidth,14);
    display->drawString(x+((i+0.5)*pxPerLabel),y,(BUTTONS[i].actLabel));
  }
  display->setFont(HUGE_FONT);
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->drawString(64 + x, 11 + y, String(localPreset) );
}

void frameAbout(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  if (ampName=="") { // welcome 
    display->setFont(ArialMT_Plain_16);
    display->setTextAlignment(TEXT_ALIGN_CENTER);
    display->drawString(display->width()/2 + x, 14 + y, DEVICE_NAME);
    display->drawString(display->width()/2 + x, 36 + y, VERSION);
  } else {
    display->setFont(ArialMT_Plain_10);
    display->setTextAlignment(TEXT_ALIGN_CENTER);    
    display->drawString(display->width()/2 + x, 0 + y, "amp: " + ampName);
    display->drawString(display->width()/2 + x, 20 + y, "s/n: " + serialNum);
    display->drawString(display->width()/2 + x, 40 + y, "f/w: " + firmwareVer);
  }
}

void frameLevel(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  display->setFont(SMALL_FONT);
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->drawString(display->width()/2 + x,  y, fxCaption);
  
  display->setFont(HUGE_FONT);
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  sprintf(str,"%3.1f",(float)(level)/10);
  display->drawString(64 + x, 11 + y , String(str) );
} 

// array of frame drawing functions
FrameCallback frames[] = { frameBtConnect, frameEffects, framePresets, frameAbout, frameLevel };

// number of frames in UI
int frameCount = 5;

// Overlays are statically drawn on top of a frame eg. a clock
OverlayCallback overlays[] = { screenOverlay };
int overlaysCount = 1;


// BLUETOOTH Init ============================================================================
#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run `make menuconfig` to and enable it
#endif


// SETUP() WWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWW
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
  ui.setTimePerTransition(TRANSITION_TIME);
  ui.setOverlays(overlays, overlaysCount);
  ui.disableAutoTransition();
  // Initialising the UI will init the display too.
  ui.init();
  display.flipScreenVertically();
  display.setColor(INVERSE);
  ui.switchToFrame(MODE_ABOUT);// show welcome screen
  ui.update();
  delay(1000);
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
  buttonConfig->setEventHandler(handleButtonEvent);
  buttonConfig->setFeature(ace_button::ButtonConfig::kFeatureClick);
  buttonConfig->setFeature(ace_button::ButtonConfig::kFeatureDoubleClick);
  buttonConfig->setFeature(ace_button::ButtonConfig::kFeatureLongPress);
  buttonConfig->setFeature(ace_button::ButtonConfig::kFeatureSuppressAfterLongPress);
  buttonConfig->setFeature(ace_button::ButtonConfig::kFeatureSuppressAfterDoubleClick);
  buttonConfig->setFeature(ace_button::ButtonConfig::kFeatureSuppressAfterClick);

  // btInit();
  DEBUG("Setup(): done");
}

// LOOP() WWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWW 
void loop() {
// Check BT Connection and (RE-)connect if needed
btConnected = spark_comms.connected();
  if (!btConnected) {
    btConnect();
  } else {
// SSSSSSSSSSSSSSSSSS-PPPPPPPPPPPPPPPPP-AAAAAAAAAAAAAAAAA-RRRRRRRRRRRRRRRR-KKKKKKKKKKKKKKKKKKK 
    spark_io.process();
    if (spark_io.get_message(&cmdsub, &msg, &preset)) { //there is something there
      sprintf(str, "< %4.4x", cmdsub);
      DEBUG("From Spark: "  + str);
      if (cmdsub==waitSubcmd) {stopWaiting();}

      if (cmdsub == 0x0301) { //get preset info
        p = preset.preset_num;
        j = preset.curr_preset;
        if (p == 0x007f)       
          p = TMP_PRESET;
        if (j == 0x01) {
          p = CUR_EDITING;
        }
        presets[p] = preset;
        updateStatuses();
        dump_preset(preset);
      }

      if (cmdsub == 0x0306) {
        strcpy(presets[CUR_EDITING].effects[3].EffectName, msg.str2);
        DEBUG("Change to amp model ");
        DEBUG(presets[CUR_EDITING].effects[3].EffectName);
      }

      if (cmdsub == 0x0363) {
        DEBUG("Tap Tempo " + msg.val);
      }

      if (cmdsub == 0x0337) {
        DEBUG("Change parameter ");
        DEBUG(msg.str1 + " " + msg.param1+ " " + msg.val);
        fxCaption = ((String)(msg.str1) + ": " + (String)msg.param1) ;
        level = msg.val * 100;
        tempFrame(MODE_LEVEL,mode,1000);
      }
      
      if (cmdsub == 0x0338) {
        remotePreset = msg.param2;
        presets[CUR_EDITING] = presets[remotePreset];
        localPreset = remotePreset;
        updateStatuses();
        DEBUG("Change to hw preset: " + remotePreset);
      }
      
      if (cmdsub == 0x032f) {
        firmwareVer = (String)msg.param1 + "." + (String)msg.param2 + "." + (String)msg.param3 + "." + (String)msg.param4; // I know, I know.. just one time, please =)
        DEBUG("f/w: " + firmwareVer);
      }

      if (cmdsub == 0x0323) {
        serialNum = msg.str1;
        DEBUG("s/n: " + serialNum);
      }  

      if (cmdsub == 0x0311) {
        ampName = msg.str1;
        DEBUG("Amp name: " + ampName);
      }

      if (cmdsub == 0x0327) {
        remotePreset = msg.param2;
        if (remotePreset == 0x7f) {
          remotePreset=TMP_PRESET; }
        localPreset = remotePreset;
        presets[remotePreset] = presets[CUR_EDITING];
        DEBUG("Store in preset: " + remotePreset);
        updateStatuses();
      }

      if (cmdsub == 0x0415) {
        updateStatuses();
        DEBUG("OnOff: ACK");
      }
      if (cmdsub == 0x0310) {
        remotePreset = msg.param2;
        j = msg.param1;
        if (remotePreset == 0x7f) 
          remotePreset = TMP_PRESET;
        if (j == 0x01) 
          remotePreset = CUR_EDITING;
        localPreset = remotePreset;
        presets[CUR_EDITING] = presets[remotePreset];
        updateStatuses();
        DEBUG("Hadware preset is: " + remotePreset);
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
        if (localPreset>TOTAL_PRESETS-1) localPreset = 0;
      } else {
        localPreset--;
        if (localPreset<0) localPreset=TOTAL_PRESETS-1;
      }
      if (localPreset <= HW_PRESET_3 ) {
        spark_io.change_hardware_preset(localPreset);
      }
      if (localPreset==TMP_PRESET ) {
        preset.preset_num = 0x007f;
        spark_io.change_hardware_preset(0x007f);
        if (waitForResponse(spark_io.expectedSubcmd,1000)) {
          DEBUG("success 0x7f");
        } else {
          DEBUG("timed out");
          spark_io.get_hardware_preset_number();
          DEBUG("wait for: " + String(spark_io.expectedSubcmd,HEX));
          waitForResponse(spark_io.expectedSubcmd,2000);
          spark_io.get_preset_details(0x0100);
          DEBUG("wait for: " + String(spark_io.expectedSubcmd,HEX));
          waitForResponse(spark_io.expectedSubcmd,2000);
        }
      }
      if (localPreset>TMP_PRESET) {
        remotePreset = TMP_PRESET;
        // upload presets from ESP32's filesystem to 0x007f slot
        // read preset #localPreset from LITTLEFS
        // change preset.number to 0x007f
        // create_preset on amp
        // make it active
      } else {
        remotePreset = localPreset;
      }
      presets[CUR_EDITING] = presets[remotePreset];
      updateStatuses();
      DEBUG("Change to preset: " + remotePreset);
      updateStatuses();
    }
  }
  if ((millis() > timeToGoBack) && tempUI) {
    returnToMainUI();
  }
  
}

// CUSTOM FUNCTIONS =============================================================================== 
void handleButtonEvent(ace_button::AceButton* button, uint8_t eventType, uint8_t buttonState) {
  uint8_t id = button->getId();
  if (id != 4 && eventType != ace_button::AceButton::kEventLongReleased) {returnToMainUI();}
  DEBUG("Button: id: " + (String)id + " eventType: " + (String)eventType + "; buttonState: " + (String)buttonState );

//  uint8_t ledPin = BUTTONS[id].ledAddr;
  if (mode==MODE_EFFECTS) {
    switch (eventType) {
      case ace_button::AceButton::kEventPressed:
        if (id<PEDALS_NUM) {
          //OnOff
          spark_io.turn_effect_onoff(presets[CUR_EDITING].effects[BUTTONS[id].fxSlotNumber].EffectName, !presets[CUR_EDITING].effects[BUTTONS[id].fxSlotNumber].OnOff);
          presets[CUR_EDITING].effects[BUTTONS[id].fxSlotNumber].OnOff = !presets[CUR_EDITING].effects[BUTTONS[id].fxSlotNumber].OnOff;

        }
        if (id==4) {
          tempFrame(MODE_LEVEL,mode,FRAME_TIMEOUT);
        }
        break;
      case ace_button::AceButton::kEventLongPressed:
        if (id==3) {
          mode = MODE_PRESETS;
          ui.transitionToFrame(mode);
        }
        if (id==0) {
           tempFrame(MODE_ABOUT,mode,4000);
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

// Conect To Spark Amp
void btConnect() {
  // Loop until device establishes connection with amp
  while (!btConnected) {
    ui.switchToFrame(MODE_CONNECT);
    btCaption = "CONNECTING..";
    ui.update();
    delay(50); //let cores breathe as ESP's delay() has air in it
    ui.update(); // kinda workaround forcing to this frame
    DEBUG("Connecting... >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>");
    btConnected =  spark_comms.connect_to_spark();
    // If BT connection with amp is successful
    if (btConnected && spark_comms.bt->hasClient()) {
      DEBUG("BT Connected");      
      btCaption = "RETRIEVING..";
      greetings();
      mode = MODE_EFFECTS;
      tempFrame(MODE_ABOUT,mode,3000);
    } else {
      ui.switchToFrame(MODE_CONNECT);
      ui.update();
      // in case of connection loss
      btConnected = false;
      serialNum = "";
      firmwareVer = "";
      ampName = "";
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
  if (tempUI) {
    mode = returnFrame;
    ui.switchToFrame(mode);
    timeToGoBack = millis();
    tempUI = false;
  }
}

void dump_preset(SparkPreset preset) {
  int i,j;
  DEBUG(">================================");
  DEBUG(preset.curr_preset);
  DEBUG(preset.preset_num);
  DEBUG(preset.UUID);
  DEBUG(preset.Name);
  DEBUG(preset.Version); 
  DEBUG(preset.Description);
  DEBUG(preset.Icon);
  DEBUG(preset.BPM);

  for (j=0; j<7; j++) {
    DEBUG(">>===============================");
    DEBUG((String)j + ": " + preset.effects[j].EffectName) ;
    if (preset.effects[j].OnOff == true) DEBUG("On"); else DEBUG ("Off");
    for (i = 0; i < preset.effects[j].NumParameters; i++) {
      DEBUG(preset.effects[j].Parameters[i]) ;
    } 
  }
  DEBUG(preset.chksum); 
}

//returns true if response received in timely fashion, otherwise returns false (timed out) 
bool waitForResponse(unsigned int subcmd=0, unsigned long msTimeout=1000) {
  waitSubcmd = subcmd;
  if (subcmd==0x0000) { 
    stillWaiting=false;
  } else {
    stillWaiting=true;
    waitTill = msTimeout+millis();
  }
  while (stillWaiting && millis()<waitTill) {  loop(); }
  if (stillWaiting) return false; else return true;
}

void stopWaiting() {
    stillWaiting = false;
}

void updateStatuses() {
  for (int i=0 ; i< PEDALS_NUM; i++) {
    BUTTONS[i].fxState = presets[CUR_EDITING].effects[BUTTONS[i].fxSlotNumber].OnOff;
  }
}

void greetings() {
  spark_io.get_name();
  DEBUG("wait for: " + String(spark_io.expectedSubcmd,HEX));
  waitForResponse(spark_io.expectedSubcmd,2000);
  spark_io.greeting();
  DEBUG("wait for: " + String(spark_io.expectedSubcmd,HEX));
  waitForResponse(spark_io.expectedSubcmd,2000);
  spark_io.get_serial();
  DEBUG("wait for: " + String(spark_io.expectedSubcmd,HEX));
  waitForResponse(spark_io.expectedSubcmd,2000);
  spark_io.get_preset_details(0x0000);
  DEBUG("wait for: " + String(spark_io.expectedSubcmd,HEX));
  waitForResponse(spark_io.expectedSubcmd,2000);
  spark_io.get_preset_details(0x0001);
  DEBUG("wait for: " + String(spark_io.expectedSubcmd,HEX));
  waitForResponse(spark_io.expectedSubcmd,2000);
  spark_io.get_preset_details(0x0002);
  DEBUG("wait for: " + String(spark_io.expectedSubcmd,HEX));
  waitForResponse(spark_io.expectedSubcmd,2000);
  spark_io.get_preset_details(0x0003);
  DEBUG("wait for: " + String(spark_io.expectedSubcmd,HEX));
  waitForResponse(spark_io.expectedSubcmd,2000);
  spark_io.get_hardware_preset_number();
  DEBUG("wait for: " + String(spark_io.expectedSubcmd,HEX));
  waitForResponse(spark_io.expectedSubcmd,2000);
  spark_io.get_firmware_ver();
  DEBUG("wait for: " + String(spark_io.expectedSubcmd,HEX));
  waitForResponse(spark_io.expectedSubcmd,2000);
  spark_io.get_preset_details(0x0100);
  DEBUG("wait for: " + String(spark_io.expectedSubcmd,HEX));
  waitForResponse(spark_io.expectedSubcmd,2000);

}
