/* LICENSE>>
Copyright 2020 Soji Yamakawa (CaptainYS, http://www.ysflight.com)

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

<< LICENSE */
#ifndef FSSIMPLEWINDOW_CONNECTION_IS_INCLUDED
#define FSSIMPLEWINDOW_CONNECTION_IS_INCLUDED
/* { */



/*
The original plan was entirely transition to the streaming mode.  However, it turned out DirectSound gives an shameful 60ms latency (practically 80ms), and ALSA apalling 80ms latency.
macOS's AVAudioEngine easily gave a 40ms latency.

DirectSound does better job if I poll play-back state every 1ms and start new 20ms segment, which yields maximum 40ms latency.  The draw back is when you move window or click on 
the window frame, you may hear zapping noise, but I believe latency is more important for games.

ALSA has no choice.  Behind the scenes ALSA code does streaming.

So, my best decision is to use streaming mode in macOS, which should work perfectly.  I have no choice with ALSA.  But, for Windows, I keep it polling-based playback.
*/

#define AUDIO_USE_STREAMING



#include "outside_world.h"
#include "yssimplesound.h"
#include "ysgamepad.h"
#include "fssimplewindow.h"
#include <vector>

class FsSimpleWindowConnection : public Outside_World
{
public:
	class HostShortCut
	{
	public:
		bool inUse=false;
		bool ctrl=false,shift=false;
		std::string cmdStr;
	};

	#define DEFAULT_PAUSE_KEY_CODE FSKEY_SCROLLLOCK

	GLuint mainTexId,statusTexId,pauseIconTexId,menuIconTexId;

	HostShortCut hostShortCut[FSKEY_NUM_KEYCODE];
	unsigned int PAUSE_KEY_CODE=DEFAULT_PAUSE_KEY_CODE;

	bool gamePadInitialized=false;
	unsigned int *FSKEYtoTownsKEY=nullptr;
	unsigned int *FSKEYState=nullptr;
	FsSimpleWindowConnection();
	~FsSimpleWindowConnection();

	GLuint GenTexture(void);
	void UpdateTexture(GLuint texId,int wid,int hei,const unsigned char *rgba) const;
	void DrawTextureRect(int x0,int y0,int x1,int y1) const;
	void PauseKeyPressed(void);

	std::vector <unsigned char> PAUSEicon,MENUicon;

	std::vector <struct YsGamePadReading> gamePads,prevGamePads;

	// For mouse emulation by pad digital axes.
	int mouseDX=0,mouseDY=0;

	int winWid=640,winHei=480;
	unsigned int sinceLastResize=0;


	virtual std::vector <std::string> MakeDefaultKeyMappingText(void) const override;
	virtual std::vector <std::string> MakeKeyMappingText(void) const override;
	virtual void LoadKeyMappingFromText(const std::vector <std::string> &text) override;

	virtual void Start(void) override;
	virtual void Stop(void) override;
	virtual void DevicePolling(class FMTownsCommon &towns) override;
	void PollGamePads(void);
	virtual void UpdateStatusBitmap(class FMTownsCommon &towns) override;
	virtual void Render(const TownsRender::Image &img,const class FMTownsCommon &towns) override;
	void RenderBeforeSwapBuffers(const TownsRender::Image &img,const class FMTownsCommon &towns);
	virtual bool ImageNeedsFlip(void) override;

	virtual void SetKeyboardLayout(unsigned int layout) override;
	static void MakeKeyMapFromLayout(unsigned int FSKeyToTownsKEY[FSKEY_NUM_KEYCODE],unsigned int layout);

	virtual void RegisterHostShortCut(std::string hostKeyLabel,bool ctrl,bool shift,std::string cmdStr) override;
	virtual void RegisterPauseResume(std::string hostKeyLabel) override;

	virtual void ToggleMouseCursor(void) override;



	class WindowConnection : public WindowInterface
	{
	public:
		void Start(void) override;
		void Stop(void) override;
		void Interval(void) override;
		void Render(void) override;
		void Communicate(Outside_World *) override;
	};
	WindowInterface *CreateWindowInterface(void) const override;
	void DeleteWindowInterface(WindowInterface *) const override;



	class SoundConnection : public Sound
	{
	public:
		YsSoundPlayer soundPlayer;
		YsSoundPlayer::SoundData cddaChannel;
		unsigned long long cddaStartHSG;

	#ifdef AUDIO_USE_STREAMING
		YsSoundPlayer::Stream FMPCMStream;
	#else
		YsSoundPlayer::SoundData FMPCMChannel;
	#endif

		YsSoundPlayer::SoundData BeepChannel;

	public:
		virtual void Start(void) override;
		virtual void Stop(void) override;

		virtual void Polling(void) override;

		virtual void CDDAPlay(const DiscImage &discImg,DiscImage::MinSecFrm from,DiscImage::MinSecFrm to,bool repeat,unsigned int,unsigned int) override;
		virtual void CDDASetVolume(float leftVol,float rightVol) override;
		virtual void CDDAStop(void) override;
		virtual void CDDAPause(void) override;
		virtual void CDDAResume(void) override;
		virtual bool CDDAIsPlaying(void) override;
		virtual DiscImage::MinSecFrm CDDACurrentPosition(void) override;

		virtual void FMPCMPlay(std::vector <unsigned char > &wave) override;
		virtual void FMPCMPlayStop(void) override;
		virtual bool FMPCMChannelPlaying(void) override;

		virtual void BeepPlay(int samplingRate, std::vector<unsigned char> &wave) override;
		virtual void BeepPlayStop() override;
		virtual bool BeepChannelPlaying() const override;
	};
	virtual Sound *CreateSound(void) const override;
	virtual void DeleteSound(Sound *) const override;
};

/* } */
#endif
