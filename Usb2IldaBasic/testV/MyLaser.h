#ifndef _ILDA_H_
#define _ILDA_H_

#include "../../MMDevice/MMDevice.h"
#include "../../MMDevice/DeviceBase.h"
#include <string>
#include <map>

//Manufacturer Defaults
#define MCP2221_DEFAULT_VID 0x4D8
#define MCP2221_DEFAULT_PID 0xDD

//Custom Constants
#define DEVICE_OCCUPIED -1

enum BeamColor{
	red637 = 0,
	green532,
	blue445,

	colorTotal
};

enum TiltDirection{
	x = 0,
	y,

	dirTotal
};

//Binary Reference Maps
//It is the burden of the programmer to make sure the char and cmdStr match lengths
std::map<std::string, unsigned char> ILDACreateBinRefMap( std::string cmdStr[], unsigned char binaryBase[]);

//Endianness Check
bool ILDAIsBigEndian( void);

//Class Instantiations

class ILDABinaryFunctor
{
	public:
		ILDABinaryFunctor(){};
		~ILDABinaryFunctor(){};

	protected:
	  static bool endianCheck_;
      static bool bigEndian_;
};

class ILDAHub : public HubBase<ILDAHub>
{
public:
	ILDAHub();
   ~ILDAHub() {Shutdown();}


   // Device API
   // ---------
   int Initialize();
   int Shutdown();
   void GetName(char* pName) const; 
   bool Busy();

   // HUB api
   MM::DeviceDetectionStatus DetectDevice(int retries = 3);
   int VerifyListedDevice(unsigned int index, void* &handle, int retries = 3);
   int VerifiedClose(void* &ptr, int retries = 3);
   int DetectInstalledDevices(void);

   void SetShutterState(bool state) {shutterState_ = state;};
   bool GetShutterState(void) { return shutterState_;};
   int I2Cwrite(int dataLen, unsigned char slaveAddress, bool use7bitAddress, unsigned char * i2cTxData);
   int GPIOwrite(int pinIndex, bool isHigh);

   //Property Events
   int OnVID(MM::PropertyBase* pProp, MM::ActionType pAct);
   int OnPID(MM::PropertyBase* pProp, MM::ActionType pAct);

private:
   void GetPeripheralInventory();

   std::vector<std::string> peripherals_;
   //static MMThreadLock lock_;
   bool shutterState_;
   bool initialized_;
   bool busy_;
   long vid_;
   long pid_;
   void *handle_;
};

class ILDAMCP4271
{
	public:
		ILDAMCP4271(){};
		~ILDAMCP4271(){};

	int SetVoltage(long double setVoltage);

	protected:
	
	  static std::map< std::string, unsigned char > writeCmds_;

	  int addressDacI2C_;
      int writeRetries_;
	  int addressDACChannel_;
	  unsigned long resolution_;
	  ILDAHub * hub_;
	  long double voltage_;
	  long double voltageInc_;

};

#endif //_ILDA_H_