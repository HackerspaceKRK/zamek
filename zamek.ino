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
#include <RestClient.h>

#include "hardware.h"
#include "config.h"

#include "reader.h"
#include "auth.h"

Servo servo;
bool isDoorLocked = true; //assumed state of door lock on uC power on

bool previousDoorState = false;
bool previousButtonState = true;

const bool open = true;
const bool close = false;

const bool pressed = false;
const bool released = true;

int buttonEvent = 0;
int doorEvent = 0;

int toneDisableTime;
int toneEnabled = 0;

void lockDoor();
void unlockDoor();
void dumpCardToSerial();

inline bool isStable(const int lastEvent)
{
	return millis() - lastEvent > debounceDelay;
}

void setup()
{
	wdt_enable(WDTO_4S);

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

	pinMode(8, OUTPUT);
	digitalWrite(8, LOW);
}

void loop()
{
	readerProcess();

	static int lastMs = 0;
	if (millis() != lastMs) // done every 1sm
	{
		lastMs = millis();

		if (toneDisableTime)
		{
			toneDisableTime--;
			if (toneDisableTime == 0)
				noTone(pinPiezo);
		}
	}

	bool button = digitalRead(pinButtonSwitch);
	if (button != previousButtonState)
	{
		buttonEvent = millis();
	}
	if (isStable(buttonEvent) and button == pressed)
	{
		if (isDoorLocked)
		{
			unlockDoor();
		}
	}
	previousButtonState = button;

	bool door = digitalRead(pinReedSwitch);
	if (previousDoorState == open and door == close)
		lockDoor();
	previousDoorState = door;

	wdt_reset();
}

#ifdef DEBUG
void dump()
{
	for (int i = 1; i <= BUFSIZE; ++i)
		Serial.print(buffer[bufferIndex(cyclicBufferPosition+i)]);
	Serial.print("\n");
}
#endif

void onReaderNewCard()
{
	if (isDoorLocked)
	{
		// dumpCardToSerial();

#ifdef DEBUG
		Serial.print("VALID\n");
#endif
		if (authIsCardAuthorized())
		{
			tone(pinPiezo, toneAccepted);
			authReportOpened();
			unlockDoor();
		}
		else
		{
			tone(pinPiezo, toneRejected);
		}
		toneDisableTime = toneDuration;
		toneEnabled = 1;
	}
}
inline void dumpCardToSerial()
{
	Serial.write("Copy from buffer: ");
	for (int i = 0; i < LENGTH; ++i)
		Serial.write(readerCardNumber[i]);
	Serial.write(";\n");
}

// doors
void servoDo(int angle)
{
	servo.attach(pinServo);
	servo.write(angle);
	delay(lockTransitionTime);
	servo.detach();
}
void unlockDoor()
{
	servoDo(clockwise);
	isDoorLocked = false;
}
void lockDoor()
{
	servoDo(counterClockwise);
	isDoorLocked = true;
}
