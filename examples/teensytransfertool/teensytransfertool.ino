/*
  TeensyTransfer Demo
	
*/


#include <TeensyTransfer.h>

void setup() {
  //uncomment these if using Teensy audio shield
  //SPI.setSCK(14);  // Audio shield has SCK on pin 14
  //SPI.setMOSI(7);  // Audio shield has MOSI on pin 7
}


void loop() {
	ttransfer.transfer();
}
