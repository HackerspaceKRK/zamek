#ifndef __AUTH_H__
#define __AUTH_H__

#include "cards.h"

// private variables

// private functions
static bool auth_compareToStoredCard(int i);

// public functions
bool authCheckLocal()
{
	for (int i = 0; i < numOfCards; ++i)
		if (auth_compareToStoredCard(i))
			return true; 
	return false;
}

// private implementation
static bool auth_compareToStoredCard(int i)
{
	char* addr = (char*)pgm_read_word(cards + i);
	return strncmp_P(readerCardNumber, addr, LENGTH) == 0;
}

#endif
