/* yssimplesound back end for UWP (Windows 10 / Xbox): XAudio2.
   Same behaviour as the DirectSound back end: one source voice per SoundData for one-shot and looping play-back,
   and a queue-based streaming voice (44.1KHz 16-bit stereo) for the FM/PCM stream. */

#include <algorithm>
#include <atomic>
#include <mutex>
#include <vector>
#include <cmath>
#include <stdio.h>
#include <string.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <xaudio2.h>

#include "yssimplesound.h"

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

#pragma comment(lib,"xaudio2.lib")


class YsSoundPlayer::APISpecificData
{
public:
	IXAudio2 *xa=nullptr;
	IXAudio2MasteringVoice *master=nullptr;

	// Incremented every time the engine is started: a SoundData compares it against the count it was prepared with
	// to find out whether its voice died with a previous engine.
	unsigned int recycleCount=0;

	~APISpecificData()
	{
		End();
	}
	void Start(void)
	{
		End();
		if(SUCCEEDED(XAudio2Create(&xa,0,XAUDIO2_DEFAULT_PROCESSOR)))
		{
			if(FAILED(xa->CreateMasteringVoice(&master,2)))
			{
				master=nullptr;
				xa->Release();
				xa=nullptr;
			}
		}
		++recycleCount;
	}
	void End(void)
	{
		if(nullptr!=master)
		{
			master->DestroyVoice();
			master=nullptr;
		}
		if(nullptr!=xa)
		{
			xa->Release();
			xa=nullptr;
		}
	}
};

class YsSoundPlayer::SoundData::APISpecificDataPerSoundData
{
public:
	IXAudio2 *xaAssociation=nullptr;
	IXAudio2SourceVoice *voice=nullptr;
	unsigned int playerRecycleCount=~0;
	bool paused=false;
	UINT64 baseSamples=0;
	int numChannels=2;

	~APISpecificDataPerSoundData()
	{
		CleanUp();
	}
	void CleanUp(void)
	{
		if(nullptr!=voice)
		{
			voice->DestroyVoice();
			voice=nullptr;
		}
		paused=false;
	}
	void CreateVoice(IXAudio2 *xa,SoundData &dat)
	{
		CleanUp();

		const int nChannels=dat.GetNumChannel();
		const int nBlockAlign=nChannels*dat.BitPerSample()/8;

		WAVEFORMATEX fmt;
		memset(&fmt,0,sizeof(fmt));
		fmt.wFormatTag=WAVE_FORMAT_PCM;
		fmt.nChannels=nChannels;
		fmt.nSamplesPerSec=dat.PlayBackRate();
		fmt.wBitsPerSample=dat.BitPerSample();
		fmt.nBlockAlign=nBlockAlign;
		fmt.nAvgBytesPerSec=dat.PlayBackRate()*nBlockAlign;
		fmt.cbSize=0;

		numChannels=nChannels;
		if(FAILED(xa->CreateSourceVoice(&voice,&fmt)))
		{
			voice=nullptr;
		}
	}
	void Submit(const SoundData &dat,bool loop)
	{
		XAUDIO2_BUFFER buf;
		memset(&buf,0,sizeof(buf));
		buf.pAudioData=dat.DataPointer();
		buf.AudioBytes=dat.SizeInByte();
		if(loop)
		{
			buf.LoopCount=XAUDIO2_LOOP_INFINITE;
		}
		else
		{
			buf.Flags=XAUDIO2_END_OF_STREAM;
		}
		voice->SubmitSourceBuffer(&buf);
	}
};


////////////////////////////////////////////////////////////

YsSoundPlayer::APISpecificData *YsSoundPlayer::CreateAPISpecificData(void)
{
	return new APISpecificData;
}
void YsSoundPlayer::DeleteAPISpecificData(APISpecificData *ptr)
{
	delete ptr;
}

YSRESULT YsSoundPlayer::StartAPISpecific(void)
{
	api->Start();
	return (nullptr!=api->xa ? YSOK : YSERR);
}
YSRESULT YsSoundPlayer::EndAPISpecific(void)
{
	api->End();
	return YSOK;
}

void YsSoundPlayer::SetVolumeAPISpecific(SoundData &dat,float leftVol,float rightVol)
{
	auto voice=dat.api->voice;
	if(nullptr==voice)
	{
		return;
	}
	leftVol=std::max(0.0f,std::min(1.0f,leftVol));
	rightVol=std::max(0.0f,std::min(1.0f,rightVol));

	float matrix[4];
	UINT32 srcCh=(1==dat.api->numChannels ? 1 : 2);
	if(1==srcCh)
	{
		matrix[0]=leftVol;
		matrix[1]=rightVol;
	}
	else
	{
		matrix[0]=leftVol;
		matrix[1]=0;
		matrix[2]=0;
		matrix[3]=rightVol;
	}
	voice->SetVolume(1.0f);
	voice->SetOutputMatrix(nullptr,srcCh,2,matrix);
}

YSRESULT YsSoundPlayer::PlayOneShotAPISpecific(SoundData &dat)
{
	auto voice=dat.api->voice;
	if(nullptr==voice)
	{
		return YSERR;
	}
	SetVolumeAPISpecific(dat,dat.playBackVolumeLeft,dat.playBackVolumeRight);
	voice->Stop(0);
	voice->FlushSourceBuffers();
	dat.api->Submit(dat,false);
	XAUDIO2_VOICE_STATE st;
	voice->GetState(&st);
	dat.api->baseSamples=st.SamplesPlayed;
	dat.api->paused=false;
	voice->Start(0);
	return YSOK;
}

YSRESULT YsSoundPlayer::PlayBackgroundAPISpecific(SoundData &dat)
{
	auto voice=dat.api->voice;
	if(nullptr==voice)
	{
		return YSERR;
	}
	if(YSTRUE!=IsPlayingAPISpecific(dat))
	{
		SetVolumeAPISpecific(dat,dat.playBackVolumeLeft,dat.playBackVolumeRight);
		voice->Stop(0);
		voice->FlushSourceBuffers();
		dat.api->Submit(dat,true);
		XAUDIO2_VOICE_STATE st;
		voice->GetState(&st);
		dat.api->baseSamples=st.SamplesPlayed;
		dat.api->paused=false;
		voice->Start(0);
	}
	return YSOK;
}

YSBOOL YsSoundPlayer::IsPlayingAPISpecific(const SoundData &dat) const
{
	auto voice=dat.api->voice;
	if(nullptr!=voice && true!=dat.api->paused)
	{
		XAUDIO2_VOICE_STATE st;
		voice->GetState(&st,XAUDIO2_VOICE_NOSAMPLESPLAYED);
		if(0<st.BuffersQueued)
		{
			return YSTRUE;
		}
	}
	return YSFALSE;
}

void YsSoundPlayer::PauseAPISpecific(SoundData &dat)
{
	if(nullptr!=dat.api->voice)
	{
		dat.api->voice->Stop(0);
		dat.api->paused=true;
	}
}
void YsSoundPlayer::ResumeAPISpecific(SoundData &dat)
{
	if(nullptr!=dat.api->voice && true==dat.api->paused)
	{
		dat.api->voice->Start(0);
		dat.api->paused=false;
	}
}

double YsSoundPlayer::GetCurrentPositionAPISpecific(const SoundData &dat) const
{
	auto voice=dat.api->voice;
	if(nullptr!=voice && 0<dat.PlayBackRate())
	{
		XAUDIO2_VOICE_STATE st;
		voice->GetState(&st);
		if(dat.api->baseSamples<=st.SamplesPlayed)
		{
			return (double)(st.SamplesPlayed-dat.api->baseSamples)/(double)dat.PlayBackRate();
		}
	}
	return 0.0;
}

void YsSoundPlayer::StopAPISpecific(SoundData &dat)
{
	if(nullptr!=dat.api->voice)
	{
		dat.api->voice->Stop(0);
		dat.api->voice->FlushSourceBuffers();
		dat.api->paused=false;
	}
}

void YsSoundPlayer::KeepPlayingAPISpecific(void)
{
}

////////////////////////////////////////////////////////////

YsSoundPlayer::SoundData::APISpecificDataPerSoundData *YsSoundPlayer::SoundData::CreateAPISpecificData(void)
{
	return new APISpecificDataPerSoundData;
}
void YsSoundPlayer::SoundData::DeleteAPISpecificData(APISpecificDataPerSoundData *ptr)
{
	delete ptr;
}

bool YsSoundPlayer::SoundData::IsPrepared(YsSoundPlayer &player)
{
	if(player.api->recycleCount!=api->playerRecycleCount || player.api->xa!=api->xaAssociation)
	{
		return false;
	}
	return prepared;
}

YSRESULT YsSoundPlayer::SoundData::PreparePlay(YsSoundPlayer &player)
{
	if(player.api->recycleCount!=api->playerRecycleCount || player.api->xa!=api->xaAssociation)
	{
		// The voice is gone with the previous engine.
		api->voice=nullptr;
	}
	if(nullptr!=api->voice)
	{
		return YSOK;
	}
	if(nullptr==player.api->xa)
	{
		return YSERR;
	}

	api->CreateVoice(player.api->xa,*this);
	api->playerRecycleCount=player.api->recycleCount;
	api->xaAssociation=player.api->xa;
	return (nullptr!=api->voice ? YSOK : YSERR);
}

void YsSoundPlayer::SoundData::CleanUpAPISpecific(void)
{
	if(nullptr!=playerStatePtr && YsSoundPlayer::STATE_ENDED==*playerStatePtr)
	{
		// The voice is gone with the engine.
		api->voice=nullptr;
	}
	api->CleanUp();
}

////////////////////////////////////////////////////////////
// Streaming

namespace
{
constexpr int RINGBUFFER_CHANNELS=2;
constexpr int RINGBUFFER_SAMPLING_RATE=44100;
constexpr int RINGBUFFER_BITS_PER_SAMPLE=16;
}

class YsSoundPlayer::Stream::APISpecificData : public IXAudio2VoiceCallback
{
public:
	IXAudio2SourceVoice *voice=nullptr;
	int64_t ringSamples=0;
	std::atomic <int64_t> queuedSamples{0};

	// IXAudio2VoiceCallback.  Runs on the XAudio2 thread.
	void STDMETHODCALLTYPE OnVoiceProcessingPassStart(UINT32) override {}
	void STDMETHODCALLTYPE OnVoiceProcessingPassEnd() override {}
	void STDMETHODCALLTYPE OnStreamEnd() override {}
	void STDMETHODCALLTYPE OnBufferStart(void *) override {}
	void STDMETHODCALLTYPE OnLoopEnd(void *) override {}
	void STDMETHODCALLTYPE OnVoiceError(void *,HRESULT) override {}
	void STDMETHODCALLTYPE OnBufferEnd(void *context) override
	{
		auto data=(std::vector <unsigned char> *)context;
		if(nullptr!=data)
		{
			queuedSamples-=(int64_t)(data->size()/(RINGBUFFER_CHANNELS*RINGBUFFER_BITS_PER_SAMPLE/8));
			delete data;
		}
	}

	void Destroy(void)
	{
		if(nullptr!=voice)
		{
			voice->Stop(0);
			voice->DestroyVoice(); // Flushes; OnBufferEnd frees the queued buffers.
			voice=nullptr;
		}
		queuedSamples=0;
	}
};

YsSoundPlayer::Stream::APISpecificData *YsSoundPlayer::Stream::CreateAPISpecificData(void)
{
	return new YsSoundPlayer::Stream::APISpecificData;
}
void YsSoundPlayer::Stream::DeleteAPISpecificData(APISpecificData *api)
{
	if(nullptr!=api)
	{
		api->Destroy();
		delete api;
	}
}

YSRESULT YsSoundPlayer::StartStreamingAPISpecific(Stream &stream,StreamingOption opt)
{
	auto sapi=stream.api;
	if(nullptr==sapi || nullptr==api->xa)
	{
		return YSERR;
	}
	sapi->Destroy();

	sapi->ringSamples=(int64_t)RINGBUFFER_SAMPLING_RATE*opt.ringBufferLengthMillisec/1000;

	WAVEFORMATEX fmt;
	memset(&fmt,0,sizeof(fmt));
	fmt.wFormatTag=WAVE_FORMAT_PCM;
	fmt.nChannels=RINGBUFFER_CHANNELS;
	fmt.nSamplesPerSec=RINGBUFFER_SAMPLING_RATE;
	fmt.wBitsPerSample=RINGBUFFER_BITS_PER_SAMPLE;
	fmt.nBlockAlign=RINGBUFFER_CHANNELS*RINGBUFFER_BITS_PER_SAMPLE/8;
	fmt.nAvgBytesPerSec=RINGBUFFER_SAMPLING_RATE*fmt.nBlockAlign;

	if(FAILED(api->xa->CreateSourceVoice(&sapi->voice,&fmt,0,XAUDIO2_DEFAULT_FREQ_RATIO,sapi)))
	{
		sapi->voice=nullptr;
		return YSERR;
	}
	sapi->voice->Start(0);
	return YSOK;
}

void YsSoundPlayer::StopStreamingAPISpecific(Stream &stream)
{
	if(nullptr!=stream.api && nullptr!=stream.api->voice)
	{
		stream.api->voice->Stop(0);
		stream.api->voice->FlushSourceBuffers();
		stream.api->voice->Start(0);
	}
}

YSBOOL YsSoundPlayer::StreamPlayerReadyToAcceptNextNumSampleAPISpecific(const Stream &stream,unsigned int numSamples) const
{
	if(nullptr!=stream.api && nullptr!=stream.api->voice)
	{
		if(stream.api->queuedSamples.load()+(int64_t)numSamples<=stream.api->ringSamples)
		{
			return YSTRUE;
		}
	}
	return YSFALSE;
}

YSRESULT YsSoundPlayer::AddNextStreamingSegmentAPISpecific(Stream &stream,const SoundData &dat)
{
	if(nullptr==stream.api || nullptr==stream.api->voice)
	{
		return YSERR;
	}

	int64_t numSamplesIn=dat.GetNumSamplePerChannel();
	int64_t numSamplesOut=numSamplesIn*RINGBUFFER_SAMPLING_RATE/dat.PlayBackRate();
	if(numSamplesOut<=0)
	{
		return YSOK;
	}
	int64_t room=stream.api->ringSamples-stream.api->queuedSamples.load();
	if(room<=0)
	{
		return YSOK;
	}
	numSamplesOut=std::min(numSamplesOut,room);

	auto out=new std::vector <unsigned char>((size_t)numSamplesOut*RINGBUFFER_CHANNELS*(RINGBUFFER_BITS_PER_SAMPLE/8));
	const unsigned char *readPtr=dat.DataPointer();
	const unsigned char *readEnd=readPtr+dat.SizeInByte();
	const int inChannels=dat.GetNumChannel();

	// Nearest-neighbour conversion to 44.1KHz stereo, exactly like the other back ends.
	unsigned char *writePtr=out->data();
	int64_t balance=RINGBUFFER_SAMPLING_RATE;
	for(int64_t i=0; i<numSamplesOut; ++i)
	{
		if(readEnd<readPtr+2*inChannels)
		{
			// ran out of input: pad silence
			memset(writePtr,0,4);
		}
		else if(1==inChannels)
		{
			writePtr[0]=readPtr[0];
			writePtr[1]=readPtr[1];
			writePtr[2]=readPtr[0];
			writePtr[3]=readPtr[1];
		}
		else
		{
			writePtr[0]=readPtr[0];
			writePtr[1]=readPtr[1];
			writePtr[2]=readPtr[2];
			writePtr[3]=readPtr[3];
		}
		writePtr+=4;

		balance-=dat.PlayBackRate();
		while(balance<=0)
		{
			balance+=RINGBUFFER_SAMPLING_RATE;
			readPtr+=2*inChannels;
		}
	}

	XAUDIO2_BUFFER buf;
	memset(&buf,0,sizeof(buf));
	buf.pAudioData=out->data();
	buf.AudioBytes=(UINT32)out->size();
	buf.pContext=out;
	stream.api->queuedSamples+=numSamplesOut;
	if(FAILED(stream.api->voice->SubmitSourceBuffer(&buf)))
	{
		stream.api->queuedSamples-=numSamplesOut;
		delete out;
		return YSERR;
	}
	return YSOK;
}
