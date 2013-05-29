/*
 * Copyright 2013 Jakub Kramarz <lenwe@lenwe.net>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");		   
 * you may not use this file except in compliance with the License.		   
 * You may obtain a copy of the License at								   
 *                                                                         
 *     http://www.apache.org/licenses/LICENSE-2.0						   
 *                                                                         
 * Unless required by applicable law or agreed to in writing, software	   
 * distributed under the License is distributed on an "AS IS" BASIS,	   
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 * 
 */

#include <avr/pgmspace.h>
#include <Servo.h> 


#define LENGTH 10

#define PROD :-)
#define DEBUG

//Długość numeru karty
#ifdef PROD
#define BUFSIZE 2*LENGTH
#else
#define BUFSIZE 6+LENGTH
#endif


#define SERVO_PIN 9
//pin D2
#define REED_INTERRUPT 0
//pin D3
#define SWITCH_INTERRUPT 1
//workaround for PROGMEM bug in gcc-c++
#define PROG_MEM __attribute__((section(".progmem.mydata"))) 

#define PIEZO 10

char card_00[] PROG_MEM = "1800100000";
char card_01[] PROG_MEM = "1800100000";
char card_02[] PROG_MEM = "1800100000";
char card_03[] PROG_MEM = "1800100000";
char card_04[] PROG_MEM = "1800100000";
char card_05[] PROG_MEM = "1800100000";
char card_06[] PROG_MEM = "1800100000";
char card_07[] PROG_MEM = "1800100000";
char card_08[] PROG_MEM = "1800100000";
char card_09[] PROG_MEM = "1800100000";

char card_10[] PROG_MEM = "1800100000";
char card_11[] PROG_MEM = "1800100000";
char card_12[] PROG_MEM = "1800100000";
char card_13[] PROG_MEM = "1800100000";
char card_14[] PROG_MEM = "1800100000";
char card_15[] PROG_MEM = "1800100000";
char card_16[] PROG_MEM = "1800100000";
char card_17[] PROG_MEM = "1800100000";
char card_18[] PROG_MEM = "1800100000";
char card_19[] PROG_MEM = "1800100000";

char card_20[] PROG_MEM = "1800100000";
char card_21[] PROG_MEM = "1800100000";
char card_22[] PROG_MEM = "1800100000";
char card_23[] PROG_MEM = "1800100000";


PROG_MEM const char *cards[] = {
  card_00,
  card_01,
  card_02,
  card_03,
  card_04,
  card_05,
  card_06,
  card_07,
  card_08,
  card_09,
  card_10,
  card_11,
  card_12,
  card_13,
  card_14,
  card_15,
  card_16,
  card_17,
  card_18,
  card_19,
  card_20,
  card_21,
  card_22,
  card_23, 
};
byte numOfCards = 23;



inline void detachInterrupts(){
  detachInterrupt(REED_INTERRUPT);
  detachInterrupt(SWITCH_INTERRUPT);
}

inline void attachInterrupts(){
  attachInterrupt(REED_INTERRUPT, closeDoor,  RISING);
  attachInterrupt(SWITCH_INTERRUPT, toggleDoor, RISING);
}

void setup() {
  #ifdef PROD
  Serial.begin(57600);
  #else
  Serial.begin(9600);
  #endif
  pinMode(13, OUTPUT);
  attachInterrupts();
}

char buffer[BUFSIZE];
char card[LENGTH];
char temp[LENGTH+1];
byte pos = 0;
Servo servo;
volatile boolean servoDo = false;
volatile boolean isOpened = false;

void loop() {
 if(servoDo){
   detachInterrupts();
   servo.attach(9);
   if(isOpened){
     servo.write(180);
   }else{
     servo.write(0);
   }
   delay(2000);
   servo.detach();
   servoDo = false;
   attachInterrupts();
  }
}

#ifdef DEBUG
void dump(){
  for(int i = 1; i <= BUFSIZE; i++)
    Serial.print(buffer[bufferIndex(pos+i)]);
  Serial.print("\n");
}
#endif

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


inline void cleanBuffer(){
 pos = 0;
 for(byte i = 0; i < BUFSIZE; i++)
  buffer[i]=0; 
}


#ifdef PROD

inline boolean isBufferValid(){
 for(byte i = 1; i <= LENGTH; i++){
  if(buffer[bufferIndex(pos + i)] != buffer[bufferIndex(pos + i + LENGTH)])
    return false;
 } 
 return true;
}
#else
inline boolean isBufferValid(){
 if(buffer[bufferIndex(pos+1)] == '0' and buffer[bufferIndex(pos+2)] == 'x')
   return true;
 else
   return false;
}
#endif

n
#ifdef PROD
inline void copyFromBuffer(){
  Serial.write("Copy from buffer:");
 for(byte i = 0; i < LENGTH; i++){
   card[i] = buffer[bufferIndex(pos + i + 1)];
   Serial.write(card[i]);
 }
 Serial.write(";\n");
}
#else
inline void copyFromBuffer(){
  Serial.write("Copy from buffer:");
 for(byte i = 0; i < LENGTH; i++){
   card[i] = buffer[bufferIndex(pos + i + 3)];
   Serial.write(card[i]);
 }
 Serial.write(";\n");
}
#endif

inline boolean isCardValid(){
  for(byte i = 0; i < numOfCards; i++){
    if(verifyCard(i)){
     tone(PIEZO, 2000, 100);
     return true; 
    }
  }
  tone(PIEZO, 100, 100);
  return false;
}

/*
inline boolean verifyCard(byte i){
    if(memcmp_P(card, (cards[i]), LENGTH) == 0)
      return true;
    return false;
}
*/

inline char charToUpper(const char c){
 if ( c >= 'a' and c <= 'z'){
   return c - ('a'-'A');
 }else{
   return c;
 }  
}

inline boolean verifyCard(byte i){
    strcpy_P(temp, (char*)pgm_read_word(&(cards[i])));
    for(int i = 0; i < LENGTH; i++){
       if(temp[i] != charToUpper(card[i]))
         return false;
    }
    return true;
}

inline byte bufferIndex(byte index){
 return index%(BUFSIZE);
}

inline void openDoor(){
  if(isOpened == false){
    digitalWrite(13, HIGH);
    isOpened = true;
    servoDo = true;
  }
}

inline void closeDoor(){
  if(isOpened == true){
    cleanBuffer();
    digitalWrite(13, LOW);
    isOpened = false;
    servoDo = true;
  }
}

inline void toggleDoor(){
  if(isOpened == true)
    closeDoor();
  else
    openDoor();
}
