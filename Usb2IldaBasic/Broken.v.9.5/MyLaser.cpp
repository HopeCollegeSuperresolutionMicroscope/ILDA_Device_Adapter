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

//MCP4271 DAC Channel Address Bits (add more here and adjust Beam Color enum)
const char g_ILDARed637LaserDACChannelBit = 0x00;
const char g_ILDAGreen532LaserDACChannelBit = 0x02;
const char g_ILDABlue445LaserDACChannelBit = 0x04;
const char g_ILDASystemShutterChannelBit = 0x06;

const char g_ILDALaserDACChannelBits[(int) colorTotal] = {g_ILDARed637LaserDACChannelBit, g_ILDAGreen532LaserDACChannelBit, g_ILDABlue445LaserDACChannelBit};

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
const char g_ShutterAndLaserDACI2CAddress = 0x61;

//Beam Axis Specific Descriptions (add more here and adjust TiltDirection enum)
const char* g_ILDAXTiltDescrip = "Horizontal Axis Tilt Control";
const char* g_ILDAYTiltDescrip = "Vertical Axis Tilt Control";

const char* g_ILDATiltDescrips[(int) dirTotal] = {g_ILDAXTiltDescrip, g_ILDAYTiltDescrip};

//Beam Axis Specific DAC I2C Address (add more here and adjust TiltDirection enum)
const char g_ILDAXTiltDACI2CAddress = 0x4E;
const char g_ILDAYTiltDAC12CAddress = 0x4C;

const char g_ILDATiltDACI2CAddresses[(int) dirTotal] = {g_ILDAXTiltDACI2CAddress, g_ILDAYTiltDAC12CAddress};

//BeamAxis Specific Negative Switch Address (add more here and adjust TiltDirection enum)
//GPIO Values
const int g_ILDAXTiltNegAddress = 3;
const int g_ILDAYTiltNegAddress = 2;

const int g_ILDATiltNegAddresses[(int) dirTotal] = {g_ILDAXTiltNegAddress, g_ILDAYTiltNegAddress};

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

//Binary Command References
//Corresponds to cmdType enum index of class
const char ILDAMCP4271::writeCmds_[] = { 0x41, 0x00 };

const char ILDADac8571::writeCmds_[] = { 0x10 };


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
   //SetErrorText(E_ERR_CONNECTION_ALREADY_OPENED, "Device Already Open");
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

//Accepts The Pin number (i.e. GPIO 3) as declared in the constants and writes to it
int ILDAHub::GPIOwrite(int pinIndex, bool isLow)
{
	//Default no change values
	unsigned char modGpioPins[5] = { 0xFF, 0xFF, 0xFF, 0xFF };

	if(isLow)
	{
	  LogMessage("Should Be seting Pin High right now");
	}
	else
	{
		LogMessage("Should Be seting Pin Low right now");
	}
	modGpioPins[pinIndex] = (isLow) ? 0x00 : 0x01;

	std::ostringstream os;
	for( int i = 0; i<4; i++)
	{
	  os << "dataByte " << i << " Is: " << (int) modGpioPins[i] << "\n";
	}
    LogMessage(os.str().c_str(), true);


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
ILDAMCP4271::ILDAMCP4271(unsigned long resolution)
{
   //Initialize Base Class Members
   addressDacI2C_ = g_ShutterAndLaserDACI2CAddress;
   resolution_ = resolution;
   voltage_ = 0;
   writeRetries_ = 1;
   voltageMax_ = 5;
   voltageMin_ = 0;

   //Fixed since Laser controller is static 0-5Volts currently
   voltageInc_ = (voltageMax_ - voltageMin_) / resolution;
}

int ILDAMCP4271::SetVoltage(long double setVoltage, WriteCmdTypes writeCmd)
{
	if (!hub_)
	{
	  return DEVICE_COMM_HUB_MISSING;
	}

    if ( setVoltage >= voltageMax_ )
	{
	  setVoltage = voltageMax_ - voltageInc_;
	}

	unsigned int voltageCode;
	voltageCode = (unsigned int) ((setVoltage - voltageMin_) / voltageInc_);

	//Write only to one DAC Channel
	const int dataBytes = 3;
	unsigned char data[dataBytes + 1];
	//Determine Command Byte Operation
	switch(writeCmd)
	{
	  case singleWrite:
		data[0] = writeCmds_[ writeCmd ] | addressDACChannel_ & ~1;
		data[1] = (voltageCode >> 8) & 0x0F;
		data[2] = voltageCode & 0xFF;
		break;
	  default:
		  return DEVICE_ERR;
	}

	//trasmitbyte switch
	int i =0, ret;
	while( i < writeRetries_ )
	{
	  // Upon Successful Write
	  ret = hub_->I2Cwrite(dataBytes, addressDacI2C_, true, data);
	  if( ret == 0  )
	  {
		  voltage_ = voltageCode *voltageInc_;
		  break;
	  }

	  i++;
	}

	return ret;

}


/***************************************************************
  ILDALaser Implementation
  *************************************************************/

ILDALaser::ILDALaser( BeamColor color, unsigned long resolution ) : ILDAMCP4271( resolution ),
initialized_(false),
busy_(false),
addressSwitch_(g_LaserSwitchAddress),
color_(color),
powerPos_(0),
numPos_(16)
{
   //MCP4171 Object Specific Hardware Properties
   //Color Specific Address Bits
   addressDACChannel_= g_ILDALaserDACChannelBits[color]; 


   //
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
	SetVoltage(0, singleWrite);

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
	  currentVoltage = SetVoltage(currentVoltage, singleWrite);
	  LogMessageCode( (const int) currentVoltage, false);
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


ILDASystemShutter::ILDASystemShutter( unsigned long resolution ) : ILDAMCP4271( resolution ),
initialized_(false), 
name_(g_SystemShutterName)
{

   //MCP4171 Object Specific Hardware Properties
   //Shutter DAC Channel Address
   addressDACChannel_= g_ILDASystemShutterChannelBit; 

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
   SetVoltage(0, singleWrite);

   if (initialized_)
   {
      initialized_ = false;
   }
   return DEVICE_OK;
}

//Note:  Currently Toggles DAC through I2C, however, switch implementation considered for later
int ILDASystemShutter::SetOpen(bool open)
{
	std::ostringstream os;
	os << "Request " << open;
	LogMessage(os.str().c_str(), true);

	int ret =0;

   if (open)
   {
	  ret = SetVoltage(voltageMax_, singleWrite);
	  LogMessageCode(ret, false);
      return SetProperty("OnOff", "1");
   }
   else
   {
	  ret = SetVoltage(voltageMin_, singleWrite);
	  LogMessageCode(ret, false);
      return SetProperty("OnOff", "0");
   }
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


/************************************************************
MCP4721 Base Class Member Functions
*************************************************************/
ILDADac8571::ILDADac8571(TiltDirection axis, unsigned long resolution )
{
   //Initialize Base Class Members
   addressDacI2C_ = g_ILDATiltDACI2CAddresses[axis];
   resolution_ = resolution;
   voltage_ = 0;
   writeRetries_ = 1;
   voltageMax_ = 10;
   voltageMin_ = 0;

   //Fixed since Laser controller is static 0-5Volts currently
   voltageInc_ = (voltageMax_ - voltageMin_) / resolution;
}

int ILDADac8571::SetVoltage(long double setVoltage, WriteCmdTypes writeCmd)
{
	if (!hub_)
	{
	  return DEVICE_COMM_HUB_MISSING;
	}

	bool neg = false;

	if( setVoltage < 0 )
	{
		neg = true;
	}
	//Change voltage to absolute Value
	setVoltage = abs( setVoltage );

	//Ensures no overflow value to 0 setting
	if ( setVoltage >= voltageMax_ )
	{
	  setVoltage = voltageMax_ - voltageInc_;
	}

	
	unsigned int voltageCode;
	voltageCode = (unsigned int) ((setVoltage - voltageMin_) / voltageInc_);

	//Write only to one DAC Channel
	const int dataBytes = 3;
	unsigned char data[dataBytes + 1];
	//Command Byte Selection and Packaging
	switch(writeCmd)
	{
	  case dispWrite:
		data[0] = writeCmds_[ writeCmd ];
		data[1] = (voltageCode >> 8) & 0xFF;
		data[2] = voltageCode & 0xFF;
		break;
	  default:
		  return DEVICE_ERR;
	}


	//trasmitbyte switch
	int i = 0, ret;
	while( i < writeRetries_ )
	{
	  // Upon Successful Write
	  ret = hub_->I2Cwrite(dataBytes, addressDacI2C_, true, data);

	  if( ret == 0  )
	  {
		  voltage_ = (neg) ? voltageCode * voltageInc_ * -1 : voltageCode * voltageInc_;
		  break;
	  }

	  i++;
	}

	return ret;

}

/*************************************************************
ILDABeamTilt Implementation
*************************************************************/

ILDABeamTilt::ILDABeamTilt(TiltDirection axis, unsigned long resolution) : ILDADac8571(axis, resolution),
	  initialized_(false),
      busy_(false), 
	  isNeg_(false)
{
   InitializeDefaultErrorMessages();

   //Fixed since Laser controller is static 0-5Volts currently
   voltageInc_ = (voltageMax_ - voltageMin_) / resolution;

   //Set Voltage Window to the Overall Window
   vWindowMax_ = voltageMax_;
   vWindowMin_ = -1 * voltageMax_;

   name_ = g_ILDATiltNames[axis];

   addressNeg_ = g_ILDATiltNegAddresses[axis];

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

   hub_ = hub;

   // set property list
   // -----------------

   //Voltage Window Values
   //1 = true, 0 = false
   CPropertyActionEx* pXAct = new CPropertyActionEx(this, &ILDABeamTilt::OnWindowVoltage, 1);
   CreateProperty("WindowMaxVoltage", NumToToken(vWindowMax_), MM::Float, false, pXAct);
   SetPropertyLimits("WindowMaxVoltage", vWindowMin_, voltageMax_);

   pXAct = new CPropertyActionEx(this, &ILDABeamTilt::OnWindowVoltage, 0);
   CreateProperty("WindowMinVoltage", NumToToken(vWindowMin_), MM::Float, false, pXAct);
   SetPropertyLimits("WindowMinVoltage", -1 * voltageMax_, vWindowMax_);

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
   	SetVoltage(0, dispWrite);

   initialized_ = false;

   return DEVICE_OK;
}

//Sets Absolute Value of Signal
//Negative Positive Controlled By SetInversion()
int ILDABeamTilt::SetSignal(double currentVoltage)
{
	long bits;
	int ret = 0;
	bool signChange = false;

	bits = (long) (currentVoltage / voltageInc_);

	//Activate Inverting Switch if there is a change in inversion and +/-
    if( (currentVoltage < 0 && bits != 0 && isNeg_ != true) || ( ( bits == 0 || currentVoltage >= 0 ) && isNeg_ != false ))
	{
		signChange = true;
		LogMessage("Inside of ToggleNegative", true);
		ret = ToggleNegative();
		LogMessageCode(ret, false);
	}

	LogMessage("Setting Voltage soon", true);
	//To Be Changed Later
	ret = SetVoltage( currentVoltage, dispWrite );

	if( ret != 0 )
	{
		if( signChange )
		{
			ToggleNegative();
		}
	}

	LogMessage("Voltage Attempted to be set", true);
	return ret;

}

int ILDABeamTilt::ToggleNegative(void)
{
/*   ILDAHub* hub = static_cast<ILDAHub*>(GetParentHub());
   if (!hub) {
      return DEVICE_COMM_HUB_MISSING;
   }
   char hubLabel[MM::MaxStrLength];
   hub->GetLabel(hubLabel);
   SetParentID(hubLabel); // for backward comp.*/
   // Toggles Negative Value
   isNeg_ = (isNeg_) ? false : true;

   return hub_->GPIOwrite(addressNeg_, isNeg_);

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

	  pProp->Set( (double) voltage_);
	  
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
	  //Limited By Property Limits
      (Max) ? pProp->Get(vWindowMax_) : pProp->Get(vWindowMin_);
	  if(Max)
	  {
		 if(HasProperty("WindowMinVoltage"))
		 {
			SetPropertyLimits("WindowMinVoltage", -1 * voltageMax_, vWindowMax_);
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
