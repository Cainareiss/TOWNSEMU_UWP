/* ysgamepad back end for the Tsugaru UWP build.
   There is no polling here: the host application (which already reads the Xbox controllers for its own UI) injects
   the readings with YsGamePadUwpInject; the emulator's window connection then reads them with YsGamePadRead. */
#include <mutex>
#include <string.h>

#include "../ysgamepad.h"
#include "ysgamepad_uwp.h"

namespace
{
constexpr int NUM_PADS=4;
std::mutex lock;
YsGamePadReading pads[NUM_PADS];
}

extern "C" void YsGamePadUwpInject(int gamePadId,const struct YsGamePadReading *reading)
{
	if(0<=gamePadId && gamePadId<NUM_PADS && nullptr!=reading)
	{
		std::lock_guard <std::mutex> guard(lock);
		pads[gamePadId]=*reading;
	}
}

extern "C" void YsGamePadInitialize(void)
{
	std::lock_guard <std::mutex> guard(lock);
	for(auto &p : pads)
	{
		YsGamePadClear(&p);
	}
}

extern "C" void YsGamePadTerminate(void)
{
}

extern "C" void YsGamePadWaitReady(void)
{
}

extern "C" int YsGamePadGetNumDevices(void)
{
	return NUM_PADS;
}

extern "C" void YsGamePadRead(struct YsGamePadReading *reading,int gamePadId)
{
	if(0<=gamePadId && gamePadId<NUM_PADS)
	{
		std::lock_guard <std::mutex> guard(lock);
		*reading=pads[gamePadId];
	}
	else
	{
		YsGamePadClear(reading);
	}
}
