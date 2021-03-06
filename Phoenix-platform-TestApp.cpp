#include <Windows.h>
#include <conio.h>
#include <iostream>
#include "ctre/phoenix/platform/Platform.h"

using namespace ctre::phoenix::platform;
using namespace ctre::phoenix::platform::can;

BOOL WINAPI ConsoleHandlerRoutine(DWORD dwCtrlType)
{
	if (dwCtrlType == CTRL_CLOSE_EVENT) { ctre::phoenix::platform::DisposePlatform(); }
	return FALSE;
}

int main()
{
	int32_t status;
	canframe_t rxMessages[10];

	uint32_t sessHandle = 0;


	std::cout << "Press e to exist" << std::endl;

	(void)SetConsoleCtrlHandler(ConsoleHandlerRoutine, TRUE);


	ctre::phoenix::platform::SimCreate(DeviceType::TalonSRXType, 4);


	CANComm_OpenStreamSession(&sessHandle, 0x4FC00, 0xFFF00, 100, &status);
	if (status)
	{

	}

	while (true)
	{

		/* send tester pres*/
		const uint8_t frame[] = { 0x01, 0x3e };
		CANComm_SendMessage(0x4FC3F, frame, sizeof(frame), 0, &status);

		std::cout << std::hex << 0x4FC3F << std::dec << std::endl;

		if (status)
		{

		}

		uint32_t messagesRead = 0;
		CANComm_ReadStreamSession(sessHandle, rxMessages, sizeof(rxMessages), &messagesRead, &status);
		if (status)
		{
		}
		for (uint32_t i = 0; i < messagesRead; ++i)
		{
			std::cout << std::hex << rxMessages[i].arbID << std::dec << std::endl;
		}

		Sleep(10);

		if (_kbhit() && _getch() == 'e')
			break;
	}

	ctre::phoenix::platform::DisposePlatform();
}
