# TeensyTransfer
Teensy HID Transfer Tool 

Copy files from PC to a serial flash (connected to a Teensy)

Usage:
 - open Arduino, set USB-Mode to "Raw Hid"
 - load teensytransfertool.ino, compile, and flash it to the teensy
 
The gz - file contains the Linux-version, the *.zip the Windows-version

The .mac.zip file contains the Mac OS version

Commandline:
====

Serial Flash
----
 List files from serial flash: teensytransfer -l 
 
 Write file to serial flash : teensytransfer [-w] filename
 
 Read file from serial flash : teensytransfer -r filename > file
 
 Delete file from serial flash : teensytransfer -e filename
 

Parallel connected SPI Flash
----
 List files from parallel connected spi-flash: teensytransfer -l parflash
 
 Write file to parallel connected spi-flash : teensytransfer -w parflash filename
 
 Read file from parallel connected spi-flash : teensytransfer -r parflash filename > file
 
 Delete file from parallel connected spi-flash : teensytransfer -e parflash filename
 
 
Teensy internal EEPROM
----
 Write file to internal eeprom: teensytransfer -w eeprom filename
 
 Read file from  internal eeprom : teensytransfer -r eeprom > file
 
 (Output is a binary file)
 
 
Support for more devices (i.e. SD-Card) is in progress.

