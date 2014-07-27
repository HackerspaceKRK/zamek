#ifndef __TAMPER_PROTECTION_H__
#define __TAMPER_PROTECTION_H__

void udpSendPacket(const char* data, int len);
void tamperProtectionInit();
void tamperProtectionProcess();

void tamperProtectionInit()
{
	pinMode(tamperProtectionSwitch, OUTPUT);
	digitalWrite(tamperProtectionSwitch, HIGH);
}
void tamperProtectionProcess()
{
	if (digitalRead(tamperProtectionSwitch) == LOW)
	{
		char data[] = "TAMPERTAMPER";
		udpSendPacket(data, strlen(data));
	}
}

#endif
