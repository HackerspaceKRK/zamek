#ifndef __AUTH_H__
#define __AUTH_H__

#include "cards.h"

// private variables
RestClient auth_client = RestClient(server, 80);
int auth_remoteEvent = 0;

// private functions
bool auth_checkRemote();
bool auth_checkLocal();
bool auth_compareToStoredCard(int i);

// public functions
bool authIsCardAuthorized()
{
	return auth_checkLocal() || auth_checkRemote();
}
void authReportOpened()
{
	return;
	char data[20];
	sprintf(data, "card=%s", readerCardNumber);
	auth_client.put("/api/v1/opened", data);

	// udp.beginPacket(srvIp, 10000);
	// udp.write("!OPEN");
	// udp.write("\n");
	// udp.endPacket();
}

// private implementation
bool auth_checkRemote()
{
	return false;
	char data[20];
	sprintf(data, "card=%s", readerCardNumber);
	int retval = auth_client.post("/api/v1/checkCard", data);
	auth_remoteEvent = millis();
	if (retval == 200)
	{
		return true;
	}
	else
	{
		return false;
	}
}
bool auth_checkLocal()
{
	for (int i = 0; i < numOfCards; ++i)
		if (auth_compareToStoredCard(i))
			return true; 
	return false;
}
bool auth_compareToStoredCard(int i)
{
	char* addr = (char*)pgm_read_word(cards + i);
	// char card[LENGTH];
	// strcpy_P(card, addr);

	
	// udp.beginPacket(srvIp, 10000);
	// udp.write((const uint8_t*)readerCardNumber, LENGTH);
	// udp.write("\n");
	// udp.endPacket();

	// return false;
	return strncmp_P(readerCardNumber, addr, LENGTH) == 0;
}

#endif
