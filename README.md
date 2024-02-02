# ArtnetNode 
## Introduction
This project is intended to realize a small device capable of driving led strips with data received from wifi via the Artnet protocol.
The code works on a esp32s3 

It uses a few awesome libraries :
* to receive Artnet data from wifi : https://github.com/hpwit/artnetesp32v2
* to drive leds efficiently with an esp32s3 : https://github.com/hpwit/I2SClockLessLedDriveresp32s3
* https://github.com/FastLED/FastLED
* to handle the config with a web page : https://github.com/gemi254/ConfigAssist-ESP32-ESP8266

The Artnet frames are inserted in a queue when received.
Every 50ms, the frames are picked out from the queue and passed in to the led driver.
If the queue length gets low , the refresh rate is lowered to give a chance to the artnet frames to refill the queue.

The configuration of the device is set on a web page : wifi config, number of leds, color scheme, artnet first universe number ..
