#ifndef __LCFN_COMP__
#define __LCFN_COMP__
#include <libmsp/mem.h>

// This is the quanta for energy
// every energy in the system is a multiple of this
// This represents an energy available in V = 1.08V
// This comes from 1.08^2 - 1 = 0.1664
// Any energy stored in the voltage V can be
// expressed as a quanta as
// ((V^2 - 1) * 100 / 17) E_QUANTA
// Any energy can be expressed as
// ((E*2/C) * 100 / 17) E_QUANTA
#define E_QUANTA 1664
// This represents energy in 1.31V - 1.08V
#define E_OPERATING 3*E_QUANTA 

// These assume a 4.22M : 10M divider
#define DEFAULT_LOWER_THRES V_1_12
#define DEFAULT_UPPER_THRES V_1_75
#define DEFAULT_NEARY_MAX_THRES V_1_75
// 0.5865

#define THRES_MAX V_2_42
#define NUM_LEVEL 35

typedef uint32_t energy_t;

extern volatile energy_t baseline_E;
extern volatile unsigned lower_thres;
extern volatile unsigned upper_thres;
extern volatile unsigned max_thres;
extern const energy_t level_to_E[NUM_LEVEL];

//	lower_thres = level;
#define SET_LOWER_COMP(val)\
	CECTL2 |= CEREFL_0;\
	CECTL2 = CERS_2 | level_to_reg[val];\
	CECTL2 &= ~CERSEL;\
	while (!CERDYIFG);\
	CEINT &= ~(CEIFG | CEIIFG);

//	upper_thres = level;
#define SET_UPPER_COMP(val)\
	CECTL2 |= CEREFL_0;\
	CECTL2 = CERS_2 | level_to_reg[val];\
	CECTL2 |= CERSEL;\
	while (!CERDYIFG);\
	CEINT &= ~(CEIFG | CEIIFG);

#define SET_MAX_UPPER_COMP()\
	CECTL2 |= CEREFL_0;\
	CECTL2 = CERS_2 | level_to_reg[max_thres];\
	CECTL2 |= CERSEL;\
	while (!CERDYIFG);\
	CEINT &= ~(CEIFG | CEIIFG);

// Are these supposed to be evenly spaced?
enum voltage {
	V_1_00 = 0,
	V_1_01 = 1,
	V_1_05 = 2,
	V_1_06 = 3,
	V_1_08 = 4,
	V_1_09 = 5,
	V_1_12 = 6,
	V_1_16 = 7,
	V_1_17 = 8,
	V_1_18 = 9,
	V_1_25 = 10,
	V_1_31 = 11,
	V_1_32 = 12,
	V_1_37 = 13,
	V_1_40 = 14,
	V_1_43 = 15,
	V_1_48 = 16,
	V_1_50 = 17,
	V_1_56 = 18,
	V_1_62 = 19,
	V_1_64 = 20,
	V_1_68 = 21,
	V_1_71 = 22,
	V_1_75 = 23,
	V_1_79 = 24,
	V_1_81 = 25,
	V_1_87 = 26,
	V_1_93 = 27,
	V_1_95 = 28,
	V_2_03 = 29,
	V_2_10 = 30,
	V_2_18 = 31,
	V_2_26 = 32,
	V_2_34 = 33,
	V_2_42 = 34
};

extern const unsigned level_to_reg[35];
extern const unsigned level_to_volt[35];

energy_t get_ceiled_level(energy_t e);

#endif
