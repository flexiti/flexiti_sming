#ifndef INCLUDE_NTPSMING_H_
#define INCLUDE_NTPSMING_H_

#include "SystemClock.h"

extern DateTime SdateTime;
extern bool first;

class ntpClientSming
{
public:
	ntpClientSming()
	{
		// co 10 min
		ntpcp = new NtpClient("pool.ntp.org", 600000, NtpTimeResultDelegate(&ntpClientSming::ntpResult, this));
	};

	void ntpResult(NtpClient& client, time_t ntpTime)
	{
		SystemClock.setTime(ntpTime, eTZ_UTC);
	//	Serial.print("ntpClientDemo Callback Time_t = ");
	//	Serial.print(ntpTime);
		Serial.print(" Time synchro to : ");
		Serial.println(SystemClock.getSystemTimeString());
		if (!first)
		{
		 first=true;
		 SdateTime.setTime(SystemClock.now(eTZ_Local));
		 Serial.println(".. SYSTEM START TIME RECORDED ..");
		}
	}

private:
	NtpClient *ntpcp;
};




#endif /* INCLUDE_NTPSMING_H_ */
