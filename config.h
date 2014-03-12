#ifndef __CONFIG_H__
#define __CONFIG_H__

#include <Arduino.h>
#include <IPAddress.h>

//RFID card number length in ASCII-encoded hexes
#define LENGTH 10

#define STASZEK_MODE
//#define DEBUG

const int toneAccepted = 1260; // in Hz
const int toneRejected = 440;  // in Hz
const int toneDuration = 100;  // milliseconds 

const int lockTransitionTime = 2000 - 300; // in milliseconds

const unsigned int debounceDelay = 200; // milliseconds 

const int servoLockAngle = 0;
const int servoUnlockAngle = 180;

// typical time between two consecutive frames with card number
const int timeBetweenFrames = 50;

const uint8_t mac[] = { 0xDE, 0xAD, 0xBE, 0xDD, 0xFE, 0xED };
const char server[] = "192.168.100.5";
const IPAddress ip(192,168,100,9);
const IPAddress serverIp(192,168,100,5);

// HARDWARE

/*
When using Arduino, connect:
- RX0: RFID reader TX
- D2:	 button; second side to ground
- D3:	 reed switch; second side to ground
- D5:	 servo (the yellow cable...)
- D6:	 piezo/speaker; second side to ground
- D10: tamper
*/

const int pinServo = 5;
const int pinPiezo = 6;

const int pinButtonSwitch = 2;
const int pinReedSwitch = 3;

const int tamperProtectionSwitch = 10;
#endif
