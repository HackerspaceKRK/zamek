/*

	Copyright 2013 Jakub Kramarz <lenwe@lenwe.net>

	Licensed under the Apache License, Version 2.0 (the "License");
	you may not use this file except in compliance with the License.
	You may obtain a copy of the License at

		http://www.apache.org/licenses/LICENSE-2.0

	Unless required by applicable law or agreed to in writing, software
	distributed under the License is distributed on an "AS IS" BASIS,
	WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
	See the License for the specific language governing permissions and
	limitations under the License.

*/
#include <Servo.h> 
#include <SPI.h>
#include <Ethernet.h>
#include "karty.h"
#include "ethernet.h"

/*
When using Arduino, connect:
- RX0:	RFID reader TX
- D2:	reed switch; second side to ground
- D3:	microswitch; second side to ground
- D9:	servo (the yellow cable...)
- D10:	piezo/speaker; second side to ground
*/

//RFID card number length in ASCII-encoded hexes
#define LENGTH 10

#define STASZEK_MODE
//#define DEBUG

#ifdef STASZEK_MODE
	//format: CardNumberCardNumber
	#define BUFSIZE 2*LENGTH
#else
	//format: SN CardNumber\n
	#define BUFSIZE 6+LENGTH
#endif

const int pinServo = 5;
const int pinPiezo = 6;

const int pinButtonSwitch = 2;
const int pinReedSwitch = 3;

const int toneAccepted = 1260; //Hz
const int toneRejected = 440;  //Hz
const int toneDuration = 100;  //microseconds

const int lockTransitionTime = 2000; // in microseconds

const int debounceDelay = 50; // miliseconds 

bool isDoorLocked = true; //assumed state of door lock on uC power on

// The config ends here. Also, here be dragons.

char buffer[BUFSIZE] = {0};
char card[LENGTH+1] = {0};
char temp[LENGTH+1] = {0};
Servo servo;

void setup() {
	#ifdef STASZEK_MODE
		//RFID reader we are actualy using, implemented according to the UNIQUE standard
		Serial.begin(57600);
	#else
		//RFID reader bought from chinese guys; it is violating every single standard
		Serial.begin(9600);
	#endif
	//enable internal pull-ups
	digitalWrite(pinButtonSwitch, HIGH);
	digitalWrite(pinReedSwitch, HIGH);
	Ethernet.begin(mac, ip);
}


bool previousDoorState = false;
bool previousButtonState = true;

const bool open = true;
const bool close = false;

const bool pressed = false;
const bool released = true;

int buttonEvent = 0;
int doorEvent = 0;

inline bool isStable(const int lastEvent){
	return (millis() - lastEvent) > debounceDelay;
}

void lockDoor();
void unlockDoor();
bool isCardAuthorized();
void cleanBuffer();
void turnTheLightOn();
bool isBufferValid(int cyclicBufferPosition);
void copyFromBuffer(int cyclicBufferPosition);
void dumpCardToSerial();
bool compareToAuthorizedCard(int i);


void loop() {
		bool button = digitalRead(pinButtonSwitch);
		if(button != previousButtonState){
			buttonEvent = millis();
		}
		if(isStable(buttonEvent) and button == pressed){
			if(isDoorLocked){
				unlockDoor();
			}
		}
		previousButtonState=button;

		bool door = digitalRead(pinReedSwitch);
		if(door != previousDoorState){
			doorEvent = millis();
		}
		if(isStable(doorEvent) and door == close){
			if(!isDoorLocked)
				lockDoor();
		}
		previousDoorState=door;

}

#ifdef DEBUG
	void dump(){
		for(int i = 1; i <= BUFSIZE; ++i)
			Serial.print(buffer[bufferIndex(cyclicBufferPosition+i)]);
		Serial.print("\n");
	}
#endif

void skipSerialBuffer(){
	while(Serial.available()){
		Serial.read();
	}
}
 void processCardNumber(){
	#ifdef DEBUG
		Serial.print("VALID\n");
	#endif
	if(isCardAuthorized()){
		unlockDoor();
		cleanBuffer();
		turnTheLightOn();
		return;
	}
}

inline int bufferIndex(int index){
	return index%(BUFSIZE);
}

int cyclicBufferPosition = 0;
//called by Arduino framework when there are available bytes in input buffer
void serialEvent(){
	while (Serial.available()) {
		buffer[cyclicBufferPosition] = Serial.read();
		cyclicBufferPosition = bufferIndex(cyclicBufferPosition + 1);
		if(isDoorLocked and isBufferValid(cyclicBufferPosition)){
			copyFromBuffer(cyclicBufferPosition);
			dumpCardToSerial();
			processCardNumber();
		}	
	}
}

inline void cleanBuffer(){
	cyclicBufferPosition = 0;
    memset(buffer, 0, BUFSIZE);
}

#ifdef STASZEK_MODE
	const int offset = 3;
#else
	const int offset = 1;
#endif
inline void copyFromBuffer(int cyclicBufferPosition){
	for(int i = 0; i < LENGTH; ++i)
		card[i] = buffer[bufferIndex(cyclicBufferPosition + i + offset)];
}

#ifdef STASZEK_MODE
	inline bool isBufferValid(int cyclicBufferPosition){
		for(byte i = 1; i <= LENGTH; ++i){
			if(buffer[bufferIndex(cyclicBufferPosition + i)] != buffer[bufferIndex(cyclicBufferPosition + i + LENGTH)])
				return false;
		}
		return true;
	}
#else
	inline bool isBufferValid(int cyclicBufferPosition){
		if(buffer[bufferIndex(cyclicBufferPosition+1)] == '0' && buffer[bufferIndex(cyclicBufferPosition+2)] == 'x')
			return true;
		else
			return false;
	}
#endif

inline void dumpCardToSerial(){
	Serial.write("Copy from buffer: ");
	for (int i = 0; i < LENGTH; ++i)
		Serial.write(card[i]);
	Serial.write(";\n");
}

inline bool isCardAuthorized(){
	for(int i = 0; i < numOfCards; ++i){
        	if(compareToAuthorizedCard(i)){
	        	tone(pinPiezo, toneAccepted, toneDuration);
	        	return true; 
        	}
	}
	tone(pinPiezo, toneRejected, toneDuration);
	return false;
}

inline bool compareToAuthorizedCard(int i){
	//copy number from progmem
	strcpy_P(temp, (char*)pgm_read_word(&(cards[i])));
	return strcmp(temp, card) == 0;
}

const int counterClockwise = 0;
const int clockwise = 180;

void servoDo(int angle){
	servo.attach(pinServo);
	servo.write(angle);
	delay(lockTransitionTime);
	servo.detach();
}

void turnTheLightOn(){
    if (client.connect(server, 80)) {
      client.println("GET /api/v1/doorlock HTTP/1.1");
      client.println("Host: light.local");
      client.println("Connection: close");
      client.println();
      client.stop();
    }
}

void unlockDoor(){
    servoDo(clockwise);
    isDoorLocked = false;

}
void lockDoor(){
	cleanBuffer();
	servoDo(counterClockwise);
	isDoorLocked = true;
}
