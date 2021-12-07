# deviceio

A class designed for DeviceIO connectivity.

## :exclamation: IMPORTANT :exclamation:

Info
More Info
For more info on DeviceIO see:

- [DeviceIO on Arduino](https://deviceio.goodprototyping.com/)

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
