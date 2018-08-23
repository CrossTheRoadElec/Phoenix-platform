#include "ctre/phoenix/platform/Platform.h"
#include "ctre/phoenix/ErrorCode.h"
#include <linux/can.h> //Probably doesn't exist in cross build tools (also can lib)
#include <ifaddrs.h>

#include <linux/can/raw.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <algorithm>
#include <dlfcn.h>
#include <stdio.h>
#include <unistd.h>

#include <cstring>
#include <chrono>
#include <thread>
#include <iostream> // std::cout
#include <string>

namespace ctre {
namespace phoenix {
namespace platform {
namespace can {

    static int socket = -1;
    int InitializeSocket(struct ifreq &ifr) {
        std::cout << "using interface: " << ifr.ifr_name << std::endl;
        
        struct sockaddr_can addr;
        addr.can_family = AF_CAN;
        addr.can_ifindex = ifr.ifr_ifindex;

        bind(socket, (struct sockaddr *)&addr, sizeof(addr));
        return 0;
    }
    int32_t SetCANInterface(const char * interface) {
        socket = ::socket(PF_CAN, SOCK_RAW, CAN_RAW);
        struct ifreq ifr;
        strcpy(ifr.ifr_name, interface );
        ioctl(socket, SIOCGIFINDEX, &ifr);
        
        return InitializeSocket(ifr);
    }


	void CANbus_GetStatus(float * /*percentBusUtilization*/, uint32_t * /*busOffCount*/, uint32_t * /*txFullCount*/, uint32_t * /*receiveErrorCount*/,
		uint32_t * /*transmitErrorCount*/, int32_t * /*status*/)
	{
		std::cout << "CANbus_GetStatus (WIP)" << std::endl;
	}
	int32_t CANbus_SendFrame(uint32_t messageID, const uint8_t *data, uint8_t dataSize)
	{
		struct can_frame frame;

        std::memcpy(frame.data, data, dataSize);
        frame.can_id = messageID | CAN_EFF_FLAG;
        frame.can_dlc = dataSize;

        errno = 0;

        ssize_t err =  write(socket, &frame, sizeof(struct can_frame));

        if(err == -1) {

            std::cout << "Socket Can Error: " << strerror(errno) << std::endl;

            return -1;
        }
        
		return 0;
	}
	int32_t CANbus_ReceiveFrame(canframe_t * toFillArray, uint32_t capacity, uint32_t & numberFilled)
	{
        struct can_frame frame;
       
        numberFilled = 0;

        if(capacity <= 0) {
            return 0; //Shouldn't happen
        }
 
        auto bytesRead = read(socket, &frame, sizeof(struct can_frame)); //Only can read one at a time it appears
        if(bytesRead <= 0) { //Error or nothing recieved
            return 1;
        }
        else if((bytesRead < (int) sizeof(struct can_frame)) != 0) {  //partial read, shouldn't ever happen
            return 1;
        }
        else if(bytesRead > (int) (sizeof(struct can_frame))) { //Too much recieved, shouldn't ever happen
            return 1;
        }
        else { //Success
            //See https://www.kernel.org/doc/Documentation/networking/can.txt section 
            //4.1.1.1 CAN filter usage optimisation for masking details
             
            //Don't set any flags on toFill for right now
            toFillArray[0].arbID = frame.can_id & CAN_EFF_MASK;
            std::memcpy(toFillArray[0].data, frame.data, frame.can_dlc);
            toFillArray[0].dlc = frame.can_dlc;
            toFillArray[0].timeStampUs = static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count() - 1530000000000);//take a bit off the top to ensure no overflow (shouldn't matter, but still...)
            numberFilled = 1;
        }

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

int32_t SimCreate(DeviceType /*type*/, int /*id*/) {
    return 0;
}

int32_t SimDestroy(DeviceType /*type*/, int /*id*/) {
	return phoenix::ErrorCode::NotImplemented;
}
int32_t SimDestroyAll() {
	return phoenix::ErrorCode::NotImplemented;
}

int32_t DisposePlatform() {
	return phoenix::ErrorCode::OK;
}

int32_t StartPlatform() {
	return phoenix::ErrorCode::OK;
}

int32_t SimConfigGet(DeviceType /*type*/, uint32_t /*param*/, uint32_t /*valueToSend*/, uint32_t & /*outValueReceived*/, uint32_t & /*outSubvalue*/, uint32_t /*ordinal*/, uint32_t /*id*/) {
    return 0;
}

int32_t SimConfigSet(DeviceType /*type*/, uint32_t /*param*/, uint32_t /*value*/, uint32_t /*subValue*/, uint32_t /*ordinal*/, uint32_t /*id*/) {
    return 0;
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

} // namespace platform
} // namespace phoenix
} // namespace ctre
