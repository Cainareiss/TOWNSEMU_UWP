/* LICENSE>>
Copyright 2020 Soji Yamakawa (CaptainYS, http://www.ysflight.com)

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

<< LICENSE */


#ifndef EVENTLOG_IS_INCLUDED
#define EVENTLOG_IS_INCLUDED
/* { */

#include <list>
#include <vector>
#include <string>
#include <chrono>
#include "cheapmath.h"

class TownsEventLog
{
public:
	enum
	{
		MODE_NONE,
		MODE_RECORDING,
		MODE_PLAYBACK
	};

	// File-Open and File-Exec logs will be logged only when call-stack is enabled.
	enum
	{
		EVT_LBUTTONDOWN,
		EVT_LBUTTONUP,
		EVT_RBUTTONDOWN,
		EVT_RBUTTONUP,
		EVT_TBIOS_MOS_START,
		EVT_TBIOS_MOS_END,
		EVT_FILE_OPEN,  // INT 21H AH=3DH
		EVT_FILE_EXEC,  // INT 21H AH=4BH
		EVT_KEYCODE,
		EVT_PAD0_A_DOWN,
		EVT_PAD0_A_UP,
		EVT_KEYPRESS,
		EVT_KEYRELEASE,
		EVT_REPEAT, // Go all the way back.
		EVT_PAD0_B_DOWN,
		EVT_PAD0_B_UP,
		EVT_PAD0_RUN_DOWN,
		EVT_PAD0_RUN_UP,
		EVT_PAD0_SEL_DOWN,
		EVT_PAD0_SEL_UP,
	NUMBER_OF_EVENT_TYPES
	};

	enum
	{
		REP_INFINITY=0x7fffffff
	};

	class Event
	{
	public:
		std::chrono::milliseconds  t;
		mutable std::chrono::time_point <std::chrono::system_clock> tPlayed;
		long long int townsTime;
		int eventType;
		Vec2i mos;
		int mosTolerance=0; // Not saved
		std::string fName;
		unsigned char keyCode[2];
		int repCount=0,repCountMax=REP_INFINITY;
	};

	int mode=MODE_NONE;
	bool received_MOS_start=false,received_MOS_end=false;
	std::list <Event>::iterator playbackPtr;
	bool dontWaitFileEventInPlayback=true;
	std::chrono::time_point <std::chrono::system_clock>  t0;
	long long int townsTime0;
	std::list <Event> events;



	TownsEventLog();
	~TownsEventLog();
	void CleanUp(void);


	/*! Programatically add an event.
	    e.t is taken as the duration.
	*/
	void AddEvent(Event e);


	/*! Sets t0 to std::chrono::system_clock::now().
	*/
	void BeginRecording(long long int townsTime);



	/*!
	*/
	void BeginPlayback(void);
private:
	void SkipPlaybackFileEvent(void);


public:
	void StopPlayBack(void);



public:
	/*! Called from FMTownsThread.
	*/
	inline void Interval(class FMTownsCommon &towns)
	{
		if(MODE_PLAYBACK==mode)
		{
			Playback(towns);
		}
	}
private:
	void Playback(class FMTownsCommon &towns);



public:
	/*!
	*/
	static std::string EventTypeToString(int evtType);


	void LogMouseStart(long long int townsTime);
	void LogMouseEnd(long long int townsTime);
	void LogLeftButtonDown(long long int townsTime,int mx,int my);
	void LogLeftButtonUp(long long int townsTime,int mx,int my);
	void LogRightButtonDown(long long int townsTime,int mx,int my);
	void LogRightButtonUp(long long int townsTime,int mx,int my);
	void LogFileOpen(long long int townsTime,std::string fName);
	void LogFileExec(long long int townsTime,std::string fName);
	void LogKeyCode(long long int townsTime,unsigned char keyCode1,unsigned char keyCode2);

	std::vector <std::string> GetText(void) const;

	void MakeRepeat(void);

	void AddClick(int x,int y,int t0=100,int t1=100);

	bool SaveEventLog(std::string fName) const;
	bool LoadEventLog(std::string fName);
};


/* } */
#endif
