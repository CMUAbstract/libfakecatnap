#include "comp.h"
#include <msp430.h>

__nv volatile energy_t baseline_E = 0;
__nv volatile unsigned lower_thres = DEFAULT_LOWER_THRES;
__nv volatile unsigned upper_thres = DEFAULT_UPPER_THRES;
__nv volatile unsigned max_thres = DEFAULT_NEARY_MAX_THRES;

// Volt * 100
__ro_nv const unsigned level_to_volt[NUM_LEVEL] = {
	100, 101, 105, 106, 108,
	109, 112, 116, 117, 118,
	125, 131, 132, 137, 140,
	143, 148, 150, 156, 162,
	164, 168, 171, 175, 179,
	181, 187, 193, 195, 203,
	210, 218, 226, 234, 242
};

// Pre-calculated (V^2-1) * 10000
// Note that E for V_1_08 is 1664 (E_QUANTA)
__ro_nv const energy_t level_to_E[NUM_LEVEL] = {
	0, 201, 1025, 1236, 1664,
	1881, 2544, 3456, 3689, 3924,
	5625, 7161, 7424, 8769, 9600,
	10449, 11904, 12500, 14336, 16244,
	16896, 18224, 19241, 20625, 22041,
	22761, 24969, 27249, 28025, 31209,
	34100, 37524, 41076, 44756, 48564
};


__ro_nv const unsigned level_to_reg[NUM_LEVEL] = {
	CEREFL_2 | CEREF0_15 | CEREF1_16, // 0
	CEREFL_1 | CEREF0_26 | CEREF1_27, // 1
	CEREFL_1 | CEREF0_27 | CEREF1_28, // 2
	CEREFL_2 | CEREF0_16 | CEREF1_17, // 3
	CEREFL_1 | CEREF0_28 | CEREF1_29, // 4
	CEREFL_3 | CEREF0_13 | CEREF1_14, // 5
	CEREFL_1 | CEREF0_29 | CEREF1_30, // 6
	CEREFL_1 | CEREF0_30 | CEREF1_31, // 7
	CEREFL_3 | CEREF0_14 | CEREF1_15, // 8
	CEREFL_2 | CEREF0_18 | CEREF1_19, // 9
	CEREFL_2 | CEREF0_19 | CEREF1_20, // 10
	CEREFL_2 | CEREF0_20 | CEREF1_21, // 11
	CEREFL_3 | CEREF0_16 | CEREF1_17, // 12
	CEREFL_2 | CEREF0_21 | CEREF1_22, // 13
	CEREFL_3 | CEREF0_17 | CEREF1_18, // 14
	CEREFL_2 | CEREF0_22 | CEREF1_23, // 15
	CEREFL_3 | CEREF0_18 | CEREF1_19, // 16
	CEREFL_2 | CEREF0_23 | CEREF1_24, // 17
	CEREFL_2 | CEREF0_24 | CEREF1_25, // 18
	CEREFL_2 | CEREF0_25 | CEREF1_26, // 19
	CEREFL_3 | CEREF0_20 | CEREF1_21, // 20
	CEREFL_2 | CEREF0_26 | CEREF1_27, // 21
	CEREFL_3 | CEREF0_21 | CEREF1_22, // 22
	CEREFL_2 | CEREF0_27 | CEREF1_28, // 23
	CEREFL_3 | CEREF0_22 | CEREF1_23, // 24
	CEREFL_2 | CEREF0_28 | CEREF1_29, // 25
	CEREFL_2 | CEREF0_29 | CEREF1_30, // 26
	CEREFL_2 | CEREF0_30 | CEREF1_31, // 27
	CEREFL_3 | CEREF0_24 | CEREF1_25, // 28
	CEREFL_3 | CEREF0_25 | CEREF1_26, // 29
	CEREFL_3 | CEREF0_26 | CEREF1_27, // 30
	CEREFL_3 | CEREF0_27 | CEREF1_28, // 31
	CEREFL_3 | CEREF0_28 | CEREF1_29, // 32
	CEREFL_3 | CEREF0_29 | CEREF1_30, // 33
	CEREFL_3 | CEREF0_30 | CEREF1_31 // 34
};

#if 0 // These may be important later
// Return the level (voltage) so that
// (V-1)^2 * 10000 > e
energy_t get_ceiled_level(energy_t e) {
	// linear search. Because I am lazy
	for (unsigned i = 0; i < NUM_LEVEL; ++i) {
		// TODO: This should be pre-calculated
		energy_t e2 = level_to_E[i];
		if (e2 > e) {
			return i;
		}
	}
	return INVALID_LEVEL;
}

// Given a lower level (voltage), return a
// upper level so that
// (V_u^2-1) * 100000 ~ (V_l^2-1) * 100000 + E_OPERATING
energy_t get_upper_level(unsigned lower_level) {
	energy_t upper_e = level_to_E[lower_level] + E_OPERATING;

	return get_ceiled_level(upper_e);
}
#endif
