#include "catnap.h"

void start_timer(uint16_t time) {
	// Set and fire Timer A
	TA0CCR0 = time;
	TA0CTL = MC_1 | TASSEL_1 | TACLR | ID_3 | TAIE;
  TA0CCTL0 |= CCIE;
}



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




