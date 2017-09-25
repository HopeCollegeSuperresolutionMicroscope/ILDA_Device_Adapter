#pragma once
#ifndef _PreInitSettings_
#define _PreInitSettings_

#include <string>
#include <map>
#include <iostream>
#include <fstream>
#include "MMDeviceConstants.h"
#include "../../MMDevice/MMDevice.h"
#include "../../MMDevice/DeviceBase.h"

HMODULE GetCurrentModule( void * address)
{ // NB: XP+ solution!
  HMODULE hModule = NULL;
  GetModuleHandleEx(
    GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
    (LPCTSTR)address,
    &hModule);

  return hModule;
};

std::string trim(const std::string& str,
                 const std::string& whitespace = " \t")
{
    const auto strBegin = str.find_first_not_of(whitespace);
    if (strBegin == std::string::npos)
        return ""; // no content

    const auto strEnd = str.find_last_not_of(whitespace);
    const auto strRange = strEnd - strBegin + 1;

    return str.substr(strBegin, strRange);
};


//Development of shared Attributes Class
class ModuleSpecificSettings
{
	template< class T>
	friend class PreInitSettings;

	public:

		ModuleSpecificSettings( std::string pathName, char * delimiter = "," ): 
		  bufferPop_(false), settingFileName_(pathName), shareCount_(0), delimiter_(delimiter)
		{

			tempSettingFileName_ = settingFileName_ + "_temp";
			populateSettingsBuffer();
		};

		~ModuleSpecificSettings()
		{
			populateSettingsFile();
			fileStream_.close();
		};

		int incShareCount()
		{
			return shareCount_++;
		};

		int decShareCount()
		{
			return shareCount_--;
		};



		void populateSettingsBuffer()
		{

			//Since this is a static object the original population occurs once
			if( !fileStream_.is_open() )
			{
				//Error But simply set true so that program doesn't die
				fileStream_ = std::fstream( settingFileName_ , std::fstream::in | std::fstream::out );
				bufferPop_ = true;
			}

			if( !bufferPop_ )
			{
				std::string line;
				fileStream_.seekg( 0, fileStream_.beg );
	
				while( std::getline( fileStream_, line ) )
				{
					//Account for whiteSpace in hand-written format
					trim( line );
					settingsBuffer_.push_back( line );
				}
	
				bufferPop_ = true;
			}

			return;	 

		};


		bool populateSettingsFile()
		{
			
			//Close our constant read stream
			fileStream_.close();
			
			fileStream_.open( tempSettingFileName_ , std::fstream::out | std::fstream::trunc );
			fileStream_.seekp( 0 , fileStream_.beg );

			for( int i = 0; i < settingsBuffer_.size(); i++)
			{

				fileStream_ << settingsBuffer_[i] << "\n";

			}

			bool writeCheck = checkFileCooperation();

			fileStream_.close();

			if( writeCheck)
			{
				remove( settingFileName_.c_str() );
				rename( tempSettingFileName_.c_str(), settingFileName_.c_str() );
			}
			else
			{
				remove( tempSettingFileName_.c_str() );
			}

			//not necessary for destructor, but small overhead regardless
			fileStream_.open( settingFileName_ , std::fstream::in | std::fstream::out );

			return writeCheck;
		};

		//For Running Implementations, ensures that some version of the buffer corresponds
		//to the settings file that currently is referenced
		void runningPopulateSettingsFile()
		{
			if( populateSettingsFile() == false )
			{
				EnsureFileCooperation();
			}
			
			return;
		}

		void EnsureFileCooperation()
		{

			std::string line;
			int lineIdx = 0;
			int bufSize = settingsBuffer_.size();

			fileStream_.seekg( 0, fileStream_.beg );

			while( std::getline( fileStream_, line) )
			{
				trim( line );
				//Rewrite any lines that have been changed
				if( lineIdx < bufSize && line.compare( settingsBuffer_[ lineIdx ] ) != 0 )
				{
					settingsBuffer_[ lineIdx ] = line;
				}
				else if( lineIdx >= bufSize )
				{
					settingsBuffer_.push_back( line );
				}

				lineIdx++;
			}

			if( lineIdx < bufSize )
			{
				//Remove any extra buffers that may have been created by problem processes
				//Should not be the case
				for( int i = lineIdx - 1; i < bufSize; i++ )
				{
					settingsBuffer_.erase( settingsBuffer_.begin() + i );
				}
			}			
			return;
		};

		bool checkFileCooperation()
		{
			std::string line;
			int lineIdx = 0;
			int bufSize = settingsBuffer_.size();

			while( std::getline( fileStream_, line ) )
			{
				trim( line );
				if( lineIdx >= bufSize || line.compare( settingsBuffer_[ lineIdx ] ) != 0 )
				{
					//File has overhang
					return false;
				}

				lineIdx++;
			}

			if( lineIdx == bufSize )
			{
				return true;
			}
			else
			{
				//File wasn't written to completely
				return false;
			}
		};

	private:

		bool bufferPop_;
		int shareCount_;
		int numLines_;
		char * delimiter_;

		std::string settingFileName_;
		std::string tempSettingFileName_;
		std::vector< std::string > settingsBuffer_;
		std::fstream fileStream_;
};

template< class T >
class PreInitSettings
{
   public:

	typedef T* ChildObj;

	PreInitSettings()
		{

			sharedSettingsObj_ = Resolver::Register( (void*) this );

		};
	
	~PreInitSettings() 
		{ 
			Resolver::DeRegister( sharedSettingsObj_->settingFileName_ ); 
		};

   protected:
	   //All Protected Functions Assume CDevice< MM::Device > ChildType Inheritance
	   //There is little need to replace virtual functions, except if changing ChildType Base
	   virtual int RecordPreInitProperties()
		   {

				T* test = NULL;
				//Check to make sure the child Class is of the desired type
				if( dynamic_cast< MM::Device* >( test ) )
				{

					//downcast to Inheriting Class
					T* dev = dynamic_cast< T* >(this);
					char devName[MM::MaxStrLength];		
					std::string line;
					int lineNum = 1;

					dev->GetName( devName );
						
					//Step through settingsBuffer for Properties
					for( int i = 0; i < sharedSettingsObj_->settingsBuffer_.size(); i++ )
					{
						line = sharedSettingsObj_->settingsBuffer_[ i ];
						if( line.find( devName ) == 0 )
						{
							if( ParseLineSettings( line, StoreSettingLine ) != 0 )
							{
								dev->LogMessage( "Settings File Line could not be stored.  Possible Formatting Error", false );
							}
						}
					}

				}
				else
				{
					//error
					return DEVICE_ERR;
				}

				return DEVICE_OK;
			};

	   //Assumes MM::Device Properties but can be changed to meet a specific Child specification
	   virtual void SetAllPreInitProperties()
		   {
				std::map<std::string, std::string >::iterator it;
				for( it = properties_.begin(); it != properties_.end(); it++) 
				{
					//Ignores Default case of Improper Child Cast
					SetPreInitProperty( it->first() );
				}

				return;

			};

	   virtual int SetPreInitProperty( std::string name, std::string defaultValue = "0" )
		   {
				if( ImproperDefaultChildType() )
				{
					return DEVICE_ERR;
				}

				MM::Device* dev = dynamic_cast< MM::Device* >(this);

				//If property has not yet been instantiated (missing file or dynamic process),
				//Create New slot in properties_ map
				if( properties_.find( name ) == properties_.end() )
				{
					properties_[ name ] = defaultValue;
				}

				return dev->SetProperty( name , properties_.at( name ) );

			};
	   
	   virtual std::map<std::string, std::string> GetAllPreInitProperties()
		   {

				std::map< std::string, std::string > tempProperties;
				std::string tempValue;

				std::map<std::string, std::string >::iterator it;
			    for( it = properties_.begin(); it != properties_.end(); it++) 
				{
					//Ignores Default case of Improper Child Cast
					if( GetPreInitProperty( it->first(), tempValue ) == DEVICE_OK )
					{
						tempProperties[ it->first() ] = tempValue;
					}
			    }

				return tempProperties;

			};

	   virtual int GetPreInitProperty( std::string name, std::string& valueStr )
		   {
				if( ImproperDefaultChildType() )
				{
					return DEVICE_ERR;
				}

				MM::Device* dev = dynamic_cast< MM::Device* >(this);

				int ret = dev->GetProperty( name, valueStr );

				if( ret != DEVICE_OK )
				{
					dev->LogMessage( "Pre-Initialized Values: Error getting current value of" + name.c_str(), false);
				}
					
				return ret;

			};

	   int WritePreInitProperty()
		   {

				if( !sharedSettingsObj_->fileStream_ )
				{
					//There is a failure to Access the settings file
					return DEVICE_ERR;
				}
				else
				{

					ChildObj dev = dynamic_cast< ChildObj >(this);
					char devName[MM::MaxStrLength];
					std::string line;
					size_t search, firstDelim, secondDelim;
						
					dev->GetName( devName );
			
					std::map< std::string, std::string > writeProperties = getAllPreInitProperties();

					std::map<std::string, std::string >::iterator it;

					properties_ = writeProperties;

					for( int i = 0; i< sharedSettingsObj_->settingsBuffer_.size(); i++ )
					{
							
						line = sharedSettingsObj_->settingsBuffer_[ i ];

						if( line.find( devName ) == 0 )
						{
							//Non-Delimiter safe (Should use parseSettingLine at some point)
							firstDelim = line.find( sharedSettingsObj_->delimiter_ );
							secondDelim = line.find( sharedSettingsObj_->delimiter_ , firstDelim + 1 );

							for( it = writeProperties.begin(); it != writeProperties.end(); it++) 
							{
								search = line.find( it->first(), firstDelim );
									
								//Check to see if key values are the same
								if( search != std::string::npos &&  search + it->first().length() + 1 == secondDelim )
								{
									sharedSettingsObj_->settingsBuffer_[ i ] = devName + sharedSettingsObj_->delimiter_ + it->first() + sharedSettingsObj_->delimiter_ + it->second();
									writeProperties.erase( it );
								}

						    }

						}

					}

					//For any New initialized Values
					for( it = writeProperties.begin(); it != writeProperties.end(); it++ )
					{
						sharedSettingsObj_->settingsBuffer_.push_back( devName + sharedSettingsObj_->delimiter_ + it->first() + sharedSettingsObj_->delimiter_ + it->second() );
						writeProperties.erase(it);
					}

				}

				return DEVICE_OK;
			};

   private:

	   int ParseLineSettings( std::string line, void (*settingAct)( std::string, std::string) ,const char * delimiter )
		   {

				std::size_t firstIndex = line.find( delimiter );
				std::size_t secondIndex = line.find( delimiter, firstIndex +1 );
				if( firstIndex != std::string::npos && secondIndex != std::string::npos )
				{
				   settingAct( line.substr( firstIndex + 1, secondIndex - firstIndex ), line.substr( secondIndex + 1) );
				}
				else
				{
					return -1;
				}

				return 0;
			};

	   //Currently only supports name, value pair but can be expanded or changed
	   virtual void StoreSettingLine( std::string key, std::string value )
		   {
				properties_[ key ] = value;
				return;
		   };

	   bool ImproperDefaultChildType()
		   {
				if( dynamic_cast< CDevice<MM::Device, T>& >(ChildTypePtr))
				{
					return false;
				}

				return true;

			};

	   ModuleSpecificSettings* sharedSettingsObj_;

	   std::map< std::string, std::string > properties_;

	   T* ChildTypePtr;

};

//Registry Class

class Resolver
{
	public:
		static ModuleSpecificSettings* Register( void* classReference, char * delimiter = "," )
		{
			std::string pathName = GetSettingFileName( classReference );

			//Test for 0 condition on pathName as well

			if( !ModuleSettings_.at( pathName ) )
			{
				ModuleSettings_[ pathName ] = new ModuleSpecificSettings( pathName, delimiter );
			}
			
			ModuleSpecificSettings* Mod = ModuleSettings_.at( pathName );

			Mod->incShareCount();

			return Mod;
		};

		static bool DeRegister( std::string pathName )
		{
			if( ModuleSettings_.find( pathName ) == ModuleSettings_.end() )
			{
				ModuleSpecificSettings* Mod = ModuleSettings_.at( pathName );
			
				if( Mod->decShareCount() == 0 )
				{
					delete ModuleSettings_.at( pathName );
					ModuleSettings_.erase( pathName );
				}
				
				return true;

			}

			return false;

		};

	private:
		
		static std::string GetSettingFileName( void* classReference );

		static std::map< std::string, ModuleSpecificSettings* > ModuleSettings_;
};

std::string Resolver::GetSettingFileName( void* classReference )
{

   TCHAR dllPath[MM::MaxStrLength/sizeof(TCHAR)];
   int len = 0;

   len = GetModuleFileName( GetCurrentModule( classReference ), dllPath, 200 );
   
   //Assumes wstring and always converts to string
   std::wstring temp( dllPath[0], dllPath[len-1] );
   
   std::string dllPathStr( temp.begin(), temp.end() );

   if( len == 0 || dllPathStr.rfind( ".dll" ) == std::string::npos)
   {
	   //LogMessage( "Error: Module not locatable, no settings can be stored", false );
	   return "";
   }
   
   return dllPathStr + ".settings" ;
   
};
/*
void* FindAndCastToType( MM::Device* dev )
{
	MM::DeviceType type = dev->GetType();
	switch(type)
	{
	case  MM::UnknownType :

		break;
	case MM::AnyType :
			return dynamic_cast< MM::Generic* >(dev);
		break;
	case MM::CameraDevice :
			return dynamic_cast< MM::Camera* >(dev);
		break;
	case MM::ShutterDevice:
			return dynamic_cast< MM::Shutter* >(dev);
		break;
	case MM::StateDevice:
			return dynamic_cast< MM::State* >(dev);
		break;
	case MM::StageDevice:
			return dynamic_cast< MM::Stage* >(dev);
		break;
	case MM::XYStageDevice:
			return dynamic_cast< MM::XYStage* >(dev);
		break;
	case MM::SerialDevice:
			return dynamic_cast< MM::Serial* >(dev);
		break;
	case MM::GenericDevice:
			return dynamic_cast< MM::Generic* >(dev);
		break;
     case MM::AutoFocusDevice:
		 	return dynamic_cast< MM::AutoFocus* >(dev);
		 break;
	 case MM::CoreDevice:
		 	return dynamic_cast< MM::Core* >(dev);
		 break;
	 case MM::ImageProcessorDevice:
		 	return dynamic_cast< MM::ImageProcessor* >(dev);
		 break;
	 case MM::SignalIODevice:
		 	return dynamic_cast< MM::SignalIO* >(dev);
		  break;
      case MM::MagnifierDevice:
		  	return dynamic_cast< MM::Magnifier* >(dev);
		  break;
      case MM::SLMDevice:
		  	return dynamic_cast< MM::SLM* >(dev);
		  break;
      case MM::HubDevice:
		  	return dynamic_cast< MM::Hub* >(dev);
		  break;
      case MM::GalvoDevice
		  	return dynamic_cast< MM::Galvo* >(dev);
		  break;
	  default:
		  break;


	}


}

*/

/*

//The step process should be storing a static settingsBuffer 
//Static Buffer can be used to check against the implemented settings file
//Problems with Static Buffer, if this is to be implemented across settings classes,
//It should be a static reference unique to the class calling it


//This static reference will be filled with lines on readFromtheFile (One Time Operation)
//When I assign properties_, I will EnsureFileCooperation
//If non-cooperative, change lines that don't agree
//Unless fatal read condition, return cooperative
//Coopertive file is tested for properties_

//When writing, EnsureFileCooperation 
//Alter Lines according to file specification
//Recomposite whole txt file and overwrite


PreInitSettings< class T >::PreInitSettings()
{

	sharedSettingsObj_ = Resolver::Register( (void*) this );

};

//Stores Corresponding PreInit Value for given obj
int PreInitSettings< class T >::RecordPreInitProperties()
{

	T* test = NULL;
	//Check to make sure the child Class is of the desired type
	if( dynamic_cast< MM::Device* >( test ) )
	{

		//downcast to Inheriting Class
		ChildObj dev = dynamic_cast< T* >(this);
		char devName[MM::MaxStrLength];
		std::string line;
		int lineNum = 1;

		dev->GetName( devName );

		//Step through settingsBuffer for Properties
		for( int i = 0; i < sharedSettingsObj_->settingsBuffer_.size(); i++ )
		{
			line = sharedSettingsObj_->settingsBuffer_[ i ];
			if( line.find( devName ) == 0 )
			{
				if( ParseLineSettings( line, storeSettingLine ) != 0 )
				{
					dev->LogMessage( "Settings File Line could not be stored.  Possible Formatting Error", false );
				}
			}
		}

	}
	else
	{
		//error
		return DEVICE_ERR;
	}

	return DEVICE_OK;
};



void PreInitSettings< class T >::StoreSettingLine( std::string key, std::string value )
{
	 properties_[ key ] = value;
	 return;
};




bool PreInitSettings< class T >::ImproperDefaultChildType()
{
	if( dynamic_cast< CDevice<MM::Device, T>& >(ChildTypePtr))
	{
		return false;
	}

	return true;

}

int PreInitSettings< class T >::WritePreInitProperty()
{

	if( !sharedSettingsObj_->fileStream_ )
	{
		//There is a failure to Access the settings file
		return DEVICE_ERR;
	}
	else
	{

		ChildObj dev = dynamic_cast< ChildObj >(this);
		char devName[MM::MaxStrLength];
		std::string line;
		size_t search, firstDelim, secondDelim;

		dev->GetName( devName );
		
		std::map< std::string, std::string > writeProperties = getAllPreInitProperties();

		std::map<std::string, std::string >::iterator it;

		properties_ = writeProperties;

		for( int i = 0; i< sharedSettingsObj_->settingsBuffer_.size(); i++ )
		{

			line = sharedSettingsObj_->settingsBuffer_[ i ];

			if( line.find( devName ) == 0 )
			{
				//Non-Delimiter safe (Should use parseSettingLine at some point)
				firstDelim = line.find( sharedSettingsObj_->delimiter_ );
				secondDelim = line.find( sharedSettingsObj_->delimiter_ , firstDelim + 1 );

				for( it = writeProperties.begin(); it != writeProperties.end(); it++) 
				{
					search = line.find( it->first(), firstDelim );

					//Check to see if key values are the same
					if( search != std::string::npos &&  search + it->first().length() + 1 == secondDelim )
					{
						sharedSettingsObj_->settingsBuffer_[ i ] = devName + sharedSettingsObj_->delimiter_ + it->first() + sharedSettingsObj_->delimiter_ + it->second();
						writeProperties.erase( it );
					}

			    }

			}

		}

		//For any New initialized Values
		for( it = writeProperties.begin(); it != writeProperties.end(); it++ )
		{
			sharedSettingsObj_->settingsBuffer_.push_back( devName + sharedSettingsObj_->delimiter_ + it->first() + sharedSettingsObj_->delimiter_ + it->second() );
			writeProperties.erase(it);
		}

	}

	return DEVICE_OK;
};

//Serves Two functions, will populate a new properties_ spot if none exists
//And will also set the SetProperty
int PreInitSettings< class T >::SetPreInitProperty( std::string name,  std::string defaultValue )
{
	if( ImproperDefaultChildType() )
	{
		return DEVICE_ERR;
	}

	MM::Device* dev = dynamic_cast< MM::Device* >(this);

	//If property has not yet been instantiated (missing file or dynamic process),
	//Create New slot in properties_ map
	if( properties_.find( name ) == properties_.end() )
	{
		properties_[ name ] = defaultValue;
	}

	return dev->SetProperty( name , properties_.at( name ) );

};


int PreInitSettings< class T >::GetPreInitProperty( std::string name, std::string& valueStr )
{
	if( ImproperDefaultChildType() )
	{
		return DEVICE_ERR;
	}

	MM::Device* dev = dynamic_cast< MM::Device* >(this);

	int ret = dev->GetProperty( name, valueStr );

	if( ret != DEVICE_OK )
	{
		dev->LogMessage( "Pre-Initialized Values: Error getting current value of" + name.c_str(), false);
	}

	return ret;

}

std::map<std::string, std::string> PreInitSettings< class T >::GetAllPreInitProperties()
{

	std::map< std::string, std::string > tempProperties;
	std::string tempValue;

	std::map<std::string, std::string >::iterator it;
    for( it = properties_.begin(); it != properties_.end(); it++) 
	{
		//Ignores Default case of Improper Child Cast
		if( GetPreInitProperty( it->first(), tempValue ) == DEVICE_OK )
		{
			tempProperties[ it->first() ] = tempValue;
		}
    }

	return tempProperties;

}

void PreInitSettings< class T >::SetAllPreInitProperties()
{
	std::map<std::string, std::string >::iterator it;
    for( it = properties_.begin(); it != properties_.end(); it++) 
	{
		//Ignores Default case of Improper Child Cast
		SetPreInitProperty( it->first() );
    }

	return;

}

//Assumes Format is not tinkered with.  Will not throw exceptions except if the line is corrupt
int PreInitSettings< class T >::ParseLineSettings( std::string line, void (*settingAct)( std::string, std::string) ,const char * delimiter = "," )
{

	std::size_t firstIndex = line.find( delimiter );
	std::size_t secondIndex = line.find( delimiter, firstIndex +1 );
	if( firstIndex != std::string::npos && secondIndex != std::string::npos )
	{
	   settingAct( line.substr( firstIndex + 1, secondIndex - firstIndex ), line.substr( secondIndex + 1) );
	}
	else
	{
		return -1;
	}

	return 0;
};

*/

#endif
