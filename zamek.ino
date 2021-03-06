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
#include "hardware.h"
#include "auth.h"

#include "lock.h"

#include "tamperProtection.h"

EthernetUDP udp;

const bool DOOR_OPENED = true;
const bool DOOR_CLOSED = false;

const bool BUTTON_PRESSED = false;
const bool BUTTON_RELEASED = true;

bool previousDoorState = DOOR_CLOSED;
bool previousButtonState = true;

int doorEvent = 0;

int soundDelayCounter = 0;

int udpResponseCounter = 0;

int lastCardChkCounter = 0;
uint8_t lastCardChk = 0;

void bootDelay();
void lockDoor();
void unlockDoor();
void unlockDoorForce();
void cardAccepted();
void cardRejected();
static void processIncomingDatagrams();
void udpSendPacket(const char* data, int len = -1);

void setup()
{
	hardwareInit();

	readerInit();
	tamperProtectionInit();
}

void loop()
{
	tamperProtectionProcess();
	readerProcess();

	processIncomingDatagrams();

	// decrementing counters every 1ms
	static unsigned long lastMs = 0;
	if (millis() != lastMs)
	{
		lastMs = millis();

		if (soundDelayCounter)
			soundDelayCounter--;

		lockEventTick();

		if (udpResponseCounter)
			udpResponseCounter--;

		if (lastCardChkCounter)
			lastCardChkCounter--;

		// ping server
		static unsigned long lastPingCounter = 2000;
		if (lastPingCounter)
		{
			lastPingCounter--;
			if (lastPingCounter == 0)
			{
				lastPingCounter = 2000;
				udpSendPacket("*");
			}
		}
	}

	// processing button events
	static unsigned long buttonEvent = 0;
	bool button = digitalRead(pinButtonSwitch);
	if (button != previousButtonState)
	{
		unsigned long time = millis() - buttonEvent;

		if (button == BUTTON_RELEASED)
		{
			// force door unlock in case of staying in wrong state
			if (time > forceDoorUnlockPressTime)
			{
				if (isDoorLocked)
					lockDoorForce();
				else
					unlockDoorForce();
			}
			else if (time > debounceDelay)
			{
				// sending notification with button press time
				char buf[10];
				int len = snprintf(buf, sizeof(buf), "%%%05lu", time);
				udpSendPacket(buf, len);

				if (isDoorLocked)
					unlockDoor();
				else if (previousDoorState == DOOR_CLOSED)
					lockDoor();
			}
		}

		buttonEvent = millis();
		previousButtonState = button;
	}
	if (button == BUTTON_PRESSED)
	{
		unsigned long t = millis() - buttonEvent;
		if (t >= forceDoorUnlockPressTime && t <= forceDoorUnlockPressTime + 100)
			tone(pinPiezo, toneAccepted * 2, 100);
		else if (t >= customActionPressTime && t <= customActionPressTime + 100)
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
	if (reedToDebounce && time > debounceDelay)
	{
		reedToDebounce = false;
		// sending notification about door state
		char buf[12];
		int doorState = door == DOOR_CLOSED ? 0 : 1;
		int len = snprintf(buf, sizeof(buf), "^%d|%08ld", doorState, time);
		udpSendPacket(buf, len);

		if (door == DOOR_CLOSED)
		{
			lockDoor();
		}
		if (door == DOOR_OPENED)
		{
			// when door are opened during locking time, unlock them, but only
			// within specific time since locking started (to prevent from opening
			// due to reed switch problems)
			if (isDoorLocked && doorRevertCounter)
			{
				unlockDoor();
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
	if (lastCardChkCounter == 0)
	{
		lastCardChk = cardChk;
		lastCardChkCounter = 500;
	}
	else // it's second number, check its checksum with previous one
	{
		if (cardChk == lastCardChk)
		{
			// if two consecutive card numbers are equal try to authorize card locally and
			// if it is not in local database, send authorization request to server
			lastCardChkCounter = 0;
			char buf[1 + LENGTH];
			memcpy(buf + 1, readerCardNumber, LENGTH);
			if (authCheckLocal())
			{
				cardAccepted();
				buf[0] = '!';
			}
			else
			{
				udpResponseCounter = 1000;
				buf[0] = '@';
			}
			udpSendPacket(buf, sizeof(buf));
		}
		else
		{
			// if we have another card, store its number hoping next will be the same
			lastCardChk = cardChk;
			lastCardChkCounter = 500;
		}
	}
}

void cardAccepted()
{
	if (!soundDelayCounter)
	{
		tone(pinPiezo, toneAccepted, toneDuration);
		soundDelayCounter = 500;
	}
	if (isDoorLocked)
		unlockDoor();
}
void cardRejected()
{
	if (!soundDelayCounter)
	{
		tone(pinPiezo, toneRejected, toneDuration);
		soundDelayCounter = 500;
	}
}

void processIncomingDatagrams()
{
	int packetSize = udp.parsePacket();
	if (packetSize)
	{
		if (packetSize == 3)
		{
			char msgBuf[3];
			udp.read(msgBuf, 3);
			if (udpResponseCounter) // process only if we are waiting for any response packet
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
}
void udpSendPacket(const char* data, int len)
{
	udp.beginPacket(serverIp, 10000);
	if (len == -1)
		udp.write(data);
	else
		udp.write((uint8_t*)data, len);
	udp.endPacket();
}
