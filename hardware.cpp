#include "hardware.h"

#include "config.h"
#include <Ethernet.h> 
#include <avr/wdt.h>

extern EthernetUDP udp;

static void bootDelay();

void hardwareInit()
{
	wdt_enable(WDTO_2S);

	// enable internal pull-ups
	digitalWrite(pinButtonSwitch, HIGH);
	digitalWrite(pinReedSwitch, HIGH);

	pinMode(pinLed, OUTPUT);
	digitalWrite(pinLed, LOW);

	uint8_t macTmp[6];
	memcpy(macTmp, mac, 6);
	Ethernet.begin(macTmp, ip);
	udp.begin(10000);

	// delaying in order to properly boot the device
	// we really need this time (about 7-10 secs)
	bootDelay();
}

void enableLed()
{
	digitalWrite(pinLed, HIGH);
}
void disableLed()
{
	digitalWrite(pinLed, LOW);
}

static void bootDelay()
{
	const int soundFreq = 100;
	const int bootTime = 7500;
	const int bootPartTime = 500;

	tone(pinPiezo, toneAccepted, soundFreq);
	for (uint8_t i = 0; i < bootTime / bootPartTime; i++)
	{
		delay(bootPartTime);
		wdt_reset();
	}
	tone(pinPiezo, toneAccepted, soundFreq);
}
