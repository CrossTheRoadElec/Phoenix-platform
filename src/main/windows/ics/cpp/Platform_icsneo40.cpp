#include "ctre/phoenix/platform/Platform.h"
#include "ctre/phoenix/runtime/LibLoader.h"
#include "ctre/phoenix/ErrorCode.h"
#include <chrono>
#include <thread>
#include <mutex>
#include <deque>
#include <iostream> // std::cout

#include "icsneo40DLLAPI.h"
#include "icsnVC40.h"

using namespace ctre::phoenix;
using namespace ctre::phoenix::platform;
using namespace ctre::phoenix::platform::can;
/* ------------------------------ */
class ValueCANWrapper
{
private:
	/* singleton pattern */
	ValueCANWrapper() {	
		_state = eRunning;
	}
	~ValueCANWrapper() { 
		Dispose();
	}
	ValueCANWrapper(const ValueCANWrapper &) {}

	/* dumb allocation for icsneo*/
	icsSpyMessage _rxCache[20000];

	/* rx coll*/
	std::deque<canframe_t> _rxFrames;
	std::recursive_timed_mutex _lckRx;

	/* DLL and Hardware management */
	ctre::phoenix::runtime::LibLoader _lib;
	void * _device = 0;
	std::recursive_timed_mutex _lckTool;

	/* state */
	enum {
		eRunning,
		eDisposing,
		eDisposed
	} _state;


	void SetStateDisposed()
	{
		std::lock_guard < std::recursive_timed_mutex > lock(_lckTool);
		_state = eDisposing;
	}
	void SetStateDisposing()
	{
		std::lock_guard < std::recursive_timed_mutex > lock(_lckTool);
		_state = eDisposed;
	}
	int32_t CheckState()
	{
		std::lock_guard < std::recursive_timed_mutex > lock(_lckTool);

		switch (_state) {
			case eRunning:
				return 0;
			default:
				return ErrorCode::GeneralError;
		}
	}
	int32_t LoadDll()
	{
		std::lock_guard < std::recursive_timed_mutex > lock(_lckTool); 

		if (_lib.IsOpen() == false) {

			try { _lib.Open("icsneo40.dll"); }
			catch (...)
			{

			}

			if (_lib.IsOpen() == false) { return ctre::phoenix::ErrorCode::LibraryCouldNotBeLoaded; }
		}
		return ctre::phoenix::ErrorCode::OK;
	}
	int32_t OpenDevice()
	{
		std::lock_guard < std::recursive_timed_mutex > lock(_lckTool);

		if (_device == 0)
		{
			/* find it */
			NeoDevice temp[10];
			int num = 10;
			_lib.LookupFunc<FINDNEODEVICES>("icsneoFindNeoDevices")(0xFFFFFFFF, temp, &num);
			/* open it */
			uint8_t nets[] = { 0,1,2,3,4,5,6,7,8,9,10 };
			(void)_lib.LookupFunc<OPENNEODEVICE>("icsneoOpenNeoDevice")(temp, &_device, nets, 0, 0);
		}
		/* let caller know if open was successful */
		if (_device == 0)
			return ErrorCode::ResourceNotAvailable;

		return ErrorCode::OK;
	}
	int32_t CloseDevice()
	{
		/* helpful */
		using Ms = std::chrono::milliseconds;

		/* temps for API call */
		int numError = 0;

		/* attempt to lock - with 500ms timeout*/
		if (_lckTool.try_lock_for(Ms(500))) {
			// [

			/* safely close device */
			_lib.LookupFunc<CLOSEPORT>("icsneoClosePort")(_device, &numError);
			_device = 0;

			// ]
			_lckTool.unlock();

			return ErrorCode::GeneralError;
		}
		else {
			/* close it anyway*/
			_lib.LookupFunc<CLOSEPORT>("icsneoClosePort")(_device, &numError);
			_device = 0;

			return ErrorCode::OK;
		}
	}
	void ProcError(unsigned long err)
	{
		if (err == 75) // NEOVI_ERROR_DLL_NEOVI_NO_RESPONSE
		{
			std::lock_guard < std::recursive_timed_mutex > lock(_lckTool);

			/* close device */
			(void)CloseDevice();

			/* lock and clear rx frames */
			{
				std::lock_guard < std::recursive_timed_mutex > lockRx(_lckRx);
				_rxFrames.clear();
			}
		}
	}
	int32_t Send(uint32_t messageID, const uint8_t * data, uint8_t dataSize)
	{
		/* encode ICS tx message */
		icsSpyMessage msg;
		memset(&msg, 0, sizeof(msg));
		msg.StatusBitField |= SPY_STATUS_XTD_FRAME;
		msg.ArbIDOrHeader = messageID;
		memcpy(msg.Data, data, dataSize);
		msg.NumberBytesData = dataSize;
		/* pass it to icsneo api */
		int ret = _lib.LookupFunc<TXMESSAGES>("icsneoTxMessages")(_device, &msg, NETID_HSCAN, 1);
		if (ret == 1)
			return ctre::phoenix::ErrorCode::OK;
		return ctre::phoenix::ErrorCode::GeneralError;
	}
	int32_t Rec(canframe_t * toFillArray, uint32_t capacity, uint32_t * numberFilled)
	{
		/* initialize outputs */
		*numberFilled = 0;

		/* wait for frames */
		int numMessages = 0, numErr = 0;
		const int timeoutMs = 100;
		int bOneIfMsgReceived = _lib.LookupFunc<WAITFORRXMSGS>("icsneoWaitForRxMessagesWithTimeOut")(_device, timeoutMs);

		/* retrieve them */
		if (bOneIfMsgReceived == 1)
			_lib.LookupFunc<GETMESSAGES>("icsneoGetMessages")(_device, _rxCache, &numMessages, &numErr);
		else if (bOneIfMsgReceived < 0) {
			/* error condition*/
			unsigned long errorNumber = 0;
			_lib.LookupFunc<GETLASTAPIERROR>("icsneoGetLastAPIError")(_device, &errorNumber);
			ProcError(errorNumber);
		}

		/* did we get anything from ics */
		if (numMessages > 0)
		{
			/* lock the container */
			std::lock_guard < std::recursive_timed_mutex > lock(_lckRx);

			/* queue up the received messages */
			for (int i = 0; i < numMessages; ++i)
			{
				/* get ics msg */
				const icsSpyMessage & newMsg = _rxCache[i];

				if (newMsg.NetworkID != NETID_HSCAN) {
					/* not HSCAN, ignore it */
				}
				else if (newMsg.StatusBitField & SPY_STATUS_TX_MSG) {
					/* this is a tx receipt, not a message event */
				}
				else if (newMsg.StatusBitField & SPY_STATUS_GLOBAL_ERR) {
					/* this is a bus error event, not a message event */
				}
				else {

					/* copy to our format*/
					canframe_t cf;
					cf.arbID = newMsg.ArbIDOrHeader;
					cf.dlc = newMsg.NumberBytesData;
					memcpy(cf.data, newMsg.Data, newMsg.NumberBytesData);

					/* insert to coll */
					{
						if (_rxFrames.size() > 1000) {
							/* overflow*/
							CTRE_ASSERT(0);
						}
						else {
							_rxFrames.push_back(cf);
						}
					}
				}
			}
		}


		/* pass to caller */
		{
			std::lock_guard < std::recursive_timed_mutex > lock(_lckRx);
			
			size_t avail = _rxFrames.size();
			size_t numToCopy = (avail < capacity) ? avail : capacity;

			for (size_t i = 0; i < numToCopy; ++i)
			{
				toFillArray[i] = _rxFrames.front();
				_rxFrames.pop_front();
			}

			*numberFilled = static_cast<uint32_t>(numToCopy);

		}
		return 0;
	}

public:
	static ValueCANWrapper & GetInstance()
	{
		static ValueCANWrapper instance;
		return instance;
	}
	int32_t SendFrame(uint32_t messageID, const uint8_t * data, uint8_t dataSize)
	{
		int32_t retval = 0;

		if (retval == 0)
			retval = CheckState();

		if (retval == 0)
			retval = LoadDll();

		if (retval == 0)
			retval = OpenDevice();

		if (retval == 0)
			retval = Send(messageID, data, dataSize);

		return retval;
	}
	int32_t ReceiveFrame(canframe_t * toFillArray, uint32_t capacity, uint32_t * numberFilled)
	{
		int32_t retval = 0;

		if (retval == 0)
			retval = CheckState();

		if (retval == 0)
			retval = LoadDll();

		if (retval == 0)
			retval = OpenDevice();

		if (retval == 0)
			retval = Rec( toFillArray, capacity,  numberFilled);

		return retval;
	}
	void Dispose() {
		SetStateDisposing();
		CloseDevice();
		SetStateDisposed();
	}
};


namespace ctre {
	namespace phoenix {
		namespace platform {
			namespace can {
				void CANbus_GetStatus(float * /*percentBusUtilization*/, uint32_t * /*busOffCount*/, uint32_t * /*txFullCount*/, uint32_t * /*receiveErrorCount*/, uint32_t * /*transmitErrorCount*/, int32_t * /*status*/)
				{
				}
				int32_t CANbus_SendFrame(uint32_t messageID, const uint8_t * data, uint8_t dataSize)
				{
					return ValueCANWrapper::GetInstance().SendFrame(messageID, data, dataSize);
				}
				int32_t CANbus_ReceiveFrame(canframe_t * toFillArray, uint32_t capacity, uint32_t * numberFilled)
				{
					return ValueCANWrapper::GetInstance().ReceiveFrame( toFillArray, capacity,  numberFilled);
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
				return ErrorCode::NotImplemented;
			}

			int32_t SimDestroy(DeviceType /*type*/, int /*id*/) {
				return ErrorCode::NotImplemented;
			}

			int32_t SimDestroyAll() {
				return ErrorCode::NotImplemented;
			}

			int32_t SimConfigGet(DeviceType /*type*/, uint32_t /*param*/, uint32_t /*valueToSend*/, uint32_t & /*outValueReceived*/, uint32_t & /*outSubvalue*/, uint32_t /*ordinal*/, uint32_t /*id*/) {
				return ErrorCode::NotImplemented;
			}

			int32_t SimConfigSet(DeviceType /*type*/, uint32_t /*param*/, uint32_t /*value*/, uint32_t /*subValue*/, uint32_t /*ordinal*/, uint32_t /*id*/) {
				return ErrorCode::NotImplemented;
			}

			int32_t DisposePlatform() {

				//ctre::phoenix::platform::can::CANComm_Dispose();

				ValueCANWrapper::GetInstance().Dispose();
				return ErrorCode::OK;
			}
            int32_t StartPlatform() {
            	//TODO
                return phoenix::ErrorCode::OK;
            }

		} // namespace platform
	} // namespace phoenix
} // namespace ctre
