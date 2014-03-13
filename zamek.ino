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
// #include <RestClient.h>

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

int doorEvent = 0;

int toneDisableTimeout;
int toneEnabled = 0;

int soundDelayTimeout = 0;

int lockTransitionTimeTimeout = 0;
int udpResponseTimeout = 0;

int lastCardChkTimeout = 0;
uint8_t lastCardChk = 0;

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
	wdt_enable(WDTO_2S);

	pinMode(9, OUTPUT);
	digitalWrite(9, LOW);

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

#define NOTE_E (329)
#define NOTE_F (349)
#define NOTE_G (392)
#define NOTE_D (293)
#define NOTE_C (261)
	int tones[] = {
		NOTE_E,
		NOTE_E,
		NOTE_F,
		NOTE_G,
		NOTE_G,
		NOTE_F,
		NOTE_E,
		NOTE_D,
		NOTE_C,
		NOTE_C,
		NOTE_D,
		NOTE_E,
		NOTE_E,
		NOTE_D,
		NOTE_D,
	};

	for (int i = 0; i < 15; i++)
	{
		int tm = i >= 13 ? 250 : 500;
		if (i == 12) tm = 700;
		tone(pinPiezo, tones[i], tm);
		delay(tm);
		wdt_reset();
	}

	digitalWrite(9, HIGH);
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
				else if (strncmp(msgBuf, ">CB", 3) == 0)
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

		if (toneDisableTimeout)
		{
			toneDisableTimeout--;
			if (toneDisableTimeout == 0)
				noTone(pinPiezo);
		}

		if (soundDelayTimeout)
			soundDelayTimeout--;

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

		if (lastCardChkTimeout)
			lastCardChkTimeout--;
	}
#endif

	static unsigned long buttonEvent = 0;
	bool button = digitalRead(pinButtonSwitch);
	if (button != previousButtonState)
	{
		unsigned long time = millis() - buttonEvent;

		if (button == released)
		{
			if (time > 200)
			{
				udp.beginPacket(srvIp, 10000);
				char buf[10];
				sprintf(buf, "%%%05d", time);
				udp.write(buf);
				udp.endPacket();

				if (isDoorLocked)
					unlockDoor();
			}
		}

		buttonEvent = millis();
		previousButtonState = button;
	}

	static unsigned long reedEvent = 0;
	bool door = digitalRead(pinReedSwitch);
	if (previousDoorState != door)
	{
		unsigned long time = millis() - reedEvent;

		if (door == close)
		{
			lockDoor();
		}
		if (door == open)
		{
			isDoorLocked = false;
		}

		if (time > 20)
		{
			udp.beginPacket(srvIp, 10000);
			char buf[10];
			sprintf(buf, "^%d|%06ld", door == close ? 0 : 1, time);
			udp.write(buf);
			udp.endPacket();
		}

		reedEvent = millis();
		previousDoorState = door;
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

void onReaderNewCard()
{
	uint8_t cardChk = 0;
	for (int i = 0; i < LENGTH; i++)
		cardChk ^= readerCardNumber[i];

	if (lastCardChkTimeout == 0)
	{
		lastCardChk = cardChk;
		lastCardChkTimeout = 500;
	}
	else
	{
		if (cardChk == lastCardChk)
		{
			lastCardChkTimeout = 0;
			if (auth_checkLocal())
			{
				authAccepted();

				udp.beginPacket(srvIp, 10000);
				udp.write("!");
				udp.write((const uint8_t*)readerCardNumber, LENGTH);
				udp.endPacket();
			}
			else
			{
				udp.beginPacket(srvIp, 10000);
				udp.write("@");
				udp.write((const uint8_t*)readerCardNumber, LENGTH);
				udp.endPacket();
				udpResponseTimeout = 1000;
			}
		}
		return true;
	}
#else
	inline bool isBufferValid(int cyclicBufferPosition){
		if(buffer[bufferIndex(cyclicBufferPosition+1)] == '0' && buffer[bufferIndex(cyclicBufferPosition+2)] == 'x')
			return true;
		else
		{
			lastCardChk = cardChk;
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
void unlockDoor()
{
	if (!isDoorLocked)
		return;
	servoDo(clockwise);
	isDoorLocked = false;
}
void lockDoor()
{
	if (isDoorLocked)
		return;
	servoDo(counterClockwise);
	isDoorLocked = true;
}

void authAccepted()
{
	if (soundDelayTimeout == 0)
	{
		tone(pinPiezo, toneAccepted);
		toneDisableTimeout = toneDuration;
		toneEnabled = 1;
		soundDelayTimeout = 500;
	}
	authReportOpened();
	if (isDoorLocked)
		unlockDoor();
}
void authRejected()
{
	if (soundDelayTimeout == 0)
	{
		tone(pinPiezo, toneRejected);
		toneDisableTimeout = toneDuration;
		toneEnabled = 1;
		soundDelayTimeout = 500;
	}
}
