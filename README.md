# ot-sensor-sed
Broadcasting Sleepy Thread Device with a BME280 (SHT40) sensor on a Nordic nRF52840 Dongle (PCA10059). Programmed with nRF Connect for Desktop/Zephyr.

As there is no support for OpenThread in Arduino, I'm trying my luck with the Nordic nRF Connect SDK/Zephyr. 
After struggling with the new concepts in Zephyr (device tree and overlay files), here is the first working example.
It broadcasts a JSON datagram with temperature, humidity and air pressure data from a BME280 I2C sensor over UDP and goes to sleep for 60 seconds.

* Unsolved mystery: The dongle sucks about 1.6 mA in deep sleep, but it should be about 5µA.
* Changed the sensor to an Infineon SHT40. Power consumption is now down to 600µA - still way to much.

Giving up on low power with the Zephyr SDK. It's just not possible with its core concept of a device tree.
* the i2c bus and the device (sensor) is exclusively initialized during boot.
* there is no way to power-up/initialize a sensor device during runtime
* there is no way to de-initialize/power-down a sensor device during runtime

So I'm in a limbo now:
* I have working Arduino BLE Examples which run below 5µA in deep sleep - but there is no OpenThread Library for Arduino
* I have a working Zephyr OpenThread Example which wastes 600µA in deep sleep - not usable in real world scenarios with battery powered devices