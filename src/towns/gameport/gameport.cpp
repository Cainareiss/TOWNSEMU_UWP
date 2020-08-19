/* LICENSE>>
Copyright 2020 Soji Yamakawa (CaptainYS, http://www.ysflight.com)

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

<< LICENSE */
#include "device.h"
#include "towns.h"
#include "townsdef.h"
#include "gameport.h"
#include "cpputil.h"



void TownsGamePort::Port::Write(bool COM,bool T1,bool T2)
{
	if(MOUSE==device)
	{
		if((0==state || 2==state) && true!=COM)
		{
			++state;
		}
		else if((1==state || 3==state) && true==COM)
		{
			state=(state+1)&3;
		}
	}
	this->COM=COM;
	this->TRIG[0]=T1;
	this->TRIG[1]=T2;
}
unsigned char TownsGamePort::Port::Read(long long int townsTime)
{
	unsigned char data=0;
	if(GAMEPAD==device || MOUSE==device)
	{
		if(true==COM)
		{
			data|=0x40;
		}
		if(MOUSE==device)
		{
			if(true!=button[0])
			{
				data|=0x10;
			}
			if(true!=button[1])
			{
				data|=0x20;
			}
			if(MOUSEREAD_RESET_TIMEOUT<(townsTime-lastReadTime))
			{
				state=0;
			}
			switch(state)
			{
			case 0:
				mouseMotionCopy=mouseMotion;
				data|=((mouseMotionCopy.x()>>4)&0x0F);
				break;
			case 1:
				data|=((mouseMotionCopy.x()   )&0x0F);
				break;
			case 2:
				data|=((mouseMotionCopy.y()>>4)&0x0F);
				break;
			case 3:
				data|=((mouseMotionCopy.y()   )&0x0F);
				mouseMotion.Set(0,0);
				break;
			}
		}
		else if(GAMEPAD==device)
		{
			data|=0x3F;
			if(true==run)
			{
				data&=0b11110011;
			}
			if(true==right)
			{
				data&=0b11110111;
			}
			if(true==left)
			{
				data&=0b11111011;
			}

			if(true==pause)
			{
				data&=0b11111100;
			}
			if(true==down)
			{
				data&=0b11111101;
			}
			if(true==up)
			{
				data&=0b11111110;
			}
			if(true==button[0])
			{
				data&=0b11101111;
			}
			if(true==button[1])
			{
				data&=0b11011111;
			}
		}
		else
		{
			data|=0x0F;
		}
	}
	lastReadTime=townsTime;
	return data;
}


////////////////////////////////////////////////////////////

TownsGamePort::TownsGamePort(class FMTowns *townsPtr) : Device(townsPtr)
{
	this->townsPtr=townsPtr;
	state.ports[0].device=GAMEPAD;
	state.ports[1].device=MOUSE;
	state.PowerOn();
}

void TownsGamePort::BootSequenceStarted(void)
{
	state.bootKeyComb=BOOT_KEYCOMB_NONE;
}
void TownsGamePort::SetBootKeyCombination(unsigned int keyComb)
{
	if(BOOT_KEYCOMB_PAD_A==keyComb ||
	   BOOT_KEYCOMB_PAD_B==keyComb ||
	   BOOT_KEYCOMB_NONE==keyComb)
	{
		state.bootKeyComb=keyComb;
	}
}

void TownsGamePort::State::PowerOn(void)
{
}
void TownsGamePort::State::Reset(void)
{
	for(auto &p : ports)
	{
		p.state=0;
		p.button[0]=false;
		p.button[1]=false;
		p.run=false;
		p.pause=false;
		p.left=false;
		p.right=false;
		p.up=false;
		p.down=false;
		p.COM=false;
		p.mouseMotion.Set(0,0);
		p.lastReadTime=0;
	}
}


////////////////////////////////////////////////////////////


/* virtual */ void TownsGamePort::PowerOn(void)
{
	state.PowerOn();
}
/* virtual */ void TownsGamePort::Reset(void)
{
	state.Reset();
}
/* virtual */ void TownsGamePort::IOWriteByte(unsigned int ioport,unsigned int data)
{
	switch(ioport)
	{
	case TOWNSIO_GAMEPORT_A_INPUT://        0x4D0,
		break;
	case TOWNSIO_GAMEPORT_B_INPUT://        0x4D2,
		break;
	case TOWNSIO_GAMEPORT_OUTPUT://         0x4D6,
		state.ports[0].Write(0!=(data&0x10),0!=(data&0x01),0!=(data&0x02));
		state.ports[1].Write(0!=(data&0x20),0!=(data&0x04),0!=(data&0x08));
		break;
	}
}
/* virtual */ unsigned int TownsGamePort::IOReadByte(unsigned int ioport)
{
	switch(ioport)
	{
	case TOWNSIO_GAMEPORT_A_INPUT://        0x4D0,
		if(BOOT_KEYCOMB_PAD_A==state.bootKeyComb)
		{
			return 0x2F;
		}
		else if(BOOT_KEYCOMB_PAD_B==state.bootKeyComb)
		{
			return 0x1F;
		}
		else
		{
			return state.ports[0].Read(townsPtr->state.townsTime);
		}
		break;
	case TOWNSIO_GAMEPORT_B_INPUT://        0x4D2,
		return state.ports[1].Read(townsPtr->state.townsTime);
		break;
	case TOWNSIO_GAMEPORT_OUTPUT://         0x4D6,
		break;
	}
	return 0xff;
}

