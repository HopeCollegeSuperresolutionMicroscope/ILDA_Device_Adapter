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
   ~ILDAHub() {Shutdown();};


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
	
	  static const std::map< std::string, unsigned char > writeCmds_;

	  int addressDacI2C_;
      int writeRetries_;
	  int addressDACChannel_;
	  unsigned long resolution_;
	  ILDAHub * hub_;
	  long double voltage_;
	  long double voltageInc_;

};

class ILDALaser : public CStateDeviceBase<ILDALaser>, ILDABinaryFunctor, ILDAMCP4271
{
public:
   ILDALaser( BeamColor color, unsigned long resolution );
   ~ILDALaser() { Shutdown(); };
  
   // MMDevice API
   // ------------
   int Initialize();
   int Shutdown();

   bool Busy();
  
   void GetName(char* pszName) const;
   
   void SetPowerPos(unsigned long powerPos) { powerPos_ = powerPos; };

   unsigned long GetNumberOfPositions()const {return numPos_;};

   // action interface
   // ----------------
   int OnState(MM::PropertyBase* pProp, MM::ActionType eAct);
   int OnVoltage(MM::PropertyBase* pProp, MM::ActionType eAct);
   int OnRetries(MM::PropertyBase* pProp, MM::ActionType eAct);
  // int OnDelay(MM::PropertyBase* pProp, MM::ActionType eAct);
   //int OnRepeatTimedPattern(MM::PropertyBase* pProp, MM::ActionType eAct);
   /*
   int OnSetPattern(MM::PropertyBase* pProp, MM::ActionType eAct);
   int OnGetPattern(MM::PropertyBase* pProp, MM::ActionType eAct);
   int OnPatternsUsed(MM::PropertyBase* pProp, MM::ActionType eAct);
   
   int OnSkipTriggers(MM::PropertyBase* pProp, MM::ActionType eAct);
   int OnStartTrigger(MM::PropertyBase* pProp, MM::ActionType eAct);
   int OnStartTimedOutput(MM::PropertyBase* pProp, MM::ActionType eAct);
   int OnBlanking(MM::PropertyBase* pProp, MM::ActionType eAct);
   int OnBlankingTriggerDirection(MM::PropertyBase* pProp, MM::ActionType eAct);

   int OnSequence(MM::PropertyBase* pProp, MM::ActionType eAct);*/

private:
   static const unsigned int NUMPATTERNS = 12;

//   static std::map< std::string, unsigned char > writeCmds_;
   

   /*
   int OpenPort(const char* pszName, long lnValue);
   int WriteToPort(long lnValue);
   int ClosePort();
   int LoadSequence(unsigned size, unsigned char* seq);*/

   //int addressDacI2C_;
   int addressSwitch_;
   //int addressDACChannel_;
   int addressSwitchChannel_;
   //unsigned long resolution_;
   std::string name_;
   double voltageMax_;
   double voltageMin_;
   unsigned long powerPos_;
   //long double voltage_;
   //long double voltageInc_;
   BeamColor color_;
   
   /*unsigned currentDelay_;
   bool sequenceOn_;
   bool blanking_;*/
   //int writeRetries_;
   int numPos_;
   bool initialized_;
   bool busy_;
};

class ILDASystemShutter : public CShutterBase<ILDASystemShutter>, ILDABinaryFunctor, ILDAMCP4271
{
public:
   ILDASystemShutter( unsigned long resolution );
   ~ILDASystemShutter();
  
   // MMDevice API
   // ------------
   int Initialize();
   int Shutdown();
  
   void GetName(char* pszName) const;
   bool Busy();
   
   // Shutter API
   int SetOpen(bool open = true);
   int GetOpen(bool& open);
   int Fire(double deltaT);

   // action interface
   // ----------------
   int OnOnOff(MM::PropertyBase* pProp, MM::ActionType eAct);

private:
   //int WriteToPort(long lnValue);

   MM::MMTime changedTime_;
   bool initialized_;
   std::string name_;
};

class ILDABeamTilt : public CSignalIOBase<ILDABeamTilt>, ILDABinaryFunctor
{
public:
   ILDABeamTilt(TiltDirection axis, unsigned long resolution);
   ~ILDABeamTilt();
  
   // MMDevice API
   // ------------
   int Initialize();
   int Shutdown();
  
   void GetName(char* pszName) const;
   bool Busy() {return busy_;}

   // DA API
   int SetGateOpen(bool open);
   int GetGateOpen(bool& open) {return DEVICE_OK;}
   int SetSignal(double volts);
   int GetSignal(double& volts) {return DEVICE_UNSUPPORTED_COMMAND;}     
   int GetLimits(double& minVolts, double& maxVolts) {minVolts = voltageMin_; maxVolts = voltageMax_; return DEVICE_OK;}
   
   int IsDASequenceable(bool& isSequenceable) const {isSequenceable = false; return DEVICE_OK;}

   // action interface
   // ----------------
   int OnVoltage(MM::PropertyBase* pProp, MM::ActionType eAct);
   int OnWindowVoltage(MM::PropertyBase* pProp, MM::ActionType eAct, long isMax);

   //Additional Methods
   int SetInversion(void);

private:

   bool initialized_;
   bool inversion_;
   bool busy_;
   unsigned long resolution_;
   double voltageMin_;
   double voltageMax_;
   double voltage_;
   double voltageInc_;

   double vWindowMax_;
   double vWindowMin_; 
   //double gatedVolts_;
  // unsigned channel_;
  // unsigned maxChannel_;
  // bool gateOpen_;
   int addressI2C_;
   int addressInv_;
   std::string name_;
};

#endif //_ILDA_H_