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
#include <avr/wdt.h>

#include "config.h"

#include "reader.h"
#include "auth.h"

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

int soundDelayTimeout = 0;

int lockTransitionTimeTimeout = 0;
int doorServerRevertTimeout = 0;

int udpResponseTimeout = 0;

int lastCardChkTimeout = 0;
uint8_t lastCardChk = 0;

void playSong();
void lockDoor();
void unlockDoor();
void unlockDoorForce();
void cardAccepted();
void cardRejected();

void setup()
{
	wdt_enable(WDTO_2S);

	pinMode(9, OUTPUT);
	digitalWrite(9, LOW);

	readerInit();

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

		if (lockTransitionTimeTimeout)
		{
			lockTransitionTimeTimeout--;
			if (lockTransitionTimeTimeout == 0)
				servo.detach();
		}

		if (doorServerRevertTimeout)
			doorServerRevertTimeout--;

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
				udp.beginPacket(srvIp, 10000);
				udp.write("*");
				udp.endPacket();
			}
		}
	}

	// processing button events
	static unsigned long buttonEvent = 0;
	bool button = digitalRead(pinButtonSwitch);
	if (button != previousButtonState)
	{
		unsigned long time = millis() - buttonEvent;

		if (button == released)
		{
			// force door unlock in case of staying in wrong state
			if (time > 20000)
			{
				unlockDoorForce();
			}
			else if (time > 200)
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
		udp.beginPacket(srvIp, 10000);
		char buf[12];
		int len = snprintf(buf, 12, "^%d|%08ld", door == close ? 0 : 1, time);
		udp.write((uint8_t*)buf, len);
		udp.endPacket();

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
				int timeLeft = lockTransitionTime - lockTransitionTimeTimeout;
				servoDo(servoUnlockAngle);
				lockTransitionTimeTimeout = timeLeft;
				isDoorLocked = false;
			}
		}
	}

	wdt_reset();
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
		else
		{
			// if we have another card, store its number hoping next will be the same
			lastCardChk = cardChk;
			lastCardChkTimeout = 500;
		}
	}
}

// doors
void servoDo(int angle)
{
	servo.attach(pinServo);
	servo.write(angle);
	lockTransitionTimeTimeout = lockTransitionTime;
}
void servoDoTime(int angle, int time)
{
	servo.attach(pinServo);
	servo.write(angle);
	lockTransitionTimeTimeout = time;
}
void unlockDoor()
{
	if (!isDoorLocked)
		return;
	servoDo(servoUnlockAngle);
	isDoorLocked = false;
}
void unlockDoorForce()
{
	servoDo(servoUnlockAngle);
	isDoorLocked = false;
}
void lockDoor()
{
	if (isDoorLocked)
		return;
	servoDo(servoLockAngle);
	isDoorLocked = true;
	doorServerRevertTimeout = 3000;
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
