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

int delayTime = 0;

int lockTransitionTimeTimeout = 0;
int udpResponseTimeout = 0;

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


void authAccepted();
void authRejected();


EthernetUDP udp;
void setup()
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


#ifdef STASZEK_MODE
	// RFID reader we are actualy using, implemented according to the UNIQUE standard
	Serial.begin(57600);
#else
	// RFID reader bought from chinese guys; it is violating every single standard
	Serial.begin(9600);
#endif
	// enable internal pull-ups
	digitalWrite(pinButtonSwitch, HIGH);
	digitalWrite(pinReedSwitch, HIGH);

	Ethernet.begin(mac, ip);
	udp.begin(10000);

	pinMode(9, OUTPUT);
	digitalWrite(9, LOW);
}

void loop()
{
	readerProcess();

  int packetSize = udp.parsePacket();
  // if (udp.available())
	if (packetSize)
	{
		if (packetSize == 3)
		{
			char msgBuf[3];
			udp.read(msgBuf, 3);
			if (udpResponseTimeout)
			{
				if (strncmp(msgBuf, ">CO", 3) == 0)
				{
					authAccepted();
				}
				if (strncmp(msgBuf, ">CB", 3) == 0)
				{
					authRejected();
				}
			}
		}
		else
		{
			while (packetSize--)
			{
				udp.read();
			}
		}
	}

	static unsigned long lastMs = 0;
	if (millis() != lastMs) // done every 1ms
	{
		lastMs = millis();

		if (toneDisableTime)
		{
			toneDisableTime--;
			if (toneDisableTime == 0)
				noTone(pinPiezo);
		}

		if (delayTime)
			delayTime--;

		if (lockTransitionTimeTimeout)
		{
			lockTransitionTimeTimeout--;
			if (lockTransitionTimeTimeout == 0)
			{
				servo.detach();
			}
		}

		if (udpResponseTimeout)
			udpResponseTimeout--;
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

int lastNumberChk = 0;

void onReaderNewCard()
{

	{
		// dumpCardToSerial();
#ifdef DEBUG
		Serial.print("VALID\n");
#endif
		if (auth_checkLocal())
		{
			authAccepted();
		}
		return true;
	}
#else
	inline bool isBufferValid(int cyclicBufferPosition){
		if(buffer[bufferIndex(cyclicBufferPosition+1)] == '0' && buffer[bufferIndex(cyclicBufferPosition+2)] == 'x')
			return true;
		else
		{
			udp.beginPacket(srvIp, 10000);
			udp.write("@");
			udp.write((const uint8_t*)readerCardNumber, LENGTH);
			udp.write("\n");
			udp.endPacket();
			udpResponseTimeout = 1000;
		}
	}
#endif

inline void dumpCardToSerial(){
	Serial.write("Copy from buffer: ");
	for (int i = 0; i < LENGTH; ++i)
		Serial.write(card[i]);
	Serial.write(";\n");
}

// doors
void servoDo(int angle)
{
	// return;
	servo.attach(pinServo);
	servo.write(angle);
	lockTransitionTimeTimeout = lockTransitionTime;
	// delay(lockTransitionTime);
	// servo.detach();
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

void authAccepted()
{
	if (delayTime == 0)
	{
		tone(pinPiezo, toneAccepted);
		toneDisableTime = toneDuration;
		toneEnabled = 1;
		delayTime = 500;
	}
	authReportOpened();
	if (isDoorLocked)
		unlockDoor();
}
void authRejected()
{
	if (delayTime == 0)
	{
		tone(pinPiezo, toneRejected);
		toneDisableTime = toneDuration;
		toneEnabled = 1;
		delayTime = 500;
	}
}
