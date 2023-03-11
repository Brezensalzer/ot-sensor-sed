# ot-sensor-sed
Broadcasting Sleepy Thread Device with  a BME280 sensor on a Nordic nRF52840 Dongle (PCA10059). Programmed with nRF Connect for Desktop/Zephyr.

As there is no support for OpenThread in Arduino, I'm trying my luck with the Nordic nRF Connect SDK/Zephyr. 
After struggling with the new concepts in Zephyr (device tree and overlay files), here is the first working example.
It broadcasts a JSON datagram with temperature, humidity and air pressure data from a BME280 I2C sensor over UDP and goes to sleep for 60 seconds.

Unsolved mystery: The dongle sucks about 1.6 mA in deep sleep, but it should be about 5µA.
