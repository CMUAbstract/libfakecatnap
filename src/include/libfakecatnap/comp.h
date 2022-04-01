#ifndef __LCFN_COMP__
#define __LCFN_COMP__
#include <libmsp/mem.h>
#include <stdint.h>

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

#define DEFAULT_LOWER_THRES V_2_00
#define DEFAULT_MIN_THRES V_1_60
#define DEFAULT_UPPER_THRES V_2_44
#define DEFAULT_NEARLY_MAX_THRES V_2_44
// 0.5865
#define V_NEARLY_MAX 260


#define THRES_MAX V_2_48
#define NUM_LEVEL 55


#define LEVEL_MASK 0x7f1f

typedef uint32_t energy_t;

extern __nv volatile energy_t baseline_E;
extern __nv volatile unsigned lower_thres;
extern __nv volatile unsigned upper_thres;
extern __nv volatile unsigned max_thres;
extern const energy_t level_to_E[NUM_LEVEL];

//	lower_thres = level;
#ifndef LFCN_CONT_POWER

#define SET_LOWER_COMP(val)\
	CECTL0 = CEIMEN | CEIMSEL_13;\
	CECTL2 = CEREFL_0;\
	CECTL2 = CERS_2 | level_to_reg[val];\
	CECTL2 &= ~CERSEL;\
  CECTL1 = CEPWRMD_2 | CEON; \
	while (!CERDYIFG);\
	CEINT &= ~(CEIFG | CEIIFG);\
	CEINT |= CEIE;\
  if (CECTL1 & CEOUT) { \
    CEINT |= CEIFG;\
  }\
  CEINT &= ~(CEIE | CEIIE);


//	upper_thres = level;
#define SET_UPPER_COMP(val)\
	CECTL0 = CEIPEN | CEIPSEL_13; \
	CECTL2 = CEREFL_0;\
	CECTL2 = CERS_2 | level_to_reg[val];\
	CECTL2 |= CERSEL;\
  CECTL1 = CEPWRMD_2 | CEON; \
	while (!CERDYIFG);\
	CEINT &= ~(CEIFG | CEIIFG);\
	CEINT |= CEIE;\
  if (CECTL1 & CEOUT) {\
    CEINT |= CEIFG;\
  }\
  CEINT &= ~(CEIE | CEIIE);

#define SET_MAX_UPPER_COMP()\
	CECTL0 = CEIPEN | CEIPSEL_13; \
	CECTL2 = CEREFL_0;\
	CECTL2 = CERS_2 | level_to_reg[max_thres];\
	CECTL2 |= CERSEL;\
  CECTL1 = CEPWRMD_2 | CEON; \
	while (!CERDYIFG);\
	CEINT &= ~(CEIFG | CEIIFG); \
	CEINT |= CEIE;\
  if (CECTL1 & CEOUT) {\
    CEINT |= CEIFG;\
  }\
  CEINT &= ~(CEIE | CEIIE);

#error WTF

#else//CONT_POWER

#define SET_LOWER_COMP(val)

#define SET_UPPER_COMP(val)

#define SET_MAX_UPPER_COMP()

#endif//CONT_POWER


#if 1
enum voltage {
V_1_39 = 0 ,
V_1_41 = 1 ,
V_1_43 = 2 ,
V_1_46 = 3 ,
V_1_48 = 4 ,
V_1_50 = 5 ,
V_1_52 = 6 ,
V_1_54 = 7 ,
V_1_56 = 8 ,
V_1_58 = 9 ,
V_1_60 = 10 ,
V_1_62 = 11 ,
V_1_64 = 12 ,
V_1_66 = 13 ,
V_1_68 = 14 ,
V_1_70 = 15 ,
V_1_72 = 16 ,
V_1_74 = 17 ,
V_1_76 = 18 ,
V_1_78 = 19 ,
V_1_80 = 20 ,
V_1_82 = 21 ,
V_1_84 = 22 ,
V_1_86 = 23 ,
V_1_88 = 24 ,
V_1_90 = 25 ,
V_1_92 = 26 ,
V_1_94 = 27 ,
V_1_96 = 28 ,
V_1_98 = 29 ,
V_2_00 = 30 ,
V_2_02 = 31 ,
V_2_04 = 32 ,
V_2_06 = 33 ,
V_2_08 = 34 ,
V_2_10 = 35 ,
V_2_12 = 36 ,
V_2_14 = 37 ,
V_2_16 = 38 ,
V_2_18 = 39 ,
V_2_20 = 40 ,
V_2_22 = 41 ,
V_2_24 = 42 ,
V_2_26 = 43 ,
V_2_28 = 44 ,
V_2_30 = 45 ,
V_2_32 = 46 ,
V_2_34 = 47 ,
V_2_36 = 48 ,
V_2_38 = 49 ,
V_2_40 = 50 ,
V_2_42 = 51 ,
V_2_44 = 52 ,
V_2_46 = 53 ,
V_2_48 = 54 
};
#endif
#if 0
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
#endif

extern const unsigned level_to_reg[NUM_LEVEL];
extern const unsigned level_to_volt[NUM_LEVEL];

energy_t get_ceiled_level(energy_t e);

#endif
