/* LICENSE>>
Copyright 2020 Soji Yamakawa (CaptainYS, http://www.ysflight.com)

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

<< LICENSE */
#ifndef FDC_IS_INCLUDED
#define FDC_IS_INCLUDED
/* { */

#include <vector>
#include <string>
#include <unordered_map>
#include "diskdrive.h"


class TownsFDC : public DiskDrive
{
public:
	class FMTownsCommon *townsPtr;
	class TownsPIC *PICPtr;
	class TownsDMAC *DMACPtr;

	bool debugBreakOnCommandWrite=false;
	bool fdcMonitor=false;

	virtual const char *DeviceName(void) const{return "FDC";}

	TownsFDC(class FMTownsCommon *townsPtr,class TownsPIC *PICPtr,class TownsDMAC *dmacPtr);

	void SendCommand(unsigned int cmd);

	/*! Turns off BUSY flag.  Also if IRQ is not masked it raises IRR flag of PIC.
	*/
	void MakeReady(void);

	virtual void RunScheduledTask(unsigned long long int townsTime);
	virtual void IOWriteByte(unsigned int ioport,unsigned int data);
	virtual unsigned int IOReadByte(unsigned int ioport);

	virtual void Reset(void);



	class BreakOnAccess
	{
	public:
		bool monitorOnly=false;
	};

	std::unordered_map <unsigned int,BreakOnAccess> breakOnReadSector,breakOnReadAddress;

	void SetBreakOnReadSector(unsigned char CHRN[4],bool monitorOnly);
	void ClearBreakOnReadSector(unsigned char CHRN[4]);
	void ClearAllBreakOnReadSector(void);

	void SetBreakOnReadAddress(unsigned char CHRN[4],bool monitorOnly);
	void ClearBreakOnReadAddress(unsigned char CHRN[4]);
	void ClearAllBreakOnReadAddress(void);
};


/* } */
#endif
