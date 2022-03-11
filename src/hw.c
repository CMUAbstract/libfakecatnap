
// Clock running, may not need this separate from Capy
void init_clock() {
	CSCTL0_H = CSKEY_H;
	CSCTL1 = DCOFSEL_6;
	CSCTL2 = SELM__DCOCLK | SELS__DCOCLK | SELA__VLOCLK;
	CSCTL3 = DIVA__1 | DIVS__1 | DIVM__1;
	CSCTL4 = LFXTOFF | HFXTOFF;
	CSCTL0_H = 0;
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
	SET_UPPER_COMP();
	CECTL1 = CEPWRMD_2 | CEON; // Ultra low power mode, on
	// Let the comparator output settle before checking or setting up interrupt
	while (!CERDYIFG);
	// clear int flag and enable int
	CEINT &= ~(CEIFG | CEIIFG);
	CEINT |= CEIE;
}




