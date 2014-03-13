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
#include <Ethernet.h> 
#include <SPI.h>
#include <avr/wdt.h>

#include "config.h"

#include "reader.h"
#include "auth.h"
#include "lock.h"

EthernetUDP udp;

const bool open = true;
const bool close = false;

bool previousDoorState = close;
bool previousButtonState = true;

const bool pressed = false;
const bool released = true;

int doorEvent = 0;

int soundDelayTimeout = 0;


int udpResponseTimeout = 0;

int lastCardChkTimeout = 0;
uint8_t lastCardChk = 0;

void playSong();
void lockDoor();
void unlockDoor();
void unlockDoorForce();
void cardAccepted();
void cardRejected();
void udpSendPacket(const char* data, int len = -1);

void setup()
{
	wdt_enable(WDTO_2S);

	pinMode(9, OUTPUT);
	digitalWrite(9, LOW);

	readerInit();
	lockInit();

	// enable internal pull-ups
	digitalWrite(pinButtonSwitch, HIGH);
	digitalWrite(pinReedSwitch, HIGH);

	uint8_t macTmp[6];
	memcpy(macTmp, mac, 6);
	Ethernet.begin(macTmp, ip);
	udp.begin(10000);

	// playing song in order to properly... boot the device
	// we really need this time (about 7-10 secs)
	playSong();

	digitalWrite(9, HIGH);
}

void playSong()
{
#ifdef SILENT
	tone(pinPiezo, toneAccepted, 100);
	for (uint8_t i = 0; i < 15; i++)
	{
		delay(500);
		wdt_reset();
	}
	tone(pinPiezo, toneAccepted, 100);
#else
#define NOTE_E 329
#define NOTE_F 349
#define NOTE_G 392
#define NOTE_D 293
#define NOTE_C 261
	int tones[] = { NOTE_E, NOTE_E, NOTE_F, NOTE_G, NOTE_G, NOTE_F, NOTE_E,
		NOTE_D, NOTE_C, NOTE_C, NOTE_D, NOTE_E, NOTE_E, NOTE_D, NOTE_D };

	for (uint8_t i = 0; i < 15; i++)
	{
		int tm = 500;
		if (i == 12) tm = 700;
		else if (i == 13 || i == 14) tm = 250;
		tone(pinPiezo, tones[i], tm);
		delay(tm);
		wdt_reset();
	}
#endif
}

void loop()
{
	readerProcess();
	lockProcess();

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
			// just flush (I don't know if this is needed,
			// Arduino docs doesn't say anything about unprocessed packets)
			while (packetSize--)
				udp.read();
		}
	}

	// decrementing timeouts every 1ms
	static unsigned long lastMs = 0;
	if (millis() != lastMs)
	{
		lastMs = millis();

		if (soundDelayTimeout)
			soundDelayTimeout--;

		lockEvent1MS();

		if (udpResponseTimeout)
			udpResponseTimeout--;

		if (lastCardChkTimeout)
			lastCardChkTimeout--;

		// ping server
		static unsigned long lastPingTimeout = 2000;
		if (lastPingTimeout)
		{
			lastPingTimeout--;
			if (lastPingTimeout == 0)
			{
				lastPingTimeout = 2000;
				udpSendPacket("*");
			}
		}
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
			// force door unlock in case of staying in wrong state
			if (time > 15000)
			{
				if (isDoorLocked)
					lockDoorForce();
				else
					unlockDoorForce();
			}
			else if (time > 200)
			{
				// sending notification with button press time
				char buf[10];
				int len = snprintf(buf, 10, "%%%05d", time);
				udpSendPacket(buf, len);

				if (isDoorLocked)
					unlockDoor();
				else if (previousDoorState == close)
					lockDoor();
			}
		}

		buttonEvent = millis();
		previousButtonState = button;
	}
	if (button == pressed)
	{
		unsigned long t = millis() - buttonEvent;
		if (t >= 15000 && t <= 15100)
			tone(pinPiezo, toneAccepted * 2, 100);
		else if (t >= 5000 && t <= 5100)
			tone(pinPiezo, toneAccepted * 2, 100);
	}

	// processing reed switch events
	static unsigned long reedEvent = 0;
	static bool reedToDebounce = false;
	bool door = digitalRead(pinReedSwitch);
	if (previousDoorState != door)
	{
		reedEvent = millis();
		previousDoorState = door;
		reedToDebounce = true;
	}

	unsigned long time = millis() - reedEvent;
	if (reedToDebounce && time > 200)
	{
		reedToDebounce = false;
		// sending notification about door state
		char buf[12];
		int len = snprintf(buf, 12, "^%d|%08ld", door == close ? 0 : 1, time);
		udpSendPacket(buf, len);

		if (door == close)
		{
			lockDoor();
		}
		if (door == open)
		{
			// when door are opened during locking time, unlock door, but only
			// within specific time since locking started
			if (isDoorLocked && doorServerRevertTimeout)
			{
				unlockDoor();
			}
		}
	}

inline int bufferIndex(int index){
	return index%(BUFSIZE);
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
	else // it's second number, check its checksum with previous one
	{
		if (cardChk == lastCardChk)
		{
			// if two consecutive card numbers are equal try to authorize card locally and
			// if it is not in local database, send authorization request to server
			lastCardChkTimeout = 0;
			char buf[1 + LENGTH];
			memcpy(buf + 1, readerCardNumber, LENGTH);
			if (authCheckLocal())
			{
				cardAccepted();
				buf[0] = '!';
			}
			else
			{
				udpResponseTimeout = 1000;
				buf[0] = '@';
			}
			udpSendPacket(buf, sizeof(buf));
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
}

void cardAccepted()
{
	if (soundDelayTimeout == 0)
	{
		tone(pinPiezo, toneAccepted, toneDuration);
		soundDelayTimeout = 500;
	}
	if (isDoorLocked)
		unlockDoor();
}
void cardRejected()
{
	if (soundDelayTimeout == 0)
	{
		tone(pinPiezo, toneRejected, toneDuration);
		soundDelayTimeout = 500;
	}
}

void udpSendPacket(const char* data, int len)
{
	udp.beginPacket(srvIp, 10000);
	if (len == -1)
		udp.write(data);
	else
		udp.write((uint8_t*)data, len);
	udp.endPacket();
}
