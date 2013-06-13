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
#include <avr/pgmspace.h>
#include <Servo.h> 


/*
When using Arduino, connect:
- RX0:	RFID reader TX
- D2:	reed switch; second side to ground
- D3:	microswitch; second side to ground
- D9:	servo (the yellow cable...)
- D10:	piezo/speaker; second side to ground
*/


//RFID card number length in ASCII-encoded hexes
#define LENGTH 10

#define PROD
#define DEBUG

#ifdef PROD
//format: CardNumberCardNumber
#define BUFSIZE 2*LENGTH
#else
//format: SN CardNumber\n
#define BUFSIZE 6+LENGTH
#endif

//pin on bottom right at Atmega8, the first one PWM-enabled
#define SERVO_PIN 9
//pin above servo pin, the second one PWM-enabled
#define PIEZO 10

//pin D2 
#define REED_INTERRUPT 0
//pin D3
#define SWITCH_INTERRUPT 1

//workaround for PROGMEM bug in gcc-c++
#define PROG_MEM __attribute__((section(".progmem.mydata"))) 

#include "progmem.c"

byte numOfCards = sizeof(cards)/sizeof(char*);

char buffer[BUFSIZE];
char card[LENGTH];
char temp[LENGTH+1];
byte pos = 0;
Servo servo;

//equal to led's status
volatile boolean isOpened = false;



void setup() {
	#ifdef PROD
		//RFID reader we are actualy using, implemented according to the UNIQUE standard
		Serial.begin(57600);
	#else
		//RFID reader bought from chinese guys; it is violating every single standard
		Serial.begin(9600);
	#endif
	//enable internal pull-ups
	digitalWrite(2, HIGH); //INT0
	digitalWrite(3, HIGH); //INT1
	//set mode output for led
	pinMode(13, OUTPUT);
}


volatile bool lastReedState = false;
volatile bool lastSwitchState = true;

void loop() {
        if(lastSwitchState == false and !digitalRead(2)){
          toggleDoor();
          lastSwitchState=true;
        }else if(lastSwitchState == true and digitalRead(2)){
          lastSwitchState = false;
        }
        
        if(lastReedState == true and !digitalRead(3)){
          closeDoor();
          lastReedState=false;
        }else if(lastReedState == false and digitalRead(3)){
          lastReedState=true;
        }
}

#ifdef DEBUG
void dump(){
	for(int i = 1; i <= BUFSIZE; i++)
	Serial.print(buffer[bufferIndex(pos+i)]);
	Serial.print("\n");
}
#endif

//called by Arduino framework when there are available bytes in input buffer
void serialEvent(){
	while (Serial.available()) {
		buffer[pos] = Serial.read();
		#ifdef DEBUG
			dump();
		#endif
		if(!isOpened && isBufferValid()){
			#ifdef DEBUG
			Serial.print("VALID\n");
			#endif
			copyFromBuffer();
			if(isCardValid()){
			 openDoor();
			 cleanBuffer();
			 return;
			}
		}
		#ifdef DEBUG
		else if(isOpened){
			Serial.print("opened\n");
		}else{
			Serial.print("invalid\n");
		}
		#endif
		pos = bufferIndex(pos + 1);
	}
}


void cleanBuffer(){
	pos = 0;
	for(byte i = 0; i < BUFSIZE; i++)
		buffer[i]=0; 
}


#ifdef PROD
boolean isBufferValid(){
	for(byte i = 1; i <= LENGTH; i++){
		if(buffer[bufferIndex(pos + i)] != buffer[bufferIndex(pos + i + LENGTH)])
			return false;
	}
	return true;
}
#else
boolean isBufferValid(){
	if(buffer[bufferIndex(pos+1)] == '0' && buffer[bufferIndex(pos+2)] == 'x')
		return true;
	else
		return false;
}
#endif


#ifdef PROD
void copyFromBuffer(){
	Serial.write("Copy from buffer:");
	for(byte i = 0; i < LENGTH; i++){
		card[i] = buffer[bufferIndex(pos + i + 1)];
		Serial.write(card[i]);
	}
	Serial.write(";\n");
}
#else
void copyFromBuffer(){
	Serial.write("Copy from buffer:");
	for(byte i = 0; i < LENGTH; i++){
		card[i] = buffer[bufferIndex(pos + i + 3)];
		Serial.write(card[i]);
	}
	Serial.write(";\n");
}
#endif

boolean isCardValid(){
	for(byte i = 0; i < numOfCards; i++){
	if(verifyCard(i)){
	 tone(PIEZO, 2000, 100);
	 return true; 
	}
	}
	tone(PIEZO, 100, 100);
	return false;
}

char charToUpper(const char c){
	if ( c >= 'a' and c <= 'z'){
		return c - ('a'-'A');
	}else{
		return c;
	}  
}

/*
//better, but unfortunately - buggy
boolean verifyCard(byte i){
	if(memcmp_P(card, (cards[i]), LENGTH) == 0)
		return true;
	return false;
}
*/
boolean verifyCard(byte i){
	//copy number from progmem
	strcpy_P(temp, (char*)pgm_read_word(&(cards[i])));
	for(int i = 0; i < LENGTH; i++){
		if(temp[i] != charToUpper(card[i]))
			return false;
	}
	return true;
}

byte bufferIndex(byte index){
	return index%(BUFSIZE);
}


void servoDo(){
		servo.attach(9);
		if(isOpened){
			//clockwise
			servo.write(180);
		}else{
			//counter-clockwise
			servo.write(0);
		}
		delay(2000);
		servo.detach();
}

void openDoor(){
	if(isOpened == false){
		digitalWrite(13, HIGH);
		isOpened = true;
                servoDo();
	}
}

void closeDoor(){
	if(isOpened == true){
		cleanBuffer();
		digitalWrite(13, LOW);
		isOpened = false;
		servoDo();
	}
}

void toggleDoor(){
	if(isOpened == true)
		closeDoor();
	else
		openDoor();
}
