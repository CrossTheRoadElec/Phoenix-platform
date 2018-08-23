#include "ctre/phoenix/platform/Platform.h"
#include "ctre/phoenix/ErrorCode.h"

#include <chrono>
#include <thread>
#include <iostream> // std::cout

namespace ctre {
namespace phoenix {
namespace platform {
namespace can {
    
    void CANbus_GetStatus(float * /*percentBusUtilization*/, uint32_t * /*busOffCount*/, uint32_t * /*txFullCount*/, uint32_t * /*receiveErrorCount*/,
		uint32_t * /*transmitErrorCount*/, int32_t * /*status*/)
	{
	}
	int32_t CANbus_SendFrame(uint32_t /*messageID*/, const uint8_t * /*data*/, uint8_t /*dataSize*/)
	{
		return 0;
	}
	int32_t CANbus_ReceiveFrame(canframe_t * /*toFillArray*/, uint32_t /*capacity*/, uint32_t & /*numberFilled*/)
	{
		return 0;
	}
    int32_t SetCANInterface(const char * /*interface*/) 
    {
        return 0;
    }
} //namespace can
} //namespace platform
} //namespace phoenix
} //namespace ctre


namespace ctre {
namespace phoenix {
namespace platform {

void SleepUs(int timeUs)
{
    std::this_thread::sleep_for(std::chrono::microseconds(timeUs));
}

/**
* Get a stack trace, ignoring the first "offset" symbols.
*
* @param offset The number of symbols at the top of the stack to ignore
*/
std::string GetStackTrace(int /*offset*/) {

	return "GetStackTrace is not implemented.";
}

void ReportError(int /*isError*/, int32_t /*errorCode*/, int /*isLVCode*/,
	const char *details, const char *location, const char * /*callStack*/)
{
	std::cout << details << std::endl << "\t" << location << std::endl;
}

int32_t SimCreate(DeviceType /*type*/, int /*id*/) {
    return 0;
}

int32_t SimDestroy(DeviceType /*type*/, int /*id*/) {
	return phoenix::ErrorCode::NotImplemented;
}
int32_t SimDestroyAll() {
	return phoenix::ErrorCode::NotImplemented;
}

int32_t SimConfigGet(DeviceType /*type*/, uint32_t /*param*/, uint32_t /*valueToSend*/, uint32_t & /*outValueReceived*/, uint32_t & /*outSubvalue*/, uint32_t /*ordinal*/, uint32_t /*id*/) {
    return 0;
}

int32_t SimConfigSet(DeviceType /*type*/, uint32_t /*param*/, uint32_t /*value*/, uint32_t /*subValue*/, uint32_t /*ordinal*/, uint32_t /*id*/) {
    return 0;
}

int32_t DisposePlatform() {
	return phoenix::ErrorCode::OK;
}

int32_t StartPlatform() {
	return phoenix::ErrorCode::OK;
}

} // namespace platform
} // namespace phoenix
} // namespace ctre
