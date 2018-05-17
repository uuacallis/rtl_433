#include "rtl_433.h"
#include "pulse_demod.h"
#include "data.h"
#include "util.h"

// Protocol of the SimpliSafe Sensors
//
// The data is sent leveraging a PiWM Encoding where a long is 1, and a short is 0
//
// All bytes are sent with least significant bit FIRST (1000 0111 = 0xE1)
//
//  2 Bytes   | 1 Byte       | 5 Bytes   | 1 Byte  | 1 Byte  | 1 Byte	    | 1 Byte
//  Sync Word | Message Type | Device ID | CS Seed | Command | SUM CMD + CS | Epilogue
//
// Copyright (C) 2018, Adam Callis, adam.callis@gmail.com 
// License: GPL v2+ (or at your choice, any other OSI-approved Open Source license)

static char time_str[LOCAL_TIME_BUFLEN];

static int
ss_sensor_parser (bitbuffer_t *bitbuffer)
{
	//bitbuffer_invert(bitbuffer); // Invert the Bits
	bitrow_t *bb = bitbuffer->bb;
	uint16_t bitcount = bitbuffer->bits_per_row[0];

	data_t *data;
	char *extradata = "";

	local_time_str(0, time_str);

	// each row needs to have exactly 92 bits 
	if (bitcount != 92) 
		return 0;

	// Change to least-significant-bit last (protocol uses least-siginificant-bit first) for hex representation:
	char id[5] = ""; 
	for (uint16_t k = 3; k <= 7; k++) {
		bitbuffer->bb[0][k] = reverse8(bitbuffer->bb[0][k]);
		char str[2];
		sprintf(str, "%c", (char)bb[0][k]);
		strcat(id, str);
	}

	data = data_make(
		"time",		"",	DATA_STRING, time_str,
		"model",	"",	DATA_STRING, "SimpliSafe Sensor",
		"device",	"Device ID",	DATA_STRING, id,
		"seq",		"Sequence",	DATA_INT, bb[0][8],
		"state",	"State",	DATA_INT, bb[0][9],
		"extradata",	"Extra Data",	DATA_STRING, extradata,
		NULL
	);
        data_acquired_handler(data);

	return 1;
}

static int 
ss_pinentry_parser(bitbuffer_t *bitbuffer)
{
	// In a keypad message the pin is encoded in bytes 10 and 11 with the the digits each using 4 bits
	// However the bits are low order to high order 

	bitrow_t *bb = bitbuffer->bb;
	uint16_t bitcount = bitbuffer->bits_per_row[0];

	int digits[5];
	int pina = reverse8(bb[0][10]);
	int pinb = reverse8(bb[0][11]);

	digits[0] = (pina & 0xf);
	digits[1] = ((pina & 0xf0)>>4);
	digits[2] = (pinb & 0xf);
	digits[3] = ((pinb & 0xf0)>>4);

	data_t *data;
	local_time_str(0, time_str);

	// Change to least-significant-bit last (protocol uses least-siginificant-bit first) for hex representation:
	char id[5] = ""; 
	for (uint16_t k = 3; k <= 7; k++) {
		bitbuffer->bb[0][k] = reverse8(bitbuffer->bb[0][k]);
		char str[2];
		sprintf(str, "%c", (char)bb[0][k]);
		strcat(id, str);
	}

	char str[2];
	char extradata[20] = "Disarm Pin: ";
	sprintf(str, "%x", digits[0]);
	strcat(extradata, str);
	for (uint16_t k = 1; k < 4; k++) {
		char str[2];
		sprintf(str, "%x", digits[k]);
		strcat(extradata, str);
	}

	data = data_make(
		"time",		"",	DATA_STRING, time_str,
		"model",	"",	DATA_STRING, "SimpliSafe Keypad",
		"device",	"Device ID",	DATA_STRING, id,
		"seq",		"Sequence",	DATA_INT, bb[0][9],
		"extradata",	"Extra Data",	DATA_STRING, extradata,
		NULL
	);
		//"state",	"State",	DATA_INT, bb[0][9],
        data_acquired_handler(data);

	return 1;
}

static int 
ss_keypad_arming (bitbuffer_t *bitbuffer)
{
	bitrow_t *bb = bitbuffer->bb;
	uint16_t bitcount = bitbuffer->bits_per_row[0];

	data_t *data;
	local_time_str(0, time_str);

//	/* DEBUG: print out the received packet */
//	fprintf (stderr, "SS bitcount=%d; data= ", bitcount);
//	for (int i = 0 ; i <= bitcount/8 ; i++) {
//		fprintf (stderr, "%02x", bb[0][i]);
//	}
//	fprintf(stderr, "\n");

	// Change to least-significant-bit last (protocol uses least-siginificant-bit first) for hex representation:

	char extradata[20] = "Arming: ";
	if (bb[0][10] == 0x6a) { 
		strcat(extradata, "Away");
	} else if (bb[0][10] == 0xca) { 
		strcat(extradata, "Home");
	} else if (bb[0][10] == 0x3a) { 
		strcat(extradata, "Cancelled");
	} else { 
		char tmp[20] = "";
		sprintf(tmp, "Unknown Keypad: %02x", bb[0][10]);
		strcpy(extradata,tmp);
	}

	char id[5] = ""; 
	for (uint16_t k = 3; k <= 7; k++) {
		bitbuffer->bb[0][k] = reverse8(bitbuffer->bb[0][k]);
		char str[2];
		sprintf(str, "%c", (char)bb[0][k]);
		strcat(id, str);
	}

	data = data_make(
		"time",		"",	DATA_STRING, time_str,
		"model",	"",	DATA_STRING, "SimpliSafe Keypad",
		"device",	"",	DATA_STRING, id,
		"seq",	"Sequence",DATA_INT, bb[0][9],
		"extradata",	"",	DATA_STRING, extradata,
		NULL
	);
        data_acquired_handler(data);

	return 1;
}

static int
ss_sensor_callback (bitbuffer_t *bitbuffer)
{
	bitbuffer_invert(bitbuffer); // Invert the Bits
	bitrow_t *bb = bitbuffer->bb;
	// fprintf (stderr, "%02x %02x %02x", bb[0][0], bb[0][1], bb[0][2]);

	if (bb[0][0] != 0x33 && bb[0][1] != 0xa0) { // All Messages Must start with 0x33a0
		return 0;
	} else {
		if (bb[0][2] == 0x88) { 
			return ss_sensor_parser (bitbuffer);
		} else if (bb[0][2] == 0x66) {
			return ss_pinentry_parser (bitbuffer);
		} else if (bb[0][2] == 0x44) { 
			return ss_keypad_arming (bitbuffer);
		} else {
			fprintf(stderr, "Unknown Message Type: %02x\n", bb[0][2]);

	/* DEBUG: print out the received packet */
//	fprintf (stderr, "SS bitcount=%d; data= ", bitcount);
//	for (int i = 0 ; i <= bitcount/8 ; i++) {
//		fprintf (stderr, "%02x", bb[0][i]);
//	}
//	fprintf(stderr, "\n");
			return 0;
		}
	} 
}


static char *sensor_output_fields[] = {
    "time",
    "model",
    "device",
    "seq",
    "state",
    "extradata",
    NULL
};

r_device ss_sensor = {
    .name           = "SimpliSafe Sensors",
    .modulation     = OOK_PULSE_PIWM_DC,
    .short_limit    = 500,  // half-bit width 500 us
    .long_limit     = 1000, // bit width 1000 us
    .reset_limit    = 1500,
    .tolerance      = 100, // us
    .json_callback  = &ss_sensor_callback,
    .disabled       = 1,
    .fields         = sensor_output_fields
};
