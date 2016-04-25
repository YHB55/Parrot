
#include <math.h>
#include <UTFT.h>
#include <DallasTemperature.h>	// library for Dallas Temperature
#include <OneWire.h>            // library for 1-Wire    
#include <Wire.h>               // library for Wire (I2C)
#include <Flash.h>
#include <UTFT_Geometry.h>
#include <toneAC.h>
#include "MyBoard.h"   		// File with My Personal Board definitions
#include "TtoABV.h"

/*-----( Definitions )-----*/
#define CHECK_ALARM_CONDITIONS_EVERY 50
#define READ_SENSORS_EVERY 400
#define TEMPERATURE_PRECISION 12
#define READ_ROTARY_EVERY 100
#define UPDATE_DISPLAY_EVERY 500
#define DISPLAY_TRANSITION_DELAY 1500
#define BLINK_DELAY 1000
#define OFF 0
#define BLINK_ONLY 1
#define BLINK_AND_BEEP 2
#define BEEPER_FREQUENCY 700
#define MAX_BLINKS 2

/*-----( Declare constants )-----*/
const float currentPressure = 1013.25;  // No BMP180 sensor

/*-----( Declare objects )-----*/
// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);

// Pass our oneWire reference to Dallas Temperature.
DallasTemperature sensors(&oneWire);

// See the tutorial on how to obtain these addresses:
// http://arduino-info.wikispaces.com/Brick-Temperature-DS18B20#Read%20individual
DeviceAddress vaporProbe = { 0x28, 0x33, 0x47, 0x1E, 0x07, 0x00, 0x00, 0x45 }; // Front
DeviceAddress boilerProbe = { 0x28, 0xE3, 0xD7, 0x1D, 0x07, 0x00, 0x00, 0xBE }; // Back
DeviceAddress productCondensorInletProbe  = { 0x28, 0x9C, 0xE8, 0x1C, 0x07, 0x00, 0x00, 0x66 }; // Front
DeviceAddress productCondensorOutletProbe = { 0x28, 0x7F, 0x5E, 0x1C, 0x07, 0x00, 0x00, 0xC5 };

UTFT myGLCD(SSD1289, 38, 39, 40, 41);
UTFT_Geometry geo(&myGLCD);

/*-----( Declare variables )-----*/
float boilerTempC;
float boilerABV;
float vaporTempC;
float vaporABV;
float PCTempIn;
float PCTempOut;

int currentRotary = -1;
int previousRotary = -1;
bool shutDown = false;

unsigned long lastAlarmConditionsCheck;
unsigned long lastSensorRead;
unsigned long lastRotaryRead;
unsigned long lastDisplayUpdate;
unsigned long lastBlink;

int currentScreen = -2;
int previousScreen = -3;

int blinkAndBeepMode = OFF;
int blinkOutput = false;
int beepOutput = false;
int blinkCount = 0;

bool alarmCondition = false;

// Declare which fonts we will be using
extern uint8_t SmallFont[];
extern uint8_t BigFont[];
extern uint8_t SevenSegNumFont[];
extern uint8_t SevenSegNumFontPlusPlus[];
extern uint8_t GroteskBold16x32[];
extern uint8_t Retro8x16[];

void setup() {

  Serial.begin(115200);

  // Initialise rotary pin values
  initRotary();

  // Initialise sensors
  sensors.begin();
  sensors.setResolution(vaporProbe, TEMPERATURE_PRECISION);
  sensors.setResolution(boilerProbe, TEMPERATURE_PRECISION);
  sensors.setResolution(productCondensorInletProbe, TEMPERATURE_PRECISION);
  sensors.setResolution(productCondensorOutletProbe, TEMPERATURE_PRECISION);

  // Automatic startup
  autoStartup();

  // Initialise LCD
  myGLCD.InitLCD();
  myGLCD.clrScr ();

  // Display introduction screen;
  intro();

  //Initialise asynchronous function variables
  unsigned long now = millis();
  lastAlarmConditionsCheck = now;
  lastSensorRead = now;
  lastRotaryRead = now;
  lastDisplayUpdate = now;
  lastBlink = now;

}

void doFunctionAtInterval(void (*callBackFunction)(), unsigned long *lastEvent, unsigned long Interval) {

  unsigned long now = millis();

  if ((now - *lastEvent) >= Interval) {
    *lastEvent = now;
    callBackFunction();
  }

}

void setBlinkMode(int mode) {

  blinkAndBeepMode = mode;
  blinkCount = 0;

}

void loop() {

  doFunctionAtInterval(checkAlarmConditions, &lastAlarmConditionsCheck, CHECK_ALARM_CONDITIONS_EVERY); // check alarm conditions

  if (!alarmCondition) {
    doFunctionAtInterval(readSensors, &lastSensorRead, READ_SENSORS_EVERY);  // read the sensors
    if (boilerTempC < 78) {
      currentScreen = -2; //Warmup Screen1
    }
    else if (boilerTempC < 82) {
      currentScreen = -1; //Warmup Screen2 check coolant
    }
    // Boiler above 82 but column below 40
    else if (vaporTempC < 30) {
      currentScreen = 0; //Warmup Screen2 distillation iminent
    }
    else {
      doFunctionAtInterval(readRotary, &lastRotaryRead, READ_ROTARY_EVERY); // read rotary switch
      currentScreen = currentRotary;
    }
  }

  if ((currentScreen != previousScreen) && (currentScreen <= 0)) {
    setBlinkAndBeep();
  }

  doFunctionAtInterval(writeDisplay, &lastDisplayUpdate, UPDATE_DISPLAY_EVERY); // write display every 1/2 second
  doFunctionAtInterval(blinkAndBeep, &lastBlink, BLINK_DELAY);  // blink and/or beep if necessary

}

void writeDisplay() {

  if (currentScreen != previousScreen) {
    // call these functions only when the screen changes
    writeDisplayBackground();
    writeScreenFixed(currentScreen);
    previousScreen = currentScreen;
  }
  // call these functions every time
  writeScreenDynamic(currentScreen);

}

void writeDisplayBackground() {

  // Write code to clear and fill the background of the display
  myGLCD.fillScr (VGA_YELLOW);
  myGLCD.setBackColor(VGA_TRANSPARENT);

}

void writeScreenFixed(int screen) {

  // Write code to draw shapes and 'non-updating' display text
  switch (screen) {

    case -4:
      // End Of Run
      myGLCD.setColor(VGA_BLACK);
      myGLCD.setBackColor(VGA_YELLOW);
      myGLCD.fillRoundRect(180, 5, 300, 235);
      myGLCD.setFont(GroteskBold16x32);
      myGLCD.print("Degrees C", 60, 55, 90);
      break;

    case -3:
      // End Of Run
      myGLCD.setColor(VGA_BLACK);
      myGLCD.setBackColor(VGA_YELLOW);
      myGLCD.fillRoundRect(180, 5, 300, 235);
      myGLCD.setFont(GroteskBold16x32);
      myGLCD.print("Degrees C", 60, 55, 90);
      break;
    case -2:
      // Warmup Screen1
      myGLCD.setColor(VGA_BLACK);
      myGLCD.setBackColor(VGA_YELLOW);
      myGLCD.fillRoundRect(180, 5, 300, 235);
      myGLCD.setFont(GroteskBold16x32);
      myGLCD.print("Degrees C", 60, 55, 90);
      break;
    case -1:
      // Warmup Screen2
      myGLCD.setColor(VGA_BLACK);
      myGLCD.setBackColor(VGA_YELLOW);
      myGLCD.fillRoundRect(180, 5, 300, 235);
      myGLCD.setFont(GroteskBold16x32);
      myGLCD.print("Degrees C", 160, 55, 90);
      break;
    case 0:
      // Warmup Screen3
      myGLCD.setColor(VGA_BLACK);
      myGLCD.setBackColor(VGA_YELLOW);
      myGLCD.fillRoundRect(180, 5, 300, 235);
      myGLCD.setFont(GroteskBold16x32);
      myGLCD.print("Degrees C", 60, 55, 90);
      break;
    case 1:
      // Distilling Mode1
      myGLCD.setColor(VGA_BLACK);
      myGLCD.fillRoundRect(130, 10, 270, 225);
      myGLCD.setColor(VGA_AQUA);
      myGLCD.fillRoundRect(135, 15, 265, 220);
      myGLCD.setColor(VGA_BLACK);
      myGLCD.setBackColor(VGA_TRANSPARENT);
      myGLCD.setFont(GroteskBold16x32);
      myGLCD.print("% ABV", 180, 80, 90);
      break;
    case 2:
      // Distilling Mode2
      myGLCD.setColor(VGA_RED);
      myGLCD.setBackColor(VGA_YELLOW);
      myGLCD.setFont(GroteskBold16x32);
      myGLCD.print("VAPOUR", 320, 1, 90);
      myGLCD.print("BOILER", 150, 1, 90);
      myGLCD.setColor(VGA_BLUE);
      myGLCD.setFont(BigFont);
      myGLCD.print("Degrees C", 260, 45, 90);
      myGLCD.print("Degrees C", 90, 45, 90);
      myGLCD.setColor(VGA_BLACK);
      myGLCD.print("Percentage ABV", 190, 10, 90);
      myGLCD.print("Percentage ABV", 20, 10, 90);
      break;
    case 3:
      // Distilling Mode3
      myGLCD.fillScr(VGA_YELLOW);
      //Draw Boiler
      myGLCD.setColor(VGA_BLUE);
      myGLCD.fillRect(20, 20, 130, 120);
      //Draw bucket
      myGLCD.fillRect(20, 127, 100, 130);
      myGLCD.fillRect(20, 210, 100, 213);
      myGLCD.fillRect(20, 130, 23, 210);
      //Fill bucket
      myGLCD.setColor(VGA_RED);
      myGLCD.fillRect(23, 130, 80, 210);
      //Draw Riser
      myGLCD.setColor(VGA_BLUE);
      myGLCD.fillRect(130, 73, 148, 84);
      myGLCD.fillCircle(225, 23, 23);
      myGLCD.setColor(VGA_YELLOW);
      myGLCD.fillCircle(225, 23, 11);
      myGLCD.setColor(VGA_YELLOW);
      geo.fillTriangle(204, 1, 250, 59, 190, 56);
      myGLCD.setColor(VGA_BLUE);
      geo.fillTriangle(144, 73, 210, 5, 150, 85);
      geo.fillTriangle(150, 85, 210, 5, 218, 14);
      geo.fillTriangle(232, 32, 223, 58, 216, 48);
      geo.fillTriangle(232, 32, 223, 58, 239, 41);
      geo.fillTriangle(217, 52, 113, 163, 221, 56);
      geo.fillTriangle(217, 52, 113, 163, 110, 159);
      geo.fillTriangle(213, 68, 117, 144, 203, 59);
      geo.fillTriangle(213, 68, 117, 144, 130, 153);
      geo.fillTriangle(203, 74, 219, 82, 208, 71);
      geo.fillTriangle(203, 74, 219, 82, 215, 85);
      geo.fillTriangle(131, 148, 148, 156, 136, 143);
      geo.fillTriangle(131, 148, 148, 156, 143, 160);
      myGLCD.setFont(BigFont);
      myGLCD.setColor(VGA_YELLOW);
      myGLCD.setBackColor(VGA_BLUE);
      myGLCD.print("Deg C", 90, 35, 90);
      myGLCD.print("% ABV", 40, 26, 90);
      myGLCD.setBackColor(VGA_RED);
      myGLCD.print("% ABV", 40, 130, 90);
      myGLCD.setFont(BigFont);
      myGLCD.setColor(VGA_RED);
      myGLCD.setBackColor(VGA_YELLOW);
      myGLCD.print("T(in)", 160, 150, 90);
      myGLCD.print("T(out)", 230, 70, 90);
      myGLCD.print("T(diff)", 195, 110, 90);
      myGLCD.setFont(BigFont);
      myGLCD.setColor(VGA_BLUE);
      myGLCD.print("Deg C", 285, 15, 90);
      break;
    case 4:
      // Distilling Mode4
      myGLCD.fillScr(VGA_YELLOW);
      //Draw Boiler
      myGLCD.setColor(VGA_BLUE);
      myGLCD.fillRect(20, 15, 130, 115);
      //Draw bucket
      myGLCD.fillRect(20, 122, 100, 125);
      myGLCD.fillRect(20, 205, 100, 208);
      myGLCD.fillRect(20, 125, 23, 205);
      //Fill bucket
      myGLCD.setColor(VGA_RED);
      myGLCD.fillRect(23, 125, 80, 205);
      //Draw Riser
      myGLCD.setColor(VGA_BLUE);
      myGLCD.fillRect(130, 58, 255, 69);
      myGLCD.fillRect(130, 141, 255, 152);
      myGLCD.fillCircle(255, 105, 47);
      myGLCD.setColor(VGA_YELLOW);
      myGLCD.fillCircle(255, 105, 36);
      myGLCD.fillRect(200, 69, 255, 141);
      //Draw Condenser
      myGLCD.setColor(VGA_BLUE);
      myGLCD.fillRect(150, 131, 255, 162);
      myGLCD.fillRect(160, 162, 165, 172);
      myGLCD.fillRect(240, 162, 245, 172);
      myGLCD.setFont(BigFont);
      myGLCD.setColor(VGA_YELLOW);
      myGLCD.setBackColor(VGA_BLUE);
      myGLCD.print("Deg", 90, 35, 90);
      myGLCD.print("% ABV", 40, 26, 90);
      myGLCD.setBackColor(VGA_RED);
      myGLCD.print("% ABV", 40, 128, 90);
      myGLCD.setFont(BigFont);
      myGLCD.setColor(VGA_RED);
      myGLCD.setBackColor(VGA_YELLOW);
      myGLCD.print("in", 125, 170, 90);
      myGLCD.print("out", 270, 170, 90);
      myGLCD.print("dif", 200, 170, 90);
      myGLCD.setFont(BigFont);
      myGLCD.setColor(VGA_BLUE);
      myGLCD.print("Deg", 280, 12, 90);
      myGLCD.setFont(GroteskBold16x32);
      myGLCD.setColor(VGA_RED);
      myGLCD.setBackColor(VGA_YELLOW);
      myGLCD.print("<-", 177, 180, 90);
      myGLCD.print("->", 257, 180, 90);
      break;
    case 5:
      // Distilling Mode5
      myGLCD.fillScr(VGA_YELLOW);
      //Draw Boiler
      myGLCD.setColor(VGA_BLUE);
      myGLCD.fillRect(5, 15, 70, 115);
      //Draw bucket
      myGLCD.fillRect(10, 122, 90, 125);
      myGLCD.fillRect(10, 205, 90, 208);
      myGLCD.fillRect(10, 125, 13, 205);
      //Fill bucket
      myGLCD.setColor(VGA_RED);
      myGLCD.fillRect(13, 125, 70, 205);
      //Draw Riser
      myGLCD.setColor(VGA_BLUE);
      myGLCD.fillCircle(270, 115, 47);
      myGLCD.setColor(VGA_YELLOW);
      myGLCD.fillCircle(270, 115, 36);
      myGLCD.fillRect(190, 79, 270, 151);
      myGLCD.setColor(VGA_BLUE);
      myGLCD.fillRect(70, 62, 270, 85);
      myGLCD.fillRect(120, 151, 270, 162);
      //Draw Condenser
      myGLCD.setColor(VGA_BLUE);
      myGLCD.fillRect(140, 141, 245, 172);
      myGLCD.fillRect(150, 172, 155, 182);
      myGLCD.fillRect(230, 172, 235, 182);
      myGLCD.setFont(BigFont);
      myGLCD.setColor(VGA_YELLOW);
      myGLCD.setBackColor(VGA_BLUE);
      myGLCD.print("% ABV", 30, 26, 90);
      myGLCD.setBackColor(VGA_RED);
      myGLCD.print("% ABV", 30, 128, 90);
      myGLCD.setFont(BigFont);
      myGLCD.setColor(VGA_RED);
      myGLCD.setBackColor(VGA_YELLOW);
      myGLCD.print("in", 120, 180, 90);
      myGLCD.print("out", 270, 180, 90);
      myGLCD.print("dif", 180, 180, 90);
      myGLCD.setFont(BigFont);
      myGLCD.setColor(VGA_BLUE);
      myGLCD.setFont(GroteskBold16x32);
      myGLCD.setColor(VGA_RED);
      myGLCD.setBackColor(VGA_YELLOW);
      myGLCD.print("<-", 167, 190, 90);
      myGLCD.print("->", 247, 190, 90);
      //Draw tees
      myGLCD.setColor(VGA_BLACK);
      myGLCD.fillCircle(90, 73, 18 );
      myGLCD.fillCircle(130, 73, 18 );
      myGLCD.fillCircle(170, 73, 18 );
      myGLCD.fillCircle(210, 73, 18 );
      myGLCD.setColor(VGA_AQUA);
      myGLCD.fillCircle(90, 73, 10 );
      myGLCD.fillCircle(130, 73, 10 );
      myGLCD.fillCircle(170, 73, 10 );
      myGLCD.fillCircle(210, 73, 10 );
      myGLCD.setColor(VGA_BLUE);
      myGLCD.fillRect(230, 50, 235, 65);
      myGLCD.fillRect(260, 50, 265, 65);
      myGLCD.setColor(VGA_RED);
      myGLCD.setBackColor(VGA_YELLOW);
      myGLCD.print("<-", 247, 10, 90);
      myGLCD.print("->", 277, 10, 90);
      myGLCD.setFont(BigFont);
      myGLCD.print("%", 95, 40, 90);
      myGLCD.print("%", 135, 40, 90);
      myGLCD.print("%", 175, 40, 90);
      myGLCD.print("%", 215, 40, 90);
      myGLCD.setColor(VGA_YELLOW);
      myGLCD.setBackColor(VGA_BLUE);
      myGLCD.setFont(BigFont);
      myGLCD.setColor(VGA_BLACK);
      myGLCD.setFont(GroteskBold16x32);

      break;
    case 6:
      // Distilling Mode6
      myGLCD.fillScr(VGA_YELLOW);
      //Draw Boiler
      myGLCD.setColor(VGA_BLUE);
      myGLCD.setFont(BigFont);
      myGLCD.setBackColor(VGA_YELLOW);
      myGLCD.print("BOILER", 100, 10, 90);
      myGLCD.print("PLATE 1", 120, 10, 90);
      myGLCD.print("PLATE 2", 140, 10, 90);
      myGLCD.print("PLATE 3", 160, 10, 90);
      myGLCD.print("PLATE 4", 180, 10, 90);
      myGLCD.print("PLATE 5", 200, 10, 90);
      myGLCD.print("PLATE 6", 220, 10, 90);
      myGLCD.print("PLATE 7", 240, 10, 90);
      myGLCD.print("PLATE 8", 260, 10, 90);
      myGLCD.print("OUTPUT", 280, 10, 90);

      myGLCD.print("%", 100, 210, 90);
      myGLCD.print("%", 120, 210, 90);
      myGLCD.print("%", 140, 210, 90);
      myGLCD.print("%", 160, 210, 90);
      myGLCD.print("%", 180, 210, 90);
      myGLCD.print("%", 200, 210, 90);
      myGLCD.print("%", 220, 210, 90);
      myGLCD.print("%", 240, 210, 90);
      myGLCD.print("%", 260, 210, 90);
      myGLCD.print("%", 280, 210, 90);

      myGLCD.print("PROBE", 310, 10, 90);
      myGLCD.print("%ABV", 310, 150, 90);
      myGLCD.print("T Diff", 70, 80, 90);
      myGLCD.print("Deflag", 50, 20, 90);
      myGLCD.print("P.C.", 50, 150, 90);
      break;
    case 7:
      // Distilling Mode7
      myGLCD.fillScr(VGA_YELLOW);
      myGLCD.setColor(VGA_BLACK);
      myGLCD.fillRect(210, 30, 310, 210);
      myGLCD.setColor(VGA_FUCHSIA);
      myGLCD.fillRect(215, 35, 305, 205);

      myGLCD.setColor(VGA_BLACK);
      myGLCD.setFont(BigFont);
      myGLCD.setBackColor(VGA_FUCHSIA);


      myGLCD.print("Adjust", 290, 75, 90);
      myGLCD.print("Product", 270, 70, 90);
      myGLCD.print("Condenser", 250, 50, 90);

      myGLCD.setColor(VGA_BLACK);
      myGLCD.setFont(BigFont);
      myGLCD.setBackColor(VGA_YELLOW);

      myGLCD.setFont(BigFont);
      myGLCD.print("T(in)  Deg C", 200, 20, 90);
      myGLCD.print("T(out) Deg C", 140, 20, 90);
      myGLCD.print("T(diff)Deg C", 80, 20, 90);

      break;

    case 8:
      // Distilling Mode8
      myGLCD.fillScr(VGA_YELLOW);
      myGLCD.setColor(VGA_BLACK);
      myGLCD.fillRect(210, 30, 310, 210);
      myGLCD.setColor(VGA_AQUA);
      myGLCD.fillRect(215, 35, 305, 205);

      myGLCD.setColor(VGA_BLACK);
      myGLCD.setFont(BigFont);
      myGLCD.setBackColor(VGA_AQUA);


      myGLCD.print("Adjust", 290, 75, 90);
      myGLCD.print("Dephlag", 270, 70, 90);
      myGLCD.print("Cooling", 250, 70, 90);

      myGLCD.setColor(VGA_BLACK);
      myGLCD.setFont(BigFont);
      myGLCD.setBackColor(VGA_YELLOW);

      myGLCD.setFont(BigFont);
      myGLCD.print("T(in)  Deg C", 200, 20, 90);
      myGLCD.print("T(out) Deg C", 140, 20, 90);
      myGLCD.print("T(diff)Deg C", 80, 20, 90);
    default:
      // Something went wrong to get here
      break;
  }

}

void writeScreenDynamic(int screen) {

  // Write code to draw shapes and 'updating' display text, such as sensor data
  switch (screen) {
    case -4:
      // PC Overheating
      if (blinkOutput) {
        myGLCD.setColor(VGA_RED);
      } else {
        myGLCD.setColor(VGA_AQUA);
      }
      myGLCD.fillRoundRect(185, 11, 295, 230);
      myGLCD.setBackColor(VGA_TRANSPARENT);
      myGLCD.setColor(VGA_BLACK);
      myGLCD.setFont(GroteskBold16x32);
      //myGLCD.print("WARMING UP", 250, 40, 90);
      myGLCD.print("PC TOO HOT", 270, 40, 90);
      myGLCD.print("ACTION REQ'D", 230, 25, 90);
      //Display boiler temp
      myGLCD.setBackColor(VGA_YELLOW);
      myGLCD.setColor(VGA_BLACK);
      myGLCD.setFont(SevenSegNumFontPlusPlus);
      myGLCD.print(String(PCTempOut), 120, 45, 90);
    case -3:
      // End Of Run
      if (blinkOutput) {
        myGLCD.setColor(VGA_RED);
      } else {
        myGLCD.setColor(VGA_AQUA);
      }
      myGLCD.fillRoundRect(185, 11, 295, 230);
      myGLCD.setBackColor(VGA_TRANSPARENT);
      myGLCD.setColor(VGA_BLACK);
      myGLCD.setFont(GroteskBold16x32);
      myGLCD.print("END OF RUN", 270, 45, 90);
      myGLCD.print("CLOSING DOWN", 230, 30, 90);
      //Display boiler temp
      myGLCD.setBackColor(VGA_YELLOW);
      myGLCD.setColor(VGA_BLACK);
      myGLCD.setFont(SevenSegNumFontPlusPlus);
      myGLCD.print(String(boilerTempC), 120, 45, 90);
      break;
    case -2:
      // Warmup Screen1
      if (blinkOutput) {
        myGLCD.setColor(VGA_RED);
      } else {
        myGLCD.setColor(VGA_AQUA);
      }
      myGLCD.fillRoundRect(185, 11, 295, 230);
      myGLCD.setBackColor(VGA_TRANSPARENT);
      myGLCD.setColor(VGA_BLACK);
      myGLCD.setFont(GroteskBold16x32);
      myGLCD.print("WARMING UP", 250, 40, 90);
      //Display boiler temp
      myGLCD.setBackColor(VGA_YELLOW);
      myGLCD.setColor(VGA_BLACK);
      myGLCD.setFont(SevenSegNumFontPlusPlus);
      myGLCD.print(String(boilerTempC), 120, 45, 90);
      break;
    case -1:
      // Warmup Screen2
      if (blinkOutput) {
        myGLCD.setColor(VGA_RED);
      } else {
        myGLCD.setColor(VGA_AQUA);
      }
      myGLCD.fillRoundRect(185, 11, 295, 230);
      myGLCD.setBackColor(VGA_TRANSPARENT);
      myGLCD.setColor(VGA_BLACK);
      myGLCD.setFont(GroteskBold16x32);
      myGLCD.print("CHECK COOLANT", 280, 15, 90);
      myGLCD.print("SUPPLY", 240, 70, 90);
      //Display boiler temp
      myGLCD.setBackColor(VGA_YELLOW);
      myGLCD.setColor(VGA_BLACK);
      myGLCD.setFont(SevenSegNumFontPlusPlus);
      myGLCD.print(String(boilerTempC), 120, 45, 90);
      break;
    case 0:
      // Warmup Screen3
      if (blinkOutput) {
        myGLCD.setColor(VGA_RED);
      } else {
        myGLCD.setColor(VGA_AQUA);
      }
      myGLCD.fillRoundRect(185, 11, 295, 230);
      myGLCD.setBackColor(VGA_TRANSPARENT);
      myGLCD.setColor(VGA_BLACK);
      myGLCD.setFont(GroteskBold16x32);
      myGLCD.print("DISTILLATION", 280, 25, 90);
      myGLCD.print("IMMINENT", 240, 50, 90);
      //Display boiler and vapor temp
      myGLCD.setBackColor(VGA_YELLOW);
      myGLCD.setFont(SevenSegNumFontPlusPlus);
      myGLCD.print(String(boilerTempC), 120, 45, 90);
      myGLCD.print(String(vaporTempC), 1, 1, 90);
      break;
    case 1:
      // Distilling Mode1
      myGLCD.setColor(VGA_BLACK);
      myGLCD.setBackColor(VGA_TRANSPARENT);
      myGLCD.setFont(SevenSegNumFontPlusPlus);
      myGLCD.print(String( vaporABV ), 240, 35, 90);
      break;
    case 2:
      // Distilling Mode2
      myGLCD.setBackColor(VGA_YELLOW);
      myGLCD.setColor(VGA_BLUE);
      myGLCD.setFont(BigFont);
      myGLCD.print(String( vaporTempC ), 280, 75, 90);
      myGLCD.print(String( boilerTempC ), 110, 75, 90);
      myGLCD.setColor(VGA_BLACK);
      myGLCD.setFont(SevenSegNumFontPlusPlus);
      myGLCD.print(String( vaporABV ), 240, 35, 90);
      myGLCD.print(String( boilerABV ), 70, 35, 90);
      break;
    case 3:
      // Distilling Mode3
      myGLCD.setColor(VGA_YELLOW);
      myGLCD.setBackColor(VGA_BLUE);
      myGLCD.setFont(BigFont);
      myGLCD.print (String( boilerTempC ), 110, 35, 90);
      myGLCD.setColor(VGA_BLUE);
      myGLCD.setBackColor(VGA_YELLOW);
      myGLCD.print(String( vaporTempC ), 300, 15, 90);
      myGLCD.setColor(VGA_BLACK);
      myGLCD.setFont(GroteskBold16x32);
      myGLCD.setColor(VGA_YELLOW);
      myGLCD.setBackColor(VGA_RED);
      myGLCD.print(String( vaporABV ), 70, 130, 90);
      myGLCD.setColor(VGA_YELLOW);
      myGLCD.setBackColor(VGA_BLUE);
      myGLCD.print(String( boilerABV ), 70, 25, 90);
      myGLCD.setColor(VGA_RED);
      myGLCD.setFont(BigFont);
      myGLCD.setBackColor(VGA_YELLOW);
      myGLCD.print(String(PCTempIn), 245, 70, 90);
      myGLCD.print(String(PCTempOut), 175, 150, 90);
      myGLCD.print(String(PCTempIn - PCTempOut), 210, 110, 90);
      break;
    case 4:
      // Distilling Mode4
      myGLCD.setColor(VGA_YELLOW);
      myGLCD.setBackColor(VGA_BLUE);
      myGLCD.setFont(BigFont);
      myGLCD.print (String( boilerTempC ), 110, 25, 90);
      myGLCD.setColor(VGA_BLUE);
      myGLCD.setBackColor(VGA_YELLOW);
      myGLCD.print(String( vaporTempC ), 300, 5, 90);
      myGLCD.setColor(VGA_BLACK);
      myGLCD.setFont(GroteskBold16x32);
      myGLCD.setColor(VGA_YELLOW);
      myGLCD.setBackColor(VGA_RED);
      myGLCD.print(String( vaporABV ), 70, 130, 90);
      myGLCD.setColor(VGA_YELLOW);
      myGLCD.setBackColor(VGA_BLUE);
      myGLCD.print(String( boilerABV ), 70, 25, 90);
      myGLCD.setColor(VGA_RED);
      myGLCD.setFont(BigFont);
      myGLCD.setBackColor(VGA_YELLOW);
      myGLCD.print(String(PCTempIn), 285, 160, 90);
      myGLCD.print(String(PCTempOut), 145, 160, 90);
      myGLCD.print(String(PCTempIn - PCTempOut), 220, 160, 90);
      break;
    case 5:
      // Distilling Mode5
      myGLCD.setFont(GroteskBold16x32);
      myGLCD.setColor(VGA_YELLOW);
      myGLCD.setBackColor(VGA_RED);
      myGLCD.print(String( vaporABV ), 70, 130, 90);
      myGLCD.setColor(VGA_YELLOW);
      myGLCD.setBackColor(VGA_BLUE);
      myGLCD.print(String( boilerABV ), 70, 25, 90);
      myGLCD.setColor(VGA_RED);
      myGLCD.setFont(BigFont);
      myGLCD.setBackColor(VGA_YELLOW);
      myGLCD.print(String(PCTempIn - PCTempOut), 200, 160, 90);
      break;
    case 6:
      // Distilling Mode6
      myGLCD.setFont(BigFont);
      myGLCD.setColor(VGA_BLACK);
      myGLCD.setBackColor(VGA_YELLOW);
      myGLCD.print(String( vaporABV ), 280, 140, 90);
      myGLCD.setColor(VGA_YELLOW);
      myGLCD.setBackColor(VGA_BLUE);
      myGLCD.print(String( boilerABV ), 100, 140, 90);
      myGLCD.setColor(VGA_RED);
      myGLCD.setFont(BigFont);
      myGLCD.setBackColor(VGA_YELLOW);
      myGLCD.print(String(PCTempIn - PCTempOut), 20, 140, 90);
      break;
    case 7:
      // Distilling Mode7
      myGLCD.setFont(GroteskBold16x32);
      myGLCD.setColor(VGA_BLACK);
      myGLCD.setBackColor(VGA_YELLOW);
      myGLCD.print(String(PCTempIn), 180, 80, 90);
      myGLCD.print(String(PCTempOut), 120, 80, 90);
      myGLCD.print(String(PCTempIn - PCTempOut), 60, 80, 90);
      break;
    default:
      // Something went wrong to get here
      break;
  }

}

void initRotary() {

  for ( int i = FIRST_ROTARY_PIN; i <= LAST_ROTARY_PIN; i++) {
    pinMode( i, INPUT);
    digitalWrite( i, HIGH); // turn on internal pullup resistor
  }

}

void readRotary() {

  previousRotary = currentRotary;

  for ( int i = FIRST_ROTARY_PIN; i <= LAST_ROTARY_PIN; i++) {
    if ( digitalRead( i ) == LOW ) { // pin( i ) is selected
      currentRotary = (i - FIRST_ROTARY_PIN + 1);
    }
  }
  if ( currentRotary != previousRotary ) {
    serialDivider();
    Serial.print("Rotary value changing from "); Serial.print(previousRotary); Serial.print(" to "); Serial.println(currentRotary);
    serialDivider();
  }

}

void readSensors() {

  sensors.requestTemperatures();
  vaporTempC = sensors.getTempC(vaporProbe);
  boilerTempC = sensors.getTempC(boilerProbe);
  vaporABV = DCtoVaporABV( Raw2DCatP( sensors.getTemp(vaporProbe), currentPressure, true) );
  boilerABV = DCtoLiquidABV( Raw2DCatP(sensors.getTemp(boilerProbe), currentPressure, true) );
  PCTempIn = sensors.getTempC(productCondensorInletProbe);
  PCTempOut = sensors.getTempC(productCondensorOutletProbe);

  Serial.println("Sensor ABVs");
  serialDivider();
  Serial.print("Boiler: "); Serial.print(boilerABV); Serial.print("&   ");
  Serial.print("Vapor: "); Serial.print(vaporABV); Serial.println("%");
  serialDivider();

}

void setBlinkAndBeep() {

  blinkCount = 0;
  blinkAndBeepMode = BLINK_AND_BEEP;

}

void blinkAndBeep() {

  switch (blinkAndBeepMode) {
    case OFF:
      blinkOutput = false;
      beepOutput = false;
      break;
    case BLINK_ONLY:
      blinkOutput = !blinkOutput;
      beepOutput = false;
      break;
    case BLINK_AND_BEEP:
      blinkOutput = !blinkOutput;
      beepOutput = !beepOutput;
      break;
    default:
      // wrong mode inputed.  Let's switch off
      blinkOutput = false;
      beepOutput = false;
      break;
  }

  // write output to screen backlight and beeper here!
  if (blinkOutput) {
    blinkCount++;
  }
  if (beepOutput) {
    //toneAC(BEEPER_FREQUENCY);
    toneAC( 700, 10, 1000, 0);
  } else {
    noToneAC();
  }
  if (blinkCount > MAX_BLINKS) {
    setBlinkMode(OFF);
  }

}

void checkAlarmConditions() {

  alarmCondition = false;

  //End of run based on the temperature of the boiler.
  if ( boilerTempC > 95 ) {
    alarmCondition = true;
    currentScreen = -3; //End Of Run Boiler Overheating
    autoShutdown();
  }

  //Excessive heat in condenser output.
  if (PCTempOut > 50) {
    alarmCondition = true;
    currentScreen = -4;
    autoShutdown();
  }

  // if(Vapour escaping to atmosphere) {
  // }

  // if(Over pressure in the boiler) {
  // }

}

void autoStartup() {

  //Turn on heater element and coolant pump relays
  digitalWrite(ELEMENT_PIN, HIGH);
  digitalWrite(PUMP_PIN, HIGH);

}

void autoShutdown() {

  //Turn off heater element
  digitalWrite(ELEMENT_PIN, LOW);
  delay(30000); // delay 30 seconds
  digitalWrite(PUMP_PIN, LOW);

}

void serialDivider() {

  Serial.println("/----------------------------/");

}

void intro() {

  //Introduction Screen 1
  writeDisplayBackground();
  //Add Text
  // add YHB Logo
  myGLCD.fillScr(VGA_WHITE);
  myGLCD.setColor(VGA_WHITE);
  myGLCD.fillRect(80, 10, 230, 230);
  //Draw Boiler
  myGLCD.setColor(VGA_BLUE);
  myGLCD.fillRect(100, 51, 135, 85);
  //Draw Column
  myGLCD.fillRect(135, 65, 225, 71);
  myGLCD.setColor(VGA_BLUE);
  geo.fillTriangle(210, 71, 215, 71, 210, 90);
  geo.fillTriangle(210, 71, 210, 90, 205, 90);
  myGLCD.fillRect(190, 90, 209, 95);
  //Draw bucket
  myGLCD.fillRect(100, 90, 103, 110);
  myGLCD.fillRect(100, 90, 130, 93);
  myGLCD.fillRect(100, 110, 130, 113);
  myGLCD.setColor(VGA_RED);
  myGLCD.fillRect(103, 93, 125, 110);
  //Draw Seat
  myGLCD.setColor(VGA_GRAY);
  myGLCD.fillRect(100, 150, 125, 180);
  //Draw Head
  myGLCD.setColor(VGA_BLACK);
  myGLCD.fillCircle(180, 143, 11);
  //Draw Foot
  myGLCD.fillCircle(104, 124, 4);
  myGLCD.fillRect(100, 124, 108, 139);
  //Draw Knee
  myGLCD.fillCircle(134, 128, 4);
  //Draw Calf
  geo.fillTriangle(108, 139, 132, 125, 132, 132);
  geo.fillTriangle(108, 130, 132, 125, 108, 139);
  //Draw Bum
  myGLCD.fillCircle(135, 160, 7);
  //Draw Thigh
  geo.fillTriangle(130, 128, 128, 160, 142, 160);
  geo.fillTriangle(130, 128, 138, 128, 142, 160);
  //Draw Shoulder
  myGLCD.fillCircle(163, 152, 7);
  //Draw Body
  geo.fillTriangle(135, 153, 162, 145, 162, 159);
  geo.fillTriangle(162, 159, 135, 153, 136, 167);
  //Draw Upper Arm
  geo.fillTriangle(134, 124, 134, 132, 155, 148);
  geo.fillTriangle(134, 132, 155, 148, 155, 156);
  //Draw Hand
  myGLCD.fillCircle(165, 136, 4);
  //Draw Fore Arm
  geo.fillTriangle(134, 129, 134, 135, 167, 133);
  geo.fillTriangle(134, 135, 167, 133, 167, 139);

  delay(DISPLAY_TRANSITION_DELAY);

  myGLCD.setColor(VGA_BLACK);
  myGLCD.setFont(GroteskBold16x32);
  // myGLCD.print("YHB", 290, 90, 90);
  //myGLCD.print("Presents", 70, 60, 90);

  delay(DISPLAY_TRANSITION_DELAY);

  //Draw Parrot

  myGLCD.fillScr(VGA_WHITE);
  myGLCD.setColor(VGA_RED);
  myGLCD.fillCircle(196, 130, 94);
  myGLCD.setColor(VGA_WHITE);
  myGLCD.fillCircle(200, 120, 82);
  myGLCD.setColor(VGA_BLUE);
  myGLCD.fillCircle(200, 115, 84);
  myGLCD.setColor(VGA_WHITE);
  myGLCD.fillCircle(208, 106, 81);
  myGLCD.setColor(VGA_RED);
  myGLCD.fillCircle(198, 102, 63);
  myGLCD.setColor(VGA_WHITE);
  myGLCD.fillCircle(206, 90, 40);
  //geo.fillTriangle(80,112,182,117,93,170);
  //geo.fillTriangle(122,150,122,195,93,170);
  //myGLCD.fillRect(78,20,220,116);
  myGLCD.fillRect(120, 25, 220, 80);
  geo.fillTriangle(235, 65, 272, 67, 277, 100);
  myGLCD.fillRect(210, 40, 260, 70);
  myGLCD.setColor(VGA_RED);
  geo.fillTriangle(110, 95, 182, 115, 120, 165);
  myGLCD.setColor(VGA_GRAY);
  myGLCD.fillCircle(215, 83, 29);
  myGLCD.setColor(VGA_WHITE);
  myGLCD.fillRect(187, 54, 208, 112);
  geo.fillTriangle(208, 54, 208, 110, 248, 72);
  myGLCD.fillCircle(225, 94, 23);
  myGLCD.setColor(VGA_RED);
  myGLCD.fillCircle(153, 88, 38);
  myGLCD.setColor(VGA_GRAY);
  geo.fillTriangle(226, 61, 243, 68, 230, 75);
  geo.fillTriangle(221, 63, 225, 76, 210, 75);
  myGLCD.setColor(VGA_BLACK);
  myGLCD.fillCircle(236, 98, 5);
  myGLCD.setColor(VGA_WHITE);
  myGLCD.drawLine(125, 160, 180, 115);
  myGLCD.drawLine(125, 160, 115, 190);
  myGLCD.setBackColor(VGA_RED);
  myGLCD.setFont(GroteskBold16x32);
  myGLCD.print("VS", 160, 75, 90);
  myGLCD.setBackColor(VGA_WHITE);
  myGLCD.setColor(VGA_BLACK);
  myGLCD.setFont(GroteskBold16x32);
  myGLCD.print("Digital Parrot", 90, 5, 90);
  myGLCD.print("By", 50, 105, 90);

  delay(DISPLAY_TRANSITION_DELAY);

  myGLCD.fillScr(VGA_WHITE);


  myGLCD.setColor(VGA_BLACK);
  myGLCD.setFont(GroteskBold16x32);
  myGLCD.print("VisionStills", 310, 20, 90);
  //Draw Arcs
  myGLCD.setColor(VGA_BLUE);
  myGLCD.fillCircle(162, 153, 76);
  myGLCD.setColor(VGA_WHITE);
  myGLCD.fillCircle(155, 155, 67);
  myGLCD.fillRect(56, 63, 206, 185);
  myGLCD.setColor(VGA_AQUA);
  myGLCD.fillCircle(152, 150, 62);
  myGLCD.setColor(VGA_WHITE);
  myGLCD.fillCircle(147, 140, 61);
  myGLCD.fillRect(76, 130, 215, 175);
  geo.fillTriangle(206, 105, 215, 135, 198, 140);

  //Draw V
  myGLCD.setColor(VGA_BLUE);
  myGLCD.fillRect(212, 8, 222, 51);
  myGLCD.fillRect(192, 70, 200, 105);
  geo.fillTriangle(213, 40, 84, 58, 212, 18);
  geo.fillTriangle(213, 40, 84, 58, 130, 68);
  geo.fillTriangle(192, 81, 84, 72, 84, 58);
  geo.fillTriangle(192, 81, 84, 72, 192, 96);

  //Draw S
  myGLCD.setColor(VGA_BLUE);
  //myGLCD.fillRect(176,143,217,156);
  myGLCD.fillCircle(120, 141, 39);
  myGLCD.fillCircle(120, 119, 39);
  myGLCD.fillRect(81, 120, 88, 142);
  myGLCD.fillCircle(189, 132, 25);
  myGLCD.setColor(VGA_WHITE);
  myGLCD.fillCircle(120, 132, 30);
  myGLCD.fillCircle(190, 133, 15);
  myGLCD.setColor(VGA_BLUE);
  myGLCD.fillRect(176, 143, 217, 156);
  myGLCD.fillRect(81, 120, 88, 142);
  myGLCD.setColor(VGA_WHITE);
  myGLCD.fillCircle(120, 132, 30);

  myGLCD.fillRect(165, 135, 178, 175);
  myGLCD.setColor(VGA_BLUE);
  geo.fillTriangle(175, 112, 153, 163, 182, 120);
  geo.fillTriangle(175, 112, 153, 163, 150, 139);
  myGLCD.setColor(VGA_WHITE);
  //myGLCD.setColor(VGA_RED);
  geo.fillTriangle(171, 115, 135, 159, 145, 120);

  //myGLCD.fillCircle(160,120,56);

  myGLCD.setColor(VGA_BLACK);
  myGLCD.setBackColor(VGA_WHITE);
  myGLCD.setFont(Retro8x16);
  myGLCD.print("VisionStills.org", 50, 55, 90);

  delay(DISPLAY_TRANSITION_DELAY);
}


