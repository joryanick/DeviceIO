// DeviceIO.h
// Client for deviceio.goodprototyping.com
//
// (c) GoodPrototyping 2020-21, All Rights Reserved
//
// DeviceIO is an embedded firmware provisioning
// system for connected Arduino platforms

// Tested platforms:
// ESP8266 Arduino Core v2.7.4
// ESP32 Arduino Core 1.0.4

// All apologies for using String and overly 
// cautious expression evaluation, in time these
// will be optimized away.
//
// This library is subject to change without notice,
// however the intention is to never break the service.
// If you encounter a problem you cannot resolve please
// feel free to contact us at deviceio-support@goodprototyping.com
// and we will do our best to help.

#ifndef DeviceIO_h
#define DeviceIO_h

#include <Arduino.h>
#include "Effortless_SPIFFS.h"
#include <WiFiUdp.h>
#include <time.h>
#include <NTPClient.h>

// version control
#define DEVICE_IO_BUILD_NUMBER		12
#define ONE_MINUTE					60 * 1000		// interval in ms
#define ONE_HOUR					ONE_MINUTE * 60	// interval in ms
#define FOUR_HOURS					ONE_HOUR * 4	// OTA check-in interval is every 4 hours

struct sensordata
{
	String 		date;
	int 		sensornumber;
	float 		sensorvalue;
};

class DeviceIO
{
public:
    DeviceIO();
    ~DeviceIO(void);

	uint8_t 			debugSerial 		= 1;
	long 				buildNumber 		= 1;
	long 				lastCheckInTimeMS 	= 0;
	long 				checkinInterval 	= FOUR_HOURS;
	String 				productIDname 		= "na";
	String				productIDpassword 	= "";
	
	
    void 				initialize(void);
	uint8_t 			doCheckIn(void);
	void 				addSensorValue(int sensorNumber, float sensorValue);
	void 				unprovisionDevice(void);
	
	void 				getTime(struct tm &t);	
	String 				ntpTimeZoneInfo 	= "MST7MDT";

protected:

private:
	void 				debugMsg(String);
	void 				debugMsg(String, long);
	void 				debugMsg(String, String);
	void				debugMsgError(String, String, long);
	void				debugMsgHttpError(int);
	
	uint8_t 			sendSensorData(void);
    uint8_t 			doOTA(void);
	uint8_t 			getNewFirmware(void);
	uint8_t 			getDeviceToken(void);
	long 				getRemoteVersionNumber(void);
	uint8_t 			getWifiSignalStrength(void);

	uint8_t    			getNTPtime(void);
	uint8_t 			doNTP(int);
	
	String 				newSSLGET(String);
	String 				newSSLPOST(String, String);
	String 				getStringFromReturnValue(String data, char separator, uint8_t index);
	
	// last HTTP return code
	int					_DeviceIO_LastHTTPcode = 0;
	
	// 20 rotating sensor samples
	int    				_DeviceIO_sensorindex = 0;
	struct sensordata	_DeviceIO_sensorsamples[20];
	
	// effortless filesystem
	eSPIFFS 			_DeviceIO_fileSystem;	
	
	// clock and ntp
	uint8_t 			_DeviceIO_clockneverset = 1;
	struct tm 			_DeviceIO_timeinfo;
		
	// provisioning settings
	uint8_t 			_DeviceIO_deviceProvisioned = 0;
	String 				_DeviceIO_deviceToken = "";
	const char *		_DeviceIO_provisionKeyFilename 		= "/deviceProvisioned.txt";
	const char *		_DeviceIO_provisionTokenFilename 	= "/deviceToken.txt";
	
	// host connection strings
	const char *  		_DeviceIO_OTAhost   				= "deviceio-devices.goodprototyping.com";
	const char * 		_DeviceIO_OTAHTTPSprefix 			= "https://";
	const char *		_DeviceIO_OTAprodIDpass 			= "&prodIDpass=";
	const char * 		_DeviceIO_OTAqueryprefix 			= "/manage-device?cmd=";
	const char * 		_DeviceIO_OTAgetversionprefix 		= "getversion&prodID=";
	const char *		_DeviceIO_OTAtokenprefix			= "&token=";
	const char * 		_DeviceIO_httpsreq 					= "HTTPS request";

	
	// error strings
	const char *		_DeviceIO_errmsg_failedwitherror 	= " failed with error #";
	const char *		_DeviceIO_errmsg_CERTfactoryreset 	= "Check SSL CA or factory reset";
	
	const char *		DEVICEIO_SERVER_DIRECTIVE_REBOOT	= "REBOOT";
	const char *		DEVICEIO_SERVER_DIRECTIVE_SET		= "SETCMD";
	
	// SSL/TLS for https://deviceio-devices.goodprototyping.com
	#ifdef ESP32
		// cert for ESP32 HTTPClient
		const char* 	_DeviceIO_OTAserverCertificate		=	\
"-----BEGIN CERTIFICATE-----\n" \
"MIIDBzCCAnCgAwIBAgIJALaHl013FkeYMA0GCSqGSIb3DQEBCwUAMIGZMQswCQYD\n" \
"VQQGEwJDQTEPMA0GA1UECAwGUXVlYmVjMREwDwYDVQQHDAhNb250cmVhbDEdMBsG\n" \
"A1UECgwUR29vZFByb3RvdHlwaW5nIFtST10xGDAWBgNVBAsMD0dvb2RQcm90b3R5\n" \
"cGluZzEtMCsGA1UEAwwkZGV2aWNlaW8tZGV2aWNlcy5nb29kcHJvdG90eXBpbmcu\n" \
"Y29tMCAXDTIxMDEwOTIyMjcxOVoYDzIwODAxMjI1MjIyNzE5WjCBmTELMAkGA1UE\n" \
"BhMCQ0ExDzANBgNVBAgMBlF1ZWJlYzERMA8GA1UEBwwITW9udHJlYWwxHTAbBgNV\n" \
"BAoMFEdvb2RQcm90b3R5cGluZyBbUk9dMRgwFgYDVQQLDA9Hb29kUHJvdG90eXBp\n" \
"bmcxLTArBgNVBAMMJGRldmljZWlvLWRldmljZXMuZ29vZHByb3RvdHlwaW5nLmNv\n" \
"bTCBnzANBgkqhkiG9w0BAQEFAAOBjQAwgYkCgYEA4kAT5YbaRpPg/Tz7+gyeAVoH\n" \
"hDA/Qtii/9FUE8LZszCapmdANNdLUDuTvWtCc8VgWymdA0OoF43RmWU+p2IuN20Y\n" \
"XXf3CQMeBjgeCdG3jOVOUjYyFvrJPA5OK1eqx1WlorVf86rhlGGTDNTiWR+FArew\n" \
"NL/vq9pUSbDjxp0MdFECAwEAAaNTMFEwHQYDVR0OBBYEFDQP7UEOfif5RGF8n2vr\n" \
"hv5JYE4PMB8GA1UdIwQYMBaAFDQP7UEOfif5RGF8n2vrhv5JYE4PMA8GA1UdEwEB\n" \
"/wQFMAMBAf8wDQYJKoZIhvcNAQELBQADgYEAPKvd34ZkD77B8E/37oS3K+Ju9uWh\n" \
"fuODJTg+9OqgLwjaW8ueaq+kG5nPSIwCP2K69I1bXwwbaFXW2plL8VqPT/Pvv2S3\n" \
"nctPTAfI5t8RFCWSSE4VzQyW5Dc76gb3OWUPc+1TCllC9cv5lgoVUjOMAeHG8ubr\n" \
"/aHW8ixdgc1fRUs=\n" \
"-----END CERTIFICATE-----\n";
	#else
		#ifdef ESP8266
			// SHA-1 fingerprint for BEARSSL
			const uint8_t _DeviceIO_OTAserverFingerprint[20] = {0x93, 0x1C, 0x03, 0x1E, 0x5E, 0x3C, 0x34, 0x16, 0xE3, 0x1D, 0xD5, 0xD1, 0xE6, 0xA1, 0x60, 0xDB, 0x22, 0x48, 0xB3, 0x30};
		#endif
	#endif
};

#endif /* deviceio_h */