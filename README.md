# DeviceIO

A class designed for DeviceIO connectivity.

## :exclamation: IMPORTANT :exclamation:

DeviceIO is a service to help you manage your IoT and embedded devices.

Create a DeviceIO product using the DeviceIO web console, specifying a product name, password, version number, and upload a firmware binary.

With your product defined in the web console, next add the DeviceIO library to your existing ESP32 or ESP8266 Arduino project and specify the product details. You can now compile and upload the firmware, and then deploy the device and it will be automatically managed with the DeviceIO web console.

Management features include remote device firmware update, remote device rebooting, and device sensor reporting.

The first time the DeviceIO library runs on a device it will contact the DeviceIO service via Wi-Fi to obtain a token. The device will save the token to flash memory, then reboot and re-contact the DeviceIO service to obtain the latest firmware version number, and if a newer version is available it will automatically download and install the new firmware. 

For more info on DeviceIO see:
- [DeviceIO](https://deviceio.goodprototyping.com/)

## Quick Start

This example is all you need to get started with DeviceIO.

``` c++
#include <DeviceIO.h>

DeviceIO provisioner;

void setup()
{
    Serial.begin(115200);
    Serial.println();
	
	provisioner.initialize();
	provisioner.debugSerial = 1;
	provisioner.buildNumber = 1;
	provisioner.productIDname ="product";
	provisioner.productIDpassword ="password";
}

void loop() 
{
	provisioner.doCheckIn();
}
```

## Usage

### As an Object

This library can be used directly by creating an object using the DeviceIO class.

``` c++
// Definition
DeviceIO provisioner;
```

## Contributing and Feedback

This is an MVP Arduino library with plenty room for improvement. Feel free to make improvements and ask for pull-requests.
