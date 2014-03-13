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

Servo servo;
EthernetUDP udp;
bool isDoorLocked = true; //assumed state of door lock on uC power on

const bool open = true;
const bool close = false;

bool previousDoorState = close;
bool previousButtonState = true;

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

void playSong();
void lockDoor();
void unlockDoor();
bool isCardAuthorized();
void cleanBuffer();
void reportOpened();
bool isBufferValid(int cyclicBufferPosition);
void copyFromBuffer(int cyclicBufferPosition);
void dumpCardToSerial();
void cardAccepted();
void cardRejected();

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

	// playing song in order to properly... boot the device
	// we really need this time (about 7-10 secs)
	playSong();

	digitalWrite(9, HIGH);
}

void playSong()
{
#define NOTE_E 329
#define NOTE_F 349
#define NOTE_G 392
#define NOTE_D 293
#define NOTE_C 261
	int tones[] = { NOTE_E, NOTE_E, NOTE_F, NOTE_G, NOTE_G, NOTE_F, NOTE_E,
		NOTE_D, NOTE_C, NOTE_C, NOTE_D, NOTE_E, NOTE_E, NOTE_D, NOTE_D };

	for (int i = 0; i < 15; i++)
	{
		int tm = 500;
		if (i == 12) tm = 700;
		else if (i == 13 || i == 14) tm = 250;
		tone(pinPiezo, tones[i], tm);
		delay(tm);
		wdt_reset();
	}
}

void loop()
{
	readerProcess();

	// process incoming UDP datagrams
	int packetSize = udp.parsePacket();
	if (packetSize)
	{
		if (packetSize == 3)
		{
			char msgBuf[3];
			udp.read(msgBuf, 3);
			if (udpResponseTimeout) // process only if we are waiting for any response packet
			{
				if (strncmp(msgBuf, ">CO", 3) == 0)
					cardAccepted();
				else if (strncmp(msgBuf, ">CB", 3) == 0)
					cardRejected();
			}
		}
		else
		{
			while (packetSize--) // just flush (I don't know if this is needed,
				udp.read();        // Arduino docs doesn't say anything about unprocessed packets)
		}
	}

	// decrementing timeouts every 1ms
	static unsigned long lastMs = 0;
	if (millis() != lastMs)
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
				servo.detach();
		}

		if (udpResponseTimeout)
			udpResponseTimeout--;

		if (lastCardChkTimeout)
			lastCardChkTimeout--;
	}
#endif

	// processing button events
	static unsigned long buttonEvent = 0;
	bool button = digitalRead(pinButtonSwitch);
	if (button != previousButtonState)
	{
		unsigned long time = millis() - buttonEvent;

		if (button == released)
		{
			if (time > 200)
			{
				// sending notification with button press time
				udp.beginPacket(srvIp, 10000);
				char buf[10];
				int len = snprintf(buf, 10, "%%%05d", time);
				udp.write((uint8_t*)buf, len);
				udp.endPacket();

				if (isDoorLocked)
					unlockDoor();
			}
		}

		buttonEvent = millis();
		previousButtonState = button;
	}

	// processing reed switch events
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
			// sending notification about door state
			udp.beginPacket(srvIp, 10000);
			char buf[10];
			int len = snprintf(buf, 10, "^%d|%06ld", door == close ? 0 : 1, time);
			udp.write((uint8_t*)buf, len);
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
	// compute card checksum
	uint8_t cardChk = 0;
	for (int i = 0; i < LENGTH; i++)
		cardChk ^= readerCardNumber[i];

	// if there is no previous card, store its checksum and wait for next number
	if (lastCardChkTimeout == 0)
	{
		lastCardChk = cardChk;
		lastCardChkTimeout = 500;
	}
	else // it's second number, check its checksum with previous
	{
		if (cardChk == lastCardChk)
		{
			// if two consecutive cards numbers are equal try to authorize card locally and
			// if it is not in local database, send authorization request to server
			lastCardChkTimeout = 0;
			if (authCheckLocal())
			{
				cardAccepted();

				udp.beginPacket(srvIp, 10000);
				udp.write("!");
				udp.write((uint8_t*)readerCardNumber, LENGTH);
				udp.endPacket();
			}
			else
			{
				udp.beginPacket(srvIp, 10000);
				udp.write("@");
				udp.write((uint8_t*)readerCardNumber, LENGTH);
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
			// if we have another card, store its number hoping next will be the same
			lastCardChk = cardChk;
			lastCardChkTimeout = 500;
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
	servo.attach(pinServo);
	servo.write(angle);
	lockTransitionTimeTimeout = lockTransitionTime;
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

void cardAccepted()
{
	if (soundDelayTimeout == 0)
	{
		tone(pinPiezo, toneAccepted);
		toneDisableTimeout = toneDuration;
		toneEnabled = 1;
		soundDelayTimeout = 500;
	}
	if (isDoorLocked)
		unlockDoor();
}
void cardRejected()
{
	if (soundDelayTimeout == 0)
	{
		tone(pinPiezo, toneRejected);
		toneDisableTimeout = toneDuration;
		toneEnabled = 1;
		soundDelayTimeout = 500;
	}
}
