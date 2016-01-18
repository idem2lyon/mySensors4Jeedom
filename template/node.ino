/**

 * The MySensors Arduino library handles the wireless radio link and protocol

 * between your home built sensors/actuators and HA controller of choice.

 * The sensors forms a self healing radio network with optional repeaters. Each

 * repeater and gateway builds a routing tables in EEPROM which keeps track of the

 * network topology allowing messages to be routed to nodes.

 *

 * Created by Henrik Ekblad <henrik.ekblad@mysensors.org>

 * Copyright (C) 2013­2015 Sensnology AB

 * Full contributor list: https://github.com/mysensors/Arduino/graphs/contributors

 *

 * Documentation: http://www.mysensors.org

 * Support Forum: http://forum.mysensors.org

 *

 * This program is free software; you can redistribute it and/or

 * modify it under the terms of the GNU General Public License

 * version 2 as published by the Free Software Foundation.

 *

 *******************************

 *

 * REVISION HISTORY

 * Version 1.0 ­ Henrik Ekblad

 * 

 * DESCRIPTION

 * Example sketch for a "light switch" where you can control light or something 

 * else from both HA controller and a local physical button 

 * (connected between digital pin 3 and GND).

 * This node also works as a repeader for other nodes

 * http://www.mysensors.org/build/relay

 */ 

#include <MySensor.h>

#include <SPI.h>

#include <Bounce2.h>

// Capteur de température, librairies

#include <DallasTemperature.h>

#include <OneWire.h>

MySensor gw;

// Relay et interrupteur

#define RELAY_PIN   4  // Arduino Digital I/O pin number for relay 

#define BUTTON_PIN  3  // Arduino Digital I/O pin number for button 

#define CHILD_RELAY_ID    1   // Id of the sensor child

#define RELAY_ON    1

#define RELAY_OFF   0

Bounce debouncer = Bounce(); 

int   stateOldValue=0;

bool  state;

MyMessage         msgRelayLight(CHILD_RELAY_ID, V_LIGHT);

// Sondes de température

#define           COMPARE_TEMP 1 // Send temperature only if changed? 1 = Yes 0 = No

#define           ONE_WIRE_BUS 5 // Pin where dallase sensor is connected 

#define           MAX_ATTACHED_DS18B20 16

unsigned long     SLEEP_TIME = 2000; // Sleep time between reads (in milliseconds)

OneWire           oneWire(ONE_WIRE_BUS); // Setup a oneWire instance to communicate 

with any OneWire devices (not just Maxim/Dallas temperature ICs)

DallasTemperature sensors(&oneWire); // Pass the oneWire reference to Dallas 

Temperature. 

float             lastTemperature[MAX_ATTACHED_DS18B20];

int               numSensors=0;

boolean           receivedConfig = false;

boolean           metric = true; 

unsigned long     lastTempReadTime = millis();

// Initialize temperature message

#define           CHILD_TEMP_ID    2   // Id of the sensor child

MyMessage         msgTemperature(CHILD_TEMP_ID, V_TEMP);

void setup()  

{  

  gw.begin(incomingMessage, AUTO, true);

  // Send the sketch version information to the gateway and Controller

  gw.sendSketchInfo("Relay & Button & Sensor", "1.0");

  

  setupRelayLight();

  setupTemperature();

}

void setupRelayLight() {

 // Setup the button

  pinMode(BUTTON_PIN,INPUT);

  

  // Activate internal pull­up

  digitalWrite(BUTTON_PIN,HIGH);

  

  // After setting up the button, setup debouncer

  debouncer.attach(BUTTON_PIN);

  debouncer.interval(5);

  // Register all sensors to gw (they will be created as child devices)

  gw.present(CHILD_RELAY_ID, S_LIGHT);

  // Make sure relays are off when starting up

  digitalWrite(RELAY_PIN, RELAY_OFF);

  // Then set relay pins in output mode

  pinMode(RELAY_PIN, OUTPUT);   

      

  // Set relay to last known state (using eeprom storage) 

  state = gw.loadState(CHILD_RELAY_ID);

  digitalWrite(RELAY_PIN, state?RELAY_ON:RELAY_OFF);

}

void setupTemperature() {

   

  // Startup up the OneWire library

  sensors.begin();

  

  // requestTemperatures() will not block current thread

  sensors.setWaitForConversion(false);

  // Fetch the number of attached temperature sensors  

  numSensors = sensors.getDeviceCount();

  // Present all sensors to controller

  for (int i = 0; i < numSensors && i < MAX_ATTACHED_DS18B20; i++) {   

     Serial.println("Sensor found on" + i);

     gw.present(i, S_TEMP);

  }

  Serial.println("Sensor found ??");

}

/*

*  Example on how to asynchronously check for new messages from gw

*/

void loop() 

{

  gw.process();

  loopRelayLight(); 

  

  loopTemperature();

}

void loopRelayLight() {

  

  debouncer.update();

  // Get the update value

  int value = debouncer.read();

  

  if (value != stateOldValue && value==0) {

      gw.send(msgRelayLight.set(state?false:true), true); // Send new state and request ack 

back

  }

  

  stateOldValue = value;

}

void incomingMessage(const MyMessage &message) {

  // We only expect one type of message from controller. But we better check anyway.

  if (message.isAck()) {

     Serial.println("This is an ack from gateway");

  }

  Serial.println("Incomine message !!");

   

  if (message.type == V_LIGHT) {

    Serial.println("Incomine message Type LIGHT");

     // Change relay state

     state = message.getBool();

     digitalWrite(RELAY_PIN, state?RELAY_ON:RELAY_OFF);

     // Store state in eeprom

     gw.saveState(CHILD_TEMP_ID, state);

    

     // Write some debug info

     Serial.print("Incoming change for sensor:");

     Serial.print(message.sensor);

     Serial.print(", New status: ");

     Serial.println(message.getBool());

   } 

}

void loopTemperature(){ 

  unsigned long current = millis();

  

  // ON ne traite la lecture de température qu'au bout du temps SLEEP_TIME

  if (lastTempReadTime == 0 || 

      ((current ­ lastTempReadTime) < 0) || 

      ((current ­ lastTempReadTime) > SLEEP_TIME)) {

    // Fetch temperatures from Dallas sensors

    sensors.requestTemperatures();

  

    // query conversion time and sleep until conversion completed

    int16_t conversionTime = sensors.millisToWaitForConversion(sensors.getResolution());

    // sleep() call can be replaced by wait() call if node need to process incoming messages 

(or if node is repeater)

    //gw.wait(conversionTime);

  

    // Read temperatures and send them to controller 

    for (int i=0; i<numSensors && i<MAX_ATTACHED_DS18B20; i++) {

   

      // Fetch and round temperature to one decimal

      float temperature = 

static_cast<float>(static_cast<int>((gw.getConfig().isMetric?sensors.getTempCByIndex(i):se

nsors.getTempFByIndex(i)) * 10.)) / 10.;

   

      // Only send data if temperature has changed and no error

      #if COMPARE_TEMP == 1

      if (lastTemperature[i] != temperature && temperature != ­127.00 && temperature != 

85.00) {

      #else

      if (temperature != ­127.00 && temperature != 85.00) {

      #endif

   

        // Send in the new temperature

        gw.send(msgTemperature.setSensor(i).set(temperature, 1));

        // Save new temperatures for next compare

        lastTemperature[i]=temperature;

        Serial.print("send temp value :");

        Serial.println(temperature);

      }

    }

     //ON mémorise le last time

    lastTempReadTime = current;

  }

  //Use sleep if powersave AND no other process is done on the same node !!!

  //gw.wait(SLEEP_TIME);

}
