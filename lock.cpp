#include "lock.h"

#include "config.h"
#include <Servo.h> 

Servo servo;

bool isDoorLocked = true; //assumed state of door lock on uC power on
int doorRevertCounter = 0;

int lockTransitionTimeCounter = 0;

void servoDoTime(int angle, int time);

void lockEventTick()
{
	if (lockTransitionTimeCounter)
	{
		lockTransitionTimeCounter--;
		if (lockTransitionTimeCounter == 0)
			servo.detach();
	}

	if (doorRevertCounter)
		doorRevertCounter--;
}

void servoDoTime(int angle, int time)
{
	servo.attach(pinServo);
	servo.write(angle);
	lockTransitionTimeCounter = time;
}
void unlockDoor()
{
	if (!isDoorLocked)
		return;

	int timeLeft = lockTransitionTime;

	// if within locking process, calculate time to revert process
	if (lockTransitionTimeCounter)
		timeLeft = lockTransitionTime - lockTransitionTimeCounter;

	servoDoTime(servoUnlockAngle, timeLeft);
	isDoorLocked = false;
}
void lockDoor()
{
	if (isDoorLocked)
		return;

	int timeLeft = lockTransitionTime;

	// if within unlocking process, calculate time to revert process
	if (lockTransitionTimeCounter)
		timeLeft = lockTransitionTime - lockTransitionTimeCounter + lockTransitionDriftTime;

	servoDoTime(servoLockAngle, timeLeft);
	isDoorLocked = true;
	// we can start revert process only within some time since lock started,
	// this protects from opening doors in case of reed switch problems
	doorRevertCounter = doorRevertTimeout;
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
