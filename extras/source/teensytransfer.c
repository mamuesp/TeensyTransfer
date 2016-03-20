#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <time.h>

#if defined(OS_LINUX) || defined(OS_MACOSX)
#include <sys/ioctl.h>
#include <termios.h>
#elif defined(OS_WINDOWS)
#include <conio.h>
#include <fcntl.h>
#include <windows.h>
unsigned int _CRT_fmode = _O_BINARY;
#endif

#include "hid.h"


#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))


	
const int hid_device = 0;

unsigned char buf[64];

// options (from user via command line args)
int device = 0; 
int mode = 0;
int verbose = 0;
const char *fname=NULL;

/***********************************************************************************************************/
void die(const char *format, ...)
{
	va_list args;
	va_start(args, format);
	fprintf(stderr, "teensy_memloader: ");
	vfprintf(stderr, format, args);
	fprintf(stderr, "\n");
	exit(1);
}


void usage(const char *err)
{	
	if(err != NULL) fprintf(stderr, "%s\n\n", err);
	fprintf(stderr,
		"Usage: teensy_memloader [-w] [-r] [-l] [-e] [-v] [device] <file>\n"
		"\t-w : write (default)\n"
		"\t-r : read\n"
		"\t-e : erase file\n"
		//"\t-E : erase device\n"
		"\t-l : list files\n"		
		//"\t-v : Verbose output\n"
		"\n"
		"\t Devices:\n"
		"\tserflash (default)\n"
		//"\teeprom\n"	
		"");
	exit(1);
}

void parse_flag(char *arg)
{
	int i;
	for(i=1; arg[i]; i++) {
		switch(arg[i]) {
			case 'w': mode = 0; break;
			case 'r': mode = 1; break;
			case 'l': mode = 2; break;
			case 'e': mode = 3; break;
			case 'E': mode = 4; break;
			case 'v': verbose = 1; break;
			default:
				fprintf(stderr, "Unknown flag '%c'\n\n", arg[i]);
				usage(NULL);
		}
	}
}

void parse_options(int argc, char **argv)
{
	int i;
	char *arg;

	for (i=1; i<argc; i++) {
		arg = argv[i];

		if(arg[0] == '-') {
			if(arg[1] == '-') {
				char *name = &arg[2];
				char *val  = strchr(name, '=');
				if(val == NULL) {
					//value must be the next string.
					val = argv[++i];
				}
				else {
					//we found an =, so split the string at it.
					*val = '\0';
					 val = &val[1];
				}

			}
			else parse_flag(arg);
		}
		else if (strcmp("serflash", arg) == 0) device = 0;
		else if (strcmp("eeprom", arg) == 0) device = 1;
		else fname = arg;
	}
	
	//printf("mode:%d device:%d verbose:%d filename:%s\n", mode, device, verbose, fname);
}
/************************************************************************************************************/

void commErr() {
	die("Communication error");
}

int hid_sendAck(void) {
  return rawhid_send(hid_device, buf, sizeof(buf), 100);
}

int hid_checkAck() {
char buf2[sizeof(buf)];
int n;
	n = rawhid_recv(hid_device, buf2, sizeof(buf2), 100);
	if (n < 1) return n;
	n = memcmp(buf, buf2, sizeof(buf));
	return n;
}

void hid_sendWithAck() {
char buf2[sizeof(buf)];
int n;	
	n = rawhid_send(hid_device, buf, sizeof(buf), 100);	
	if (n < 1) commErr();
	n = rawhid_recv(hid_device, buf2, sizeof(buf2), 100);
	if (n < 1) commErr();
	n = memcmp(buf, buf2, sizeof(buf));	
	if (n) commErr();
	return;
}

void hid_rcvWithAck() {
int n;	
	n = rawhid_recv(hid_device, buf, sizeof(buf), 100);
	if (n < 1) commErr();
	n = rawhid_send(hid_device, buf, sizeof(buf), 100);
	if (n < 1) commErr();
}

/***********************************************************************************************************
  extflash
************************************************************************************************************/


void extflash_write(void);
void extflash_read(void);
void extflash_list(void);
void extflash_delfile(void);
void extflash_erase(void);

void extflash() {
  switch (mode) {
	case 0 : extflash_write(); break;
	case 1 : extflash_read();break;
	case 2 : extflash_list();break;
	case 3 : extflash_delfile();break;
	case 4 : extflash_erase();break;
  }	
}

void extflash_write(void) {
  FILE *fp;
  int sz,r, pos;  
  char * buffer;	
	
	fp = fopen(fname, "rb");
	if (fp == NULL) {
		fprintf(stderr, "Unable to read %s\n\n", fname);
		usage(NULL);
	}
  
	//get size of file
	fseek(fp, 0, SEEK_END);
	sz = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	//if (verbose) printf("Size of file is %d bytes.\n",sz);
	
	if (sz == 0) die("");
	
	memset(buf, 0, sizeof(buf));
	buf[0] = mode;
	buf[1] = device;
	buf[2] = (sz >> 24) & 0xff;
	buf[3] = (sz >> 16) & 0xff;
	buf[4] = (sz >> 8) & 0xff;
	buf[5] = sz & 0xff;
	hid_sendWithAck();
	
	strncpy((char*)buf, fname, sizeof(buf));
	hid_sendWithAck();
	
	//Todo: check for free space on flash here
	
	// allocate memory to contain the whole file:
	buffer = (char*) malloc (sizeof(char) * sz);
	if (buffer == NULL) die ("Memory error");

	// copy the file into the buffer:
	r = fread (buffer, 1, sz, fp);
	if (r != sz) die("File reading error");
	
	fclose (fp);
	
	//Transfer the file to the Teensy
	pos = 0;
	while (pos < sz) {
		int c = MIN(sizeof(buf), sz-pos);
		memset(buf, 0xff, sizeof(buf));
		memcpy(buf, buffer + pos, c);
		pos += c;
		r = rawhid_send(hid_device, buf, sizeof(buf), 200);
		//int i;
		//for (i=0; i<sizeof(buf); i++) printf("%c",buf[i]);
		//printf("%d %d\n",pos, c);
		if (r < 0 ) die("Transfer error");
	}
	free (buffer);
	
	if (hid_checkAck() != 0)  commErr() ;	
	//printf("%d\n",pos);	
	
};	
	
void extflash_read(void) {  
  unsigned sz, pos;
  int r;
		
	//printf("Read\r\n");
	memset(buf, 0, sizeof(buf));
	buf[0] = mode;
	buf[1] = device;
	hid_sendWithAck();
	
	strncpy((char*)buf, fname, sizeof(buf));
	hid_sendWithAck();
	
	//r = rawhid_recv(hid_device, buf, sizeof(buf), 100);
	hid_rcvWithAck();
	//if (r < 1)  commErr() ;	
	if (buf[0]==2) die("File not found");
	else if (buf[0]!=1)  commErr() ;
	//hid_sendAck();
	
	sz = (buf[1] << 24) | (buf[2] << 16) | (buf[3] << 8) | buf[4];
	//printf("size:%d %d %d %d %d\r\n",sz, buf[1], buf[2], buf[3], buf[4]);
	
		
	pos = 0;
	int i;
	do {
		hid_rcvWithAck();
		r = sz - pos;
		if (r > sizeof(buf)) r = sizeof(buf);
		pos+= r;				
		if (r) for (i=0; i < r; i++) putc(buf[i], stdout);
	} while (r==sizeof(buf));
	
	fflush(stdout);

};

void extflash_list(void) {
uint32_t sz,i;	
	//printf("List\r\n");
	memset(buf, 0, sizeof(buf));
	buf[0] = mode;
	buf[1] = device;
	hid_sendWithAck();
	
	while (1) {
		hid_rcvWithAck();
		if (buf[0]==0) break;
		sz = (buf[1] << 24) | (buf[2] << 16) | (buf[3] << 8) | buf[4];
		printf("%d\t",sz);
		hid_rcvWithAck();
		i = 0;
		while( i<= sizeof(buf) && buf[i]) {putc(buf[i], stdout); i++;}
		printf("\n");
	}
	

};

void extflash_delfile(void) {
uint32_t n;	
	//printf("del file \n");
	memset(buf, 0, sizeof(buf));
	buf[0] = mode;
	buf[1] = device;
	hid_sendWithAck();	
	
	strncpy((char*)buf, fname, sizeof(buf));
	rawhid_send(hid_device, buf, sizeof(buf), 100);
	
	
	n= rawhid_recv(hid_device, buf, sizeof(buf), 100);
	if (n < 1) commErr();
	if (buf[0]==0) die("File not found");

}	

void extflash_erase(void) {
	printf("erase chip\n");
	memset(buf, 0, sizeof(buf));
	buf[0] = mode;
	buf[1] = device;
	hid_sendWithAck();	
	
	rawhid_recv(hid_device, buf, sizeof(buf), 0);
	
};

/***********************************************************************************************************/
int main(int argc, char **argv)
{
	
	parse_options(argc, argv);
	
	if ((mode == 0 || mode == 1 || mode == 3) && (fname==NULL)) usage("Filename required");	
	if (mode == 4 && fname != NULL) usage("Filename not allowed");		
	

	int r;
	// C-based example is 16C0:0480:FFAB:0200
	r = rawhid_open(1, 0x16C0, 0x0480, 0xFFAB, 0x0200);
	if (r <= 0) {
		// Arduino-based example is 16C0:0486:FFAB:0200
		r = rawhid_open(1, 0x16C0, 0x0486, 0xFFAB, 0x0200);
		if (r <= 0) {
			die("no rawhid device found\n");
		}
	}
	
	//clock_t t = clock();
	
	switch(device) {
		case 0: extflash();break;
		default: usage(NULL);
	}	
	/*
	{
		t = clock() - t;
		double time_taken = ((double)t)/CLOCKS_PER_SEC; // in seconds
		printf("it took %f seconds to execute \n", time_taken);
	}
	*/
	
	exit(0);
}


