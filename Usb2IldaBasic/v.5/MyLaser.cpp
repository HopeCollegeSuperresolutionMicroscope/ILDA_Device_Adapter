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
const std::map< std::string, unsigned char > ILDAMCP4271::writeCmds_ = ILDACreateBinRefMap(g_cmdStrLaserShutterDac , g_cmdBinLaserShutterDac);
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
   }
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
      return new ILDALaser(blue445, (unsigned long) pow(2.0, 12.0)); // channel 1
   }
   else if (strcmp(deviceName, g_SystemShutterName) == 0)
   {
      return new ILDASystemShutter( pow(2.0, 12.0) ); // channel 2
   }
   else if (strcmp(deviceName, g_XTiltName) == 0)
   {
      return new ILDABeamTilt(x, (unsigned long) pow(2.0, 16.0));
   }
   else if (strcmp(deviceName, g_YTiltName) == 0)
   {
	  return new ILDABeamTilt(y, (unsigned long) pow(2.0, 16.0));
   }
   
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

/************************************************************
MCP4721 Base Class Member Functions
*************************************************************/

int ILDAMCP4271::SetVoltage(long double setVoltage)
{
	if (!hub_)
	{
	  return DEVICE_COMM_HUB_MISSING;
	}

	unsigned int voltageCode;
	voltageCode = (unsigned int) (setVoltage / voltageInc_);

	//Write only to one DAC Channel
	const int dataBytes = 3;
	unsigned char data[dataBytes + 1];
	//Command Byte
	data[0] = writeCmds_.find("SingleWrite")->second | addressDACChannel_ & ~1;
	data[1] = (dataBytes >> 8) & 0x0F;
	data[2] = dataBytes & 0xFF;
		
	//trasmitbyte switch
	int i =0, ret;
	while( i < writeRetries_ )
	{
	  // Upon Successful Write
	  ret = hub_->I2Cwrite(dataBytes, addressDacI2C_, true, data);
	  if( ret == 0  )
	  {
		  voltage_ = dataBytes *voltageInc_;
		  break;
	  }

	  i++;
	}

	return ret;

}


/***************************************************************
  ILDALaser Implementation
  *************************************************************/

ILDALaser::ILDALaser( BeamColor color, unsigned long resolution ) :
initialized_(false),
busy_(false),
addressSwitch_(g_LaserSwitchAddress),
color_(color),
powerPos_(0),
voltageMax_(5),
voltageMin_(0),
numPos_(16)
{
   //Initialize Base Class Members
   addressDacI2C_ = g_ShutterAndLaserDACI2CAddress;
   resolution_ = resolution;
   voltage_ = 0;
   writeRetries_ = 1;

   //Fixed since Laser controller is static 0-5Volts currently
   voltageInc_ = (voltageMax_ - voltageMin_) / resolution;

   //Color Specific Address Bits
   addressDACChannel_= g_ILDALaserDACChannelBits[color]; 
	
   addressSwitchChannel_ = g_ILDALaserSwitchChannelBits[color];

   // Description
   int ret = CreateProperty(MM::g_Keyword_Description, g_ILDALaserDescrips[color], MM::String, true);
   assert(DEVICE_OK == ret);

   // Name
   ret = CreateProperty(MM::g_Keyword_Name, g_ILDALaserNames[color], MM::String, true);
   assert(DEVICE_OK == ret);
   
   name_ =  g_ILDALaserNames[color];

   //Check Endianness

   if( !endianCheck_ )
   {
	   LogMessage("Checking Endianness", false);
	   bigEndian_ = ILDAIsBigEndian();
	   endianCheck_ = true;
   }

   // parent ID display
   CreateHubIDProperty();

}

void ILDALaser::GetName(char* pName) const
{
   CDeviceUtils::CopyLimitedString(pName, name_.c_str());
}
	  
bool ILDALaser::Busy()
{
	return false;
}

int ILDALaser::Initialize()
{
   ILDAHub* hub = static_cast<ILDAHub*>(GetParentHub());

   char hubLabel[MM::MaxStrLength];
   hub->GetLabel(hubLabel);
   SetParentID(hubLabel); // for backward comp.

   hub_ = hub;

   // set property list
   // -----------------

   // create positions and labels for power settings
   // create positions and labels
   const int bufSize = 65;
   char buf[bufSize];
   for (long i=0; i<numPos_; i++)
   {
	  long percentage = 100/numPos_*i;
      snprintf(buf, bufSize, "%d%%", (unsigned)percentage);
      SetPositionLabel(i, buf);
   }

   // State
   // -----
   CPropertyAction* pAct = new CPropertyAction (this, &ILDALaser::OnState);
   int nRet = CreateProperty(MM::g_Keyword_State, "0", MM::Integer, false, pAct);
   if (nRet != DEVICE_OK)
      return nRet;
   SetPropertyLimits(MM::g_Keyword_State, 0, numPos_ - 1);

   // Label
   // -----
   pAct = new CPropertyAction (this, &CStateBase::OnLabel);
   nRet = CreateProperty(MM::g_Keyword_Label, "", MM::String, false, pAct);
   if (nRet != DEVICE_OK)
      return nRet;

   //Raw Voltage Control
   pAct = new CPropertyAction (this, &ILDALaser::OnVoltage);
   nRet = CreateProperty("Voltage", "0", MM::Float, false, pAct);
   if (nRet != DEVICE_OK)
      return nRet;
   SetPropertyLimits("Voltage", voltageMin_, voltageMax_);

   //I2C write retries
   pAct = new CPropertyAction (this, &ILDALaser::OnRetries);
   nRet = CreateProperty("Write Retries", NumToToken( writeRetries_ ), MM::Integer, false, pAct);
   if (nRet != DEVICE_OK)
      return nRet;

   nRet = UpdateStatus();

   if (nRet != DEVICE_OK)
      return nRet;

   initialized_ = true;

   return DEVICE_OK;
}

int ILDALaser::Shutdown()
{
	initialized_ = false;
	
	return DEVICE_OK;
}

/******************************************************
  Property Actions
******************************************************/

int ILDALaser::OnState(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	//In reality, Collect Data on Power Levels and Then Change

   if (eAct == MM::BeforeGet)
   {
      // nothing to do, let the caller use cached property
   }
   else if (eAct == MM::AfterSet)
   {
      long powerPos;
      pProp->Get(powerPos);
      SetPowerPos(powerPos);
    }
                                          

   return DEVICE_OK;
}

int ILDALaser::OnVoltage(MM::PropertyBase* pProp, MM::ActionType eAct)
{

   if (eAct == MM::BeforeGet)
   {
      // nothing to do, let the caller use cached property
   }
   else if (eAct == MM::AfterSet)
   {
      double currentVoltage;
      pProp->Get(currentVoltage);
	  SetVoltage(currentVoltage);
	  pProp->Set((double) voltage_);
	  UpdateProperty( "Voltage" );
   }
                                          
   return DEVICE_OK;

}

int ILDALaser::OnRetries(MM::PropertyBase* pProp, MM::ActionType eAct)
{

   if (eAct == MM::BeforeGet)
   {
      // nothing to do, let the caller use cached property
   }
   else if (eAct == MM::AfterSet)
   {
	  double proxy;
      pProp->Get(proxy);
	  writeRetries_ = (int) proxy;
   }
                                          
   return DEVICE_OK;

}

/***************************************************************
ILDASystemShutter Implementation
***************************************************************/


ILDASystemShutter::ILDASystemShutter( unsigned long resolution ) : 
initialized_(false), 
name_(g_SystemShutterName)
{

   //Initialize Base Class Members
   addressDacI2C_ = g_ShutterAndLaserDACI2CAddress;
   resolution_ = resolution;
   voltage_ = 0;
   writeRetries_ = 1;

   InitializeDefaultErrorMessages();
   //EnableDelay();

   // Name
   int ret = CreateProperty(MM::g_Keyword_Name, g_SystemShutterName, MM::String, true);
   assert(DEVICE_OK == ret);

   // Description
   ret = CreateProperty(MM::g_Keyword_Description, "Whole System Shutter (Meant for On-Off Purposes)", MM::String, true);
   assert(DEVICE_OK == ret);

   if( !endianCheck_ )
   {
	   LogMessage("Checking Endianness", false);
	   bigEndian_ = ILDAIsBigEndian();
	   endianCheck_ = true;
   }

   // parent ID display
   CreateHubIDProperty();
}

ILDASystemShutter::~ILDASystemShutter()
{
   Shutdown();
}

void ILDASystemShutter::GetName(char* name) const
{
   CDeviceUtils::CopyLimitedString(name, name_.c_str());
}

bool ILDASystemShutter::Busy()
{
   //Electronic Shutter, unnecessary
   /*MM::MMTime interval = GetCurrentMMTime() - changedTime_;

   if (interval < (1000.0 * GetDelayMs() ))
      return true;
   else*/
       return false;
}

int ILDASystemShutter::Initialize()
{
   ILDAHub* hub = static_cast<ILDAHub*>(GetParentHub());
   if (!hub) {
      //return ;
   }
   char hubLabel[MM::MaxStrLength];
   hub->GetLabel(hubLabel);
   SetParentID(hubLabel); // for backward comp.

   hub_ = hub;

   // set property list
   // -----------------
   
   // OnOff
   // ------
   CPropertyAction* pAct = new CPropertyAction (this, &ILDASystemShutter::OnOnOff);
   int ret = CreateProperty("OnOff", "0", MM::Integer, false, pAct);
   if (ret != DEVICE_OK)
      return ret;

   // set shutter into the off state
   //WriteToPort(0);

   std::vector<std::string> vals;
   vals.push_back("0");
   vals.push_back("1");
   ret = SetAllowedValues("OnOff", vals);
   if (ret != DEVICE_OK)
      return ret;

   ret = UpdateStatus();
   if (ret != DEVICE_OK)
      return ret;

   changedTime_ = GetCurrentMMTime();
   initialized_ = true;

   return DEVICE_OK;
}

int ILDASystemShutter::Shutdown()
{
   if (initialized_)
   {
      initialized_ = false;
   }
   return DEVICE_OK;
}

int ILDASystemShutter::SetOpen(bool open)
{
	std::ostringstream os;
	os << "Request " << open;
	LogMessage(os.str().c_str(), true);

   if (open)
      return SetProperty("OnOff", "1");
   else
      return SetProperty("OnOff", "0");
}

int ILDASystemShutter::GetOpen(bool& open)
{
   char buf[MM::MaxStrLength];
   int ret = GetProperty("OnOff", buf);
   if (ret != DEVICE_OK)
      return ret;
   long pos = atol(buf);
   pos > 0 ? open = true : open = false;

   return DEVICE_OK;
}

int ILDASystemShutter::Fire(double /*deltaT*/)
{
   return DEVICE_UNSUPPORTED_COMMAND;
}
/*
int ILDASystemShutter::WriteToPort(long value)
{
   CArduinoHub* hub = static_cast<CArduinoHub*>(GetParentHub());
   if (!hub || !hub->IsPortAvailable())
      return ERR_NO_PORT_SET;

   MMThreadGuard myLock(hub->GetLock());

   value = 63 & value;
   if (hub->IsLogicInverted())
      value = ~value;

   hub->PurgeComPortH();

   unsigned char command[2];
   command[0] = 1;
   command[1] = (unsigned char) value;
   int ret = hub->WriteToComPortH((const unsigned char*) command, 2);
   if (ret != DEVICE_OK)
      return ret;

   MM::MMTime startTime = GetCurrentMMTime();
   unsigned long bytesRead = 0;
   unsigned char answer[1];
   while ((bytesRead < 1) && ( (GetCurrentMMTime() - startTime).getMsec() < 250)) {
      ret = hub->ReadFromComPortH(answer, 1, bytesRead);
      if (ret != DEVICE_OK)
         return ret;
   }
   if (answer[0] != 1)
      return ERR_COMMUNICATION;

   hub->SetTimedOutput(false);

   return DEVICE_OK;
}*/

///////////////////////////////////////////////////////////////////////////////
// Action handlers
///////////////////////////////////////////////////////////////////////////////

int ILDASystemShutter::OnOnOff(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   ILDAHub* hub = static_cast<ILDAHub*>(GetParentHub());
   if (eAct == MM::BeforeGet)
   {
      // get initialized state
      pProp->Set((long)hub->GetShutterState());
   }
   else if (eAct == MM::AfterSet)
   {
      long pos;
      pProp->Get(pos);
      if (pos != 0 && pos != 1)
	  {
         return DEVICE_ERR; // turn everything off
	  }
      else
	  {
		//Communicate to Hub
        hub->SetShutterState( (bool) pos );
	  }
   }

   return DEVICE_OK;
}


/*************************************************************
ILDABeamTilt Implementation
*************************************************************/

ILDABeamTilt::ILDABeamTilt(TiltDirection axis, unsigned long resolution) :
	  initialized_(false),
      busy_(false), 
      voltageMax_(10),
      voltageMin_(-10),
      voltage_(0),
	  resolution_(resolution),
	  inversion_(false)
{
   InitializeDefaultErrorMessages();

   //Fixed since Laser controller is static 0-5Volts currently
   voltageInc_ = (voltageMax_ - voltageMin_) / resolution;

   //Set Voltage Window to the Overall Window
   vWindowMax_ = voltageMax_;
   vWindowMin_ = voltageMin_;

   name_ = g_ILDATiltNames[axis];

   addressI2C_ = g_ILDATiltDACI2CAddresses[axis];

   addressInv_ = g_ILDATiltInvAddresses[axis];

   // Description
   int nRet = CreateProperty(MM::g_Keyword_Description, g_ILDATiltDescrips[axis], MM::String, true);
   assert(DEVICE_OK == nRet);

   // Name
   nRet = CreateProperty(MM::g_Keyword_Name, name_.c_str(), MM::String, true);
   assert(DEVICE_OK == nRet);

   if( !endianCheck_ )
   {
	   LogMessage("Checking Endianness", false);
	   bigEndian_ = ILDAIsBigEndian();
	   endianCheck_ = true;
   }

   // parent ID display
   CreateHubIDProperty();
}

ILDABeamTilt::~ILDABeamTilt()
{
   Shutdown();
}

void ILDABeamTilt::GetName(char* name) const
{
   CDeviceUtils::CopyLimitedString(name, name_.c_str());
}


int ILDABeamTilt::Initialize()
{
   ILDAHub* hub = static_cast<ILDAHub*>(GetParentHub());
   if (!hub) {
      //return ERR_NO_PORT_SET;
   }
   char hubLabel[MM::MaxStrLength];
   hub->GetLabel(hubLabel);
   SetParentID(hubLabel); // for backward comp.

   // set property list
   // -----------------

   //Voltage Window Values
   CPropertyActionEx* pXAct = new CPropertyActionEx(this, &ILDABeamTilt::OnWindowVoltage, 1);
   CreateProperty("WindowMaxVoltage", NumToToken(vWindowMax_), MM::Float, false, pXAct);
   SetPropertyLimits("WindowMaxVoltage", vWindowMin_, voltageMax_);

   pXAct = new CPropertyActionEx(this, &ILDABeamTilt::OnWindowVoltage, 0);
   CreateProperty("WindowMinVoltage", NumToToken(vWindowMin_), MM::Float, false, pXAct);
   SetPropertyLimits("WindowMinVoltage", voltageMin_, vWindowMax_);

   // State
   // -----
   CPropertyAction* pAct = new CPropertyAction (this, &ILDABeamTilt::OnVoltage);
   int nRet = CreateProperty("Voltage", "0.0", MM::Float, false, pAct);
   if (nRet != DEVICE_OK)
      return nRet;
   SetPropertyLimits("Voltage", vWindowMin_, vWindowMax_);

   nRet = UpdateStatus();
   if (nRet != DEVICE_OK)
      return nRet;

   initialized_ = true;

   return DEVICE_OK;
}

int ILDABeamTilt::Shutdown()
{
   initialized_ = false;
   return DEVICE_OK;
}
/*
int ILDABeamTilt::WriteSignal(double volts)
{
   long value = (long) ( (volts - minV_) / maxV_ * 4095);

   std::ostringstream os;
    os << "Volts: " << volts << " Max Voltage: " << maxV_ << " digital value: " << value;
    LogMessage(os.str().c_str(), true);

   return WriteToPort(value);
}*/


//Sets Absolute Value of Signal
//Negative Positive Controlled By SetInversion()
int ILDABeamTilt::SetSignal(double currentVoltage)
{
	int byte, ret;
	byte = (int) (currentVoltage / voltageInc_);

	//Activate Inverting Switch if there is a change in inversion and +/-
    if( currentVoltage < 0 && byte > 0 && inversion_ != true)
	{
		SetInversion();
		inversion_ = true;
	}
	else if( ( byte == 0 || currentVoltage >= 0 ) && inversion_ != false )
	{
		SetInversion();
		inversion_ = false;
	}

	/*
	char * data [byte;

	for(int i=0; i<byte; i = i + 8)
	{
		data
	}
	*/
//	SetPosition();

	//Upon Success

	voltage_ = byte * voltageInc_;

	return DEVICE_OK;

}

int ILDABeamTilt::SetInversion(void)
{
   ILDAHub* hub = static_cast<ILDAHub*>(GetParentHub());
   if (!hub) {
      return DEVICE_COMM_HUB_MISSING;
   }
   char hubLabel[MM::MaxStrLength];
   hub->GetLabel(hubLabel);
   SetParentID(hubLabel); // for backward comp.

   return hub->GPIOwrite(addressInv_, inversion_);

}

int ILDABeamTilt::SetGateOpen(bool open)
{
   return DEVICE_OK;
}

///////////////////////////////////////////////////////////////////////////////
// Action handlers
///////////////////////////////////////////////////////////////////////////////

int ILDABeamTilt::OnVoltage(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::BeforeGet)
   {
      // nothing to do, let the caller use cached property
   }
   else if (eAct == MM::AfterSet)
   {
      double voltage;
      pProp->Get(voltage);
	  int ret;
      ret = SetSignal(voltage);
	  
	  if( ret != DEVICE_OK )
		  return DEVICE_ERR;

	  pProp->Set(voltage_);
	  
   }

   return DEVICE_OK;
}

//IsMax treated as Boolean (0 = Min, !0 = Max)
int ILDABeamTilt::OnWindowVoltage(MM::PropertyBase* pProp, MM::ActionType eAct, long isMax)
{
   bool Max = (isMax != 0) ? true : false;

   if (eAct == MM::BeforeGet)
   {
	  //Static Property
	  (Max) ? pProp->Set(vWindowMax_) : pProp->Set(vWindowMin_);
   }
   else if (eAct == MM::AfterSet)
   {
/*	  double voltageTest;

	  pProp->Get(voltageTest);

	  if(Max)
	  {
		  if(voltageTest > voltageMax_)
		  {
			  voltageTest = voltageMax_;
		  }
		  else if(voltageTest < voltageMin
	  }*/
	  //Limited By Property Limits
      (Max) ? pProp->Get(vWindowMax_) : pProp->Get(vWindowMin_);
	  if(Max)
	  {
		 if(HasProperty("WindowMinVoltage"))
		 {
			SetPropertyLimits("WindowMinVoltage", voltageMin_, vWindowMax_);
			UpdateProperty( "WindowMinVoltage" );
		 }
	  }
	  else
	  {
		  if(HasProperty("WindowMaxVoltage"))
		  {
			SetPropertyLimits("WindowMaxVoltage", vWindowMin_, voltageMax_);
			 UpdateProperty( "WindowMaxVoltage" );
		  }
	  }

      if (HasProperty("Voltage"))
	  {
         SetPropertyLimits("Voltage", vWindowMin_, vWindowMax_);
		 UpdateProperty( "Voltage" );
	  }

   }
   return DEVICE_OK;
}
/*
int ILDABeamTilt::OnChannel(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::BeforeGet)
   {
      pProp->Set((long int)channel_);
   }
   else if (eAct == MM::AfterSet)
   {
      long channel;
      pProp->Get(channel);
      if (channel >=1 && ( (unsigned) channel <=maxChannel_) )
         channel_ = channel;
   }
   return DEVICE_OK;
}
*/