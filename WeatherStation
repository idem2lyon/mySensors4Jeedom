#include <SPI.h>
#include <MySensor.h>  
#include <DHT.h>  
#include <Wire.h>
#include <Adafruit_BMP085.h>

#define CHILD_ID_HUM 0
#define CHILD_ID_TEMP 1
#define CHILD_ID_BARO 2
#define CHILD_ID_BARO_TEMP 3

#define HUMIDITY_SENSOR_DIGITAL_PIN 3
int BATTERY_SENSE_PIN = A0;  // select the input pin for the battery sense point

#define SLEEP_MINUTE 60000
#define SLEEP_FIVE_MINUTES 300000

MySensor gw;
DHT dht;
Adafruit_BMP085 bmp = Adafruit_BMP085();      // Digital Pressure Sensor 

float   lastTemp = -1.0;
float   lastHum = -1.0;
float   lastBaroTemp = -1.0;
int     lastForecast = -1;
char    *weather[] = { "Stable", "Sunny", "Cloudy", "Unstable", "Thunderstorm", "Unknown" };
int     minutes;
int     pressureSamples[5];
float   lastPressureAvg = -1.0;
int     lastPressure = -1;
int     minuteCount = 0;
float   pressureAvg;
int     pressure;
float   dP_dt;
boolean metric = true;

MyMessage msgHum(CHILD_ID_HUM, V_HUM);
MyMessage msgTemp(CHILD_ID_TEMP, V_TEMP);
MyMessage msgBaroTemp(CHILD_ID_BARO_TEMP, V_TEMP);
MyMessage msgBaro(CHILD_ID_BARO, V_PRESSURE);
MyMessage msgForecast(CHILD_ID_BARO, V_FORECAST);

void setup() {
    // use the 1.1 V internal reference
    analogReference(INTERNAL);
    gw.begin();
    dht.setup(HUMIDITY_SENSOR_DIGITAL_PIN);
    if (!bmp.begin()) {
        Serial.println("Could not find a valid BMP085 sensor, check wiring!");
    }
    // Send the Sketch Version Information to the Gateway
    gw.sendSketchInfo("Mini Weather Station", "3.2");

    // Register all sensors to gw (they will be created as child devices)
    gw.present(CHILD_ID_HUM, S_HUM);
    gw.present(CHILD_ID_TEMP, S_TEMP);
    gw.present(CHILD_ID_BARO, S_BARO);
    gw.present(CHILD_ID_BARO_TEMP, S_TEMP);
    metric = gw.getConfig().isMetric;
}

void loop() {
  
    Serial.print("minuteCount = ");
    Serial.println(minuteCount);

        // The pressure Sensor Stuff
    int forecast = SamplePressure();


        if ( minuteCount > 4 ) { // only every 5 minutes
                // Process the barometric sensor data 
            float baro_temperature = bmp.readTemperature();
            if (!metric) {    // Convert to fahrenheit
                baro_temperature = baro_temperature * 9.0 / 5.0 + 32.0;
            }

            if (baro_temperature != lastBaroTemp) {
                gw.send(msgBaroTemp.set(baro_temperature, 1));
                lastBaroTemp = baro_temperature;
            }

            if (pressure != lastPressure) {
                gw.send(msgBaro.set(pressure, 0));
                        //delay(1000);
//              gw.send(msgBaro.set(pressure));
                lastPressure = pressure;
            }

            if (forecast != lastForecast) {
                gw.send(msgForecast.set(weather[forecast]));
                lastForecast = forecast;
            }


                // The humidity sensor stuff
                delay(dht.getMinimumSamplingPeriod());
      
                float temperature = dht.getTemperature();
            if (isnan(temperature)) {
                Serial.println("Failed reading temperature from DHT");
            } else if (temperature != lastTemp) {
                lastTemp = temperature;
                    if (!metric) {
                        temperature = dht.toFahrenheit(temperature);
                    }
                    gw.send(msgTemp.set(temperature, 1));
                    Serial.print("Temperature: ");
                    Serial.println(temperature);
                }
      
            float humidity = dht.getHumidity();
            if (isnan(humidity)) {
                Serial.println("Failed reading humidity from DHT");
            } else if (humidity != lastHum) {
                lastHum = humidity;
                gw.send(msgHum.set(humidity, 1));
                Serial.print("Humidity: ");
                Serial.println(humidity);
            }
      
      
            // get the battery Voltage
            long sensorValue = analogRead(BATTERY_SENSE_PIN);
      
            // 1M, 100K divider across battery and using internal ADC ref of 1.1V
            // Sense point is bypassed with 0.1 uF cap to reduce noise at that point
            // ((1e6+100e3)/100e3)*1.1 = Vmax = 12.1 Volts
            // 12.1/1023 = Volts per bit = 0.011827957
            // sensor val at 9v = 9/0.011827957 = 760.909090217
            // float batteryV  = sensorValue * 0.011827957;
            long batteryValue = sensorValue * 100L;
            int batteryPcnt = batteryValue / 761;
            gw.sendBatteryLevel((batteryPcnt > 100 ? 100 : batteryPcnt)); // this allows for batteries that have slightly over 9v
      
            Serial.print("Batt %:");
            Serial.println(batteryPcnt);
        }

        if ( minuteCount < 5 )  // sleep a bit
            gw.sleep(SLEEP_MINUTE); //while pressure sampling
        else
            gw.sleep(SLEEP_FIVE_MINUTES); 
        
}

int SamplePressure() {
    // This is a simplification of Algorithm found here to same memory
    // http://www.freescale.com/files/sensors/doc/app_note/AN3914.pdf

    pressure = bmp.readSealevelPressure(60) / 100; // 60 meters above sealevel

    if (minuteCount > 9) { // we are going to test pressure change every 30 min (5*1min + 5*5min)
        lastPressureAvg = pressureAvg;
        minuteCount = 0;
    }

    if (minuteCount < 5) {
        pressureSamples[minuteCount] = pressure; // Collect 5 minutes of samples every 30 min
                Serial.print("  Sample(");
                Serial.print(minuteCount);
                Serial.print(") = ");
                Serial.println(pressure);
        }

    if (minuteCount == 4) { // the 5th minute
        // Avg pressure in first 5 min, value averaged from 0 to 5 min.
        pressureAvg = ((pressureSamples[0] + pressureSamples[1]
                + pressureSamples[2] + pressureSamples[3] + pressureSamples[4])
                / 5);
        float change = pressureAvg - lastPressureAvg;
        dP_dt = (((65.0 / 1023.0) * change) / 0.5); // divide by 0.5 as this is the difference in time from last sample 0.5 hours
        Serial.print("dP_dt = ");
        Serial.println(dP_dt);
    }

    minuteCount++;

    if (lastPressureAvg < 0) // no previous pressure sample.
        return 5; // Unknown, more time needed
    else if (dP_dt < (-0.25))
        return 4; // Quickly falling LP, Thunderstorm, not stable
    else if (dP_dt > 0.25)
        return 3; // Quickly rising HP, not stable weather
    else if ((dP_dt > (-0.25)) && (dP_dt < (-0.05)))
        return 2; // Slowly falling Low Pressure System, stable rainy weather
    else if ((dP_dt > 0.05) && (dP_dt < 0.25))
        return 1; // Slowly rising HP stable good weather
    else if ((dP_dt > (-0.05)) && (dP_dt < 0.05))
        return 0; // Stable weather
    else
        return 5; // Unknown

}
