/* ////////////////////////////////////////////////////////////

File Name: fsuwpwrapper.cpp
Copyright (c) 2017 Soji Yamakawa.  All rights reserved.
http://www.ysflight.com

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice,
   this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS
BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

//////////////////////////////////////////////////////////// */

/* Tsugaru UWP version (2026): no C++/CX.  The UWP host application owns the window and the renderer; it feeds input
   through tsugaru_uwp_host.h, and this file turns it into the fssimplewindow polling API that the emulator's
   window connection uses.  Everything here is thread-safe: the host calls from the UI thread, the emulator polls from
   its own thread. */

#include "../fssimplewindow.h"
#include "../windows/fswin32keymap.h"
#include "tsugaru_uwp_host.h"

#include <mutex>
#include <chrono>
#include <thread>
#include <atomic>
#include <stdio.h>
#include <string.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <direct.h>


namespace
{

class FsMouseEventLog
{
public:
	int eventType=FSMOUSEEVENT_NONE,lb=0,mb=0,rb=0,mx=0,my=0;
};

constexpr int NKEYBUF=256;
constexpr int NEVTBUF=256;

std::mutex lock;

int keyBuffer[NKEYBUF];
int nKeyBufUsed=0;
int charBuffer[NKEYBUF];
int nCharBufUsed=0;

FsMouseEventLog mosBuffer[NEVTBUF];
int nMosBufUsed=0;
FsMouseEventLog lastKnownMos;

unsigned char keyState[FSKEY_NUM_KEYCODE];

int winWid=640,winHei=480;
int exposure=0;
int mouseCursorVisible=1;

FsWin32KeyMapper &KeyMapper(void)
{
	static FsWin32KeyMapper mapper;
	return mapper;
}

void PushMouseEvent(int evt)
{
	// lock is held
	lastKnownMos.eventType=evt;
	if(nMosBufUsed<NEVTBUF)
	{
		mosBuffer[nMosBufUsed++]=lastKnownMos;
	}
}

}

extern "C" void TsugaruUwpKeyEvent(int virtualKey,int down)
{
	if(virtualKey<0 || (int)FS_NUM_VK<=virtualKey)
	{
		return;
	}
	const int fskey=KeyMapper().VkToFsKey(virtualKey);
	if(FSKEY_NULL==fskey)
	{
		return;
	}
	std::lock_guard <std::mutex> guard(lock);
	keyState[fskey]=(down ? 1 : 0);
	if(down && nKeyBufUsed<NKEYBUF)
	{
		keyBuffer[nKeyBufUsed++]=fskey;
	}
}

extern "C" void TsugaruUwpCharEvent(int unicode)
{
	std::lock_guard <std::mutex> guard(lock);
	if(nCharBufUsed<NKEYBUF)
	{
		charBuffer[nCharBufUsed++]=unicode;
	}
}

extern "C" void TsugaruUwpMouseMove(int x,int y)
{
	std::lock_guard <std::mutex> guard(lock);
	lastKnownMos.mx=x;
	lastKnownMos.my=y;
	PushMouseEvent(FSMOUSEEVENT_MOVE);
}

extern "C" void TsugaruUwpMouseButton(int button,int down,int x,int y)
{
	std::lock_guard <std::mutex> guard(lock);
	lastKnownMos.mx=x;
	lastKnownMos.my=y;
	int evt=FSMOUSEEVENT_NONE;
	switch(button)
	{
	case 0:
		lastKnownMos.lb=(down ? 1 : 0);
		evt=(down ? FSMOUSEEVENT_LBUTTONDOWN : FSMOUSEEVENT_LBUTTONUP);
		break;
	case 1:
		lastKnownMos.mb=(down ? 1 : 0);
		evt=(down ? FSMOUSEEVENT_MBUTTONDOWN : FSMOUSEEVENT_MBUTTONUP);
		break;
	case 2:
		lastKnownMos.rb=(down ? 1 : 0);
		evt=(down ? FSMOUSEEVENT_RBUTTONDOWN : FSMOUSEEVENT_RBUTTONUP);
		break;
	}
	if(FSMOUSEEVENT_NONE!=evt)
	{
		PushMouseEvent(evt);
	}
}

extern "C" void TsugaruUwpResetInput(void)
{
	std::lock_guard <std::mutex> guard(lock);
	memset(keyState,0,sizeof(keyState));
	nKeyBufUsed=0;
	nCharBufUsed=0;
	nMosBufUsed=0;
	if(lastKnownMos.lb || lastKnownMos.mb || lastKnownMos.rb)
	{
		lastKnownMos.lb=0;
		lastKnownMos.mb=0;
		lastKnownMos.rb=0;
		PushMouseEvent(FSMOUSEEVENT_LBUTTONUP);
	}
}

extern "C" void TsugaruUwpSetWindowSize(int wid,int hei)
{
	std::lock_guard <std::mutex> guard(lock);
	if(0<wid && 0<hei && (winWid!=wid || winHei!=hei))
	{
		winWid=wid;
		winHei=hei;
		exposure=1;
	}
}


////////////////////////////////////////////////////////////
// fssimplewindow API.  FsOpenWindowOption::FsOpenWindowOption, FsOpenWindow (all overloads but the option one),
// FsClearEventQueue, FsCheckKeyHeldDown, key-name conversion and the Register* functions are in fssimplewindowcommon.cpp.

void FsPushKey(int fskey)
{
	std::lock_guard <std::mutex> guard(lock);
	if(nKeyBufUsed<NKEYBUF)
	{
		keyBuffer[nKeyBufUsed++]=fskey;
	}
}

void FsPushChar(int c)
{
	TsugaruUwpCharEvent(c);
}

// The host owns the window: opening, closing and sizing are no-ops.
void FsOpenWindow(const FsOpenWindowOption &)
{
}

int FsCheckWindowOpen(void)
{
	return 1;
}

void FsResizeWindow(int,int)
{
}

void FsCloseWindow(void)
{
}

void FsMaximizeWindow(void)
{
}

void FsUnmaximizeWindow(void)
{
}

void FsMakeFullScreen(void)
{
}

const FsSimpleWindowInternal *FsGetSimpleWindowInternal(void)
{
	return nullptr;
}

void FsGetWindowSize(int &wid,int &hei)
{
	std::lock_guard <std::mutex> guard(lock);
	wid=winWid;
	hei=winHei;
}

void FsGetWindowPosition(int &x0,int &y0)
{
	x0=0;
	y0=0;
}

void FsSetWindowTitle(const char [])
{
}

void FsShowMouseCursor(int showFlag)
{
	mouseCursorVisible=showFlag;
}

int FsIsMouseCursorVisible(void)
{
	return mouseCursorVisible;
}

void FsSetMousePosition(int,int)
{
}

void FsPushOnPaintEvent(void)
{
}

void FsPollDevice(void)
{
}

void FsSleep(int ms)
{
	if(0<ms)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(ms));
	}
}

long long int FsSubSecondTimer(void)
{
	// Milliseconds, like the other back ends.
	static const auto t0=std::chrono::steady_clock::now();
	return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-t0).count();
}

long long int FsPassedTime(void)
{
	static std::atomic <long long int> lastTick(-1);
	const long long int tick=FsSubSecondTimer();
	long long int prev=lastTick.exchange(tick);
	if(prev<0)
	{
		prev=tick;
	}
	return tick-prev;
}

void FsGetMouseState(int &lb,int &mb,int &rb,int &mx,int &my)
{
	std::lock_guard <std::mutex> guard(lock);
	lb=lastKnownMos.lb;
	mb=lastKnownMos.mb;
	rb=lastKnownMos.rb;
	mx=lastKnownMos.mx;
	my=lastKnownMos.my;
}

int FsGetMouseEvent(int &lb,int &mb,int &rb,int &mx,int &my)
{
	std::lock_guard <std::mutex> guard(lock);
	if(0<nMosBufUsed)
	{
		const FsMouseEventLog e=mosBuffer[0];
		for(int i=1; i<nMosBufUsed; ++i)
		{
			mosBuffer[i-1]=mosBuffer[i];
		}
		--nMosBufUsed;
		lb=e.lb;
		mb=e.mb;
		rb=e.rb;
		mx=e.mx;
		my=e.my;
		return e.eventType;
	}
	lb=lastKnownMos.lb;
	mb=lastKnownMos.mb;
	rb=lastKnownMos.rb;
	mx=lastKnownMos.mx;
	my=lastKnownMos.my;
	return FSMOUSEEVENT_NONE;
}

void FsSwapBuffers(void)
{
}

int FsInkey(void)
{
	std::lock_guard <std::mutex> guard(lock);
	if(0<nKeyBufUsed)
	{
		const int keyCode=keyBuffer[0];
		--nKeyBufUsed;
		for(int i=0; i<nKeyBufUsed; ++i)
		{
			keyBuffer[i]=keyBuffer[i+1];
		}
		return keyCode;
	}
	return FSKEY_NULL;
}

int FsInkeyChar(void)
{
	std::lock_guard <std::mutex> guard(lock);
	if(0<nCharBufUsed)
	{
		const int c=charBuffer[0];
		--nCharBufUsed;
		for(int i=0; i<nCharBufUsed; ++i)
		{
			charBuffer[i]=charBuffer[i+1];
		}
		return c;
	}
	return 0;
}

int FsGetKeyState(int fsKeyCode)
{
	if(0<=fsKeyCode && fsKeyCode<FSKEY_NUM_KEYCODE)
	{
		std::lock_guard <std::mutex> guard(lock);
		return keyState[fsKeyCode];
	}
	return 0;
}

int FsCheckWindowExposure(void)
{
	std::lock_guard <std::mutex> guard(lock);
	const int e=exposure;
	exposure=0;
	return e;
}

void FsChangeToProgramDir(void)
{
	wchar_t path[MAX_PATH*2];
	const DWORD n=GetModuleFileNameW(nullptr,path,MAX_PATH*2);
	if(0<n && n<MAX_PATH*2)
	{
		for(DWORD i=n; 0<i; --i)
		{
			if(L'\\'==path[i-1] || L'/'==path[i-1])
			{
				path[i-1]=0;
				break;
			}
		}
		_wchdir(path);
	}
}

int FsGetNumCurrentTouch(void)
{
	return 0;
}

const FsVec2i *FsGetCurrentTouch(void)
{
	return nullptr;
}

////////////////////////////////////////////////////////////

int FsEnableIME(void)
{
	return 0;
}

void FsDisableIME(void)
{
}

////////////////////////////////////////////////////////////

int FsIsNativeTextInputAvailable(void)
{
	return 0;
}

int FsOpenNativeTextInput(int,int,int,int)
{
	return 0;
}

void FsCloseNativeTextInput(void)
{
}

void FsSetNativeTextInputText(const wchar_t [])
{
}

int FsGetNativeTextInputTextLength(void)
{
	return 0;
}

void FsGetNativeTextInputText(wchar_t str[],int bufLen)
{
	if(0<bufLen)
	{
		str[0]=0;
	}
}

int FsGetNativeTextInputEvent(void)
{
	return FSNATIVETEXTEVENT_NONE;
}
