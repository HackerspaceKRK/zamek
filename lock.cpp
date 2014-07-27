#include "lock.h"

#include "config.h"
#include <Servo.h> 

Servo servo;

bool isDoorLocked = true; //assumed state of door lock on uC power on
int doorRevertTimeout = 0;

int lockTransitionTimeTimeout = 0;

void servoDoTime(int angle, int time);

void lockEventTick()
{
	if (lockTransitionTimeTimeout)
	{
		lockTransitionTimeTimeout--;
		if (lockTransitionTimeTimeout == 0)
			servo.detach();
	}

	if (doorRevertTimeout)
		doorRevertTimeout--;
}

void servoDoTime(int angle, int time)
{
	servo.attach(pinServo);
	servo.write(angle);
	lockTransitionTimeTimeout = time;
}
void unlockDoor()
{
	if (!isDoorLocked)
		return;

	int timeLeft = lockTransitionTime;

	// if within locking process, calculate time to revert process
	if (lockTransitionTimeTimeout)
		timeLeft = lockTransitionTime - lockTransitionTimeTimeout;

	servoDoTime(servoUnlockAngle, timeLeft);
	isDoorLocked = false;
}
void lockDoor()
{
	if (isDoorLocked)
		return;

	int timeLeft = lockTransitionTime;

	// if within unlocking process, calculate time to revert process
	if (lockTransitionTimeTimeout)
		timeLeft = lockTransitionTime - lockTransitionTimeTimeout + lockTransitionDriftTime;

	servoDoTime(servoLockAngle, timeLeft);
	isDoorLocked = true;
	// we can start revert process only within some time since lock started,
	// this protects from opening doors in case of reed switch problems
	doorRevertTimeout = doorRevertTime;
}

void lockDoorForce()
{
	servoDoTime(servoLockAngle, lockTransitionTime);
	isDoorLocked = true;
}
void unlockDoorForce()
{
	servoDoTime(servoUnlockAngle, lockTransitionTime);
	isDoorLocked = false;
}
