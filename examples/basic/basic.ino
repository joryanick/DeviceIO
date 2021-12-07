// basic.ino
// Example to provision and maintain a device automatically
// using the DeviceIO IoT Device Management System
// https://deviceio.goodprototyping.com

// Documentation:
// https://deviceio.goodprototyping.com/device-provisioning

// DeviceIO Management Console:
// https://deviceio.goodprototyping.com/login

// (c) 2020-2021 GoodPrototyping
// https://goodprototyping.com

#include <DeviceIO.h>
#ifdef ESP8266
	#include <ESP8266WiFi.h>
#else
	#include <WiFi.h>	
#endif

  DeviceIO provisioner;

  const char *ssid = "SSID";
  const char *password = "PASSWORD";

void setup()
{
  Serial.begin(115200);
  Serial.print("Connecting");
  WiFi.begin(ssid, password);

  // wait for connection to complete
  while (WiFi.status() != WL_CONNECTED)
    { delay(250); Serial.print("."); }

  Serial.println(""); Serial.print("Connected to ");
  Serial.println(ssid); Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // provision this device automatically
  provisioner.initialize();
  provisioner.debugSerial = 1;
  provisioner.buildNumber = 1;
  provisioner.productIDname ="basic";
  provisioner.productIDpassword ="password";
}

void loop() 
{
  // put your project loop code here
  
  // leave this at the bottom of loop() to maintain
  // automatic device management
  provisioner.doCheckIn();
}