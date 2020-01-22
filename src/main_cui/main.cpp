#include <iostream>

#include "towns.h"
#include "cpputil.h"


void RunUntil(FMTowns &towns,unsigned int runUntil)
{
	for(;;)
	{
		auto inst=towns.FetchInstruction();
		auto disasm=towns.cpu.Disassemble(inst,towns.cpu.state.CS,towns.cpu.state.EIP,towns.mem);
		std::cout << disasm << std::endl;

		auto eip=towns.cpu.state.EIP;
		towns.RunOneInstruction();
		if(true==towns.CheckAbort())
		{
			break;;
		}
		if(eip<runUntil && runUntil<=towns.cpu.state.EIP)
		{
			break;
		}
	}
}


int main(int ac,char *av[])
{
	if(ac<2)
	{
		printf("Usage:\n");
		printf("main_cui rom_directory_name\n");
		return 1;
	}

	FMTowns towns;
	if(true!=towns.LoadROMImages(av[1]))
	{
		return 1;
	}

	printf("Loaded ROM Images.\n");

	towns.Reset();

	printf("Virtual Machine Reset.\n");

	unsigned int eightBytesCSEIP[8]=
	{
		towns.FetchByteCS_EIP(0),
		towns.FetchByteCS_EIP(1),
		towns.FetchByteCS_EIP(2),
		towns.FetchByteCS_EIP(3),
		towns.FetchByteCS_EIP(4),
		towns.FetchByteCS_EIP(5),
		towns.FetchByteCS_EIP(6),
		towns.FetchByteCS_EIP(7)
	};
	for(auto b : eightBytesCSEIP)
	{
		std::cout << cpputil::Uitox(b) << std::endl;
	}


	RunUntil(towns,0x7F0);
	std::cout << ">";
	std::string cmd;
	std::cin >> cmd;

	RunUntil(towns,0xE2C);
	std::cout << ">";
	std::cin >> cmd;

	for(;;)
	{
		auto inst=towns.FetchInstruction();

		towns.cpu.PrintState();
		towns.PrintStack(32);

		auto disasm=towns.cpu.Disassemble(inst,towns.cpu.state.CS,towns.cpu.state.EIP,towns.mem);

		std::cout << disasm << std::endl;
		std::cout << ">";
		std::string cmd;
		std::cin >> cmd;

		if(true!=towns.CheckAbort())
		{
			auto clocksPassed=towns.RunOneInstruction();
			std::cout << clocksPassed << " clocks passed." << std::endl;
			towns.CheckAbort();
		}
	}

	return 0;
}
