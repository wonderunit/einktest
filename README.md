# einktest

https://github.com/wonderunit/einktest/blob/master/src/main.cpp

![Overview](https://github.com/wonderunit/einktest/raw/master/IMG_20190626_161934.jpg)

(**Ignore what is on the screen that is from testing with the Raspberry Pi**)

![Pinout](https://github.com/wonderunit/einktest/raw/master/IMG_20190626_162000.jpg)

Waveshare HAT SPI | NodeMCU-32S
------------ | -------------
5V | 5V
GND | GND
MISO | P19
MOSI | P23
SCK | CLK
CS | P4
RST | P26
HRDY | P4

## serial output:

```
setup
init reset_to_ready : 1716001
_writeCommand16(0x302) : 208797
GetIT8951SystemInfo : 1
Panel(W,H) = (0,0)
Image Buffer Address = 0
FW Version = 
LUT Version = 
VCOM = -0.00V
VCOM = -0.00V
_PowerOn : 4
refresh : 1
refresh : 1
_PowerOff : 1
_PowerOn : 1
refresh : 1
refresh : 1
refresh : 1
refresh : 1
_PowerOff : 1
```
