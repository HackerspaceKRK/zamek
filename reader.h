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
void reader_enable();
void reader_disable();
bool reader_isBufferValid();

// public functions
extern void onReaderNewCard();

void readerInit()
{
	reader_enable();
}
void readerProcess()
{
	// clear buffer if we are in the middle of gap between two frames and we have some data (n<LENGTH) in the buffer
	if (reader_bufferIdx > 0 && millis() - reader_lastReceiveTime >= timeBetweenFrames * 2 / 5)
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
			reader_disable(); // disabling communicaion to prevent internal buffer filling
			if (reader_isBufferValid())
				onReaderNewCard();
			reader_enable();
			Serial.flush();
			reader_bufferIdx = 0;
		}
	}
	reader_lastReceiveTime = millis();
}

void reader_enable()
{
#ifdef STASZEK_MODE
	// RFID reader we are actualy using, implemented according to the UNIQUE standard
	Serial.begin(57600);
#else
	// RFID reader bought from chinese guys; it is violating every single standard
	Serial.begin(9600);
#endif
}
void reader_disable()
{
	Serial.end();
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
