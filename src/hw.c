#include "catnap.h"

#ifndef GDB_INT_CFG
void start_timer(uint16_t time) {
	// Set and fire Timer A
	TA0CCR0 = time;
	TA0CTL = MC_1 | TASSEL_1 | TACLR | ID_3 | TAIE;
  TA0CCTL0 |= CCIE;
}
#else
void start_timer(uint16_t time) {
	// Set and fire P1.0
  P1OUT &= ~BIT0;
  P1REN |= BIT0;
  P1DIR &= ~BIT0;
  P1IFG = 0;
  P1IE |= BIT0;
}
#endif


void init_comparator() {
	// set up for checkpoint trigger
	// P3.1 is the input pin for Vcap
  //TODO set to correct pin and lineup with capybara interrupt
	P3SEL1 |= BIT1;
	P3SEL0 |= BIT1;
	CECTL3 |= CEPD3; // Disable buffer (for low power, might not needed)
  // reconfigure COMP_E
	CECTL0 = CEIPEN | CEIPSEL_13; //SET_UPPER_THRES(V_1_31);
  //TODO bring in comp.h
	//SET_UPPER_COMP();
	CECTL1 = CEPWRMD_2 | CEON; // Ultra low power mode, on
	// Let the comparator output settle before checking or setting up interrupt
	while (!CERDYIFG);
	// clear int flag and enable int
	CEINT &= ~(CEIFG | CEIIFG);
	CEINT |= CEIE;
}




