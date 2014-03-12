#ifndef __CONFIG_H__
#define __CONFIG_H__

//RFID card number length in ASCII-encoded hexes
#define LENGTH 10

#define STASZEK_MODE
//#define DEBUG

const int toneAccepted = 1260; //Hz
const int toneRejected = 440;  //Hz
const int toneDuration = 100;  //microseconds

const int lockTransitionTime = 2000; // in microseconds

const int debounceDelay = 50; // miliseconds 
const int remoteDelay = 2000;

const int counterClockwise = 0;
const int clockwise = 180;

const int timeBetweenFrames = 50;

byte mac[] = { 0xDE, 0xAD, 0xBE, 0xDD, 0xFE, 0xED };
char server[] = "192.168.100.5";
IPAddress ip(192,168,100,9);

// HARDWARE

/*
When using Arduino, connect:
- RX0:	RFID reader TX
- D2:	reed switch; second side to ground
- D3:	microswitch; second side to ground
- D9:	servo (the yellow cable...)
- D10:	piezo/speaker; second side to ground
*/

const int pinServo = 5;
const int pinPiezo = 6;

const int pinButtonSwitch = 2;
const int pinReedSwitch = 3;

#endif
