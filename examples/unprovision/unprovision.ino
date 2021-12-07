// unprovision.ino
// Example to unprovision a device automatically
// using the DeviceIO IoT Device Management System
// https://deviceio.goodprototyping.com

// Documentation:
// https://deviceio.goodprototyping.com/device-provisioning

// DeviceIO Management Console:
// https://deviceio.goodprototyping.com/login

// (c) 2020-2021 GoodPrototyping
// https://goodprototyping.com

#include <DeviceIO.h>
#include <WiFi.h>

  DeviceIO provisioner;

void setup()
{
  Serial.begin(115200);

  // unprovision this device
  provisioner.debugSerial = 1;
  provisioner.initialize();
  provisioner.unprovisionDevice();
  
}

void loop() 
{
  // put your project loop code here
}