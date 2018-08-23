#include "ctre/phoenix/platform/Platform.h"
#include "ctre/phoenix/ErrorCode.h"
#include "ctre/phoenix/runtime/LibLoader.h" // useful for plugin strategy

#include <chrono>
#include <thread>
#include <iostream> // std::cout
#include <map> 
#include <memory>
#include <cstring>
#include <sstream>
#include <fstream>
#include <mutex>

#define CTRE_CREATE_EXPORTS // we need typedefs, not proto's
#include "SimulationAdapter.h"

namespace ctre {
	namespace phoenix {
		namespace platform {

			std::map<std::pair<DeviceType, int>, std::unique_ptr<ctre::phoenix::runtime::LibLoader>> libMap;

			std::mutex simCreateLock;

            static void ClearAll(){
                std::lock_guard<std::mutex> guard(simCreateLock);
				
                libMap.clear();
			}

            #if defined(WIN32) || defined(_WIN32) || defined(_WIN64)
            
            #else
            
            void ClearAll() __attribute__ ((destructor));
            
            #endif

			void SleepUs(int timeUs)
			{
				std::this_thread::sleep_for(std::chrono::microseconds(timeUs));
			}

			ErrorCode SimGetDeviceCharacteristics(DeviceType type, int id, std::string & envVarName, std::string & tempDllName)
			{
				std::stringstream work; //!< temp for temp dll name
				ErrorCode retval = ErrorCode::OK;

				/* get device characteristics */
				if (retval == ErrorCode::OK) {
					switch (type) {
					case TalonSRXType:
						envVarName = "CTRE_TALON_LIBRARY_PATH";
						work << "./TEMPTalonObject" << id;
						break;
					case VictorSPXType:
						envVarName = "CTRE_VICTOR_LIBRARY_PATH";
						work << "./TEMPVictorObject" << id;
						break;
					case CANifierType:
						envVarName = "CTRE_CANIFIER_LIBRARY_PATH";
						work << "./TEMPCANifierObject" << id;
						break;
					case PigeonIMUType:
						envVarName = "CTRE_PIGEON_LIBRARY_PATH";
						work << "./TEMPPigeonObject" << id;
						break;
					default:
						retval = ErrorCode::FeatureNotSupported;
						break;
					}
				}

				/* finish temp Dll Name*/
				if (retval == ErrorCode::OK) {
					/* suffix .dll in windows */
#if defined(WIN32) || defined(_WIN32) || defined(_WIN64)
					work << ".dll";
#endif
					/* save the results to caller */
					tempDllName = work.str();
				}

				/* if call failed, clear the outputs*/
				if (retval != ErrorCode::OK) {
					envVarName.clear();
					tempDllName.clear();
				}

				return retval;
			}

			int32_t SimCreate(DeviceType type, int id)
			{
				/* lock out the rest of the function */
				std::lock_guard<std::mutex> guard(simCreateLock);

				/* did we already create this device? */
				std::pair<DeviceType, int> identifier(type, id);
				auto iter = libMap.find(identifier);
				if (iter != libMap.end()) {
					/* replace with error code after header repos is created */
					return -1;

				}

				/* assume success at begin*/
				int retval = 0;

				/* create a lib entry */
				libMap.insert(std::make_pair(identifier, std::make_unique<ctre::phoenix::runtime::LibLoader>()));
				iter = libMap.find(identifier);
				auto &lib = iter->second;

				/* check type and get device specific characteristics */
				std::string envVarName;
				std::string tempDllName;
				if (retval == 0)
				{
					retval = SimGetDeviceCharacteristics(type, id, envVarName, tempDllName);
				}

				/* attempt to retrieve env var telling us the DLL/SO location. */
				std::string srcLibPath; //!< deduce path to source firm library
				if (retval == 0) {
					const char * ret = std::getenv(envVarName.c_str());
					/* leave libPath blank, null ptr assignment breaks std::string */
					if (ret == nullptr) {
						/* could not get env var*/
						retval = ErrorCode::InvalidParamValue;
					}
					else {
						/* good so far */
						srcLibPath = ret;
					}
				}

				/* attempt to load the lib*/
				std::ifstream src;
				if (retval == 0) {
					src.open(srcLibPath, std::ios::binary);
					if (false == src.is_open())
					{
						/* could not open the source file*/
						retval = ErrorCode::GeneralError;
					}
				}


				/* attempt to create destination file */
				std::ofstream dst;
				if (retval == 0) {
					/* attempt to write destination file */
					dst.open(tempDllName.c_str(), std::ios::binary);
					if (false == dst.is_open())
					{
						/* could not create the dest file, bad path maybe? */
						retval = ErrorCode::GeneralError;
					}
				}
				/* attempt to copy from source to dest file */
				if (retval == 0) {
					/* copy the contents */
					dst << src.rdbuf();
					/* all done with file, this ensures file IO complete before we attempt to system-load */
					dst.close();
					/* nothing to check here, but we could do a read test later */
				}

				/* load the lib as a system resource */
				if (retval == 0) {
					try
					{
						lib->Open(tempDllName.c_str());
					}
					catch (runtime::LibLoaderException)
					{
						/* DLL contents must not be good */
						retval = ErrorCode::GeneralError;
					}
				}

				/* no need to keep the copy, remove it from the file sys regardless of success. */
				remove(tempDllName.c_str());

				/* call the start routine */
				if (retval == 0) {
					try
					{
						/* call our first routine from the loaded resource*/
						retval = LIBLOADER_LOOKUP(*lib, ctre_phoenix_simulation_adapter_Start)(id);
					}
					catch (runtime::LibLoaderException)
					{
						/* DLL was good but func is missing? */
						retval = ErrorCode::GeneralError;
					}
				}

				return retval;
			}

			int32_t SimConfigGet(DeviceType /*type*/, uint32_t /*param*/, uint32_t /*valueToSend*/, uint32_t & /*outValueReceived*/, uint32_t & /*outSubvalue*/, uint32_t /*ordinal*/, uint32_t /*id*/) {
				std::lock_guard<std::mutex> guard(simCreateLock);
				return 0;
			}

			int32_t SimConfigSet(DeviceType /*type*/, uint32_t /*param*/, uint32_t /*value*/, uint32_t /*subValue*/, uint32_t /*ordinal*/, uint32_t /*id*/) {
				std::lock_guard<std::mutex> guard(simCreateLock);
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
				const char * details, const char * location, const char * /*callStack*/)
			{
				std::cout << details << std::endl << "\t" << location << std::endl;
			}
			int32_t SimDestroy(DeviceType type, int id) {
                std::lock_guard<std::mutex> guard(simCreateLock);
                
                libMap.erase(std::make_pair(type, id));
                return 0;
            }
			int32_t SimDestroyAll() {
                ClearAll();
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

namespace ctre {
	namespace phoenix {
		namespace platform {
			namespace can {

				void CANbus_GetStatus(float * /*percentBusUtilization*/, uint32_t * /*busOffCount*/, uint32_t * /*txFullCount*/, uint32_t * /*receiveErrorCount*/,
					uint32_t * /*transmitErrorCount*/, int32_t * /*status*/)
				{
					std::lock_guard<std::mutex> guard(simCreateLock);
				}
				int32_t CANbus_SendFrame(uint32_t messageID, const uint8_t * data, uint8_t dataSize)
				{
					int32_t retval = 0;
					std::lock_guard<std::mutex> guard(simCreateLock);

					for (auto &identifiedLib : libMap) {
						auto &lib = identifiedLib.second;
						int err = 0;
						try {
							err = LIBLOADER_LOOKUP(*lib, ctre_phoenix_simulation_adapter_SendCANFrame)(messageID, data, dataSize);
						}
						catch (runtime::LibLoaderException) {
							err = -1;
						}
						if (retval == 0) { retval = err; }
					}

					return retval;
				}
				int32_t CANbus_ReceiveFrame(canframe_t * toFillArray, uint32_t capacity, uint32_t &numberFilled)
				{
					/* init outputs */
					numberFilled = 0;

					/* scrutinize inputs */
					if (capacity < 1)
						return ErrorCode::InvalidParamValue;

					int32_t retval = 0;
					std::lock_guard<std::mutex> guard(simCreateLock);

					for (auto &identifiedLib : libMap) {
						auto &lib = identifiedLib.second;
						uint32_t messageID = 0;
						uint8_t dataToFill[8];
						uint8_t dataSizeFilled = 0;

						int err = 0;

						try
						{
							err = LIBLOADER_LOOKUP(*lib, ctre_phoenix_simulation_adapter_ReceiveCANFrame)(&messageID, dataToFill, &dataSizeFilled);
						}
						catch (runtime::LibLoaderException)
						{
							err = ErrorCode::GeneralError;
						}

						if (err == 0)
						{
							/* filler caller's outputs, just one frame */
							canframe_t & toFill = toFillArray[0];
							toFill.arbID = messageID;
							toFill.dlc = dataSizeFilled;
							toFill.timeStampUs = 0;
							toFill.flags = 0;
							std::memcpy(toFill.data, dataToFill, 8);
							numberFilled = 1;
							
                            //std::cout << std::hex << "rec: " << toFill.arbID << std::endl    
        
                            return 0;
						}
						/* save first bad one */
						if (retval == 0) { retval = err; }
					}

					return retval;
				}

				int32_t SetCANInterface(const char * /*interface*/)
				{
					return 0;
				}

			} //namespace can
		} //namespace platform
	} //namespace phoenix
} //namespace ctre
