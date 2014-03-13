#include "lock.h"


#include "config.h"
#include <Servo.h> 

Servo servo;

bool isDoorLocked = true; //assumed state of door lock on uC power on

int lockTransitionTimeTimeout = 0;
int doorServerRevertTimeout = 0;

void lockInit()
{
}
void lockProcess()
{

}
void lockEvent1MS()
{
	if (lockTransitionTimeTimeout)
	{
		lockTransitionTimeTimeout--;
		if (lockTransitionTimeTimeout == 0)
			servo.detach();
	}

	if (doorServerRevertTimeout)
		doorServerRevertTimeout--;
}

void servoDo(int angle)
{
	servo.attach(pinServo);
	servo.write(angle);
	lockTransitionTimeTimeout = lockTransitionTime;
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
	servoDo(servoUnlockAngle);
	isDoorLocked = false;
}
void unlockDoorForce()
{
	servoDo(servoUnlockAngle);
	isDoorLocked = false;
}
void lockDoor()
{
	if (isDoorLocked)
		return;
	servoDo(servoLockAngle);
	isDoorLocked = true;
	doorServerRevertTimeout = 3000;
}
void lockRevert()
{
	if (doorServerRevertTimeout)
	{
		int timeLeft = lockTransitionTime - lockTransitionTimeTimeout;
		servoDo(servoUnlockAngle);
		lockTransitionTimeTimeout = timeLeft;
		isDoorLocked = false;
	}
}
