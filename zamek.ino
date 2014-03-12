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
#include <avr/wdt.h>
#include <RestClient.h>

#include "config.h"

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

const int debounceDelay = 200; // miliseconds 
const int remoteDelay = 2000;

bool isDoorLocked = true; //assumed state of door lock on uC power on

// The config ends here. Also, here be dragons.

char buffer[BUFSIZE] = {0};
char card[LENGTH+1] = {0};
char temp[LENGTH+1] = {0};
Servo servo;

void setup() {
	wdt_enable(WDTO_4S);
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

        pinMode(8, OUTPUT);
        digitalWrite(8, LOW);
}


bool previousDoorState = false;
bool previousButtonState = true;

const bool open = true;
const bool close = false;

const bool pressed = false;
const bool released = true;

int buttonEvent = 0;
int doorEvent = 0;

int cyclicBufferPosition = 0;
int lastSerialEventTime = 0;
bool eventReceived = false;

inline bool isStable(const int lastEvent){
	return (millis() - lastEvent) > debounceDelay;
}

void lockDoor();
void unlockDoor();
bool isCardAuthorized();
void cleanBuffer();
void reportOpened();
bool isBufferValid(int cyclicBufferPosition);
void copyFromBuffer(int cyclicBufferPosition);
void dumpCardToSerial();


unsigned int toneDisableTime;
int toneEnabled = 0;

int lastMs = 0;

void loop() {

		if(eventReceived and (millis() - lastSerialEventTime >= 20) ){
			eventReceived = false;
			cyclicBufferPosition = 0;
			if(isDoorLocked)
{
				copyFromBuffer(cyclicBufferPosition);
				dumpCardToSerial();
				processCardNumber();
			}
		}

if (millis() != lastMs)
{
  lastMs = millis();
  if (toneDisableTime)
  {
    toneDisableTime--;
  }
  else
  {
    noTone(pinPiezo);
  }
}


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
		if(previousDoorState == open and door == close)
                        lockDoor();
		previousDoorState=door;

		wdt_reset();
}

#ifdef DEBUG
	void dump(){
		for(int i = 1; i <= BUFSIZE; ++i)
			Serial.print(buffer[bufferIndex(cyclicBufferPosition+i)]);
		Serial.print("\n");
	}
#endif

void Buffer(){
	while(Serial.available()){
		Serial.read();
	}
}
 void processCardNumber(){
	#ifdef DEBUG
		Serial.print("VALID\n");
	#endif
	if(isCardAuthorized()){
		tone(pinPiezo, toneAccepted);
                toneDisableTime = toneDuration;
toneEnabled = 1;		
unlockDoor();
		//cleanBuffer();
		reportOpened();
		return;
	}else{
		tone(pinPiezo, toneRejected);
toneDisableTime = toneDuration;
toneEnabled = 1;
	}
}

inline int bufferIndex(int index){
	return index%(BUFSIZE);
}

//called by Arduino framework when there are available bytes in input buffer
void serialEvent(){
        digitalWrite(8, HIGH);
	while (Serial.available())
        {
                char temp = Serial.read();
                if(cyclicBufferPosition < 10){
		  buffer[cyclicBufferPosition++] = temp;
                }else{
                  Serial.flush();
                }
		
	}
	lastSerialEventTime = millis();
        eventReceived = true;
        digitalWrite(8, LOW);
}

inline void cleanBuffer(){
	cyclicBufferPosition = 0;
    memset(buffer, 0, BUFSIZE);
}

#ifdef STASZEK_MODE
	const int offset = 0;
#else
	const int offset = 1;
#endif
inline void copyFromBuffer(int cyclicBufferPosition){
	for(int i = 0; i < LENGTH; ++i)
		card[i] = buffer[i + offset];
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


inline bool compareToStoredCard(int i){
	//copy number from progmem
	strcpy_P(temp, (char*)pgm_read_word(&(cards[i])));
	return strcmp(temp, card) == 0;
}

RestClient client = RestClient(server, 80);

void reportOpened(){
	char data[20];
	sprintf(data, "card=%s", card);
	client.put("/api/v1/opened", data);
}

int remoteEvent = 0;
bool checkRemote(){
	char data[20];
	sprintf(data, "card=%s", card);
	int retval = client.post("/api/v1/checkCard", data);
        remoteEvent = millis();
	if(retval == 200){
		return true;
	}else{
		return false;
	}
}

bool checkLocal(){
	for(int i = 0; i < numOfCards; ++i){
        	if(compareToStoredCard(i)){
	        	return true; 
        	}
	}
	return false;
}
inline bool isCardAuthorized(){
	if(checkLocal())
		return true;
	if(checkRemote())
		return true;
	return false;
}

const int counterClockwise = 0;
const int clockwise = 180;

void servoDo(int angle){
	servo.attach(pinServo);
	servo.write(angle);
	delay(lockTransitionTime);
	servo.detach();
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
