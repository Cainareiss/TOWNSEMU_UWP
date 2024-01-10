/* LICENSE>>
Copyright 2020 Soji Yamakawa (CaptainYS, http://www.ysflight.com)

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

<< LICENSE */
#ifndef YM2612_IS_INCLUDED
#define YM2612_IS_INCLUDED
/* { */

#include <vector>
#include <cstdint>
#include <string>



// What exactly is the master clock given to YM2612 of TOWNS?
// FM Towns Technical Databook tells internal clock is 600KHz on page 201.
// However, after calibrating the tone to match real Towns, it must be 690KHz.  Is it correct?
// Master clock must be the internal clock times pre scaler.  But, we don't know the default pre scalar value.
// Let's say it is 3.  Then, 690KHz times 3=2070KHz.  Sounds reasonable.
// But, now FM77AV's YM2203C uses master clock frequency of 1228.8KHz.
// And after calibration we know the ratio between the two.
// From there, 1228.8*1698/1038=1999.46.  2MHz.  Makes more sense.
#ifdef MUTSU_FM77AV
	#define YM_MASTER_CLOCK 1228800LL	// 1228.8KHz
	#define YM_PRESCALER_DEFAULT 3
	#define YM_PRESCALER_CONFIGURABLE
#else
	#define YM_MASTER_CLOCK 2000000LL	// 2000.0KHz
	#define YM_PRESCALER_DEFAULT 3
#endif



// Timers A and B apparently should auto-reload when up.
// My previous implementation stopped the timer when it was up.
// XM7's opn.c suggests both timers must re-load and keep going when up.
// Remove the following macro to bring it back to the previous implementation.
#define YM_TIMER_AUTO_RELOAD

/*******************************************************************************

    YM2612 Emulator for Tsugaru:  Tsugaru-Ben

*******************************************************************************/



/*! G** D*** I*!  I didn't realize data sheet of YM2612 is not available today!
*/
class YM2612
{
public:
	// [2] pp. 200  Calculation of timer.  Intenral clock is 600KHz 1tick=(1/600K)sec=1667ns.
	// [2] pp. 201:
	// Timer A takes (12*(1024-NATick))/(600)ms to count up.  NATick is 10-bit counter.
	// Timer B takes (192*(256-NBTick))/(600)ms to count up.  NBTick is 8-bit counter.
	// NATick counts up every 12 internal-clock ticks.
	// NBTick counts up every 192 internal-clock ticks.

	enum
	{
		NUM_SLOTS=4,
		NUM_CHANNELS=6,

		// Sine table
		PHASE_STEPS=4096,      // 4096=360degrees
		PHASE_MASK=4095,
		UNSCALED_MAX=2048,
		SLOTOUT_TO_NPI=8,         // 1.0 output from an upstream slot is 8PI.

		DIV_CONNECT0=1024,
		DIV_CONNECT1=1024,
		DIV_CONNECT2=1024,
		DIV_CONNECT3=1024,
		DIV_CONNECT4=1024, // 2048,  Replacing values will take average output from output slots.
		DIV_CONNECT5=1024, // 3072,  But, Super Daisenryaku sounds very wrong.
		DIV_CONNECT6=1024, // 3072,  I have a feeling that Emerald Dragon sounds correct without averaging.
		DIV_CONNECT7=1024, // 4096,

		SLOTFLAGS_ALL=0x0F,

		TONE_CHOPOFF_MILLISEC=4000,

		WAVE_SAMPLING_RATE=44100,

		WAVE_OUTPUT_AMPLITUDE_MAX_DEFAULT=4096,
		// Was 8192.
		// volume>6000 will clip Super Daisenryaku tones.

		TL_MAX=127,

		ENVELOPE_PRECISION_SHIFT=5,  // 2^ENVELOPE_PRECISION_SHIFT=ENVELOPE_PRECISION
		ENVELOPE_PRECISION=32,       // 1/32 milliseconds

		MODE_NONE=0,
		MODE_CSM=2,
		MODE_SOUNDEFFECT=1,
	};

	enum
	{
		REG_LFO=0x22,
		REG_TIMER_A_COUNT_HIGH=0x24,
		REG_TIMER_A_COUNT_LOW=0x25,
		REG_TIMER_B_COUNT=0x26,
		REG_TIMER_CONTROL=0x27,
		REG_KEY_ON_OFF=0x28,

	#ifdef YM_PRESCALER_CONFIGURABLE
		REG_PRESCALER_0=0x2D,
		REG_PRESCALER_1=0x2E,
		REG_PRESCALER_2=0x2F,
	#endif

		REG_DT_MULTI=0x30,
		REG_TL=0x40,
		REG_KS_AR=0x50,
		REG_AM_DR=0x60,
		REG_SR=0x70,
		REG_SL_RR=0x80,
		REG_SSG_EG=0x90,

		REG_FNUM1=0xA0,
		REG_FNUM2=0xA4,

		REG_FB_CNCT=0xB0,
		REG_LR_AMS_PMS=0xB4,
	};

	enum
	{
		// FM Towns Technical Databook tells internal clock frequency is 600KHz.
		// Which is 1667ns per clock.
		// However, actual measurement suggests it is 690KHz, which makes 1449ns per clock.
		// Forget about it.
		// After figuring TOWNS's YM2612 uses 2MHz master clock and 3X pre-scaler,
		// now calculation is done based on the master clock.
		TIMER_A_PER_TICK=12,
		TIMER_B_PER_TICK=192,
		NTICK_TIMER_A=1024*TIMER_A_PER_TICK,
		NTICK_TIMER_B= 256*TIMER_B_PER_TICK,
		// Self memo:
		// TIMER_?_PER_TICK was hell confusing.  Sorry to myself (and other people who may
		// be trying to understand the meaning of this code.)
		// timerCounter[?] increments by one when the timer is enabled and one tick came in.
		// The timer is up and blasts an IRQ when timerCounter[?] reaches NTICK_TIMER_?.

		// Then what's TIMER_?_PER_TICK?
		// When timer value is set by the CPU, timerCounter[?] is set to TIMER_?_PER_TICK times
		// the value given by the CPU.
		// Tumber of ticks before timer is up is proportional to TIMER_B_PER_TICK.
		// For example, TIMER_B_PER_TICK is 192, and the CPU write 100, timer counter is
		// set to 19200.  256*192-19200=29952 ticks needs to come in before timer B is up.
		// If TIMER_B_PER_TICK is 128, and the CPU writes the same value, timer counter is
		// set to 12800.  256*128-12800=19968 ticks needs to come in before timer B is up.

		// 1024*TIMER_A_PER_TICK, this 1024 comes from TIMER_A uses 10-bit counter.
		// 256*TIMER_B_PER_TICK, this 256 comes from TIMER_B uses 8-bit counter.

		// Where are these 12 and 192 come from?  Taken from FM-Towns Technical Databook pp.201.
		// 	TA=12*(1024-NB)/InternalClockKHz [ms]
		// 	TB=192*(256-NB)/InternalClockKHz [ms]
	};

	enum {
		CH_IDLE=0,
		CH_PLAYING=1,
		CH_RELEASE=2,
	};

	enum
	{
		SSGEG_UP,
		SSGEG_DOWN,
		SSGEG_ZERO,
		SSGEG_ONE,
		SSGEG_REPT,
		SSGEG_KEEP
	};
	static const unsigned char SSG_EG_PTN[8][2];

	class Slot
	{
	public:
		unsigned int DT,MULTI;
		unsigned int TL;
		unsigned int KS,AR;
		unsigned int AM;
		unsigned int DR;
		unsigned int SR;
		unsigned int SL,RR;
		unsigned int SSG_EG;

		// Cache for wave-generation >>
		mutable unsigned long long int microsecS12;  // Microsec from start of a tone by (microsec12>>12)
		uint64_t toneDurationMicrosecS12;   // In (microsec<<12).
		mutable unsigned int phaseS12;      // 5-bit phase=((phaseS12>>12)&0x1F)
		unsigned int phaseS12Step;  // Increment of phase12 per time step.
		unsigned int env[12];       // Envelope is in Db100 scale.  0 to 9600.  Time is (microsec>>10) (=microsecS12>>22)
		                            // env[6] to env[11] only used for SSG_EG
		unsigned int envDurationCache; // in (microsec>>10)
		bool InReleasePhase;
		unsigned int ReleaseStartTime,ReleaseEndTime;
		unsigned int ReleaseStartDbX100;
		mutable unsigned int lastDbX100Cache;  // 0 to 9600 scale.
		// Cache for wave-generation <<

		void Clear(void);

		// phase is from the phaseGenerator*MULTI,DETUNE,PMS
		// phaseShift is input from the upstream slot.
		inline int UnscaledOutput(int phase,int phaseShift) const;
		inline int UnscaledOutput(int phase,int phaseShift,unsigned int FB,int lastSlot0Out) const;

		// Interpolate Envelope as Db.  Output is amplitude 4096 scale.
		// Time input is close to ms, but it is actually (microsec>>10).
		inline int EnvelopedOutputDbToAmpl(int phase,int phaseShift,unsigned int envTime,unsigned int FB,int lastSlot0Out) const;
		inline int EnvelopedOutputDbToAmpl(int phase,int phaseShift,unsigned int envTime) const;
		// DB scale: 0 to 9600
		inline int InterpolateEnvelope(unsigned int envTime) const;

		// After introducing ENVELOPE_PRECISION, envelope interpolation may overflow 32-bit integer.
		// Care must be taken.
		inline unsigned int MulDivU(unsigned int C,unsigned int numer,unsigned int denom) const;
		inline int MulDiv(int C,int numer,int denom) const;


		int DetuneContributionToPhaseStepS12(unsigned int BLOCK,unsigned int NOTE) const;
	};

	class Channel
	{
	public:
		unsigned int F_NUM,BLOCK;
		unsigned int FB,CONNECT;
		unsigned int L,R,AMS,PMS;
		unsigned int usingSlot;
		Slot slots[NUM_SLOTS];

		// Cache for wave-generation >>
		unsigned int playState;
		mutable int lastSlot0Out[2];
		// Cache for wave-generation <<

		mutable int lastAmplitudeMax=0; // Maximum absolute amplitude during the last wave generation.

		void Clear();
		unsigned int Note(void) const;
		unsigned int KC(void) const;
		static unsigned int KC(unsigned int BLOCK,unsigned int F_NUM);
	};

	class State
	{
	public:
	#ifdef YM_PRESCALER_CONFIGURABLE
		unsigned int preScaler=YM_PRESCALER_DEFAULT;
	#else
		const unsigned int preScaler=YM_PRESCALER_DEFAULT;
	#endif

		bool LFO;
		unsigned int FREQCTRL;
		unsigned long long int deviceTimeInNS;
		unsigned long long int lastTickTimeInNS;
		Channel channels[NUM_CHANNELS];
		unsigned int F_NUM_3CH[3],BLOCK_3CH[3];
		unsigned int F_NUM_6CH[3],BLOCK_6CH[3];
		unsigned char regSet[2][256];  // I guess only 0x21 to 0xB6 are used.
		unsigned long long int timerCounter[2];
		bool timerUp[2];
		unsigned int playingCh; // Bit 0 to 5.

		int volume=WAVE_OUTPUT_AMPLITUDE_MAX_DEFAULT;

		void PowerOn(void);
		void Reset(void);
	};

	State state;
	bool channelMute[NUM_CHANNELS]={false,false,false,false,false,false};

	static unsigned int attackExp[4096];
	static unsigned int attackExpInverse[4096];

	static int sineTable[PHASE_STEPS];
	static unsigned int TLtoDB100[128];   // 100 times dB
	static unsigned int SLtoDB100[16];    // 100 times dB
	static unsigned int DB100to4095Scale[9601]; // dB to 0 to 4095 scale
	static unsigned int DB100from4095Scale[4096]; // 4095 scale to dB
	static unsigned int linear4096to9600[4097]; // Linear 4096 scale to 9600 scale
	static unsigned int linear9600to4096[9601]; // Linear 9600 scale to 4096 scale
	static const unsigned int connToOutChannel[8][4];
	static int MULTITable[16];

	struct ConnectionToOutputSlot
	{
		unsigned int nOutputSlots;
		int slots[4];
	};
	static const struct ConnectionToOutputSlot connectionToOutputSlots[8];


	class RegWriteLog
	{
	public:
		unsigned char chBase,reg,data;
		unsigned int count=1;
		uint64_t systemTimeInNS;
	};
	bool takeRegLog=false;
	std::vector <RegWriteLog> regWriteLog;

	enum
	{
		REGWRITE_MEANINGLESS,
		REGWRITE_TIMER_CONTROL,
		REGWRITE_KEY_ON_OFF,
		REGWRITE_LFO,
		REGWRITE_3CH_SPECIAL,
		REGWRITE_DT_MULTI,
		REGWRITE_TL,
		REGWRITE_KS_AR,
		REGWRITE_AM_DR,
		REGWRITE_SR,
		REGWRITE_SL_RR,
		REGWRITE_SSG_EG,
		REGWRITE_FNUM1,
		REGWRITE_BLOCK_FNUM2,
		REGWRITE_FB_CONNECT,
		REGWRITE_L_R_AMS_PMS,
	#ifdef YM_PRESCALER_CONFIGURABLE
		REGWRITE_PRESCALER_0,
		REGWRITE_PRESCALER_1,
		REGWRITE_PRESCALER_2,
	#endif
	};
	uint32_t regWriteCaseTable[256];


	YM2612();
	~YM2612();
private:
	void MakeRegWriteCaseTable(void);
	void MakeSineTable(void);
	void MakeTLtoDB100(void);
	void MakeSLtoDB100(void);
	void MakeDB100to4095Scale(void);
	void MakeLinearScaleTable(void);
	void MakeAttackProfileTable(void);
public:
	void PowerOn(void);
	void Reset(void);

	/*! Writes to a register, and if a channel starts playing a tone, it calls KeyOn and returns between 0 to 5.
	    65535 otherwise.
	*/
	unsigned int WriteRegister(unsigned int channelBase,unsigned int reg,unsigned int value,uint64_t systemTimeInNS);
private:
	unsigned int ReallyWriteRegister(unsigned int channelBase,unsigned int reg,unsigned int value,uint64_t systemTimeInNS);
	unsigned int WriteRegisterSchedule(unsigned int channelBase,unsigned int reg,unsigned int value,uint64_t systemTimeInNS);
public:
#ifndef YM_PRESCALER_CONFIGURABLE
	unsigned int ReadRegister(unsigned int channelBase,unsigned int reg) const;
#else
	// If prescaler is configurable, reading 2D,2E, or 2F may change the pre-scaler, and therefore cannot be const.
	unsigned int ReadRegister(unsigned int channelBase,unsigned int reg);
#endif

	void Run(unsigned long long int systemTimeInNS);

	bool TimerAUp(void) const;
	bool TimerBUp(void) const;

	void ReloadTimerA(void);
	void ReloadTimerB(void);

	/*! Returns timer-up state of 
	*/
	bool TimerUp(unsigned int timerId) const;

	/*! Returns Channel 3 (channels[2]) mode.
	*/
	uint8_t GetChannel3Mode(void) const;

	/*! Cache parameters for calculating wave.
	*/
	void KeyOn(unsigned int ch,unsigned int slotFlags=SLOTFLAGS_ALL);

	/*!
	*/
	void CalculateHertzX16Channel3SpecialMode(unsigned int slotHertzX16[],unsigned int hertzX16Default) const;

	/*!
	*/
	unsigned int CalculateSlotKCChannel3SpecialMode(unsigned int slotNum) const;

	/*! Update phase update (times 2^12) per step for slot.
	*/
	void UpdatePhase12StepSlot(Slot &slot,const unsigned int hertzX16,int detuneContribution);

	/*! Update phase update (times 2^12) per step for channel.
	*/
	void UpdatePhase12StepSlot(Channel &ch);


	// Removed const quailifiers from MakeWaveForNSamples.  YM2612 is a state machine after all.  Generating a wave segment also needs to update its state.


	/*! Sampling rate is defined by WAVE_SAMPLING_RATE.
	    Unless register-write scheduling is used, lastWaveGenTime can be zero.
	*/
	std::vector <unsigned char> MakeWaveAllChannels(unsigned long long int millisec,uint64_t lastWaveGenTime);

	/*! For debugging purpose.  Make wave for a specific channel.
	    Unless register-write scheduling is used, lastWaveGenTime can be zero.
	*/
	std::vector <unsigned char> MakeWave(unsigned int ch,unsigned long long int millisec,uint64_t lastWaveGenTime);

public:
	/*! Adds a wave to the buffer, and returns the number of samples (number_of_bytes_filled/4).
	    Sampling rate is defined by WAVE_SAMPLING_RATE.
	    Unless register-write scheduling is used, lastWaveGenTime can be zero.
	*/
	long long int MakeWaveForNSamples(unsigned char wavBuf[],unsigned long long int numSamplesRequested,uint64_t lastWaveGenTime);

	/*! Adds a wave to the buffer, and returns the number of samples (number_of_bytes_filled/4).
	    Sampling rate is defined by WAVE_SAMPLING_RATE.
	    Unless register-write scheduling is used, lastWaveGenTime can be zero.
	*/
	long long int MakeWaveForNSamples(unsigned char wavBuf[],unsigned int nPlayingCh,unsigned int playingCh[],unsigned long long int numSamplesRequested,uint64_t lastWaveGenTime);
private:
	class WithLFO;
	class WithoutLFO;
	class WithScheduler;
	class WithoutScheduler;
	template <class LFO,class SCHEDULER>
	long long int MakeWaveForNSamplesTemplate(unsigned char wavBuf[],unsigned int nPlayingCh,unsigned int playingCh[],unsigned long long int numSamplesRequested,uint64_t lastWaveGenTime);

	/*! lastSlot0Out is input/output.  Needed for calculating feedback.
	*/
	template <class LFOClass>
	int CalculateAmplitude(int chNum,const uint64_t timeInMicrosecS12[NUM_SLOTS],const unsigned int slotPhase[4],const int AMS4096[4],int &lastSlot0Out) const;

public:
	/*! Change channel state to RELEASE.
	*/
	void KeyOff(unsigned int ch,unsigned int slotFlags=SLOTFLAGS_ALL);


	/*! Check if the tone is done, and update playingCh and playing state.
	*/
	void CheckToneDone(unsigned int chNum);


	/*! Check if the tone is done, and update playingCh and playing state.
	*/
	void CheckToneDoneAllChannels(void);


	/*! Updates slot envelope.
	*/
	void UpdateSlotEnvelope(unsigned int chNum,unsigned int slotNum);
	void UpdateSlotEnvelope(const Channel &ch,Slot &slot,unsigned int KC);


	/*!
	*/
	void UpdateRelease(const Channel &ch,Slot &slot);


	/*! BLOCK_NOTE is as calculated by [2] pp.204.  Isn't it just high-5 bits of BLOCK|F_NUM2?
	    Return value:
	       true    Envelope calculated
	       false   Envelope not calculated. (AR==0)
	    Envelope:
	       env[0]  Duration for attack (in microseconds)
	       env[1]  TL amplitude (0-127)
	       env[2]  Duration for decay (in microseconds)
	       env[3]  SL amplitude (0-127)
	       env[4]  Duration after reaching SL.
	       env[5]  Zero
	*/
	bool CalculateEnvelope(unsigned int env[12],unsigned int BLOCK_NOTE,const Slot &slot) const;
	bool CalculateEnvelopeSSG_EG(unsigned int env[12],unsigned int KC,const Slot &slot) const;
private:
	inline bool NoTone(unsigned int env[6]) const
	{
		env[0]=0;
		env[1]=0;
		env[2]=0;
		env[3]=0;
		env[4]=0;
		env[5]=0;
		return false;
	}


public:
	/*! Based on [2] Table I-5-37
		BLOCK=4		    	Freq Ratio	Freq/Fnum
		C5	523.3	1371	        	0.381692195
		B4	493.9	1294	1.05952622	0.381684699
		A4#	466.2	1222	1.059416559	0.381505728
		A4	440  	1153	1.059545455	0.381613183
		G4#	415.3	1088	1.059475078	0.381709559
		G4	392  	1027	1.059438776	0.381694255
		F4#	370 	969 	1.059459459	0.381836945
		F4	349.2	915 	1.059564719	0.381639344
		E4	329.6	864 	1.059466019	0.381481481
		D4#	311.1	815 	1.05946641	0.381717791
		D4	293.7	769 	1.059244127	0.381924577
		C4#	277.2	726 	1.05952381	0.381818182

		Average Freq/Fnum
		0.381693162

		BLOCK=7		Fnum*0.381693162* 8=Freq
		BLOCK=6		Fnum*0.381693162* 4=Freq
		BLOCK=5		Fnum*0.381693162* 2=Freq
		BLOCK=4		Fnum*0.381693162   =Freq
		BLOCK=3		Fnum*0.381693162/ 2=Freq
		BLOCK=2		Fnum*0.381693162/ 4=Freq
		BLOCK=1		Fnum*0.381693162/ 8=Freq
		BLOCK=0		Fnum*0.381693162/16=Freq

		F-Number Sampled from F-BASIC 386.
		PLAY "O4A"  -> 1038 must correspond to 440Hz -> Ratio should be 0.423892100192678.
	*/
	inline unsigned int BLOCK_FNUM_to_FreqX16(unsigned int BLOCK,unsigned int FNUM) const
	{
		/* Value based on [2] FM Towns Technical Databook.  Wrong values.  Forget about it.
		static const unsigned int scale[8]=
		{
			(3817*16/10)/16,
			(3817*16/10)/8,
			(3817*16/10)/4,
			(3817*16/10)/2,
			(3817*16/10),
			(3817*16/10)*2,
			(3817*16/10)*4,
			(3817*16/10)*8,
		}; */
		// Value calibrated for a real FM TOWNS.
		static const unsigned int scale[8]=
		{
			(uint32_t)((423892LL*3LL*YM_MASTER_CLOCK/(2000000LL))    ),   // (4238*16/10)/16,  // divide by 10 part moved below
			(uint32_t)((423892LL*3LL*YM_MASTER_CLOCK/(2000000LL))  *2),   // (4239*16/10)/8,
			(uint32_t)((423892LL*3LL*YM_MASTER_CLOCK/(2000000LL))  *4),   // (4239*16/10)/4,
			(uint32_t)((423892LL*3LL*YM_MASTER_CLOCK/(2000000LL))  *8),   // (4239*16/10)/2,
			(uint32_t)((423892LL*3LL*YM_MASTER_CLOCK/(2000000LL)) *16),   // (4239*16/10),
			(uint32_t)((423892LL*3LL*YM_MASTER_CLOCK/(2000000LL)) *32),   // (4239*16/10)*2,
			(uint32_t)((423892LL*3LL*YM_MASTER_CLOCK/(2000000LL)) *64),   // (4239*16/10)*4,
			(uint32_t)((423892LL*3LL*YM_MASTER_CLOCK/(2000000LL))*128),   // (4239*16/10)*8,
		};
		FNUM*=scale[BLOCK&7]/(1000*state.preScaler);
		FNUM/=1000;

		// Value 423892 is derived by calibrating with output from a real FM TOWNS.
		// Does it agree with the published formula?
		// This function is calculating Frequency as:
		//     Freq*16=((FNUM*423892*3*phiM/2000000)<<(BLOCK&7))/(1000000*preScaler)
		//     Freq=((FNUM*423892*3*phiM/(16*2000000))<<(BLOCK&7))/(1000000*preScaler)
		//     Freq=((FNUM*423892*3*phiM/(32000000000000))<<(BLOCK&7))/(preScaler)
		//     Freq=((FNUM*phiM*3.973989375/100000000))<<(BLOCK&7))/(preScaler)
		//     Freq=((FNUM*phiM*3.973989375/10^8))<<(BLOCK&7))/(preScaler)
		// Published formula in FM-Techknow is:
		//     F_NUM=((Freq<<20)/(phiM/(12*preScaler))>>((BLOCK&7)-1)
		//     F_NUM<<((BLOCK&7)-1)=((Freq<<20)/(phiM/(12*preScaler))
		//     (F_NUM<<((BLOCK&7)-1))*(phiM/(12*preScaler))=(Freq<<20)
		//     Freq=((F_NUM<<((BLOCK&7)-1))*(phiM/(12*preScaler)))>>20
		//     Freq=((F_NUM*phiM>>20)/(12*preScaler))<<((BLOCK&7)-1)
		//     Freq=((F_NUM*phiM/(12*2^20))<<((BLOCK&7)-1))/preScaler
		//     Freq=((F_NUM*phiM/(12*2^21))<<(BLOCK&7))/preScaler
		//     Freq=((F_NUM*phiM*3.973642985/10^8)<<(BLOCK&7))/preScaler
		// Agree within 0.0075% error.

		return FNUM;
	}


	static void GetCarrierSlotFromConnection(int &numCarrierSlots,int carrierSlots[4],unsigned int connection);


	std::vector <std::string> GetStatusText(void) const;



	/* CSM play-back requires sub-millisecond precision.
	   Let Silpheed speak correctly by enabling scheduling.
	   Schedule must be flushed:
	     After every wave generation,
	     Before saving state.
	   To enable scheduling, 
	      (1) useScheduling=true;
	      (2) VM needs to remember when the wave was generated for the last time, and pass it to MakeWaveForNSamples.
	*/
	bool useScheduling=false;
	std::vector <RegWriteLog> regWriteSched;
	void FlushRegisterSchedule(void);
};


/* } */
#endif
