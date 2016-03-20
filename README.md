# TeensyTransfer
Teensy HID Transfer Tool 

Copy files from PC to a serial flash (connected to a Teensy)

Usage:
 - open Arduino, set USB-Mode to "Raw Hid"
 - load teensytransfertool.ino, compile, and flash it to the teensy
 
The gz - file contains the Linux-version, the *.zip the Windows-version

Commandline:

 List files : teensytransfer -l 
 
 Write file to flash : teensytransfer [-w] filename
 
 Read file from flash : teensytransfer -r filename > file
 
 Delete file from flash : teensytransfer -e filename
 
 
Support for more devices (i.e. eeprom) is in the works.

