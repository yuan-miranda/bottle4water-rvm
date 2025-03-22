#include <ESP32Servo.h>

#define SERVO_PIN_1 26
#define SERVO_PIN_2 27

Servo servoMotor1;
Servo servoMotor2;

void setup() {
  servoMotor1.attach(SERVO_PIN_1);
  servoMotor2.attach(SERVO_PIN_2);
}

void loop() {
  servoMotor1.write(0);
  delay(1000);
  servoMotor1.write(180);
  delay(1000);

  servoMotor2.write(0);
  delay(1000);
  servoMotor2.write(180);
  delay(1000);

}