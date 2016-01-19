/**
 * The MySensors Arduino library handles the wireless radio link and protocol
 * between your home built sensors/actuators and HA controller of choice.
 * The sensors forms a self healing radio network with optional repeaters. Each
 * repeater and gateway builds a routing tables in EEPROM which keeps track of the
 * network topology allowing messages to be routed to nodes.
 *
 * Created by Henrik Ekblad <henrik.ekblad@mysensors.org>
 * Copyright (C) 2013-2015 Sensnology AB
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
 * EnergyMeterSCT by Patrick Schaerer
 * This Sketch is a WattMeter used with a SCT-013-030 non invasive PowerMeter
 * see documentation for schematic
 */

#include <SPI.h>
#include <MySensor.h>  
#include <EmonLib.h> 
 
#define DIGITAL_INPUT_SENSOR 3  // The digital input you attached your SCT sensor.  (Only 2 and 3 generates interrupt!)
#define INTERRUPT DIGITAL_INPUT_SENSOR-2 // Usually the interrupt = pin -2 (on uno/nano anyway)
#define CHILD_ID 1              // Id of the sensor child

EnergyMonitor emon1;
MySensor gw;
MyMessage wattMsg(CHILD_ID,V_WATT);
MyMessage kwhMsg(CHILD_ID,V_KWH);

double wattsumme = 0;
long lastmillis = 0;
int minuten = 0;

void setup()  
{  
  gw.begin(NULL, AUTO, true);
  Serial.begin(9600);
  emon1.current(DIGITAL_INPUT_SENSOR, 30);             // Current: input pin, calibration.
  // Send the sketch version information to the gateway and Controller
  gw.sendSketchInfo("Energy Meter SCT013", "0.1a");
  // Register this device as power sensor
  gw.present(CHILD_ID, S_POWER);
}


void loop()     
{ 
  gw.process();
  
  if (millis()-lastmillis > 60000) {
    double Irms = emon1.calcIrms(1480);  // Calculate Irms only
    if (Irms < 0.02) Irms = 0;
    int watt = Irms*230.0;
    wattsumme = wattsumme+watt;
    minuten = minuten + 1;
    lastmillis = millis();
    Serial.print(watt);	       // Apparent power
    Serial.print(" ");
    Serial.println(Irms);		       // Irms
    gw.send(wattMsg.set(watt));  // Send watt value to gw 
  }
  if (minuten == 60) {
  	double kwh = wattsumme/60/1000;
  	gw.send(kwhMsg.set(kwh, 4)); // Send kwh value to gw 
  	wattsumme = 0;
  	minuten = 0;
  }
}
