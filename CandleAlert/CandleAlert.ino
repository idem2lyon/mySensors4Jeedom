/*
 PROJECT: MySensors / RGB light NEOPIXEL
 PROGRAMMER: AWI
 DATE: october 10, 2015/ last update: october 14, 2015
 FILE ORIGINAL: AWI_RGB.ino
 FILE : CandleAlert.ino
 LICENSE: Public domain

 Hardware: Nano and MySensors 1.5, Wall light 16 WS2812B leds (neopixel)
        
 Special:
    uses Fastled library with NeoPixel (great & fast RBG/HSV universal library)             https://github.com/FastLED/FastLED
    
 SUMMARY:
    
    Different patterns and brightness settings
    
    Button switches on/off and cycles through all Color patterns on long press
    
 Remarks:
    Fixed node-id

*/

#include <MySensor.h>
#include <SPI.h>
#include <FastLED.h>                                        // https://github.com/FastLED/FastLED
#include <Button.h>                                         // https://github.com/JChristensen/Button

const int stripPin = 5 ;                                    // pin where 2812 LED strip is connected
const int buttonPin = 4 ;                                   // push button
const int numPixel = 16 ;                                   // set to number of pixels (x top / y bottom)

const int NODE_ID = 62 ;                                    // fixed MySensors node id
const int RGB_LightChild = 0 ;                              // Child Id's, standard light child on/off/ dim
const int RGB_RGBChild = 1 ;                                // RGB light child (on/off/dim/color, if controller supports V_RBG))
const int RGB_SolidColorChild = 2 ;                         // when set, node reads Color text from ColorTextChild
const int RGB_TextColorChild = 3 ;                          // Holds Text value for color (custom colors from controller)
const int RGB_AlarmPatternChild = 4 ;                       // Switches to alarm status
const int RGB_NextPatternChild = 5 ;                        // Move to next pattern when set

CRGB leds[numPixel];

// Kelving colors: Light & daylight (in Fastled reference only)
/// 1900 Kelvin Candle=0xFF9329 /* 1900 K, 255, 147, 41 */,
/// 2600 Kelvin Tungsten40W=0xFFC58F /* 2600 K, 255, 197, 143 */,
/// 2850 Kelvin Tungsten100W=0xFFD6AA /* 2850 K, 255, 214, 170 */,
/// 3200 Kelvin Halogen=0xFFF1E0 /* 3200 K, 255, 241, 224 */,
/// 5200 Kelvin CarbonArc=0xFFFAF4 /* 5200 K, 255, 250, 244 */,
/// 5400 Kelvin HighNoonSun=0xFFFFFB /* 5400 K, 255, 255, 251 */,
/// 6000 Kelvin DirectSunlight=0xFFFFFF /* 6000 K, 255, 255, 255 */,
/// 7000 Kelvin OvercastSky=0xC9E2FF /* 7000 K, 201, 226, 255 */,
/// 20000 Kelvin ClearBlueSky=0x409CFF /* 20000 K, 64, 156, 255 */

char controllerRGBvalue[] = "FFDEAD";                       // Controller sent RGB value, default
uint16_t curBrightness, actualBrightness, controllerRGBbrightness = 0x7F ;  // Brightness globals
unsigned long updateBrightnessDelay, lastBrightnessUpdate ; // Brightness timers
int RGBonoff ;                                              // OnOff flag

enum { pSolid, pOff, pAlarm, pFire, pFire2, pCandle, pRainbow}  ;   // Pattern globals (stored in int for convenience)
const int lastPatternIdx = pRainbow + 1 ;                   // use last pattern for patterncount
int curPattern ;                                            // current pattern
unsigned long updatePatternDelay, lastPatternUpdate ;       // Pattern timers

unsigned long idleTimer = millis() ;                        // return to idle timer
int idleTime = 10000 ;                                      // return to idle after 10 secs

// initialize MySensors (MySensors 1.5 style)
MyTransportNRF24 transport(9, 10);                          // Ceech board, 3.3v (7,8)  (pin default 9,10)
MySensor gw(transport);                     

MyMessage lightRGBMsg(RGB_LightChild,  V_RGB);              // standard messages, light
MyMessage lightdimmerMsG(RGB_LightChild ,V_DIMMER); 
MyMessage lightOnOffMessage(RGB_LightChild, V_STATUS);

Button myBtn(buttonPin, true, true, 20);                    //Declare the button (pin, pull_up, invert, debounce_ms)

// Simple state machine for button state
enum {sIdle, sBrightness, sPattern} ;                        // simple state machine for button press
int State ;

void setup() {
    FastLED.addLeds<NEOPIXEL, stripPin >(leds, numPixel) ;  // initialize led strip .setCorrection(TypicalLEDStrip); 

    gw.begin(incomingMessage, NODE_ID, false);              // initialize MySensors
    gw.sendSketchInfo("AWI RGB Wall 0", "1.0");
    gw.present(RGB_RGBChild, S_RGB_LIGHT, "RGB Wall RGB 0");// present to controller
    gw.present(RGB_LightChild, S_LIGHT, "RGB Wall Light 0");
    gw.present(RGB_SolidColorChild, S_LIGHT, "RGB Set Solid color (text) 0");
    gw.present(RGB_TextColorChild, S_INFO, "RGB Wall textcolor 0"); 
    gw.present(RGB_AlarmPatternChild, S_BINARY, "RGB Wall Alarm 0");
    gw.present(RGB_NextPatternChild, S_BINARY, "RGB Wall Pattern 0");
        
    // initialize strip with color and show (strip expects long, so convert from String)
    for(int i = 0 ; i < 6 ; i++) {                          // get color value from EEPROM (6 char)
        controllerRGBvalue[i] = gw.loadState(i) ;
        }
    setLightPattern(pSolid, NULL) ;                         // default controller Solid 
    FastLED.show();
    State = sIdle ;                                         // Initial state
    //randomSeed(analogRead(0));
}

// read button and act accordingly
// short press: on/off
// longer press: set patterns with following short press
// long press: set brightness increase 
void loop() {
    gw.process();                                           // wait for incoming messages
    myBtn.read();                                           //Read the button (only read)
    unsigned long now = millis();                           // loop timer reference
    switch (State) {
        case sIdle:                                         // default state, browse through patterns
            if (myBtn.wasReleased()){                       // light on/ off in idle
                RGBonoff = !RGBonoff ;                      // invert light state
                setLightPattern((RGBonoff == 1)?pOff:pSolid, 100);
                gw.send(lightOnOffMessage.set(RGBonoff));   // and update controller    
            } else if (myBtn.pressedFor(500)){              // move to Pattern update state with long press
                idleTimer = now ;                           // return to idle after ...
                State = sPattern ;
            }
            break ;
        case sPattern:                                      // entered after long press
            if (myBtn.pressedFor(2000)){                    // when press even longer move to Brightness update
                State = sBrightness ;
            } else if (myBtn.wasPressed()){
                setLightPattern((curPattern + 1) % lastPatternIdx, 500 ); // increase pattern and wrap
                idleTimer = now ;
            } else if ( now > idleTime + idleTimer  ){      // return to idle after ...
                State = sIdle ;
            }
            break ;
        case sBrightness:                                   // entered after looong press
            if (myBtn.wasPressed()){                            // if pressed again increase brightness
                setLightBrightness((curBrightness+1) % 0xFF, 0) ; // increase brightness and wrap (0..0xFF)
                idleTimer = now ;
            } else if ( now > idleTime + idleTimer  ){      // return to idle after ...
                State = sIdle ;
            }
            break ;
        default :
            State = sIdle ;
            break ;
        }
    updateLightBrightness();                                // update Brightness if time
    updateLightPattern();                                   // update Pattern if time
    }

// Sets the light brightness, takes value and time (ms) as input
void setLightBrightness(int newBrightness, unsigned long updateTime){
    // global: curBrightness, actualBrightness, updateBrightnessDelay
    updateBrightnessDelay = updateTime / 0xFF ;             // delay = time / max steps
    actualBrightness = curBrightness ;                      // assume curBrightness is actual
    curBrightness = newBrightness ;                         // set curBrightness to new value, rest is done in update
    }   
 
// Update the light brightness if time
void updateLightBrightness(){
    // global: curBrightness, actualBrightness, updateBrightnessDelay, lastBrightnessUpdate ;
    unsigned long now = millis() ;
    if (now > lastBrightnessUpdate + updateBrightnessDelay){// check if time for update
        if ( actualBrightness > curBrightness) {
            FastLED.setBrightness( actualBrightness-- );
            FastLED.show();
        } else if ( actualBrightness < curBrightness){
            FastLED.setBrightness( actualBrightness++ );
            FastLED.show();
            }
        lastBrightnessUpdate = now ;
        }
    }

// **** Pattern routines *****
// Sets and initializes the light pattern if nescessary
void setLightPattern( int newPattern, unsigned long updateDelay){
    // global: curPattern, updatePatternDelay
    curPattern = newPattern ;
    updatePatternDelay = updateDelay ;                      // delay for next pattern update, can be changed in pattern 
    switch(curPattern){
        case pSolid:                                        //  solid is controller value in all pixels
            for(int i = 0 ; i < numPixel ; i++) leds[i] = strtol( controllerRGBvalue, NULL, 16);
            FastLED.show();
            break ;
        case pOff:                                          //  off state all pixels off
            for(int i = 0 ; i < numPixel ; i++) leds[i] = 0 ;
            FastLED.show();
            break ;
        default :
            break ;
            }
    }   

// Update the light pattern when time for it
void updateLightPattern(){
    // global: curPattern, updatePatternDelay, lastPatternUpdate
    unsigned long now = millis() ;
    if (now > lastPatternUpdate + updatePatternDelay){      // check if time for update
        switch (curPattern) {
            case pAlarm:                                    // flash light
                patternAlarm();
                break ;
            case pFire:                                     // wild fire
                patternFire();
                break ;
            case pFire2:                                    // cosy fire
                patternFire2();
                break ;
            case pCandle:                                   // flame
                patternCandle();
                break ;
            case pRainbow:                                  // rotating rainbow
                patternRainbow();
                break ;
            case pSolid:                                    // do nothing fall through
            case pOff:
            default :                                       // def  
                break ;
            }
        lastPatternUpdate = now ;
        }
    }

// Define the different patterns
// Alarm - intermittent white and red color, full intensity, intermittent top & bottom half
void patternAlarm() {
    static boolean topBot ;                         // indicates direction for next entry
    const CRGB colorTop = CRGB(0xFF, 0, 0 );                // red color
    const CRGB colorBottom = CRGB(0xFF, 0xFF, 0xFF );       // white color
    FastLED.setBrightness(0xFF);                            // set the strip brightness
    for(int i=0; i <= (numPixel / 2 - 1) ; i++) {                   // for half of strip size
        leds[i] = topBot?colorTop:colorBottom ; 
        leds[i+ (numPixel/2)] = topBot?colorBottom:colorTop ;
        }
    topBot = !topBot ;                                      // switch direction
    FastLED.show();
    }

// Simulate fire with red color, varying number of leds intensity & tempo
void patternFire() {
    byte numberLeds = random(0,numPixel);                   // start number and end of led's for flickering
    byte lum = random(100,255);                             // set brightness
    CRGB color = CRGB(200, 50+random(1,180),0 );            // get red color with varying green
    for(int i=0; i <= numberLeds; i++) {
      leds[i] = color ;
      FastLED.setBrightness(lum);                           // set the strip brightness
      FastLED.show();
      gw.wait(random(0,10));
    }
    updatePatternDelay = 100 ; 
}

// Simulate fire with red color and varying intensity & tempo
void patternFire2() {
    CRGB color = CRGB(200, random(100,150),0);              // get red color with varying green
    for (byte p=0; p < numPixel; p++) {
      leds[p] = color;
    }
    FastLED.setBrightness(random(50,255));
    FastLED.show();
    updatePatternDelay = random(20,300);                    // variable delay
}

// Simulate candle based on fire with red color, varying number of leds intensity & tempo
void patternCandle() {
    byte numberLeds = random(0,numPixel);                   // start number and end of led's for flickering
    byte lum = random(60, 80);                              // set brightness
    CRGB color = CRGB(200, 50+random(40,100),0 );           // get red color with varying green
    for(int i=0; i <= numberLeds; i++) {
      leds[i] = color ;
      FastLED.setBrightness(lum);                           // set the strip brightness
      FastLED.show();
      gw.wait(random(5,10));
    }
    updatePatternDelay = 100 ; 
}


void patternRainbow() {
    static uint16_t hue ;                               // starting color
    FastLED.clear();
    // for(hue=10; hue<255*3; hue++) {
    hue = (hue+1) % 0xFF ;                                  // incerease hue and wrap
    fill_rainbow( leds, numPixel , hue /*static hue value */, 5);// set a rainbow from hue to last in stepsize 5
    FastLED.show();
    updatePatternDelay = 100 ;
    }

// Incoming messages from MySensors
void incomingMessage(const MyMessage &message) {
    int ID = message.sensor;
    Serial.print("Sensor: ");
    Serial.println(ID);
    switch (ID){
        case RGB_LightChild:                                // same behaviour as RGB child/ fall through
        case RGB_RGBChild:                                  // if controller can handle V_RGB
            if (message.type == V_RGB) {                    // check for RGB type
                strcpy(controllerRGBvalue, message.getString());// get the payload
                setLightPattern(pSolid, NULL);              // and set solid pattern 
            } else if (message.type == V_DIMMER) {          // if DIMMER type, adjust brightness
                controllerRGBbrightness = map(message.getLong(), 0, 100, 0, 255);
                setLightBrightness(controllerRGBbrightness, 2000) ;
            } else if (message.type == V_STATUS) {          // if on/off type, toggle brightness
                RGBonoff = message.getInt();
                setLightBrightness((RGBonoff == 1)?controllerRGBbrightness:0, 2000);
            }
            break ;
        case RGB_SolidColorChild:                           // request color from controller
            if (message.type == V_STATUS) {                 // if get color from text child
                gw.request(RGB_TextColorChild, V_TEXT);
                setLightPattern(pSolid, NULL);                  // and set solid pattern (if not alre)
                }
            break ;
        case RGB_TextColorChild:                            // Text color from controller
            if (message.type == V_TEXT) {                   // if get color from text child
                gw.request(RGB_TextColorChild, V_TEXT);
                strcpy(controllerRGBvalue, message.getString());// get the payload
                for(int i = 0 ; i < 6 ; i++) {              // save color value to EEPROM (6 char)
                    gw.saveState(i, controllerRGBvalue[i]) ;}// Save to EEPROM
                }
            break ;
        case RGB_AlarmPatternChild:                         // set Alarm pattern
            if (message.type == V_STATUS) {                 // if get color from text child
                if (message.getInt() == 1){
                    setLightPattern(pAlarm, 500);           // set slow alarm pattern
                } else {
                    setLightPattern(pSolid, NULL);          // and set solid pattern
                    FastLED.setBrightness(curBrightness);
                    }
                }
            break ;
        case RGB_NextPatternChild:                          // next pattern
            if (message.type == V_STATUS) {                 // if get color from text child
                if (message.getInt() == 1 ) {
                    setLightPattern((curPattern + 1) % lastPatternIdx, 500 ); // increase pattern and wrap
                    }
                }
            break ;
        }
    FastLED.show();
    dispRGBstat();
    }
// debug    
// display the status of all RGB: controller, requested, real
void dispRGBstat(void){
    Serial.print(" Color: "); Serial.print(controllerRGBvalue); 
    Serial.print(" Brightness: "); Serial.println(controllerRGBbrightness);
    }
