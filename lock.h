#ifndef __LOCK_H__
#define __LOCK_H__

extern bool isDoorLocked;
extern int doorRevertCounter;

void lockEventTick();

void unlockDoorForce();
void lockDoorForce();
void unlockDoor();
void lockDoor();

#endif
