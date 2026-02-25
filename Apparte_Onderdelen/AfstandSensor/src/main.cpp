// Simple program to demonstrate use of HC-SR04 sensor
// This code Measure the distance from a wall
// And prints the distance to the serial monitor
//
// F.Demonie 23/2/25
// =======================================================

#include <Arduino.h>

#define TRIG_PIN 5  // GPIO 5 for Trigger
#define ECHO_PIN 18 // GPIO 18 for Echo
#define SOUND_SPEED 0.034  // Speed of sound in cm/µs
 
void setup() {
  // Init Serial Monitor
  Serial.begin(9600);

  // Set Distance sensor PINS in correct mode
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
}
 
void loop() {
  long  duration = 0.0;
  float distance = 0.0;

  // Send a 10µs pulse to trigger pin
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  // Measure the echo pulse duration
  duration = pulseIn(ECHO_PIN, HIGH);

  // Convert time to distance
  distance = (float)((duration * SOUND_SPEED) / 2.0); 

  Serial.printf( "Distance = %5.2f cm\n", distance );
  
  // Delay 0.5sec before sending next message
  delay(500);
}