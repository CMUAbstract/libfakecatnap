#include "comp.h"

#include <msp430.h>

__nv volatile energy_t baseline_E = 0;
__nv volatile unsigned lower_thres = DEFAULT_LOWER_THRES;
__nv volatile unsigned upper_thres = DEFAULT_UPPER_THRES;
__nv volatile unsigned max_thres = DEFAULT_TASK_UPPER_HYST;

// Volt * 100
__ro_nv const unsigned level_to_volt[NUM_LEVEL] = {
140, 142, 144, 146, 148, 150, 152, 154, 156, 158, 160, 162, 164,
166, 168, 170, 172, 174, 176, 178, 180, 182, 184, 186, 188, 190,
192, 194, 196, 198, 200, 202, 204, 206, 208, 210, 212, 214, 216,
218, 220, 222, 224, 226, 228, 230, 232, 234, 236, 238, 240, 242,
244, 246, 248,
};


__ro_nv const energy_t level_to_E_catnap[NUM_LEVEL] = {
19599 , 20164 , 20736 , 21315 , 21904 , 22500 , 23104 , 23716 , 24336 , 24964 ,
25600 , 26244 , 26896 , 27556 , 28224 , 28900 , 29584 , 30276 , 30976 , 31684 ,
32400 , 33124 , 33856 , 34596 , 35344 , 36100 , 36864 , 37636 , 38416 , 39204 ,
40000 , 40804 , 41616 , 42436 , 43264 , 44100 , 44944 , 45796 , 46656 , 47524 ,
48400 , 49284 , 50176 , 51076 , 51984 , 52900 , 53824 , 54756 , 55696 , 56644 ,
57600 , 58564 , 59536 , 60516 , 61504 ,
};


__ro_nv const unsigned level_to_reg[NUM_LEVEL] = {
/* 1.40 */ CEREFL_2 | CEREF0_11 | CEREF1_12 ,
/* 1.42 */ CEREFL_3 | CEREF0_9 | CEREF1_10 ,
/* 1.44 */ CEREFL_1 | CEREF0_19 | CEREF1_20 ,
/* 1.46 */ CEREFL_1 | CEREF0_19 | CEREF1_20 ,
/* 1.48 */ CEREFL_1 | CEREF0_19 | CEREF1_20 ,
/* 1.50 */ CEREFL_1 | CEREF0_20 | CEREF1_21 ,
/* 1.52 */ CEREFL_1 | CEREF0_20 | CEREF1_21 ,
/* 1.54 */ CEREFL_1 | CEREF0_20 | CEREF1_21 ,
/* 1.56 */ CEREFL_1 | CEREF0_20 | CEREF1_21 ,
/* 1.58 */ CEREFL_1 | CEREF0_21 | CEREF1_22 ,
/* 1.60 */ CEREFL_1 | CEREF0_21 | CEREF1_22 ,
/* 1.62 */ CEREFL_1 | CEREF0_21 | CEREF1_22 ,
/* 1.64 */ CEREFL_2 | CEREF0_13 | CEREF1_14 ,
/* 1.66 */ CEREFL_1 | CEREF0_22 | CEREF1_23 ,
/* 1.68 */ CEREFL_1 | CEREF0_22 | CEREF1_23 ,
/* 1.70 */ CEREFL_1 | CEREF0_22 | CEREF1_23 ,
/* 1.72 */ CEREFL_3 | CEREF0_11 | CEREF1_12 ,
/* 1.74 */ CEREFL_1 | CEREF0_23 | CEREF1_24 ,
/* 1.76 */ CEREFL_2 | CEREF0_14 | CEREF1_15 ,
/* 1.78 */ CEREFL_2 | CEREF0_14 | CEREF1_15 ,
/* 1.80 */ CEREFL_1 | CEREF0_24 | CEREF1_25 ,
/* 1.82 */ CEREFL_1 | CEREF0_24 | CEREF1_25 ,
/* 1.84 */ CEREFL_1 | CEREF0_24 | CEREF1_25 ,
/* 1.86 */ CEREFL_1 | CEREF0_24 | CEREF1_25 ,
/* 1.88 */ CEREFL_1 | CEREF0_25 | CEREF1_26 ,
/* 1.90 */ CEREFL_1 | CEREF0_25 | CEREF1_26 ,
/* 1.92 */ CEREFL_1 | CEREF0_25 | CEREF1_26 ,
/* 1.94 */ CEREFL_1 | CEREF0_25 | CEREF1_26 ,
/* 1.96 */ CEREFL_1 | CEREF0_26 | CEREF1_27 ,
/* 1.98 */ CEREFL_1 | CEREF0_26 | CEREF1_27 ,
/* 2.00 */ CEREFL_2 | CEREF0_16 | CEREF1_17 ,
/* 2.02 */ CEREFL_2 | CEREF0_16 | CEREF1_17 ,
/* 2.04 */ CEREFL_3 | CEREF0_13 | CEREF1_14 ,
/* 2.06 */ CEREFL_3 | CEREF0_13 | CEREF1_14 ,
/* 2.08 */ CEREFL_3 | CEREF0_13 | CEREF1_14 ,
/* 2.10 */ CEREFL_1 | CEREF0_28 | CEREF1_29 ,
/* 2.12 */ CEREFL_1 | CEREF0_28 | CEREF1_29 ,
/* 2.14 */ CEREFL_2 | CEREF0_17 | CEREF1_18 ,
/* 2.16 */ CEREFL_2 | CEREF0_17 | CEREF1_18 ,
/* 2.18 */ CEREFL_1 | CEREF0_29 | CEREF1_30 ,
/* 2.20 */ CEREFL_3 | CEREF0_14 | CEREF1_15 ,
/* 2.22 */ CEREFL_3 | CEREF0_14 | CEREF1_15 ,
/* 2.24 */ CEREFL_3 | CEREF0_14 | CEREF1_15 ,
/* 2.26 */ CEREFL_1 | CEREF0_30 | CEREF1_31 ,
/* 2.28 */ CEREFL_1 | CEREF0_30 | CEREF1_31 ,
/* 2.30 */ CEREFL_1 | CEREF0_30 | CEREF1_31 ,
/* 2.32 */ CEREFL_1 | CEREF0_30 | CEREF1_31 ,
/* 2.34 */ CEREFL_2 | CEREF0_18 | CEREF1_19 ,
/* 2.36 */ CEREFL_3 | CEREF0_15 | CEREF1_16 ,
/* 2.38 */ CEREFL_2 | CEREF0_19 | CEREF1_20 ,
/* 2.40 */ CEREFL_2 | CEREF0_19 | CEREF1_20 ,
/* 2.42 */ CEREFL_2 | CEREF0_19 | CEREF1_20 ,
/* 2.44 */ CEREFL_2 | CEREF0_19 | CEREF1_20 ,
/* 2.46 */ CEREFL_2 | CEREF0_19 | CEREF1_20 ,
/* 2.48 */ CEREFL_2 | CEREF0_19 | CEREF1_20 ,


#if 0
/* 1.40 */ CEREFL_1 | CEREF0_11 | CEREF1_12 ,
/* 1.42 */ CEREFL_1 | CEREF0_11 | CEREF1_12 ,
/* 1.44 */ CEREFL_1 | CEREF0_11 | CEREF1_12 ,
/* 1.46 */ CEREFL_1 | CEREF0_11 | CEREF1_12 ,
/* 1.48 */ CEREFL_2 | CEREF0_7 | CEREF1_8 ,
/* 1.50 */ CEREFL_2 | CEREF0_7 | CEREF1_8 ,
/* 1.52 */ CEREFL_1 | CEREF0_12 | CEREF1_13 ,
/* 1.54 */ CEREFL_1 | CEREF0_12 | CEREF1_13 ,
/* 1.56 */ CEREFL_1 | CEREF0_12 | CEREF1_13 ,
/* 1.58 */ CEREFL_3 | CEREF0_6 | CEREF1_7 ,
/* 1.60 */ CEREFL_3 | CEREF0_6 | CEREF1_7 ,
/* 1.62 */ CEREFL_3 | CEREF0_6 | CEREF1_7 ,
/* 1.64 */ CEREFL_3 | CEREF0_6 | CEREF1_7 ,
/* 1.66 */ CEREFL_1 | CEREF0_13 | CEREF1_14 ,
/* 1.68 */ CEREFL_1 | CEREF0_13 | CEREF1_14 ,
/* 1.70 */ CEREFL_2 | CEREF0_8 | CEREF1_9 ,
/* 1.72 */ CEREFL_2 | CEREF0_8 | CEREF1_9 ,
/* 1.74 */ CEREFL_2 | CEREF0_8 | CEREF1_9 ,
/* 1.76 */ CEREFL_2 | CEREF0_8 | CEREF1_9 ,
/* 1.78 */ CEREFL_1 | CEREF0_14 | CEREF1_15 ,
/* 1.80 */ CEREFL_1 | CEREF0_14 | CEREF1_15 ,
/* 1.82 */ CEREFL_1 | CEREF0_14 | CEREF1_15 ,
/* 1.84 */ CEREFL_1 | CEREF0_14 | CEREF1_15 ,
/* 1.86 */ CEREFL_3 | CEREF0_7 | CEREF1_8 ,
/* 1.88 */ CEREFL_3 | CEREF0_7 | CEREF1_8 ,
/* 1.90 */ CEREFL_1 | CEREF0_15 | CEREF1_16 ,
/* 1.92 */ CEREFL_1 | CEREF0_15 | CEREF1_16 ,
/* 1.94 */ CEREFL_1 | CEREF0_15 | CEREF1_16 ,
/* 1.96 */ CEREFL_1 | CEREF0_15 | CEREF1_16 ,
/* 1.98 */ CEREFL_1 | CEREF0_15 | CEREF1_16 ,
/* 2.00 */ CEREFL_1 | CEREF0_15 | CEREF1_16 ,
/* 2.02 */ CEREFL_1 | CEREF0_15 | CEREF1_16 ,
/* 2.04 */ CEREFL_1 | CEREF0_16 | CEREF1_17 ,
/* 2.06 */ CEREFL_1 | CEREF0_16 | CEREF1_17 ,
/* 2.08 */ CEREFL_1 | CEREF0_16 | CEREF1_17 ,
/* 2.10 */ CEREFL_1 | CEREF0_16 | CEREF1_17 ,
/* 2.12 */ CEREFL_2 | CEREF0_10 | CEREF1_11 ,
/* 2.14 */ CEREFL_2 | CEREF0_10 | CEREF1_11 ,
/* 2.16 */ CEREFL_1 | CEREF0_17 | CEREF1_18 ,
/* 2.18 */ CEREFL_1 | CEREF0_17 | CEREF1_18 ,
/* 2.20 */ CEREFL_1 | CEREF0_17 | CEREF1_18 ,
/* 2.22 */ CEREFL_1 | CEREF0_17 | CEREF1_18 ,
/* 2.24 */ CEREFL_1 | CEREF0_17 | CEREF1_18 ,
/* 2.26 */ CEREFL_1 | CEREF0_17 | CEREF1_18 ,
/* 2.28 */ CEREFL_1 | CEREF0_18 | CEREF1_19 ,
/* 2.30 */ CEREFL_1 | CEREF0_18 | CEREF1_19 ,
/* 2.32 */ CEREFL_2 | CEREF0_11 | CEREF1_12 ,
/* 2.34 */ CEREFL_2 | CEREF0_11 | CEREF1_12 ,
/* 2.36 */ CEREFL_2 | CEREF0_11 | CEREF1_12 ,
/* 2.38 */ CEREFL_3 | CEREF0_9 | CEREF1_10 ,
/* 2.40 */ CEREFL_3 | CEREF0_9 | CEREF1_10 ,
/* 2.42 */ CEREFL_1 | CEREF0_19 | CEREF1_20 ,
/* 2.44 */ CEREFL_1 | CEREF0_19 | CEREF1_20 ,
/* 2.46 */ CEREFL_1 | CEREF0_19 | CEREF1_20 ,
/* 2.48 */ CEREFL_1 | CEREF0_19 | CEREF1_20
#endif
};




#if 0 // These may be important later
// Given a lower level (voltage), return a
// upper level so that
// (V_u^2-1) * 100000 ~ (V_l^2-1) * 100000 + E_OPERATING
energy_t get_upper_level(unsigned lower_level) {
	energy_t upper_e = level_to_E[lower_level] + E_OPERATING;

	return get_ceiled_level(upper_e);
}
#endif
