// https://forum.arduino.cc/index.php?topic=239959.0

#include <Servo.h>

#define mainArm 0
#define jib 1
#define bucket 2
#define slew 3

Servo servo[4];

byte angle[4] = {90, 90, 90};

byte potPin[4] = {A0, A1, A2};
byte servoPin[4] = {9, 10, 11};

void setup() {

  Serial.begin(9600);
  Serial.println("Starting DiggerCode.ino");


  for (byte n = 0; n < 3; n++) {
    servo[n].attach(servoPin[n]);
  }
}

void loop() {
  readPotentiometers();
  moveServos();
  delay(10);
}

void readPotentiometers() {
  int potVal;
  for (byte n = 0; n < 4; n++) {
    potVal = analogRead(potPin[n]);
    
    if (potVal < 450) {
      angle[n] += 1;
      if (angle[n] > 170) {
        angle[n] = 170;
      }
    }
    
    if (potVal > 570) {
      angle[n] -= 1;
      if (angle[n] < 10) {
        angle[n] = 10;
      }
    }

  }
}

void moveServos() {
  for (byte n = 0; n < 4; n++) {
    servo[n].write(angle[n]);
  }
}
