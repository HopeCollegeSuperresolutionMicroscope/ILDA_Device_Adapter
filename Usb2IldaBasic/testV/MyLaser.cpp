#include "MyLaser.h"
#include "../../MMDevice/ModuleInterface.h"
#include <sstream>
#include <cstdio>
#include <cmath>

#define MCP2221_LIB

#ifdef WIN32
   #define WIN32_LEAN_AND_MEAN
   #include <windows.h>
   #define snprintf _snprintf 
#endif

#include "mcp2221_dll_um.h"

//Macro function for quick converstion of numebers to properties
#define NumToToken( x ) std::to_string((long double) x).c_str()

//Adapter Hardware Descriptor

const char* g_ILDADefaultDescriptor = "ILDA-Scientific-Bridge";

//LaserControl Hardware Constants

//Colorspecific Description (add more here and adjust BeamColor enum)
const char* g_ILDARed637LaserDescrip = "Red (637nm) Power Control";
const char* g_ILDAGreen532LaserDescrip = "Green (532nm) Power Control";
const char* g_ILDABlue445LaserDescrip = "Blue (445nm) Power Control";

const char* g_ILDALaserDescrips[(int) colorTotal] = {g_ILDARed637LaserDescrip, g_ILDAGreen532LaserDescrip, g_ILDABlue445LaserDescrip};

//Colorspecific DAC Channel Address (add more here and adjust Beam Color enum)
const int g_ILDARed637LaserDACChannelBit = 0x01;
const int g_ILDAGreen532LaserDACChannelBit = 0x02;
const int g_ILDABlue445LaserDACChannelBit = 0x03;

const int g_ILDALaserDACChannelBits[(int) colorTotal] = {g_ILDARed637LaserDACChannelBit, g_ILDAGreen532LaserDACChannelBit, g_ILDABlue445LaserDACChannelBit};

//Colorspecific On/Off Channel Address  (Planned UART)
const int g_ILDARed637LaserSwitchChannelBit = 0x11;
const int g_ILDAGreen532LaserSwitchChannelBit = 0x12;
const int g_ILDABlue445LaserSwitchChannelBit = 0x13;

const int g_ILDALaserSwitchChannelBits[(int) colorTotal] = {g_ILDARed637LaserSwitchChannelBit, g_ILDAGreen532LaserSwitchChannelBit, g_ILDABlue445LaserSwitchChannelBit};

//Laser On/Off Switch Address (Planned UART)
const int g_LaserSwitchAddress = 0x23;

//Laser and Shutter DAC I2C Address
//Default for MCP4728 1100000 (last three programmable by non-micromanager methods)
//const int g_ShutterAndLaserDACI2CAddress = 0x60; (Real Default, However, factory misprogrammed, will fix later
const int g_ShutterAndLaserDACI2CAddress = 0x61;

//Beam Axis Specific Descriptions (add more here and adjust TiltDirection enum)
const char* g_ILDAXTiltDescrip = "Horizontal Axis Tilt Control";
const char* g_ILDAYTiltDescrip = "Vertical Axis Tilt Control";

const char* g_ILDATiltDescrips[(int) dirTotal] = {g_ILDAXTiltDescrip, g_ILDAYTiltDescrip};

//Beam Axis Specific DAC I2C Address (add more here and adjust TiltDirection enum)
const int g_ILDAXTiltDACI2CAddress = 0x01;
const int g_ILDAYTiltDAC12CAddress = 0x02;

const int g_ILDATiltDACI2CAddresses[(int) dirTotal] = {g_ILDAXTiltDACI2CAddress, g_ILDAYTiltDAC12CAddress};

//BeamAxis Specific Inversion Address (add more here and adjust TiltDirection enum)
const int g_ILDAXTiltInvAddress = 0x12;
const int g_ILDAYTiltInvAddress = 0x12;

const int g_ILDATiltInvAddresses[(int) dirTotal] = {g_ILDAXTiltInvAddress, g_ILDAYTiltInvAddress};

//Adapter Class Names
const char* g_ILDAHubName = "ILDA-Hub";
const char* g_Red637LaserPowerName = "Red-Laser-637nm";  //State Device (has a fixed value set) no programattic setting of range
const char* g_Green532LaserPowerName = "Green-Laser-532nm";  //State Device (has a fixed value set) no programattic setting of range
const char* g_Blue445LaserPowerName = "Blue-Laser-445nm";  //State Device (has a fixed value set) no programattic setting of range
const char* g_SystemShutterName = "System-Shutter";  //State Device (has a fixed value set) no programattic setting of range
const char* g_XTiltName = "X-Tilt";  //State Device (has a fixed value set) no programattic setting of range
const char* g_YTiltName = "Y-Tilt";  //State Device (has a fixed value set) no programattic setting of range

//Color Specific Class Names Array
const char* g_ILDALaserNames[(int) colorTotal] = {g_Red637LaserPowerName, g_Green532LaserPowerName, g_Blue445LaserPowerName};

//Beam Axis Specific Names Array
const char* g_ILDATiltNames[(int) dirTotal] = {g_XTiltName, g_YTiltName};

//Constants

const long g_MaxLaserResolution = 4096;

bool ILDABinaryFunctor::bigEndian_ = false;
bool ILDABinaryFunctor::endianCheck_ = false;




// static lock
//MMThreadLock ILDAHub::lock_;

//Endianness Test
bool ILDAIsBigEndian( void)
{
  int num = 1;
  if(*(char *)&num == 1)
  {
    return false;
  }
  else
  {
    return true;
  }
}


//Create Binary Reference MAp
std::map<std::string, unsigned char> ILDACreateBinRefMap( const std::string cmdStr[], const unsigned char binaryBase[])
        {
		  std::map< std::string, unsigned char> m;
		  for( int i = 0; i < sizeof(binaryBase)/sizeof(binaryBase[0]); i++)
		  {
			m[ cmdStr[i] ] = binaryBase[i];
		  }
		  
		  return m;
        }

//ReferenceMaps
//Laser and Shutter DAC
const std::string g_cmdStrLaserShutterDac[] = { "SingleWrite", "FastWrite" };
const unsigned char g_cmdBinLaserShutterDac[] = { 0x41, 0x00 };

//XY tilt DACS
//const std::map< std::string, unsigned char> BinaryReferenceMaps::writecmds_TiltDAC =  BinaryReferenceMaps::create_map( { "SingleWrite", "FastWrite" } , { 0x41, 0x00} );

/****************************************************************************
Module API
****************************************************************************/
MODULE_API void InitializeModuleData()
{

   RegisterDevice(g_ILDAHubName, MM::HubDevice, "Usb to DB25 Hub (required)");
   RegisterDevice(g_Red637LaserPowerName, MM::StateDevice, "Red Laser Power Control");
   RegisterDevice(g_Green532LaserPowerName, MM::StateDevice, "Green Laser Power Control");
   RegisterDevice(g_Blue445LaserPowerName, MM::StateDevice, "Blue Laser Power Control");
   RegisterDevice(g_SystemShutterName, MM::ShutterDevice, "System Shutter Control");
   RegisterDevice(g_XTiltName, MM::SignalIODevice, "Beam X Tilt");
   RegisterDevice(g_YTiltName, MM::SignalIODevice, "Beam Y Tilt");
}

MODULE_API MM::Device* CreateDevice(const char* deviceName)
{
   if (deviceName == 0)
      return nullptr;

   if (strcmp(deviceName, g_ILDAHubName) == 0)
   {
      return new ILDAHub;
   }/*
   else if (strcmp(deviceName, g_Red637LaserPowerName) == 0)
   {
      return new ILDALaser(red637, (unsigned long) pow(2.0, 12.0));
   }
   else if (strcmp(deviceName, g_Green532LaserPowerName) == 0)
   {
      return new ILDALaser(green532, (unsigned long) pow(2.0, 12.0));
   }
   else if (strcmp(deviceName, g_Blue445LaserPowerName) == 0)
   {
      return new ILDALaser(blue445, (unsigned long) pow(2.0, 12.0)); 
   }
   else if (strcmp(deviceName, g_SystemShutterName) == 0)
   {
      return new ILDASystemShutter( (unsigned long) pow(2.0, 12.0) ); // channel 2
   }
   else if (strcmp(deviceName, g_XTiltName) == 0)
   {
      return new ILDABeamTilt(x, (unsigned long) pow(2.0, 16.0));
   }
   else if (strcmp(deviceName, g_YTiltName) == 0)
   {
	  return new ILDABeamTilt(y, (unsigned long) pow(2.0, 16.0));
   }
   */
   return nullptr;
}

MODULE_API void DeleteDevice(MM::Device* pDevice)
{
   delete pDevice;
}

/******************************************************************************
ILDAHub Implementation
******************************************************************************/
ILDAHub::ILDAHub() :
      initialized_(false),
      busy_(false),
	  vid_(MCP2221_DEFAULT_VID),
	  pid_(MCP2221_DEFAULT_PID),
	  handle_(nullptr),
	  shutterState_(0)
{

   InitializeDefaultErrorMessages();

   SetErrorText(E_ERR_UNKOWN_ERROR, "Unknown Error:  Try to reconnect (possible error: HID device failure)");
   SetErrorText(E_ERR_CMD_FAILED, "Command Prompted Unexpected Device Reply");
   SetErrorText(E_ERR_INVALID_HANDLE, "Invalid Device Handle Usage, Re-open Device or Exit Application");
   SetErrorText(E_ERR_NO_SUCH_INDEX, "Non-Existent Access Attempted to be accessed.  Check VID and PID, or if hardware was unplugged.");
   SetErrorText(E_ERR_DEVICE_NOT_FOUND, "Device Not Found.  Is it Plugged in?");
   SetErrorText(E_ERR_OPEN_DEVICE_ERROR, "Error Occurred When Opening Device");
   SetErrorText(E_ERR_CONNECTION_ALREADY_OPENED, "Device Already Open");
   SetErrorText(E_ERR_CLOSE_FAILED, "Failed To Close Device");

   //For Later: Display and Translate Hexidecimal Values
   CPropertyAction* pAct = new CPropertyAction(this, &ILDAHub::OnVID);
   CreateProperty("Device Vendor ID", NumToToken(MCP2221_DEFAULT_VID), MM::Integer, false, pAct, true);

   pAct = new CPropertyAction(this, &ILDAHub::OnPID);
   CreateProperty("Device Product ID", NumToToken(MCP2221_DEFAULT_PID), MM::Integer, false, pAct, true);
	
}

void ILDAHub::GetName(char* pName) const
{
   CDeviceUtils::CopyLimitedString(pName, g_ILDAHubName);
}
	  
bool ILDAHub::Busy()
{
	return false;
}

int ILDAHub::Initialize()
{
   // Name
   int ret = CreateProperty(MM::g_Keyword_Name, g_ILDAHubName, MM::String, true);
   if (DEVICE_OK != ret)
      return ret;

   //   MMThreadGuard myLock(lock_);

   if( MM::CanCommunicate == DetectDevice() )
   {
     initialized_ = true;

     return DEVICE_OK;
   }
   
   return DEVICE_ERR;

}

int ILDAHub::Shutdown()
{
	initialized_ = false;

	if( handle_ )
	{
		//close connection
		if( 0 != VerifiedClose(handle_) )
		{
			//May want to stop the process completely
			LogMessage("Error: Device Could Not Be Closed.", false);
		}
	}
	
	return DEVICE_OK;
}

int ILDAHub::DetectInstalledDevices()
{  
	LogMessage("DetectInstalled Called");
	//In the case that this is called more than once
   ClearInstalledDevices();

   // make sure this method is called before we look for available devices
   InitializeModuleData();
   
   if ( handle_ ) 
   {
      std::vector<std::string> peripherals; 
      peripherals.clear();
      peripherals.push_back(g_Red637LaserPowerName);
      peripherals.push_back(g_Green532LaserPowerName);
      peripherals.push_back(g_Blue445LaserPowerName);
      peripherals.push_back(g_SystemShutterName);
      peripherals.push_back(g_XTiltName);
	  peripherals.push_back(g_YTiltName);
      for (size_t i=0; i < peripherals.size(); i++) 
      {
         MM::Device* pDev = ::CreateDevice(peripherals[i].c_str());
         if (pDev) 
         {
            AddInstalledDevice(pDev);
			pDev=nullptr;
         }
      }
   }
  
   return DEVICE_OK; 
}

//Verifies device connectivity from List accumulated from M_Mcp2221_GetConnectedDevices()
int ILDAHub::VerifyListedDevice(unsigned int index, void* &handle, int retries)
{
	
	int result = DEVICE_OK;
	handle = Mcp2221_OpenByIndex((unsigned int)vid_, (unsigned int)pid_, index);
		  if( handle == INVALID_HANDLE_VALUE )
		  {
			  int test = Mcp2221_GetLastError();
			  LogMessageCode(test, true);
			  switch( test )
			  {
			    case E_ERR_CONNECTION_ALREADY_OPENED:
					result = DEVICE_OCCUPIED;
					break;
				//Errors requiring reconnect attempt
				case E_ERR_OPEN_DEVICE_ERROR:
				case E_ERR_UNKOWN_ERROR:
					//reiterate the call
					if( retries > 0 )
					{
					  result = VerifyListedDevice(index, handle, --retries);
					}
					else
					{
						result = DEVICE_NOT_CONNECTED;
					}
				  break;
				//Errors requiring retry of GetConnected()
				case E_ERR_NO_SUCH_INDEX:
				case E_ERR_INVALID_HANDLE:
					result = DEVICE_COMM_HUB_MISSING;
					break;
				default:
					result = DEVICE_ERR;
			  }
		  }
		  
		  return result;
}

//Attempts to Close given handle
//Returns 0 on success, or Error Code on Failure
int ILDAHub::VerifiedClose(void* &ptr, int retries)
{
  int result=DEVICE_OK, ret = 0;

  ret = Mcp2221_Close(ptr);
  if( ret != 0 )
  {
	switch( Mcp2221_GetLastError() )
	{
		case E_ERR_INVALID_HANDLE:
			LogMessage("Warning: Already Closed Connection", false);
			break;
		case E_ERR_CLOSE_FAILED:
			if(retries>0)
			{
			  VerifiedClose(ptr, retries--);
			}
			else
			{
				LogMessageCode(E_ERR_CLOSE_FAILED, false);
				result = DEVICE_ERR;
			}
			break;
		default:
			result = DEVICE_ERR;
	}
  }

  return result;
}			

MM::DeviceDetectionStatus ILDAHub::DetectDevice( int retries )
{
	LogMessage("In Detect Device", false);
   //return MM::CanCommunicate;

   /*if (initialized_)
	  LogMessage("ERROR404:Already initialized", false);
      return MM::CanCommunicate;*/

   // all conditions must be satisfied...
   MM::DeviceDetectionStatus result = MM::Misconfigured;

   int ptrFails = 0;

   LogMessage("Entering the Try", false);

   try
   {
	  unsigned int numDevices = 0;
	  int ret = 0;

	  ret = Mcp2221_GetConnectedDevices((unsigned int)vid_, (unsigned int)pid_, &numDevices);
	  
	  LogMessage("GetConnected Returned", true);

	  assert(numDevices > 0);

	  if( DEVICE_OK != ret )
	  {
		  LogMessageCode(Mcp2221_GetLastError(), true);
		  result = MM::CanNotCommunicate;
		  return result;
	  }

	  void *ptr = nullptr;
	  std::string descriptor; 

	  std::ostringstream o;

	  o << "Testing with numDevices: " << numDevices;
	  LogMessage(o.str().c_str(), false);

	  for( unsigned int i = 0; i < numDevices; i++ )
	  {
		  ret = VerifyListedDevice(i, ptr);
		  if( ret == 0)
		  {

			LogMessage("returned a valid device");
			//Ensure the Device is ILDA-Scientific
			wchar_t descriptor[MM::MaxStrLength];
			char shortdescriptor[MM::MaxStrLength];

		    Mcp2221_GetProductDescriptor(ptr, descriptor);
			wcstombs(shortdescriptor, descriptor, MM::MaxStrLength);

			o << "Descriptor is: " << shortdescriptor;

			LogMessage(o.str().c_str(), false);

		    if( strcmp(shortdescriptor, g_ILDADefaultDescriptor) == 0 )
		    {
			  //Device has been found
				LogMessage("Found Device");
			  handle_ = ptr;
			  result = MM::CanCommunicate;
			  break;
		    }

			//Disconnect if the ptr corresponds to another MCP device
			if( 0 != VerifiedClose(ptr) )
			{
			  //May want to stop the process completely
			  LogMessage("Error: Device Could Not Be Closed.", false);
			}

		  }
		  else if( ret > 0)
		  {
			  LogMessageCode(ret, true);
			  if( ret == DEVICE_COMM_HUB_MISSING)
			  {
				  ptrFails++;
			  }
		  }
		  
	  }
   }
   catch(...)
   {
	   LogMessage("Exception in DetectDevice!", false);
   }

   //Try to call DetectDevice once more to resituate pointers and find ILDA-Scientific
   if(retries >0 && result != MM::CanCommunicate && ptrFails > 0)
   {
	  result = DetectDevice(retries--);
   }

   return result;

}

int ILDAHub::I2Cwrite(int dataLen, unsigned char slaveAddress, bool use7bitAddress, unsigned char * i2cTxData)
{
	return Mcp2221_I2cWrite(handle_, dataLen, slaveAddress, use7bitAddress, i2cTxData);
}

int ILDAHub::GPIOwrite(int pinIndex, bool isHigh)
{
	//Default no change values
	unsigned char modGpioPins[4] = {0xFF, 0xFF, 0xFF, 0xFF};

	modGpioPins[pinIndex] = (isHigh) ? 1 : 0;

	return Mcp2221_SetGpioValues(handle_, modGpioPins);
	
}

/*******************************************************************
Action Handlers
*******************************************************************/
int ILDAHub::OnVID(MM::PropertyBase* pProp, MM::ActionType pAct)
{
   if (pAct == MM::BeforeGet)
   {
      pProp->Set(vid_);
   }
   else if (pAct == MM::AfterSet)
   {
	  double vidDummy;
      pProp->Get(vidDummy);
	  if(!vidDummy)
	  {
		  LogMessage("Failed to Update Value");
	  }
	  else
	  {
	    vid_ = (long) vidDummy;
	  }
   }
   return DEVICE_OK;
}


int ILDAHub::OnPID(MM::PropertyBase* pProp, MM::ActionType pAct)
{
   if (pAct == MM::BeforeGet)
   {
      pProp->Set(pid_);
   }
   else if (pAct == MM::AfterSet)
   {
	  double pidDummy;
      pProp->Get(pidDummy);
	  if(!pidDummy)
	  {
		  LogMessage("Failed to Update Value");
	  }
	  else
	  {
	    pid_ = (long) pidDummy;
	  }
   }
   return DEVICE_OK;
}

