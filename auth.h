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
	char data[20];
	sprintf(data, "card=%s", readerCardNumber);
	auth_client.put("/api/v1/opened", data);
}

// private implementation
bool auth_checkRemote()
{
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
	return strcmp_P(addr, readerCardNumber) == 0;
}

#endif
