#ifndef __DSP__
#define __DSP__

#include "driver/i2c_master.h"
#include "math.h"
#include "stdio.h"

#define DSP_ADR (0x70 >> 1)


#define DSP_INPUT_MUX_ADR 0x00, 0x46
#define DSP_EQ_MUX_ADR 0x00, 0x88
#define DSP_MUTE_L_ADR 0x00, 0x44
#define DSP_MUTE_R_ADR 0x00, 0x45

enum { USB = 0, TOSLINK = 1 };

enum { EQ_ON = 0, EQ_OFF = 1 };

typedef enum {
	hz_25,
	hz_40,
	hz_63,
	hz_100,
	hz_160,
	hz_250,
	hz_400,
	hz_630,
	hz_1000,
	hz_1500,
	hz_2500,
	hz_4000,
	hz_6500,
	hz_10000,
	hz_16000,
} TypedefEQFreq;

void set_input_mux(uint16_t mux);
void set_eq_mux(uint16_t mux);
void set_mute_left(uint16_t mute);
void set_mute_right(uint16_t mute);
void set_eq(TypedefEQFreq freq, int8_t var);
void set_volume(uint8_t vol);
void dsp_init(void);

#endif // __DSP__
