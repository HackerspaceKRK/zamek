#ifndef __LOCK_H__
#define __LOCK_H__

extern bool isDoorLocked;
extern int doorServerRevertTimeout;

void lockInit();
void lockProcess();
void lockEvent1MS();

void unlockDoorForce();
void lockDoorForce();
void unlockDoor();
void lockDoor();

#endif
