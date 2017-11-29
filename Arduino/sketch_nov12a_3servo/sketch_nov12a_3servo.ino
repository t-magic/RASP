/*
Arduino Starter Kit example
Project 5 - Servo Mood Indicator
 
This sketch is written to accompany Project 5 in the
Arduino Starter Kit
 
Parts required:
servo motor
10 kilohm potentiometer
2 100 uF electrolytic capacitors
 
Created 13 September 2012
by Scott Fitzgerald
 
http://www.arduino.cc/starterKit
 
This example code is part of the public domain
*/
 
// include the servo library
#include <Servo.h>
 
Servo myServo1; // create a servo object
Servo myServo2; // create a servo object
Servo myServo3; // create a servo object

int const potPin1 = A0; // analog pin used to connect the potentiometer
int const potPin2 = A1; // analog pin used to connect the potentiometer
int const potPin3 = A2; // analog pin used to connect the potentiometer
int potVal1; // variable to read the value from the analog pin
int potVal2; // variable to read the value from the analog pin
int potVal3; // variable to read the value from the analog pin
int angle1; // variable to hold the angle for the servo motor
int angle2; // variable to hold the angle for the servo motor
int angle3; // variable to hold the angle for the servo motor

void setup() {
myServo1.attach(9); // attaches the servo on pin 9 to the servo object
myServo2.attach(10); // attaches the servo on pin 9 to the servo object
myServo3.attach(11); // attaches the servo on pin 9 to the servo object
Serial.begin(9600); // open a serial connection to your computer
}
 
void loop() {
potVal1 = analogRead(potPin1); // read the value of the potentiometer
potVal2 = analogRead(potPin2); // read the value of the potentiometer
potVal3 = analogRead(potPin3); // read the value of the potentiometer
// print out the value to the serial monitor
Serial.print("potVal1: ");
Serial.print(potVal1);
Serial.print("potVal2: ");
Serial.print(potVal2);
Serial.print("potVal3: ");
Serial.print(potVal3);

// scale the numbers from the pot
angle1 = map(potVal1, 0, 1023, 0, 179);
angle2 = map(potVal2, 0, 1023, 0, 179);
angle3 = map(potVal3, 0, 1023, 0, 179);

// print out the angle for the servo motor
Serial.print(", angle1: ");
Serial.println(angle1);
Serial.print(", angle2: ");
Serial.println(angle2);
Serial.print(", angle3: ");
Serial.println(angle3);

// set the servo position
myServo1.write(angle1);
myServo2.write(angle2);
myServo3.write(angle3);

// wait for the servo to get there
//delay(15);
delay(15);
}
