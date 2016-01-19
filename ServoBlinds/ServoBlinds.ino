// A sketch to control a servo with a button and MySensors messages


#include <MyConfig.h>
#include <MyGateway.h>
#include <MyMessage.h>
#include <MyMQTT.h>
#include <MySensor.h>
#include <Version.h>


#include <SPI.h>
#include <Servo.h>
#include <Button.h>        	//https://github.com/JChristensen/Button

#define BUTTON_PIN 3       	//Connect a tactile button switch (or something similar)
                            //from Arduino pin 3 to ground.
#define PULLUP true        	//To keep things simple, we use the Arduino's internal pullup resistor.
#define INVERT true        	//Since the pullup resistor will keep the pin high unless the
                           	//switch is closed, this is negative logic, i.e. a high state
                           	//means the button is NOT pressed. (Assuming a normally open switch.)
#define DEBOUNCE_MS 20     	//A debounce time of 20 milliseconds usually works well for tactile button switches.
#define LONG_PRESS_PERIOD 700   //How long to keep button pressed until sweeping starts

#define MAX_DEGREES 180         //Servo limits. Whatever works for you.     
#define MIN_DEGREES 0

#define CHILD_ID 0

Button myBtn(BUTTON_PIN, PULLUP, INVERT, DEBOUNCE_MS);    //Declare the button

Servo myservo;
enum {DECREASING, INCREASING};      //On a servo mounted on the left, with outer slat edge down as closed,
                                    // closing is going toward 180, opening toward 0 (on my fake futaba s3003's)

boolean invertConversions = true;   // false if opening toward 180
boolean servoDirection = INCREASING;     //A variable that keeps the current servo direction
int servoPin = 17;                  // A3 = D17
int servoPosition;
int servoSpeed = 1;                 // The bigger, the faster. 1=slow 5=fast

MySensor gw;
MyMessage msg(CHILD_ID, V_DIMMER);



void setup(void)
{
    gw.begin(incomingMessage);
    gw.sendSketchInfo("Servo", "1.0");
    gw.present(CHILD_ID, S_COVER);
    gw.request(CHILD_ID, V_DIMMER);
}

void loop(void)
{
    gw.process();

    myBtn.read();                    		//Read the button

    if (myBtn.wasReleased()){                   //If the button was pressed once sweep to end of current direction
        SweepToDirectionEnd();
        gw.send(msg.set(ConvertDegToPercent(servoPosition)));
    }
    if (myBtn.pressedFor(LONG_PRESS_PERIOD)){   //If the button is held down the servo will start to sweep
        SweepUntilStop();
        gw.send(msg.set(ConvertDegToPercent(servoPosition)));
    }
}


int ConvertPercentToDeg(int percent)
{
    int degree;
    if (invertConversions)
        degree = map(percent, 0, 100, MAX_DEGREES, MIN_DEGREES);
    if (!invertConversions)
        degree = map(percent, 0, 100, MIN_DEGREES, MAX_DEGREES);
    return degree;
}


int ConvertDegToPercent(int degree)
{
    int percent;
    if (invertConversions)
        percent = map(degree, MAX_DEGREES, MIN_DEGREES, 0, 100);
    if (!invertConversions)
        percent = map(degree, MIN_DEGREES, MAX_DEGREES, 0, 100);
    return percent;
}

void incomingMessage(const MyMessage &message) {
    myservo.attach(servoPin);
    if (message.type==V_DIMMER) {
        int val = message.getInt();
        SweepToPosition(ConvertPercentToDeg(val)); //In this case the value has to be inverted because 0 = open
        gw.send(msg.set(val));
    }
    else if (message.type==V_UP) {
        SweepToPosition(ConvertPercentToDeg(100));
        gw.send(msg.set(100));
    }
    else if (message.type==V_DOWN) {
        SweepToPosition(ConvertPercentToDeg(0));
        gw.send(msg.set(0));
    }
    else if (message.type==V_STOP)
        myservo.detach();
}




void ServoMoveUp()
{
    if ((myservo.attached()) && servoPosition < MAX_DEGREES){
        servoDirection = INCREASING;
        servoPosition += servoSpeed;
        myservo.write(servoPosition);
        delay(10);
        Serial.print("Servo Position: ");
        Serial.println(servoPosition);
    }
    if (!myservo.attached()){      
        Serial.println("Servo stopped while moving toward MAX, direction unchanged");
        delay(100 * servoSpeed);
    }    
    if (servoPosition >= MAX_DEGREES){
        Serial.println("MAX reached, changing direction toward MIN");
        servoDirection = DECREASING;
        delay(100 * servoSpeed);			// Wait for the last movement to finish
    }
}


void ServoMoveDown()
{
    if ((myservo.attached()) && servoPosition > MIN_DEGREES){
        servoDirection = DECREASING;
        servoPosition -= servoSpeed;
        delay(10);
        myservo.write(servoPosition);
        Serial.print("Servo Position: ");
        Serial.println(servoPosition);
    }
    if (!myservo.attached()){
        Serial.println("Servo stopped while moving toward MIN, direction unchanged");
        delay(100 * servoSpeed);
    }    
    if (servoPosition == MIN_DEGREES){
        Serial.println("MIN reached, changing direction toward MAX");
        servoDirection = INCREASING;
        delay(100 * servoSpeed);
    }
}


void SweepToDirectionEnd()
{
    myservo.attach(servoPin);
    if (servoDirection == INCREASING){
        Serial.println("Going to MAX and stopping there");
        while (servoPosition < MAX_DEGREES){
            ServoMoveUp();
        }
        delay(20 * servoSpeed);
        myservo.detach();
    }
    else if (servoDirection == DECREASING){
        Serial.println("Going to MIN and stopping there");
        while (servoPosition > MIN_DEGREES){
            ServoMoveDown();
        }
        delay(20 * servoSpeed);
        myservo.detach();
    }
}


void SweepUntilStop()
{
    myservo.attach(servoPin);
    while (myBtn.isPressed()){
        myBtn.read();
        if (myBtn.isReleased())
            myservo.detach();
        if (servoDirection == INCREASING)
            ServoMoveUp();
        if (servoDirection == DECREASING)
            ServoMoveDown();
    }
}


void SweepToPosition(int destination)
{
    if (abs(destination - servoPosition) >= servoSpeed)    //Don't move if destination is close to position
        myservo.attach(servoPin);
    if (destination > servoPosition && myservo.attached()){
        Serial.print("Going to ");
        Serial.print(destination);
        Serial.println(" and stopping there");
        while (servoPosition < destination){
            ServoMoveUp();
        }
        delay(20 * servoSpeed);
        myservo.detach();
    }
    if (destination < servoPosition && myservo.attached()){
        Serial.print("Going to ");
        Serial.print(destination);
        Serial.println(" and stopping there");
        while (servoPosition > destination){
            ServoMoveDown();
        }
        delay(20 * servoSpeed);
        myservo.detach();
    }
}
