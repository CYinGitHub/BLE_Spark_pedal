#include <Arduino.h>
#include <FS.h>
#include "AceButton.h"
#include "MD_REncoder.h"
// Spark*.* is a portion from Paul Hamshere https://github.com/paulhamsh/
#include "Spark.h"
#include "SparkIO.h"
#include "SparkComms.h"
#include "SparkPresets.h" // maybe it's not the right place, but....

#define ARDUINOJSON_USE_DOUBLE 1
#include "ArduinoJson.h"

/*  
      Some explanations
I am not a native programmer, so my C/C++ code looks more like VB =) sorry for that
I just tried to make it work, to look pretty and to eliminate errors/warnings.
This project is located here: https://github.com/copych/BT_Spark_pedal
Initial hardware build included:
  - DOIT ESP32 DevKit v1 : 1pcs
  - buttons : 4 pcs
  - pushable rotary encoders : 2pcs
  - SSD1306 duo-color OLED display : 1pcs
 
presets[]
 0,1,2,3 : slots 0x000-0x0003 hardware presets, associated with the amp's buttons
 4 : slot 0x007f used by the app (and this program) to hold temporary preset
 5 : slot 0x01XX (current state) - current preset + all the unsaved editing on the amp

You may (or may not) find usefull a couple of methods which works with PG json files:
loadPresetFromFile()
savePresetToFile()
*/
#ifdef SSD1306WIRE //which of the OLED displays you use: these global def's are in platformio.ini
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
#define DEBUG(t) Serial.println((String)(millis()/1000) + "s. " + t)
#else
#define DEBUG(t)
#endif

// GENERAL AND GLOBALS ======================================================================= 
#define DISPLAY_SCL 22
#define DISPLAY_SDA 21
#define ENCODER1_CLK 5 // note that GPIO5 is HardwareSerial(2) RX, don't use them together
#define ENCODER1_DT 18 // note that GPIO18 is HardwareSerial(2) TX, don't use them together
#define ENCODER1_SW 19
#define ENCODER2_CLK 17
#define ENCODER2_DT 16
#define ENCODER2_SW 4 // (on/off) Only GPIOs which have RTC functionality can be used: 0,2,4,12-15,25-27,32-39 
#define BUTTON1_PIN 25
#define BUTTON2_PIN 26
#define BUTTON3_PIN 27
#define BUTTON4_PIN 14
#define BT_ATTEMPTS_BEFORE_OFF 200
#define HW_PRESETS 4  // 4 hardware presets in amp
#define HARD_PRESETS 24  // number of hard-coded presets in SparkPresets.h
#define FLASH_PRESETS 50  // number of presets stored in on-board flash
#define TOTAL_PRESETS HW_PRESETS + FLASH_PRESETS
#define TOTAL_SCENES 10 // number of 4-efx combinations to store
#define TRANSITION_TIME 200 //(ms) ui slide effect timing
#define FRAME_TIMEOUT 3000 //(ms) to return to main UI from temporary UI frame 
#define SMALL_FONT ArialMT_Plain_10
#define MID_FONT ArialMT_Plain_16
#define BIG_FONT ArialMT_Plain_24
#define HUGE_FONT Roboto_Mono_Medium_52

// a lot of globals, I know it's not that much elegant 
enum e_amp_presets {HW_PRESET_0,HW_PRESET_1,HW_PRESET_2,HW_PRESET_3,TMP_PRESET,CUR_EDITING, TMP_PRESET_ADDR=0x007f};
// these numbers correspond to frame numbers of the UI (frames[])
enum e_mode {MODE_CONNECT, MODE_EFFECTS, MODE_PRESETS, MODE_ORGANIZE, MODE_SETTINGS, MODE_ABOUT, MODE_LEVEL}; 
e_mode mode = MODE_CONNECT;
e_mode returnFrame = MODE_EFFECTS;  // we should memorize where to return
const char* DEVICE_NAME = "Pedal for Spark";
const char* VERSION = "0.7BLE";
const uint8_t MAX_LEVEL = 100; // maximum level of effect, actual value in UI is level divided by 100
bool btConnected = false;
int scroller=0, scrollStep = -2; // speed of scrolling tone names
ulong scrollCounter;
ulong idleCounter; // for pending tone change 
ulong waitCounter;
ulong uiCounter = 0;
volatile ulong timeToGoBack;
volatile bool stillWaiting=false;
unsigned int waitSubcmd=0x0000;
bool tempUI = false;
int p, j, curKnob=0, curFx=3, curParam=4, level = 0;
String ampName="", serialNum="", firmwareVer="" ; //sorry for the Strings, I hope this won't crash the pedal =)
String btCaption, fxCaption="MASTER";
volatile ulong safeRecursion=0;
int pendingPresetNum = -1 ;
int localPresetNum; 
uint8_t remotePresetNum;
bool fxState[] = {false,false,false,false,false,false,false}; // array to store FX's on/off state before total bypass is ON
bool bypass=false;
int btAttempts;
SparkPreset flashPresets[FLASH_PRESETS];
int localScene;


// Forward declarations ======================================================================
void tempFrame(e_mode tempFrame, e_mode returnFrame, const ulong msTimeout) ;
void returnToMainUI();
void handleButtonEvent(ace_button::AceButton*, uint8_t, uint8_t);
void btConnect();
void btInit();
void dump_preset(SparkPreset);
void greetings();
bool waitForResponse(unsigned int subcmd, ulong msTimeout);
void stopWaiting();
bool blinkOn() {if(round(millis()/400)*400 != round(millis()/300)*300 ) return true; else return false;}
bool triggedOn() {if(round(millis()/2000)*2000 != round(millis()/1000)*1000 ) return true; else return false;}
void setPendingPreset(int localNum);
void updateFxStatuses();
void uploadPreset(int localNum);
s_fx_coords fxNumByName(const char* fxName);
void toggleBypass();
void toggleEffect(int slotNum);
void cycleMode();
bool createFolders();
void textAnimation(const String &s, ulong msDelay, int yShift, bool show);
void ESP_off();
void ESP_on();
void buildPresetList();
void updatePresetList(uint8_t numPreset);
char* localPresetName(int localNum);
void handlePresets(int x);
void handleScenes(int x);
SparkPreset loadPresetFromFile(int slot);
bool savePresetToFile(SparkPreset savedPreset, const String &filePath);

// SPARKIE ================================================================================== 
SparkIO spark_io(false); // do NOT do passthru as only one device here, no serial to the app
SparkComms spark_comms;

unsigned int cmdsub;
SparkMessage msg;
SparkPreset preset;
SparkPreset presets[6];

ulong last_millis;
int my_state;
int scr_line;
char str[50];

// BUTTONS Init ==============================================================================
typedef struct {
  const uint8_t pin;
  const String fxLabel; //don't like String here, but further GUI functiions require Strings, so I don't care :-/
  const String actLabel;
  const uint8_t ledAddr; //future needs for addressable RGB LEDs
  uint32_t ledState; //data type may change, as i read the docs, enum of selected rgb colors maybe
  uint8_t fxSlotNumber; // [0-6] number in fx chain
  bool fxState;
} s_buttons ;

const uint8_t BUTTONS_NUM = 6;
const uint8_t PEDALS_NUM = 4; //  first N buttons which are not encoders 

s_buttons BUTTONS[BUTTONS_NUM] = {
//  {BUTTON_NGT_PIN, "NGT", "", 0, 0, 0, false},
//  {BUTTON_CMP_PIN, "CMP", "", 0, 0, 1, false},
  {BUTTON1_PIN, "DRV", "1", 0, 0, 2, false},
//  {BUTTON_AMP_PIN, "AMP", "", 0, 0, 3, false},
  {BUTTON2_PIN, "MOD", "2", 0, 0, 4, false},
  {BUTTON3_PIN, "DLY", "3", 0, 0, 5, false},
  {BUTTON4_PIN, "RVB", "4", 0, 0, 6, false},
  {ENCODER1_SW, "", "", 0, 0, 0, false}, //encoder 1 (may or may not be here)
  {ENCODER2_SW, "", "", 0, 0, 0, false}, //encoder 2 (may or may not be here)
};

ace_button::AceButton buttons[BUTTONS_NUM];


// ENCODERS Init ============================================================================= 
MD_REncoder Encoder1 = MD_REncoder(ENCODER1_DT, ENCODER1_CLK);
MD_REncoder Encoder2 = MD_REncoder(ENCODER2_DT, ENCODER2_CLK);


// DISPLAY Init ============================================================================== 
#ifdef SSD1306WIRE
  SSD1306Wire  display(0x3c, DISPLAY_SDA , DISPLAY_SCL); //in my case GPIO's are SDA=21 , SCL=22 , addr is 0x3c 
#endif
#ifdef SH1106WIRE
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
  if(bypass){
    display->setFont(BIG_FONT);
    display->setTextAlignment(TEXT_ALIGN_CENTER);
    display->drawString(display->width()/2 + x, 20 + y, "BYPASS" );
  } else {
    int boxWidth = display->getStringWidth("WWW");
    int pxPerLabel = (display->width() - 8) / PEDALS_NUM;
    boxWidth = max(boxWidth,pxPerLabel-2);
    if (localPresetNum<HW_PRESETS) {
      display->drawRect(x+((pxPerLabel-boxWidth)/2),y + 16,boxWidth,14);
      display->drawString(boxWidth/2 + x,y + 16 ,"HW");
    }
    if (pendingPresetNum < 0) {
      display->setTextAlignment(TEXT_ALIGN_LEFT);
      display->setFont(HUGE_FONT);
      int s1w = display->getStringWidth(String(localPresetNum + 1))+5;
      display->setFont(BIG_FONT);
      int s2w = display->getStringWidth(presets[CUR_EDITING].Name)+5;
      if (s1w+s2w <= display->width()) {
        scroller = ( display->width() - s1w - s2w ) / 2;
      } else {
        if ( millis() > scrollCounter ) {
          scroller = scroller + scrollStep;
          if (scroller < (int)(display->width())-s1w-s2w-s1w-s2w) {
            scroller = scroller + s1w + s2w;
          }
          scrollCounter = millis() + 20;
        }
        display->setFont(HUGE_FONT);
        display->drawString( x + scroller + s1w + s2w, 11 + y, String(localPresetNum + 1) ); // +1 for humans
        display->setFont(BIG_FONT);
        display->drawString(x + scroller + s1w + s2w + s1w, y + display->height()/2 - 6 ,presets[CUR_EDITING].Name);
      }
      display->setFont(HUGE_FONT);
      display->drawString( x + scroller, 11 + y, String(localPresetNum + 1) ); // +1 for humans
      display->setFont(BIG_FONT);
      display->drawString(x + scroller + s1w, y + display->height()/2 - 6 ,presets[CUR_EDITING].Name);
    } else {
      display->setFont(BIG_FONT);
      display->setTextAlignment(TEXT_ALIGN_RIGHT);
      int offsetX = display->getStringWidth("00");
      display->drawString(offsetX + x, 27 + y, String(localPresetNum+1) );
      display->setFont(MID_FONT);
      display->setTextAlignment(TEXT_ALIGN_LEFT);
      offsetX = offsetX + 4;
      display->drawString(offsetX + x-1, 29 + y, localPresetName(localPresetNum) );
      display->setFont(SMALL_FONT);
      display->drawString(offsetX + x, 10 + y, localPresetName(localPresetNum-2) );
      display->drawString(offsetX + x, 20 + y, localPresetName(localPresetNum-1) );
      display->drawString(offsetX + x, 45 + y, localPresetName(localPresetNum+1) );
      display->drawString(offsetX + x, 55 + y, localPresetName(localPresetNum+2) );
    }
    display->setColor(BLACK);
    display->fillRect(x+0, y+0, display->width()-8, 16);
    display->setFont(SMALL_FONT);
    display->setColor(INVERSE);
    display->setTextAlignment(TEXT_ALIGN_CENTER);
    for (int i=0 ; i<PEDALS_NUM; i++) {
      if (BUTTONS[i].fxState) {
        display->fillRect(x+(i*pxPerLabel+(pxPerLabel-boxWidth)/2),y,boxWidth,14);
      }
      display->drawString(x+((i+0.5)*pxPerLabel),y,(BUTTONS[i].fxLabel));
    }
  }
}

void framePresets(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  display->setFont(SMALL_FONT);
  int pxPerLabel = (display->width() - 8) / PEDALS_NUM;
  int boxWidth = display->getStringWidth("WWW");
  boxWidth = max(boxWidth,pxPerLabel-2);
  for (int i=0 ; i<PEDALS_NUM; i++) {
    if (i==localPresetNum) {
      display->fillRect(x+(i*pxPerLabel+(pxPerLabel-boxWidth)/2),y,boxWidth,14);
    } else {
      display->drawRect(x+(i*pxPerLabel+(pxPerLabel-boxWidth)/2),y,boxWidth,14);
    } 
    display->drawString(x+((i+0.5)*pxPerLabel), y, String(i+1));
  }
  display->setFont(HUGE_FONT);
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->drawString((display->width())/2 + x, 11 + y, String(localScene+1) );
}


void frameOrganize(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  //
  display->setFont(HUGE_FONT);
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->drawString((display->width())/2 + x, 11 + y, "ORG" );
}

void frameSettings(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  //
  display->setFont(HUGE_FONT);
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->drawString((display->width())/2 + x, 11 + y, "SET" );
}


void frameAbout(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  if (ampName=="") { // welcome 
    display->setFont(MID_FONT);
    display->setTextAlignment(TEXT_ALIGN_CENTER);
    display->drawString(display->width()/2 + x, 14 + y, DEVICE_NAME);
    display->drawString(display->width()/2 + x, 36 + y, VERSION);
  } else {
    display->setFont(SMALL_FONT);
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
  if(display->getStringWidth(str)>display->width()) {
    display->setFont(BIG_FONT);
  }
  display->drawString((display->width())/2 + x, 11 + y , String(str) );
} 

// array of frame drawing functions
FrameCallback frames[] = { frameBtConnect, frameEffects, framePresets, frameOrganize, frameSettings, frameAbout, frameLevel };

// number of frames in UI
int frameCount = 7;

// Overlays are statically drawn on top of a frame eg. a clock
OverlayCallback overlays[] = { screenOverlay };
int overlaysCount = 1;

// SETUP() WWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWW
void setup() { 
  Serial.begin(115200);
  DEBUG(F("Serial started")); 
  localPresetNum = 1;
  btAttempts = 0;
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
  delay(1000);
  ESP_on();
  ui.switchToFrame(MODE_ABOUT);// show welcome screen
  ui.update();

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

  // Check FS
  LITTLEFS.begin();
  if(!LITTLEFS.exists("/" + (String)(TOTAL_PRESETS-1)) || !LITTLEFS.exists("/s" + (String)(TOTAL_SCENES-1))) {
    createFolders();
  }

  spark_io.comms = &spark_comms;
  spark_comms.startBLE();

  DEBUG("Setup(): done");
}

// LOOP() WWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWW 
void loop() {
safeRecursion++;
if (safeRecursion>2) {
  safeRecursion--;
  return;
}
// Check BT Connection and (RE-)connect if needed
btConnected = spark_comms.connected();
  if (!btConnected) {
    btConnect();
    DEBUG(">>> start building catalog");
    buildPresetList();
    DEBUG(">>> finish building catalog");
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
        if (p == TMP_PRESET_ADDR)       
          p = TMP_PRESET;
        if (j == 0x01) {
          p = CUR_EDITING;
        }
        presets[p] = preset;
        updateFxStatuses();
        //dump_preset(preset);
      }

      if (cmdsub == 0x0306) {
        strcpy(presets[CUR_EDITING].effects[3].EffectName, msg.str2);
        DEBUG("Change to amp model ");
        DEBUG(presets[CUR_EDITING].effects[3].EffectName);
      }

      if (cmdsub == 0x0363) {
        DEBUG("Tap Tempo " + msg.val);
        level = msg.val * 10;
        tempFrame(MODE_LEVEL,mode,1000);
      }

      if (cmdsub == 0x0337) {
        DEBUG("Change parameter ");
        DEBUG(msg.str1 + " " + msg.param1+ " " + msg.val);
        int fxSlot = fxNumByName(msg.str1).fxSlot;
        presets[CUR_EDITING].effects[fxSlot].Parameters[msg.param1] = msg.val;
        fxCaption = spark_knobs[fxSlot][msg.param1]  ;
        if (fxSlot==5 && msg.param1==4){
          //suppress the message "BPM=10.0"
        } else {
          level = msg.val * 100;
          tempFrame(MODE_LEVEL,mode,1000);
        }
      }
      
      if (cmdsub == 0x0338) { // >0x0138 or amp button
        remotePresetNum = msg.param2;
        if (remotePresetNum<HW_PRESETS){
          localPresetNum = remotePresetNum;
          presets[CUR_EDITING] = presets[remotePresetNum];
        }
        if (remotePresetNum == TMP_PRESET_ADDR) {
          //
        }
        updateFxStatuses();
        DEBUG("Change to hw preset: " + remotePresetNum);
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
        remotePresetNum = msg.param2;
        if (remotePresetNum < HW_PRESETS) {
          localPresetNum = remotePresetNum;
          spark_io.get_preset_details(remotePresetNum);
          waitForResponse(spark_io.expectedSubcmd,1000);
          presets[CUR_EDITING] = presets[remotePresetNum];
        }
        if (remotePresetNum == 0x7f) { 

        }
        DEBUG("Store in preset: " + remotePresetNum);
        updateFxStatuses();
      }

      if (cmdsub == 0x0415) {
        updateFxStatuses();
        DEBUG("OnOff: ACK");
      }
      if (cmdsub == 0x0310) {
        remotePresetNum = msg.param2;
        j = msg.param1;
        if (remotePresetNum == 0x7f) 
          remotePresetNum = TMP_PRESET;
        if (j == 0x01) 
          remotePresetNum = CUR_EDITING;
        localPresetNum = remotePresetNum;
        presets[CUR_EDITING] = presets[remotePresetNum];
        updateFxStatuses();
        DEBUG("Hadware preset is: " + remotePresetNum);
      }
    }


    if (millis() > uiCounter ) {
      uiCounter = ui.update() + millis();
    }
    for (uint8_t i = 0; i < BUTTONS_NUM; i++) {
      buttons[i].check();
    }

    uint8_t x ;
    uint16_t s;
    x = Encoder1.read(); //fx control
    s = Encoder1.speed();
    if (x) {
      curFx = knobs_order[curKnob].fxSlot;
      curParam = knobs_order[curKnob].fxNumber;
      fxCaption = spark_knobs[curFx][curParam];
      level = presets[CUR_EDITING].effects[curFx].Parameters[curParam] * 100;
      s = s/6 + 1;
      tempFrame(MODE_LEVEL,mode,FRAME_TIMEOUT);
      if (x == DIR_CW) {
        level = level + s;
        if (level>MAX_LEVEL) level = MAX_LEVEL;
      } else {
        level = level - s;
        if (level<0) level=0;
      }
      presets[CUR_EDITING].effects[curFx].Parameters[curParam] = (float)(level)/(float)(100);
      DEBUG(presets[CUR_EDITING].effects[curFx].EffectName + " " + presets[CUR_EDITING].effects[curFx].Parameters[curParam] );
      spark_io.change_effect_parameter(presets[CUR_EDITING].effects[curFx].EffectName, curParam,  presets[CUR_EDITING].effects[curFx].Parameters[curParam]);
    }

    x = Encoder2.read(); // preset selector
    if (x) {
      if (mode==MODE_EFFECTS) handlePresets(x);
      if (mode==MODE_PRESETS) handleScenes(x);
    }
    if (millis() > idleCounter && pendingPresetNum >= 0) {
      uploadPreset(localPresetNum);
    }
  }
  if ((millis() > timeToGoBack) && tempUI) {
    returnToMainUI();
  }
  
  safeRecursion--;
}





// CUSTOM FUNCTIONS =============================================================================== 
void handleButtonEvent(ace_button::AceButton* button, uint8_t eventType, uint8_t buttonState) {
  uint8_t id = button->getId();
  if (id != 4 && eventType != ace_button::AceButton::kEventLongReleased) {
    returnToMainUI();
  } 
  DEBUG("Button: id: " + (String)id + " eventType: " + (String)eventType + "; buttonState: " + (String)buttonState );

//  uint8_t ledPin = BUTTONS[id].ledAddr;
  if (mode == MODE_LEVEL) {
    if (id==4 && eventType==ace_button::AceButton::kEventPressed) {
      curKnob++;
      if (curKnob>=knobs_number) curKnob=0;
      curFx = knobs_order[curKnob].fxSlot;
      curParam = knobs_order[curKnob].fxNumber;
      fxCaption = spark_knobs[curFx][curParam];
      level = presets[CUR_EDITING].effects[curFx].Parameters[curParam] * 100;
      timeToGoBack = millis() + FRAME_TIMEOUT;
      DEBUG(curKnob);
    }
  }
  if (eventType == ace_button::AceButton::kEventClicked) {
    if (id==5) {
      cycleMode();
    }
  }
  
  if (eventType == ace_button::AceButton::kEventLongPressed) {
    if (id==5) {
      ESP_off();
    }
  }
  if (mode==MODE_EFFECTS) {
    switch (eventType) {
      case ace_button::AceButton::kEventPressed:
        if (bypass) {
          toggleBypass();
        } else {
          if (id<PEDALS_NUM) {
            toggleEffect(BUTTONS[id].fxSlotNumber);
          }
          if (id==4) {
            tempFrame(MODE_LEVEL,mode,FRAME_TIMEOUT);
          }
        }
        break;
      case ace_button::AceButton::kEventLongPressed:
        if (id<PEDALS_NUM) {
          toggleEffect(BUTTONS[id].fxSlotNumber);
        }
        if (id==0) {
           tempFrame(MODE_ABOUT,mode,4000);
        }
        if (id == 1) {
          toggleBypass();
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
        if (id<HW_PRESETS){
          spark_io.change_hardware_preset(id);
          localPresetNum = id;
          remotePresetNum = id;
        }
        break;
      case ace_button::AceButton::kEventLongPressed:
        if (id<PEDALS_NUM) {
          savePresetToFile(presets[CUR_EDITING],"");
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
    btAttempts++;
    if (btAttempts>BT_ATTEMPTS_BEFORE_OFF) ESP_off();
    delay(50); 
    ui.update(); 
    DEBUG("Connecting... >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>");
    btConnected = spark_comms.connect_to_spark();
    if (btConnected ) {
      DEBUG("BT Connected"); 
      btAttempts = 0;     
      btCaption = "RETRIEVING..";
      greetings();
      mode = MODE_EFFECTS;
      tempFrame(MODE_ABOUT, mode, 3000);
    } else {
      ui.switchToFrame(MODE_CONNECT);
      ui.update();
      // in case of connection loss
      btConnected = false;
      serialNum = "";
      firmwareVer = "";
      ampName = "";
      DEBUG("BT NOT Connected: " + btAttempts);
    }
  }
}

void tempFrame(e_mode tempFrame, e_mode retFrame, const ulong msTimeout) {
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
    curKnob = 4;
  }
}

void dump_preset(SparkPreset preset) {
  int i,j;
  DEBUG(">===========================================================");
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
bool waitForResponse(unsigned int subcmd=0, ulong msTimeout=1000) {
  DEBUG("Wait for response " + String(subcmd, HEX) + " " + msTimeout + "ms");
  waitSubcmd = subcmd;
  if (subcmd==0x0000) { 
    stillWaiting=false;
  } else {
    stillWaiting=true;
    waitCounter = msTimeout+millis();
  }
  while (stillWaiting && millis()<waitCounter) {  loop(); }
  if (stillWaiting) { 
    DEBUG("No Response! TIMED OUT!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
    return false;
  } else {
    return true;
  }
}

void stopWaiting() {
  //normal case: feedback from the amp
  stillWaiting = false;
}

void updateFxStatuses() {
  for (int i=0 ; i< PEDALS_NUM; i++) {
    BUTTONS[i].fxState = presets[CUR_EDITING].effects[BUTTONS[i].fxSlotNumber].OnOff;
  }
}

void greetings() {
  spark_io.get_name();
  waitForResponse(spark_io.expectedSubcmd,2000);
  spark_io.greeting();
  spark_io.get_serial();
  waitForResponse(spark_io.expectedSubcmd,2000);
  spark_io.get_preset_details(0x0000);
  waitForResponse(spark_io.expectedSubcmd,2000);
  spark_io.get_preset_details(0x0001);
  waitForResponse(spark_io.expectedSubcmd,2000);
  spark_io.get_preset_details(0x0002);
  waitForResponse(spark_io.expectedSubcmd,2000);
  spark_io.get_preset_details(0x0003);
  waitForResponse(spark_io.expectedSubcmd,2000);
  spark_io.get_hardware_preset_number();
  waitForResponse(spark_io.expectedSubcmd,2000);
  spark_io.get_firmware_ver();
  waitForResponse(spark_io.expectedSubcmd,2000);
  spark_io.get_preset_details(0x0100);
  if (!waitForResponse(spark_io.expectedSubcmd,2000)) {
    // Spark might have been stuck
  };
}

char* localPresetName(int localNum) {
  if (localNum > TOTAL_PRESETS-1) { localNum = localNum - (TOTAL_PRESETS) ;}
  if (localNum < 0) {localNum = localNum + TOTAL_PRESETS ;}
  if (localNum<HW_PRESETS) {
    return presets[localNum].Name;
  } else {
    return flashPresets[localNum-HW_PRESETS].Name;
  }
}

s_fx_coords fxNumByName(const char* fxName) {
  int i = 0;
  int j = 3; //3: amp is most often in use 
  for (const auto &fx: spark_amps) {
    if (strcmp(fx, fxName)==0){
      return {j,i};
    }
    i++;
  }
  for (const auto &fx: spark_amps_addon) {
    if (strcmp(fx, fxName)==0){
      return {j,i};
    }
    i++;
  }

  i = 0;
  j = 4; //4: modulation 
  for (const auto &fx: spark_modulations) {
    if (strcmp(fx, fxName)==0){
      return {j,i};
    }
    i++;
  }
  for (const auto &fx: spark_modulations_addon) {
    if (strcmp(fx, fxName)==0){
      return {j,i};
    }
    i++;
  }

  i = 0;
  j = 5; // 5: delay
  for (const auto &fx: spark_delays) {
    if (strcmp(fx, fxName)==0){
      return {j,i};
    }
    i++;
  }
  
  i = 0;
  j = 6; // 6: reverb
  for (const auto &fx: spark_reverbs) {
    if (strcmp(fx, fxName)==0){
      return {j,i};
    }
    i++;
  }

  i = 0;
  j = 2; //2: drive
  for (const auto &fx: spark_drives) {
    if (strcmp(fx, fxName)==0){
      return {j,i};
    }
    i++;
  }
  for (const auto &fx: spark_drives_addon) {
    if (strcmp(fx, fxName)==0){
      return {j,i};
    }
    i++;
  }

  i = 0;
  j = 1; // 1: compressor
  for (const auto &fx: spark_compressors) {
    if (strcmp(fx, fxName)==0){
      return {j,i};
    }
    i++;
  }

  i = 0;
  j = 0; //0: noise gate
  for (const auto &fx: spark_compressors) {
    if (strcmp(fx, fxName)==0){
      return {j,i};
    }
    i++;
  }
  return {-1,-1};
}

void setPendingPreset(int presetNum) {
    pendingPresetNum = presetNum;
    idleCounter = millis() + 600 ; // let's make some idle check before sending the preset to the amp
}

void uploadPreset(int presetNum) {
  localPresetNum = presetNum;
  if (presetNum < HW_PRESETS ) {
    spark_io.change_hardware_preset(presetNum);
    remotePresetNum = presetNum;
    presets[CUR_EDITING] = presets[presetNum];
  } else {
    remotePresetNum = TMP_PRESET_ADDR;
    // upload presets from ESP32's filesystem to TMP_PRESET_ADDR slot
    // TODO read preset #localPresetNum from LITTLEFS
 //   preset = *my_presets[presetNum-HW_PRESETS];
    preset = flashPresets[presetNum-HW_PRESETS];
    DEBUG(">>>>>uploading '" + preset.Name + "' to 0x007f");
    // change preset.number to 0x007f
    preset.preset_num = TMP_PRESET_ADDR;
    presets[TMP_PRESET] = preset;
    presets[CUR_EDITING] = preset;
    // create_preset on amp
    spark_io.create_preset(&preset);
    // make it active
    spark_io.change_hardware_preset(TMP_PRESET_ADDR);
  }
  updateFxStatuses();
  pendingPresetNum = -1;
}

void toggleBypass() {
  if (bypass) {
    for (int i=0; i<=6; i++){
      presets[CUR_EDITING].effects[i].OnOff = fxState[i];
      spark_io.turn_effect_onoff(presets[CUR_EDITING].effects[i].EffectName ,fxState[i]);
    }
  } else {    
    for (int i=0; i<=6; i++){
      fxState[i] = presets[CUR_EDITING].effects[i].OnOff;
      spark_io.turn_effect_onoff(presets[CUR_EDITING].effects[i].EffectName ,false);
    }
  }
  bypass = !bypass;
}

void handlePresets(int x) {
  scroller = 0;
  scrollStep = -abs(scrollStep);
  returnToMainUI();
  if (x == DIR_CW) {
    localPresetNum++;
    if (localPresetNum>TOTAL_PRESETS-1) localPresetNum = 0;
  } else {
    localPresetNum--;
    if (localPresetNum<0) localPresetNum=TOTAL_PRESETS-1;
  }
  if (localPresetNum < HW_PRESETS ) {
    remotePresetNum = localPresetNum;
  } else {
    remotePresetNum = TMP_PRESET_ADDR;
  }
  setPendingPreset(localPresetNum);
  DEBUG("Pending preset: " + localPresetNum + " to " + String(remotePresetNum,HEX));
  updateFxStatuses();
}

void handleScenes(int x) {
  returnToMainUI();
  if (x == DIR_CW) {
    localScene++;
    if (localScene>TOTAL_SCENES-1) localScene = 0;
  } else {
    localScene--;
    if (localScene<0) localScene=TOTAL_SCENES-1;
  }
  //setPendingScene(localScene);
  DEBUG("Pending scene: " + localScene);
  updateFxStatuses();
}

void toggleEffect(int slotNum) {
  spark_io.turn_effect_onoff(presets[CUR_EDITING].effects[slotNum].EffectName, !presets[CUR_EDITING].effects[slotNum].OnOff);
  presets[CUR_EDITING].effects[slotNum].OnOff = !presets[CUR_EDITING].effects[slotNum].OnOff;
  DEBUG(String(presets[CUR_EDITING].effects[slotNum].EffectName));
  updateFxStatuses();
}

void cycleMode(){
  switch (mode)
  {
  case MODE_LEVEL:
  case MODE_ABOUT:
  case MODE_EFFECTS:
    mode=MODE_PRESETS;
    break;
  case MODE_PRESETS:
    mode=MODE_ORGANIZE;
    break;
  case MODE_ORGANIZE:
    mode=MODE_SETTINGS;
    break;
  case MODE_SETTINGS:
    mode=MODE_EFFECTS;
    break;
  default:
    mode=MODE_EFFECTS;
    break;
  }
  ui.transitionToFrame(mode);
  ui.update();
}

bool createFolders() {
  bool noErr = true;
  for (int i=HW_PRESETS; i<TOTAL_PRESETS;i++) {
    if (!LITTLEFS.exists("/"+(String)i)) {
      noErr = noErr && LITTLEFS.mkdir("/"+(String)i);
    }
  }
  for (int i=0; i<TOTAL_SCENES;i++) {
    if (!LITTLEFS.exists("/s"+(String)i)) {
      noErr = noErr && LITTLEFS.mkdir("/s"+(String)i);
    }
  }
  return noErr;
}

SparkPreset somePreset(const char* substTitle) {
  SparkPreset ret_preset = *my_presets[random(HARD_PRESETS-1)];
  strcpy(ret_preset.Description, ret_preset.Name);
  strcpy(ret_preset.Name, substTitle);
  return ret_preset;
}

// load preset from json file in the format used by PG cloud back-up
SparkPreset loadPresetFromFile(int presetSlot) {
  SparkPreset retPreset;
  File presetFile;
  // open dir bound to the slot number
  String dirName =  "/" + (String)(presetSlot) ;
  String fileName = "";
  if (!LITTLEFS.exists(dirName)) {
    return somePreset("(No Such Slot)");
  } else {
    File dir = LITTLEFS.open(dirName);
    while (!fileName.endsWith(".json")) {
      presetFile = dir.openNextFile();
      if (!presetFile) {
        // no preset found in current slot directory, let's substitute a random one
        DEBUG(">>>> '" + dirName + "' Empty Slot < Random");
        return somePreset("(Empty Slot)");
      }
      fileName = presetFile.name();
      DEBUG(">>>>>>>>>>>>>>>>>> '" + fileName + "'");
    }
    dir.close();
    DynamicJsonDocument doc(3072);
    DeserializationError error = deserializeJson(doc, presetFile);
    if (error) {
      return somePreset("(Invalid json file)");
    } else {
      if (doc["type"] == "jamup_speaker") { // PG app's json
        retPreset.BPM = doc["bpm"];
        JsonObject meta = doc["meta"];
        strcpy(retPreset.Name, meta["name"]);
        strcpy(retPreset.Description, meta["description"]);
        strcpy(retPreset.Version, meta["version"]);
        strcpy(retPreset.Icon, meta["icon"]);
        strcpy(retPreset.UUID, meta["id"]);
        JsonArray sigpath = doc["sigpath"];
        for (int i=0; i<=6; i++) { // effects
          int numParams = 0;
          double value;
          JsonObject fx = sigpath[i];
          for (JsonObject elem : fx["params"].as<JsonArray>()) {
            // <-----> PG format sometimes uses double, and sometimes bool as char[]
            if ( elem["value"].is<bool>() ) {
              if (elem["value"]) {
                value = 0.5;
              } else {
                value = 0;
              }
            } else { // let's hope they don't invent some other type
              value = elem["value"]; 
            }
            int index = elem["index"];
            retPreset.effects[i].Parameters[index] = value;
            numParams = max(numParams,index);
          }
          retPreset.effects[i].NumParameters = numParams+1;
          strcpy(retPreset.effects[i].EffectName , fx["dspId"]);
          retPreset.effects[i].OnOff = fx["active"];
          retPreset.preset_num = 0;
          retPreset.curr_preset = 0;
        }   
      }
    }
  }
  presetFile.close();
  return retPreset;
}

// save preset to json file in the format used by PG cloud back-up
bool savePresetToFile(SparkPreset savedPreset, const String &filePath) {
  bool noErr = true;
  if(strcmp(savedPreset.Name,"(Empty Slot)")==0){
    strcpy(savedPreset.Name,savedPreset.Description);
  }
  DynamicJsonDocument doc(3072);
  doc["type"] = "jamup_speaker";
  doc["bpm"] = savedPreset.BPM;
  JsonObject meta = doc.createNestedObject("meta");
  meta["id"] = savedPreset.UUID;
  meta["version"] = savedPreset.Version;
  meta["icon"] = savedPreset.Icon;
  meta["name"] = savedPreset.Name;
  meta["description"] = savedPreset.Description;
  JsonArray sigpath = doc.createNestedArray("sigpath");
  for (int i=0; i<7; i++){
    for (int j=0; j<savedPreset.effects[i].NumParameters; j++) {
      sigpath[i]["params"][j]["index"] = j;
      sigpath[i]["params"][j]["value"] = savedPreset.effects[i].Parameters[j];
    }
    sigpath[i]["type"] = "speaker_fx";
    sigpath[i]["dspId"] = savedPreset.effects[i].EffectName;
    sigpath[i]["active"] = savedPreset.effects[i].OnOff;
  }
  serializeJson(doc,Serial);
  return noErr;
}

void textAnimation(const String &s, ulong msDelay, int yShift=0, bool show=true) {  
    display.clear();
    display.drawString(display.width()/2, display.height()/2-6 + yShift, s);
    if (show) {
      display.display();
      delay(msDelay);
    }
}

void ESP_off(){
  //  CRT-off effect =) or something
  String s = "_________________";
  display.clear();
  display.display();
  display.setFont(MID_FONT);
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  for (int i=0; i<8; i++) {
    s = s.substring(i);
    textAnimation(s,70,-6);
  }
  for (int i=0; i<3; i++) {
    textAnimation("\\",30);
    textAnimation("|",30);
    textAnimation("/",30);
    textAnimation("--",30);
  }
  textAnimation("...z-Z-Z",1000);
  // hopefully displayOff() saves energy
  display.displayOff();
  DEBUG("deep sleep");
  //  Only GPIOs which have RTC functionality can be used: 0,2,4,12-15,25-27,32-39
  esp_sleep_enable_ext0_wakeup( static_cast <gpio_num_t> (ENCODER2_SW), LOW);
  esp_deep_sleep_start() ;
};

void ESP_on () {
  display.setFont(MID_FONT);
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setContrast(100);
  textAnimation(".",200,-4);
  textAnimation("*",100,5);
  textAnimation("X",100,2);
  textAnimation("-}|{-",100);
  textAnimation("- -X- -",100,2);
  textAnimation("x",100,0);
  textAnimation(".",200,-4);
}

void updatePresetList(uint8_t numPreset) {
    preset = loadPresetFromFile(numPreset);
    flashPresets[numPreset-HW_PRESETS] = preset;
}

// catalogue presets stored on-board
void buildPresetList() {
  for (int i=HW_PRESETS; i<TOTAL_PRESETS; i++){
    updatePresetList(i);
    //dump_preset(preset);
  }
}
