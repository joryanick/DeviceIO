// DeviceIO.cpp
// Client for deviceio.goodprototyping.com
//
// (c) GoodPrototyping 2020-21, All Rights Reserved
//
// DeviceIO is an embedded firmware provisioning
// system for connected Arduino platforms

// Tested platforms:
// ESP8266 Arduino Core v2.7.4
// ESP32 Arduino Core 1.0.4

// NOTE!
// ESP8266 DEBUG PORT MUST BE SET TO SERIAL1 IN THE ARDUINO IDE
// UNSURE WHY, OTHERWISE SSL CRASHES

// All apologies for using String and overly 
// cautious expression evaluation, in time these
// will be optimized away.
//
// This library is subject to change without notice,
// however my intention is to never break the service.
// If you encounter a problem you cannot resolve please
// feel free to contact me at jory@goodprototyping.com
// and I will do my best to help.

// this module on certain ESP32 installations may require editing of file
// libraries/HTTPClient/src/HTTPClient.cpp
//
// old line:
// if(!_client->connect(_host.c_str(), _port, _connectTimeout)) {
// new line:
// if(!_client->connect(_host.c_str(), _port)) {
//
// this issue is discussed here:
// https://github.com/espressif/arduino-esp32/issues/2670

//  8.11.20 * First light
//  8.13.20 * Firmware updating works on ESP32
//   1.9.21 * Added sending of sensor data
//   1.9.21 * Added ESP8266 support
//  1.10.21 * Build 1 released for testing
//  1.12.21 * https.addHeader was missing in the new SSLpost function, fixed
//          * sensorvalue is now a float
//          * NTP will now retry until it has a valid timestamp
//          * Added ADC_MODE for correct ESP8266 VCC reporting
//          * Built-in sensors (VCC, WiFi signal strength, ESP32 temp sensor) moved to IDs 255, 256, 257
//  1.13.21 * Build 2 released for testing
//  1.13.21 * Added reboot command functionality via sensor command return
//  1.14.21 * Optimizations to reduce overhead. currently compiles to:
//          * ESP32:
//          * Sketch uses 920598 bytes (70%) of program storage space. Maximum is 1310720 bytes.
//          * Global variables use 40520 bytes (12%) of dynamic memory, leaving 287160 bytes for local variables. Maximum is 327680 bytes.
//          * Serial debug now shows DeviceIO build number
//  1.20.21 * Build 3 released for testing
//  1.29.21 * Build 4-6 released for testing
//          * SPIFFS handling on ESP8266
//          * Updated API info re: ESP8266 Flash size requirement
//          * checkinInterval is set to minimum 5 minutes to reduce server load, please do not adjust this without prior agreement
//          * Build 7 released for testing
//	5.21.21 * Added HTTPClient connection timeout (5000ms)
//          * Build 8 released for testing
//   6.4.21 * Minor improvements
//          * Build 9 released for testing
//  6.11.21 * Refactorings, object definition now 'DeviceIO' instead of 'deviceio'
//  6.11.21 * NTP limits # of retries and will fail check-in if a valid timestamp is not returned
//          * Build 10 released for testing
//   7.6.21 * NTP improvements
//          * Build 11 released for testing
// 10.24.21 * Minor improvements to support upgraded server back-end
//          * Build 12 released for testing
//  12.7.21 * Minor improvements
//          * Build 13 released for testing

#include <Arduino.h>
#include "DeviceIO.h"
#include <WiFiClientSecure.h>
#include <WiFiUdp.h>
#include <time.h>

#ifdef ESP32
	#include <Update.h> // esp32 firmware updater
	#include <HTTPClient.h>
	#include <ssl_client.h>
	#include <WiFi.h>
#else
	#ifdef ESP8266
		#include <ArduinoOTA.h>
		#include <WiFiClientSecureBearSSL.h>
		#include <ESP8266HTTPClient.h>
		
		ADC_MODE(ADC_VCC); // for ESP.getVcc() to report real values
	#endif
#endif

#ifdef ESP32
	// ESP32 internal temp sensor
	#ifdef __cplusplus
		extern "C" {
	#endif
		uint8_t temprature_sens_read();
	#ifdef __cplusplus
		}
	#endif
	uint8_t temprature_sens_read();
#endif

// constructor
DeviceIO::DeviceIO(void)
{
}

// destructor
DeviceIO::~DeviceIO(void)
{
}

// main init
void DeviceIO::initialize(void)
{
	if (debugSerial == 1)
	{
		if (!Serial)
		{
			Serial.begin(115200);
			// wait for serial to initialize
			while(!Serial)
			{};
			Serial.println("");
		}
		debugMsg(F("Init"));		
	}
  
	#ifdef ESP32
		if (!SPIFFS.begin(true))
	#else
		#ifdef ESP8266
			if (!LittleFS.begin())  
		#endif
	#endif
	{
		if (debugSerial == 1)
		{
			debugMsg(F("Error mounting SPIFFS, formatting..."));
			delay(2000);
		}
		#ifdef ESP32
			SPIFFS.format();
		#else
			#ifdef ESP8266
				if (!LittleFS.format())
				{
					debugMsg(F("SPIFFS format failed"));
				} else
				{
					// reboot
					debugMsg(F("SPIFFS format OK"));
					delay(2000);
					ESP.restart();
				}
			#endif
		#endif
		unprovisionDevice();
	}
  
	// check spiffs filesystem
	if (!_DeviceIO_fileSystem.checkFlashConfig())
	{
		if (debugSerial == 1) debugMsg(F("Flash size error"));
		delay(10000); // delay 10s to prevent the IC from being hammered if we have a flash problem
		ESP.restart();
	}
  
	// check if device is provisioned already
	_DeviceIO_fileSystem.openFromFile(_DeviceIO_provisionKeyFilename, _DeviceIO_deviceProvisioned);
	if (_DeviceIO_deviceProvisioned == 1)
		// get existing token from SPIFFS
		_DeviceIO_fileSystem.openFromFile(_DeviceIO_provisionTokenFilename, _DeviceIO_deviceToken);    
	
	// initialize for ntp service
	configTime(0, 0, "pool.ntp.org", "time.nist.gov");
	// See https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv for Timezone codes for your region
	setenv("TZ", ntpTimeZoneInfo.c_str(), 1);
}

void DeviceIO::debugMsg(String msg)
{
char buf[50];

	sprintf_P(buf, PSTR("(DeviceIO b%d) "), DEVICE_IO_BUILD_NUMBER);
	Serial.print(buf);
	Serial.println(msg);
}

void DeviceIO::debugMsgHttpError(int i)
{
const __FlashStringHelper *HTTPERR = F("HTTPERR:");
const __FlashStringHelper *errmsg;

	switch(i)
	{
		case -1:
			errmsg = F("CONNECTION_REFUSED");
			break;
		case -2:
			errmsg = F("SEND_HEADER_FAILED");
			break;
		case -3:
			errmsg = F("SEND_PAYLOAD_FAILED");
			break;
		case -4:
			errmsg = F("NOT_CONNECTED");
			break;
		case -5:
			errmsg = F("CONNECTION_LOST");
			break;
		case -6:
			errmsg = F("NO_STREAM");
			break;
		case -7:
			errmsg = F("NO_HTTP_SERVER");
			break;
		case -8:
			errmsg = F("NOT_ENOUGH_RAM");
			break;
		case -9:
			errmsg = F("ENCODING");
			break;
		case -10:
			errmsg = F("STREAM_WRITE");
			break;
		case -11:
			errmsg = F("READ_TIMEOUT");
			break;
		default:
			debugMsg(String(HTTPERR), String(i));
			return;
	}
	debugMsg(String(HTTPERR), String(errmsg));
}

void DeviceIO::debugMsg(String msg, long appendnumber)
{
	debugMsg(msg + String(appendnumber));
}

void DeviceIO::debugMsg(String msg, String msg2)
{
	debugMsg(msg + msg2);
}

void DeviceIO::debugMsgError(String msg, String msg2, long appendnumber)
{
	debugMsg(msg + msg2 + String(appendnumber));
}

void DeviceIO::unprovisionDevice(void)
{
  // delete the provisioning files
  _DeviceIO_fileSystem.saveToFile(_DeviceIO_provisionKeyFilename, "0");
  _DeviceIO_fileSystem.saveToFile(_DeviceIO_provisionTokenFilename, "");
  if (debugSerial == 1) debugMsg(F("Unprovisioned"));
}

long DeviceIO::getRemoteVersionNumber(void)
{
const char *_DeviceIO_OTAgetversionprefix = "getversion&prodID=";
long vernum;

  if (debugSerial == 1) debugMsg(F("Fetching latest build number"));
  
  // example url: https://deviceio.goodprototyping.com/manage-device?cmd=getversion&prodID=radio2prodIDpass=password&token=%token%
  String serverPath = 	String(_DeviceIO_OTAHTTPSprefix) + String(_DeviceIO_OTAhost) + \
						String(_DeviceIO_OTAqueryprefix) + String(_DeviceIO_OTAgetversionprefix) + productIDname + \
						String(_DeviceIO_OTAprodIDpass) + productIDpassword + \
						String(_DeviceIO_OTAtokenprefix) + _DeviceIO_deviceToken;
  
  //if (debugSerial == 1) debugMsg(serverPath);
  
  String payload = newSSLGET(serverPath);
  if (_DeviceIO_LastHTTPcode < 1)
  {
    if (debugSerial == 1)
	{
		debugMsgError(String(_DeviceIO_httpsreq), String(_DeviceIO_errmsg_failedwitherror), _DeviceIO_LastHTTPcode);
		debugMsgHttpError(_DeviceIO_LastHTTPcode);
		debugMsg(F("Check HTTPS certificate or factory reset"));
	}
	return -1;
  }
  
  if (_DeviceIO_LastHTTPcode != 200) 
  {
	if (debugSerial == 1)
	{
		debugMsgError(String(F("Build number fetch")), String(_DeviceIO_errmsg_failedwitherror), _DeviceIO_LastHTTPcode);	
		debugMsg(F("Check provisioning token or factory reset"));
	}
	return -1;
  }
  
  // payload should be 4 bytes maximum, just the version #
  if (payload.length() < 5)
  {
    vernum = atoi(payload.c_str());
    if (debugSerial == 1)
	{
		char buf[50];
		sprintf_P(buf, PSTR("Running build #%ld, newest build is #%ld"), buildNumber, vernum);
		debugMsg(String(buf));
	}
	
    // return the remote version #
    return vernum;
  } else
    return -1;
}

uint8_t DeviceIO::getDeviceToken(void)
{
const char *_DeviceIO_OTAgetdevicetokenprefix = "gettoken&prodID=";
String serverPath;

  // example url: https://deviceio.goodprototyping.com/manage-device?cmd=gettoken&prodID=radio2&prodIDpass=password
  serverPath = 	String(_DeviceIO_OTAHTTPSprefix) + String(_DeviceIO_OTAhost) + \
				String(_DeviceIO_OTAqueryprefix) + String(_DeviceIO_OTAgetdevicetokenprefix) + productIDname + \
				String(_DeviceIO_OTAprodIDpass) + productIDpassword;

  if (debugSerial == 1)
  {
	  debugMsg(F("Getting a device token"));
	  //debugMsg(F("URL:"), serverPath);
  }
  
  String payload = newSSLGET(serverPath);
  
  if (_DeviceIO_LastHTTPcode < 1)
  {
	if (debugSerial == 1)
	{
		debugMsgError(String(_DeviceIO_httpsreq), String(_DeviceIO_errmsg_failedwitherror), _DeviceIO_LastHTTPcode);
		debugMsgHttpError(_DeviceIO_LastHTTPcode);
		debugMsg(String(_DeviceIO_errmsg_CERTfactoryreset));
	}
	return 0;
  }

  if (_DeviceIO_LastHTTPcode != 200) 
  {
    if (debugSerial == 1)
	{
		debugMsgError(String(F("Token retrieval")), String(_DeviceIO_errmsg_failedwitherror), _DeviceIO_LastHTTPcode);
	}
    return 0;
  }
  
  if (payload.length() < 1)
  {
	if (debugSerial == 1) debugMsg(F("Got empty token"));
	return 0;
  }
  
  // set token
  _DeviceIO_deviceToken = payload;
  if (debugSerial == 1)
  {
	  debugMsg(F("Got token="), _DeviceIO_deviceToken);
  }

  // save to spiffs
  _DeviceIO_fileSystem.saveToFile(_DeviceIO_provisionKeyFilename, "1");
  _DeviceIO_fileSystem.saveToFile(_DeviceIO_provisionTokenFilename, _DeviceIO_deviceToken);

  // reboot
  delay(2000);
  ESP.restart();
  return 1;
}

uint8_t DeviceIO::getNewFirmware(void)
{
const char *_DeviceIO_OTAgetdfirmwareprefix = "getfirmware&prodID=";
HTTPClient https;

  //debugMsg(F("Getting new firmware"));

  // example url: https://deviceio.goodprototyping.com/manage-device?cmd=getfirmware&prodID=radio2&prodIDpass=password&token=%token%
  String serverPath = 	String(_DeviceIO_OTAHTTPSprefix) + String(_DeviceIO_OTAhost) + \
						String(_DeviceIO_OTAqueryprefix) + String(_DeviceIO_OTAgetdfirmwareprefix) + productIDname + \
						String(_DeviceIO_OTAprodIDpass) + productIDpassword + \
						String(_DeviceIO_OTAtokenprefix) + _DeviceIO_deviceToken;
  //debugMsg(F("URL:"), serverPath);
  
  #ifdef ESP8266
	std::unique_ptr <BearSSL::WiFiClientSecure> client (new BearSSL::WiFiClientSecure);
	client->setFingerprint(_DeviceIO_OTAserverFingerprint);
	if (!https.begin(*client, serverPath))
	{
		_DeviceIO_LastHTTPcode = 0;
		return 0;
	}
  #else
	#ifdef ESP32
		https.begin(serverPath, _DeviceIO_OTAserverCertificate);
	#endif
  #endif
  
  // get the firmware
  _DeviceIO_LastHTTPcode = https.GET();
  if (_DeviceIO_LastHTTPcode < 1)
  {
	if (debugSerial == 1)
	{
		debugMsgError(String(F("getNewFirmware HTTPS request")), _DeviceIO_errmsg_failedwitherror, _DeviceIO_LastHTTPcode);
		debugMsgHttpError(_DeviceIO_LastHTTPcode);
		debugMsg(String(_DeviceIO_errmsg_CERTfactoryreset));
	}
	https.end();
	return 0;
  }

  if (_DeviceIO_LastHTTPcode != 200)
  {
	if (debugSerial == 1)
	{
		debugMsgError(String(F("getNewFirmware retrieval")), _DeviceIO_errmsg_failedwitherror, _DeviceIO_LastHTTPcode);
	}
	https.end();
	return 0;
  }
  
  unsigned long firmwarecontentLength = https.getSize();
  if (firmwarecontentLength < 1)
  {
    if (debugSerial == 1) debugMsg(F("Got empty firmware"));
		https.end();
	return 0;
  }

  if (debugSerial == 1) debugMsg(F("Downloaded bytes = "), firmwarecontentLength);

  // check if there is enough to OTA Update
  bool canBegin = Update.begin(firmwarecontentLength);
  if (!canBegin)
  {
    // not enough partition space to begin OTA
    if (debugSerial == 1) debugMsg(F("Not enough space to begin"));
	https.end();
    return 0;
  }
  
  if (debugSerial == 1) debugMsg(F("Starting OTA, please wait..."));
  delay(20); // allow serial buffer to empty before we begin update
  
  size_t written = Update.writeStream(*https.getStreamPtr());
  if ((unsigned long)written == firmwarecontentLength)
  {
    if (debugSerial == 1) debugMsg(F("Bytes written OK: "), written);
  } else
  {
    if (debugSerial == 1) 
	{
		debugMsg(F("Error, only wrote "), written);
		debugMsg(F("Rebooting..."));
	}
	https.end();
	delay(5000);
	ESP.restart();
  }
  
  // close the connection
  https.end();
  
  // check to see if update ended properly
  if (Update.end())
  {
    if (debugSerial == 1) debugMsg(F("OTA completed"));
    if (Update.isFinished())
    {
      if (debugSerial == 1) debugMsg(F("Rebooting"));
		ESP.restart();
    } else 
    {
      if (debugSerial == 1) debugMsg(F("Update failed"));
		return 0;
    }
  } else 
  {
	if (debugSerial == 1)
	{
		debugMsg(F("Update Error #"), Update.getError());
	}
	return 0;
  }

  // reboot
  delay(2000);
  ESP.restart();
  // shouldn't get here
  return 1;
}

uint8_t DeviceIO::doOTA(void)
{			
	if (debugSerial == 1) debugMsg(F("OTA check starting"));
	
	if (WiFi.status() != WL_CONNECTED)
	{
		if (debugSerial == 1) debugMsg(F("No network, exiting"));
		return 0;
	}
	
	// get device token if we have none
	if (_DeviceIO_deviceProvisioned == 0)
	{
		if (getDeviceToken()==0)
		{
			return 0;
		}
	}
	
	// get remote version
	long remotevernum = getRemoteVersionNumber();
	if (remotevernum == -1)
	{
		if (debugSerial == 1) debugMsg(F("Remote build number query failed"));
		return 0;
	} else
	if (remotevernum <= buildNumber)
	{
		if (debugSerial == 1) debugMsg(F("No new build available"));
		return 1;
	}

	if (debugSerial == 1) debugMsg(F("Fetching firmware for build #"), remotevernum);
	if (getNewFirmware() != 1)
		return 0;

	// everything worked
	return 1;
}

uint8_t DeviceIO::doCheckIn(void)
{
unsigned long now = millis();
uint8_t wifiSignalStrength = 0;
int sendSensorDataReturnValue = 0;
float voltsMcu;

#ifdef ESP32
	uint8_t esp32temp; // we need to declare this here to allow the goto to compile
#endif

	// check timer to do a check-in, run when first called
	// checkinInterval is minimum 5 minutes
	long ci = checkinInterval < (5*ONE_MINUTE) ? (5*ONE_MINUTE) : checkinInterval;
	if ( !((now > lastCheckInTimeMS + ci) || (lastCheckInTimeMS == 0)) )
		return 0;

	if (debugSerial == 1) debugMsg(F("Check-in starting"));
	
// WIFI ////////////////////
	
	// make sure we're connected
	if (WiFi.status() != WL_CONNECTED)
	{
		if (debugSerial == 1) debugMsg(F("No Network for check-in, exiting"));
		goto checkinfailed;
	}

// NTP /////////////////////

	// get the time first	
	if (getNTPtime() == 0) 
		goto checkinfailed; // if this happens we lost WiFi

// OTA /////////////////////

	// do the OTA check first
	if (doOTA() == 0)
		goto checkinfailed;
	
// SENSORS /////////////////

	// VCC
	#ifdef ESP8266	
		// not applicable on ESP32		
		voltsMcu = ESP.getVcc() / 1000.0f;
		/*if (debugSerial == 1) 
		{
			Serial.print(F("Adding VCC to provisioner = "));
			Serial.println(voltsMcu);
		}*/
		addSensorValue(255, voltsMcu);
	#endif
	
	// built-in wifi sensor
	wifiSignalStrength = getWifiSignalStrength();
    /*if (debugSerial == 1) 
	{
		Serial.print(F("Adding WiFi sensor to provisioner = "));
		Serial.println(String(wifiSignalStrength));
	}*/
    addSensorValue(256, wifiSignalStrength);
	
	#ifdef ESP32
		// built-in temp sensor
		// convert raw temperature in F to Celsius degrees
		esp32temp = ((temprature_sens_read() - 32) / 1.8);
		/*if (debugSerial == 1) 
		{
			Serial.print(F("Adding ESP32 temp sensor to provisioner = "));
			Serial.println(String(esp32temp) + "C");
		}*/
		addSensorValue(257, esp32temp);
	#endif
	
	if (_DeviceIO_sensorindex > 0)
	{
		sendSensorDataReturnValue = sendSensorData();
		if (sendSensorDataReturnValue == 0)
			goto checkinfailed;
	}

// ALERTS ///////////////////
	
// FINISHED /////////////////

	if (debugSerial == 1)
	{
		debugMsg(F("Check-In finished at "), String(now));
	}
	
	// process reboot request if any
	if (sendSensorDataReturnValue == 2)
	{
		debugMsg(F("Processing reboot request..."));
		delay(5000);
		ESP.restart();
	}
	
	// set last check-in time
	lastCheckInTimeMS = now;
	return 1;

checkinfailed:
	if (debugSerial == 1) debugMsg(F("Check-in failed"));
	// reset the lastCheckInTimeMS so the server doesn't get hammered, but soon enough
	lastCheckInTimeMS = now - (checkinInterval/8);
	return 0;
}

uint8_t DeviceIO::getNTPtime(void)
{
uint8_t attempts = 0;

	while(1)
	{
		if (doNTP(15) == 0) // wait 5 seconds to sync
		{
			if (debugSerial == 1)
				debugMsg(F("NTP Failure, retrying..."));
			
			// limit # of retries
			attempts++;
			if (attempts > 2) return 0;
			
			// wait and try again
			delay(3000);
		} else
			break; // valid NTP call
		
		// exit if we don't have a valid WiFi connection
		if (WiFi.status() != WL_CONNECTED)
			return 0;
	}
  	
	if (debugSerial == 1)
	{
		char buftime[100];
		sprintf_P(buftime, PSTR("%d/%d/%d %d:%d:%d"), 	_DeviceIO_timeinfo.tm_mon+1, 
														_DeviceIO_timeinfo.tm_mday, 
														_DeviceIO_timeinfo.tm_year+1900, 
														_DeviceIO_timeinfo.tm_hour, 
														_DeviceIO_timeinfo.tm_min, 
														_DeviceIO_timeinfo.tm_sec);
		debugMsg(F("NTP: "), String(buftime));
	}

	// set clock set
	_DeviceIO_clockneverset = 0;
	return 1; // successful NTP
}

// The time() function only calls the NTP server every hour
uint8_t DeviceIO::doNTP(int sec)
{
time_t timenow;
		
	uint32_t start = millis();
	timenow = time(nullptr);
    do
	{
      time(&timenow);
	  localtime_r(&timenow, &_DeviceIO_timeinfo);
      delay(50);
    } while (((millis() - start) <= (1000 * sec)) && (_DeviceIO_timeinfo.tm_year < (2020 - 1900)));
    
	if (_DeviceIO_timeinfo.tm_year <= (2020 - 1900)) 
		return 0;  // the NTP call was unsuccessful
	
	// ok!
	return 1;
}

void DeviceIO::getTime(struct tm &t)
{
	if (doNTP(15) == 0) // wait 15 seconds to sync
	{
		// TODO: we should reset the date/time here
	} else
	{	
		// populate the passed time structure
		t.tm_sec = _DeviceIO_timeinfo.tm_sec;
		t.tm_min = _DeviceIO_timeinfo.tm_min;
		t.tm_hour = _DeviceIO_timeinfo.tm_hour;
		t.tm_mday = _DeviceIO_timeinfo.tm_mday;
		t.tm_mon = _DeviceIO_timeinfo.tm_mon+1;
		t.tm_year = _DeviceIO_timeinfo.tm_year+1900;
	}
}

void DeviceIO::addSensorValue(int sensorNumber, float sensorValue)
{
	if (_DeviceIO_sensorindex > 19)
	{
		// push the array forward one unit
		memcpy(&_DeviceIO_sensorsamples[0], &_DeviceIO_sensorsamples[1], sizeof(_DeviceIO_sensorsamples[1])* 19);
		// set the maximum index to 19
		_DeviceIO_sensorindex = 19;
	}
	
	if (debugSerial == 1)
	{
		char numbuf[18];
		dtostrf(sensorValue, 8, 8, numbuf);  
		char msg[100];
		sprintf_P(msg, PSTR("addSensorValue index=%d, sensorNumber=%d, sensorValue=%s"),	_DeviceIO_sensorindex,
																							sensorNumber,
																							numbuf);
	
		debugMsg(msg);
	}
	
	// get the local time, sourced from NTP
	char buftime[50];
	// sql datetime format is "2020-11-25 01:50:34"
	sprintf_P(buftime, PSTR("%d-%d-%d %d:%d:%d"), 	_DeviceIO_timeinfo.tm_year+1900, 
													_DeviceIO_timeinfo.tm_mon+1, 
													_DeviceIO_timeinfo.tm_mday, 
													_DeviceIO_timeinfo.tm_hour, 
													_DeviceIO_timeinfo.tm_min, 
													_DeviceIO_timeinfo.tm_sec);
													
	_DeviceIO_sensorsamples[_DeviceIO_sensorindex].date = String(buftime);
	_DeviceIO_sensorsamples[_DeviceIO_sensorindex].sensornumber = sensorNumber;
	_DeviceIO_sensorsamples[_DeviceIO_sensorindex].sensorvalue = sensorValue;
	_DeviceIO_sensorindex++;
}

// return values:
// 0 = failed or unused
// 1 = successful
// 2 = flagged for reboot
// 3 = other commands
uint8_t DeviceIO::sendSensorData()
{
String serverPath;
int i=0;

	if (debugSerial == 1) debugMsg(F("sendSensorData starting"));

	if (_DeviceIO_sensorindex < 1)
	{
		if (debugSerial == 1) debugMsg(F("No sensor data, exiting"));
		return 0;
	}
	
	if (WiFi.status() != WL_CONNECTED)
	{
		if (debugSerial == 1) debugMsg(F("No network, exiting"));
		return 0;
	}

	// prepare sensor data payload
	String httpRequestData = "";
	for (i=0; i < _DeviceIO_sensorindex; i++)
	{
		httpRequestData += 	"&sensor[" + String(i) + "][datetime]=" + _DeviceIO_sensorsamples[i].date + 
							"&sensor[" + String(i) + "][sensornum]=" + String(_DeviceIO_sensorsamples[i].sensornumber) + 
							"&sensor[" + String(i) + "][sensorval]=" + String(_DeviceIO_sensorsamples[i].sensorvalue);
	}
	
	// example url: https://deviceio.goodprototyping.com/manage-device?cmd=sensor&prodID=radio2&prodIDpass=password&token=token
	serverPath = 	String(_DeviceIO_OTAHTTPSprefix) + String(_DeviceIO_OTAhost) + String(_DeviceIO_OTAqueryprefix) + \
							"sensor&prodID=" + productIDname + String(_DeviceIO_OTAprodIDpass) + productIDpassword + String(_DeviceIO_OTAtokenprefix) + _DeviceIO_deviceToken;

	String payload;
	payload = newSSLPOST(serverPath, httpRequestData);
	
	//if (debugSerial == 1) debugMsg(F("Sensordata Payload="), httpRequestData);
	//if (debugSerial == 1) debugMsg(F("Sensordata Payload Len="), httpRequestData.length());
	
	if (_DeviceIO_LastHTTPcode < 1)
	{
		if (debugSerial == 1)
		{
			debugMsgError(String(F("HTTPS POST request")), String(_DeviceIO_errmsg_failedwitherror), _DeviceIO_LastHTTPcode);
			debugMsgHttpError(_DeviceIO_LastHTTPcode);
			debugMsg(String(_DeviceIO_errmsg_CERTfactoryreset));
		}
		return 0;
	}

	if (_DeviceIO_LastHTTPcode != 200) 
	{
		if (debugSerial == 1)
		{
			debugMsgError(String(F("sendSensorData")), String(_DeviceIO_errmsg_failedwitherror), _DeviceIO_LastHTTPcode);
		}
		return 0;
	}
		  
	if (payload.length() < 1)
	{
		if (debugSerial == 1) debugMsg(F("Got empty sensor return value"));
		return 0;
	}
	
	// check the response
	if ( (payload.charAt(0) == 'O') && (payload.charAt(1) == 'K'))
	{
		// POST successful
		if (debugSerial == 1) debugMsg(F("sendSensorData OK"));
	}
	else
	{
		// POST FAILED
		if (debugSerial == 1) debugMsg(F("sendSensorData FAIL"));
	}
		
	// process the return string
	uint8_t flagreboot = 0;
	
	// example return: [CR] = chr$(13)
	// deviceio OK[CR]2 sensors updated[CR]REBOOT[CR]SETCMD enable(123),disable(1),ssid(abcdef),ssidpw(abcdef)[CR]
	//              1                    2         3                                                         4
	
	// process the string by line separations that begins at separation 2 as above (after '2 sensors updated[CR]')	
	// get line count
	int srLinePos = 0;
	int chr13count = 0;
	while (srLinePos < payload.length())
	{
		int i = payload.indexOf(0x0d, srLinePos); // chr(13)
		if (i == -1)
			break;
		
		if (chr13count > 1)
		{
			// dump this one
			String commandToProcess = payload.substring(srLinePos, i);
			//if (debugSerial == 1) debugMsg(commandToProcess);
			
			// process reboot request
			if (strncmp(commandToProcess.c_str(), DEVICEIO_SERVER_DIRECTIVE_REBOOT, 6) == 0)
			{
				// flag this device for reboot. the device will reset the database flag when it checks the version #
				flagreboot = 1;
				//if (debugSerial == 1) debugMsg(F("REBOOT REQUEST"));
			}
			
			// process set command
			if (strncmp(commandToProcess.c_str(), DEVICEIO_SERVER_DIRECTIVE_SET, 6) == 0)
			{
				//if (debugSerial == 1) debugMsg(F("SETCMD REQUEST"));
				// process commands here
			}
		}
		
		chr13count++;
		srLinePos = i+1;
	}
	
	// sensor data was sent, reset the sensor index
	_DeviceIO_sensorindex = 0;
	
	if (debugSerial == 1) debugMsg(F("sendSensorData finished at "), String(millis()));

	if (flagreboot == 1)
		return 2; // reboot requested
		
	return 1; // successful
}

uint8_t DeviceIO::getWifiSignalStrength()
{
const int EVALPOINTS = 3;
long rssi = 0;
long dBm = 0;
    
  for (int i=0; i < EVALPOINTS; i++)
  {
      rssi += WiFi.RSSI();
      delay(2);
  }

  dBm = rssi / EVALPOINTS;

  // convert dBm to quality percentage
  if (dBm <= -100) return 0;
  if (dBm >= -50) return 100;
  return 2 * (dBm + 100);
}

// this function should only be called for small payloads
String DeviceIO::newSSLGET(String url)
{
HTTPClient https;
String payload = ""; // empty string

	//if (debugSerial == 1) debugMsg(F("newSSLGET:"), url);	
	
	#ifdef ESP8266
		std::unique_ptr <BearSSL::WiFiClientSecure> client (new BearSSL::WiFiClientSecure);
		client->setFingerprint(_DeviceIO_OTAserverFingerprint);
		https.setTimeout(5000); // added in 0.8
		if (!https.begin(*client, url))
		{
			_DeviceIO_LastHTTPcode = 0;
			return payload;
		}		
	#else
		#ifdef ESP32
			https.setConnectTimeout(5000); // added in 0.8
			https.begin(url, _DeviceIO_OTAserverCertificate); // specify the URL and certificate
		#endif
	#endif
	
	_DeviceIO_LastHTTPcode = https.GET();
	if (_DeviceIO_LastHTTPcode > 0)
	{
		// check for the returning code
		payload = https.getString();
	}

	https.end();
	return payload;
}

// this function should only be called for small payloads
String DeviceIO::newSSLPOST(String url, String httpRequestData)
{
HTTPClient https;
String payload = ""; // empty string

	//if (debugSerial == 1) debugMsg(F("newSSLPOST:"), url);

	#ifdef ESP8266
		std::unique_ptr <BearSSL::WiFiClientSecure> client (new BearSSL::WiFiClientSecure);
		client->setFingerprint(_DeviceIO_OTAserverFingerprint);
		if (!https.begin(*client, url))
		{
			_DeviceIO_LastHTTPcode = 0;
			return payload;
		}
	#else
		#ifdef ESP32
			https.begin(url, _DeviceIO_OTAserverCertificate); //Specify the URL and certificate
		#endif
	#endif
	
	https.addHeader("Content-Type", "application/x-www-form-urlencoded");
	_DeviceIO_LastHTTPcode = https.POST(httpRequestData);				
	if (_DeviceIO_LastHTTPcode > 0)
	{
		// check for the returning code
		payload = https.getString();
	}
	https.end(); //Free the resources
	return payload;
}
// end of DeviceIO.cpp