#ifndef __LOCK_H__
#define __LOCK_H__

extern bool isDoorLocked;

void lockInit();
void lockProcess();
void lockEvent1MS();

void servoDo(int angle);
void servoDoTime(int angle, int time);
void unlockDoor();
void unlockDoorForce();
void lockDoor();
void lockRevert();

#endif
