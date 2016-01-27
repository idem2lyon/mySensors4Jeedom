#include <SPI.h>
#include <MySensor.h>
#include <DHT.h>
#include <Bounce2.h>

#define CHILD_ID_HUM 0
#define CHILD_ID_TEMP 1
#define CHILD_ID_MOTION 2
#define CHILD_ID_LIGHT 3
#define CHILD_ID_DOOR 4

// For french friends !!!
// Seuls les pin 2 et 3 peuvent être utilisés pour gérer les interruptions
// Détecteur de lumière en pin 0 analogique
// Boutton d’ouverture de porte en pin 2 digitale (important pour interruption)
// Détecteur de mouvement en pin 3 digitale (important pour interruption)
// Détecteur d’humidité / température en pin  4 digitale


#define LIGHT_SENSOR_ANLAOG_PIN 0 // Analog input for light sensor
#define BUTTON_PIN  2  // Arduino Digital I/O pin for button/reed switch => 2 for interrupt
#define DIGITAL_INPUT_SENSOR 3   // The digital input you attached your motion sensor.  (Only 2 and 3 generates interrupt!)
#define HUMIDITY_SENSOR_DIGITAL_PIN 4 // Digital input for DHT sensor
unsigned long SLEEP_TIME = 300000; // Sleep time between reads (in milliseconds)

MySensor gw;
DHT dht;
Bounce debouncer = Bounce();
int oldValue=-1;
float lastTemp;
float lastHum;
boolean metric = true;
MyMessage msgHum(CHILD_ID_HUM, V_HUM);
MyMessage msgTemp(CHILD_ID_TEMP, V_TEMP);
MyMessage msgMot(CHILD_ID_MOTION, V_TRIPPED);
MyMessage msgLight(CHILD_ID_LIGHT, V_LIGHT_LEVEL);
MyMessage msgDoor(CHILD_ID_DOOR,V_TRIPPED);
int lastLightLevel;

void setup()
{
 gw.begin();
 dht.setup(HUMIDITY_SENSOR_DIGITAL_PIN);

 pinMode(DIGITAL_INPUT_SENSOR, INPUT);      // sets the motion sensor digital pin as input

 // Setup the button
 pinMode(BUTTON_PIN,INPUT);
 // Activate internal pull-up
 digitalWrite(BUTTON_PIN,HIGH);

 // After setting up the button, setup debouncer
 debouncer.attach(BUTTON_PIN);
 debouncer.interval(5);

 // Send the Sketch Version Information to the Gateway
 gw.sendSketchInfo("MultiSensor", "1.0");

 // Register all sensors to gw (they will be created as child devices)
 gw.present(CHILD_ID_HUM, S_HUM);
 gw.present(CHILD_ID_TEMP, S_TEMP);
 gw.present(CHILD_ID_MOTION, S_MOTION);
 gw.present(CHILD_ID_LIGHT, S_LIGHT_LEVEL);
 gw.present(CHILD_ID_DOOR, S_DOOR);
}

void loop()
{
 delay(dht.getMinimumSamplingPeriod());

 float temperature = dht.getTemperature();
 if (isnan(temperature)) {
     Serial.println("Failed reading temperature from DHT");
 } else if (temperature != lastTemp) {
   lastTemp = temperature;
   gw.send(msgTemp.set(temperature, 1));
   Serial.print("T: ");
   Serial.println(temperature);
 }

 float humidity = dht.getHumidity();
 if (isnan(humidity)) {
     Serial.println("Failed reading humidity from DHT");
 } else if (humidity != lastHum) {
     lastHum = humidity;
     gw.send(msgHum.set(humidity, 1));
     Serial.print("H: ");
     Serial.println(humidity);
 }

 // Read digital motion value
 boolean tripped = digitalRead(DIGITAL_INPUT_SENSOR) == HIGH;

 Serial.println(tripped);
 gw.send(msgMot.set(tripped?"1":"0"));  // Send tripped value to gw

 int lightLevel = (1023-analogRead(LIGHT_SENSOR_ANLAOG_PIN))/10.23;
 Serial.println(lightLevel);
 if (lightLevel != lastLightLevel) {
     gw.send(msgLight.set(lightLevel));
     lastLightLevel = lightLevel;
 }

 debouncer.update();
 // Get the update value
 int value = debouncer.read();

 if (value != oldValue) {
    // Send in the new value
    gw.send(msgDoor.set(value==HIGH ? 1 : 0));
    oldValue = value;
 }
 // sleep - wait for interrupt
 gw.sleep(digitalPinToInterrupt(CHILD_ID_DOOR), CHANGE, digitalPinToInterrupt(DIGITAL_INPUT_SENSOR), CHANGE, SLEEP_TIME);
}
