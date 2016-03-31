/* TeensyTransfer library code is placed under the MIT license
 * Copyright (c) 2016 Frank Bösing
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "TeensyTransfer.h"

#if defined(KINETISK) || defined(KINETISL)

#include <avr/eeprom.h>

TeensyTransfer ttransfer;


void TeensyTransfer::transfer(void) {
  uint8_t device, mode;
  int n;
	n = RawHID.recv(buffer, 0); // 0 timeout = do not wait
	if (n<1) return;
	  
	mode = buffer[0];
	device = buffer[1];
	if ( hid_sendAck() < 0 ) {
		//Serial.printf("timeout\n");
		return;
	}

	switch (device) {
#ifdef _HAVE_SERFLASH
		case 0 : {
				SerialFlash.begin(FlashChipSelect);
				switch (mode) {
					case 0 : serflash_write();break;
					case 1 : serflash_read();break;
					case 2 : serflash_list();break;
					case 3 : serflash_erasefile();break;
				//case 4 : serflash_erasedevice();break; todo...
				//case 9 : serflash_info();break; todo...
					default: return;
				};
				break;
				}
	#endif

	#ifdef _HAVE_PARFLASH		
		case 2 : {
				ParallelFlash.begin();
				switch (mode) {
					case 0 : parflash_write();break;
					case 1 : parflash_read();break;
					case 2 : parflash_list();break;
					case 3 : parflash_erasefile();break;
				//case 4 : parflash_erasedevice();break; todo...
				//case 9 : parflash_info();break; todo...
					default: return;
				};
				break;
				}
	#endif	
	
	#ifdef _HAVE_EEPROM
		case 1 : {
				eeprom_initialize();
				switch (mode) {
					case 0 : eeprom_write();break;
					case 1 : eeprom_read();break;
					default: return;
				}
				break;
				};
	#endif
	
		default: return;
		}
}

/********************************************************************************
 common
********************************************************************************/
int TeensyTransfer::hid_sendAck(void) {
	return RawHID.send(buffer, 100);
}

int TeensyTransfer::hid_checkAck(void) {
  char buf2[sizeof(buffer)];
  int n;
	n = RawHID.recv(buffer, 100);
	if (n < 0) return n;
	return memcmp(buffer, buf2, sizeof(buffer));
}


int TeensyTransfer::hid_sendWithAck(void) {
  char buf2[sizeof(buffer)];
  int n;
	n = RawHID.send(buffer, 100);
	if (n < 1) return -1;
	n = RawHID.recv(buf2, 100);
	if (n < 1) return -2;
	n =  memcmp(buffer, buf2, sizeof(buffer));
	if (n) return -3;
	return 0;
}

/********************************************************************************
 Serial Flash
********************************************************************************/
#ifdef _HAVE_SERFLASH

void TeensyTransfer::serflash_write(void) {
  int  n, r;
  size_t sz, pos;
  char filename[64];

	sz = (buffer[2] << 24) | (buffer[3] << 16) | (buffer[4 ] << 8) | buffer[5];

	n = RawHID.recv(buffer, 500);
	if (n < 0) {
		//Serial.printf("timeout\n");
		return;
	}	
	
	strcpy( filename, (char*) &buffer[0]);
	//Serial.printf("Filename:%s\n", &filename[0]);
	if (SerialFlash.exists(filename)) {
		//Serial.printf("File exists. Deleting.\n");
		SerialFlash.remove(filename);
	}

	hid_sendAck();

	r = SerialFlash.create(filename, sz);
	if (!r) {
		//Serial.printf("unable to create file.\n");
		return;
	}
	SerialFlashFile ff = SerialFlash.open(filename);
	if (!ff) {
		//Serial.printf("error opening freshly created file!\n");
		return;
	}
	//Serial.print("Flashing.\n");

	pos = 0;
	while (pos < sz) {
		n = RawHID.recv(buffer, 500);
		if (n < 0) {
//     	  Serial.printf("timeout\n");
			return;
		}
		ff.write(buffer, sizeof(buffer));
		pos += sizeof(buffer);
	}
	ff.close();
	hid_sendAck();
	//Serial.println("ok.");
}




void TeensyTransfer::serflash_read(void) {
  int n,r;
  unsigned int sz;
  char filename[64];

	//Serial.print("Read");
	n = RawHID.recv(buffer, 500);
	if (n >= 0) n = hid_sendAck();
	if (n < 0) {
//      Serial.printf("timeout\n");
	  return;
	}

	strcpy( filename, (char*) &buffer[0]);
	//Serial.println(filename);

	if (SerialFlash.exists(filename))
		buffer[0] = 1;
	else
		buffer[0] = 2;

	SerialFlashFile ff = SerialFlash.open(filename);

	sz = ff.size();
	buffer[1] = (sz >> 24) & 0xff;
	buffer[2] = (sz >> 16) & 0xff;
	buffer[3] = (sz >> 8) & 0xff;
	buffer[4] = sz & 0xff;
	//Serial.printf("Size:%d",sz);
	
	n = hid_sendWithAck();
	if (n) {
//      Serial.println("Error");
		return;
	}

	//Send file to PC
	do {
		r = ff.read( buffer, sizeof(buffer) );
		if (r) {
			n = hid_sendWithAck();
			if (n) {
//          	Serial.println("Error");
			return;
			}
		}
	} while (r > 0);
	ff.close();
}

void TeensyTransfer::serflash_list(void) {
  int n;
  uint32_t sz;
//    Serial.println("List");
	SerialFlash.opendir();
	char buf2[64];
	while (1) {
		if (SerialFlash.readdir(buf2, sizeof(buf2), sz)) {

			//Send Filesize
			buffer[0] = 1;
			buffer[1] = (sz >> 24) & 0xff;
			buffer[2] = (sz >> 16) & 0xff;
			buffer[3] = (sz >> 8) & 0xff;
			buffer[4] = sz & 0xff;

			n = hid_sendWithAck();
			if (n) {
	//          Serial.println("Error");
				return;
			}

			//Send Filename
			memcpy(buffer, buf2, sizeof(buffer));
			n = hid_sendWithAck();
			if (n) {
				//Serial.println("Error");
				return;
			}
	  }
	  else break;
	}
	//Send Fee Space
	/*
	  sz = SerialFlash.available();
	  buffer[0] = 2;
	  buffer[1] = (sz >> 24) & 0xff;
	  buffer[2] = (sz >> 16) & 0xff;
	  buffer[3] = (sz >> 8) & 0xff;
	  buffer[4] = sz & 0xff;

	  n = hid_sendWithAck();
	  if (n) {
	  Serial.println("Error");
	  return;
	  }
	*/
	//Send EOF
	buffer[0] = 0;
	n = hid_sendWithAck();
	if (n) {
//      Serial.println("Error");
		return;
	}
	//SerialFlash.closedir();
}

void TeensyTransfer::serflash_erasefile(void) {
  int n;
  char filename[64];
//    Serial.println("erase");
	n = RawHID.recv(buffer, 100);
	if (n < 0) {
//      Serial.printf("timeout\r\n");
		return;
	}

	strcpy( filename, (char*) &buffer[0]);
//	Serial.printf("Filename:%s\r\n", &filename[0]);
	if (SerialFlash.exists(filename)) {
//  	Serial.printf("File exists. Deleting.\r\n");
		SerialFlash.remove(filename);
		buffer[0] = 1;
	} else buffer[0] = 0;

	RawHID.send(buffer, 100);
	return;
}
#endif //#ifdef _HAVE_SERFLASH

/********************************************************************************
 Parallel Flash
********************************************************************************/
#ifdef _HAVE_PARFLASH

void TeensyTransfer::parflash_write(void) {
  int  n, r;
  size_t sz, pos;
  char filename[64];

	sz = (buffer[2] << 24) | (buffer[3] << 16) | (buffer[4 ] << 8) | buffer[5];

	n = RawHID.recv(buffer, 500);
	if (n < 0) {
		//Serial.printf("timeout\n");
		return;
	}	
	
	strcpy( filename, (char*) &buffer[0]);
	//Serial.printf("Filename:%s\n", &filename[0]);
	if (ParallelFlash.exists(filename)) {
		//Serial.printf("File exists. Deleting.\n");
		ParallelFlash.remove(filename);
	}

	hid_sendAck();

	r = ParallelFlash.create(filename, sz);
	if (!r) {
		//Serial.printf("unable to create file.\n");
		return;
	}
	ParallelFlashFile ff = ParallelFlash.open(filename);
	if (!ff) {
		//Serial.printf("error opening freshly created file!\n");
		return;
	}
	//Serial.print("Flashing.\n");

	pos = 0;
	while (pos < sz) {
		n = RawHID.recv(buffer, 500);
		if (n < 0) {
//     	  Serial.printf("timeout\n");
			return;
		}
		ff.write(buffer, sizeof(buffer));
		pos += sizeof(buffer);
	}
	ff.close();
	hid_sendAck();
	//Serial.println("ok.");
}




void TeensyTransfer::parflash_read(void) {
  int n,r;
  unsigned int sz;
  char filename[64];

	//Serial.print("Read");
	n = RawHID.recv(buffer, 500);
	if (n >= 0) n = hid_sendAck();
	if (n < 0) {
//      Serial.printf("timeout\n");
	  return;
	}

	strcpy( filename, (char*) &buffer[0]);
	//Serial.println(filename);

	if (ParallelFlash.exists(filename))
		buffer[0] = 1;
	else
		buffer[0] = 2;

	ParallelFlashFile ff = ParallelFlash.open(filename);

	sz = ff.size();
	buffer[1] = (sz >> 24) & 0xff;
	buffer[2] = (sz >> 16) & 0xff;
	buffer[3] = (sz >> 8) & 0xff;
	buffer[4] = sz & 0xff;
	//Serial.printf("Size:%d",sz);
	
	n = hid_sendWithAck();
	if (n) {
//      Serial.println("Error");
		return;
	}

	//Send file to PC
	do {
		r = ff.read( buffer, sizeof(buffer) );
		if (r) {
			n = hid_sendWithAck();
			if (n) {
//          	Serial.println("Error");
			return;
			}
		}
	} while (r > 0);
	ff.close();
}

void TeensyTransfer::parflash_list(void) {
  int n;
  uint32_t sz;
//    Serial.println("List");
	ParallelFlash.opendir();
	char buf2[64];
	while (1) {
		if (ParallelFlash.readdir(buf2, sizeof(buf2), sz)) {

			//Send Filesize
			buffer[0] = 1;
			buffer[1] = (sz >> 24) & 0xff;
			buffer[2] = (sz >> 16) & 0xff;
			buffer[3] = (sz >> 8) & 0xff;
			buffer[4] = sz & 0xff;

			n = hid_sendWithAck();
			if (n) {
	//          Serial.println("Error");
				return;
			}

			//Send Filename
			memcpy(buffer, buf2, sizeof(buffer));
			n = hid_sendWithAck();
			if (n) {
				//Serial.println("Error");
				return;
			}
	  }
	  else break;
	}
	//Send Fee Space
	/*
	  sz = ParallelFlash.available();
	  buffer[0] = 2;
	  buffer[1] = (sz >> 24) & 0xff;
	  buffer[2] = (sz >> 16) & 0xff;
	  buffer[3] = (sz >> 8) & 0xff;
	  buffer[4] = sz & 0xff;

	  n = hid_sendWithAck();
	  if (n) {
	  Serial.println("Error");
	  return;
	  }
	*/
	//Send EOF
	buffer[0] = 0;
	n = hid_sendWithAck();
	if (n) {
//      Serial.println("Error");
		return;
	}
	//ParallelFlash.closedir();
}

void TeensyTransfer::parflash_erasefile(void) {
  int n;
  char filename[64];
//    Serial.println("erase");
	n = RawHID.recv(buffer, 100);
	if (n < 0) {
//      Serial.printf("timeout\r\n");
		return;
	}

	strcpy( filename, (char*) &buffer[0]);
//	Serial.printf("Filename:%s\r\n", &filename[0]);
	if (ParallelFlash.exists(filename)) {
//  	Serial.printf("File exists. Deleting.\r\n");
		ParallelFlash.remove(filename);
		buffer[0] = 1;
	} else buffer[0] = 0;

	RawHID.send(buffer, 100);
	return;
}
#endif //#ifdef _HAVE_PARFLASH

/********************************************************************************
 EEPROM
********************************************************************************/
#ifdef _HAVE_EEPROM
void TeensyTransfer::eeprom_read(void) {
  int n;
  size_t sz;
  uint8_t *p;

	sz = (E2END) + 1;
	buffer[1] = (sz >> 24) & 0xff;
	buffer[2] = (sz >> 16) & 0xff;
	buffer[3] = (sz >> 8) & 0xff;
	buffer[4] = sz & 0xff;
	//Serial.printf("Size:%d",sz);
	n = hid_sendWithAck();
	if (n) {
//      Serial.println("Error");
	  return;
	}

	p = 0;
	//send EEprom to PC
	do {
		eeprom_read_block(buffer,p, sizeof(buffer));
		n = hid_sendWithAck();
		if (n) {
//      	Serial.println("Error");
			return;
		}
		p += sizeof(buffer);
	} while (p < (uint8_t*)sz);
}

void TeensyTransfer::eeprom_write(void) {
  int n;
  size_t sz;
  uint8_t *p;

	sz = (E2END) + 1;
	buffer[1] = (sz >> 24) & 0xff;
	buffer[2] = (sz >> 16) & 0xff;
	buffer[3] = (sz >> 8) & 0xff;
	buffer[4] = sz & 0xff;
	//Serial.printf("Size:%d",sz);
	n = hid_sendWithAck();
	if (n) {
//      Serial.println("Error");
		return;
	}

	p = 0;
	while (p < (uint8_t*)sz) {
		n = RawHID.recv(buffer, 500);
		if (n < 0) {
//      	Serial.printf("timeout\n");
			return;
		}
		eeprom_write_block(buffer,p, sizeof(buffer));
		p += sizeof(buffer);
	}

	hid_sendAck();
	//Serial.println("ok.");
}
#endif //#ifdef _HAVE_EEPROM

#endif