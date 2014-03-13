#ifndef __READER_H__
#define __READER_H__

#ifdef STASZEK_MODE
//format: CardNumber_50ms_CardNumber
#define BUFSIZE LENGTH
#define OFFSET 0
#else
//format: SN CardNumber\n
#define BUFSIZE (6 + LENGTH)
#define OFFSET 1
#endif

// private variables
uint8_t reader_bufferIdx = 0;
unsigned long reader_lastReceiveTime;

// public variables
char readerCardNumber[BUFSIZE];

// private functions
bool reader_isBufferValid();

// public functions
extern void onReaderNewCard();

void readerProcess()
{
	if (reader_bufferIdx > 0 && (millis() - reader_lastReceiveTime) >= timeBetweenFrames * 2 / 5)
	{
		reader_bufferIdx = 0;
	}
}

// private implementation
void serialEvent()
{
	while (Serial.available())
	{
		readerCardNumber[reader_bufferIdx++] = Serial.read();
		if (reader_bufferIdx == LENGTH)
		{
			// digitalWrite(9, HIGH);
			Serial.end();
			if (reader_isBufferValid())
				onReaderNewCard();
			Serial.begin(57600);
			Serial.flush();
			reader_bufferIdx = 0;
		}
	}
	reader_lastReceiveTime = millis();
	// digitalWrite(9, LOW);
}

#ifdef STASZEK_MODE
bool reader_isBufferValid()
{
	return true; // always valid...
}
#else
bool reader_isBufferValid()
{
	return readerCardNumber[1] == '0' && readerCardNumber[2] == 'x';
}
#endif

#endif
