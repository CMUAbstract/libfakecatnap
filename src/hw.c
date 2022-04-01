#include "catnap.h"
#include "hw.h"
#include <libmsp/periph.h>
#include "comp.h"
#include <libio/console.h>
uint16_t __nv event_threshold = LFCN_STARTER_THRESH; //TODO add levels
uint16_t __nv vfinal = 0;
uint16_t __nv vstart = 0;
uint16_t t_start = 0;
uint16_t t_end = 0;

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
  GPIO(LFCN_VCAP_COMP_PIN_PORT, SEL0) |= BIT(LFCN_VCAP_COMP_PIN_PIN);
  GPIO(LFCN_VCAP_COMP_PIN_PORT, SEL1) |= BIT(LFCN_VCAP_COMP_PIN_PIN);
	CECTL3 |= CEPD13; // Disable buffer (for low power, might not needed)
  // reconfigure COMP_E	
  CECTL0 = CEIMEN | CEIMSEL_13;
	CECTL2 = CERS_2 | level_to_reg[DEFAULT_MIN_THRES];
	CECTL2 &= ~CERSEL;
  CECTL1 = CEPWRMD_2 | CEON; // Ultra low power mode, on
	// Let the comparator output settle before checking or setting up interrupt
	while (!CERDYIFG);
	// clear int flag and enable int
	CEINT &= ~(CEIFG | CEIIFG);
#ifndef LFCN_CONT_POWER
	CEINT |= CEIE;
#endif
}


uint16_t read_adc(void) {
  // ======== Configure ADC ========
  // Take single sample when timer triggers and compare with threshold
  ADC12IFGR0 &= ~ADC12IFG0;
  // Use ADC12SC to trigger and single-channel
  ADC12CTL1 |= ADC12SHP | ADC12SHS_0 | ADC12CONSEQ_0 ;
  ADC12CTL0 |= (ADC12ON + ADC12ENC + ADC12SC); 			// Trigger ADC conversion

  while(!(ADC12IFGR0 & ADC12IFG0)); 			// Wait till conversion over
  uint16_t adc_reading = ADC12MEM0; 					// Read ADC value
  return adc_reading;
}

/* Doesn't assume that adc is set up before reading*/
uint16_t turn_on_read_adc(uint16_t scaler) {
	ADC12CTL0 &= ~ADC12ENC;           // Disable conversions

	ADC12CTL1 = ADC12SHP; // SAMPCON is sourced from the sampling timer
	ADC12MCTL0 = ADC12INCH_2; // VR+: VREF, VR- = AVSS, // Channel A15
	ADC12CTL0 |= ADC12SHT03 | ADC12ON; // sample and hold time 32 ADC12CLK

	while( REFCTL0 & REFGENBUSY );

	// If this works as I expect,
	// N = 4096 * (Vin + 0.5*(2.0/4096)) / (2.0) = 4096*Vin/2.0 + 0.5
  //Vin = 2.56 on Capybara, the booster provides that quite reliably so we don't
  //need to switch references
	__delay_cycles(1000);                   // Delay for Ref to settle
	ADC12CTL0 |= ADC12ENC;                  // Enable conversions

	ADC12CTL0 |= ADC12SC;                   // Start conversion
	ADC12CTL0 &= ~ADC12SC;                  // We only need to toggle to start conversion
	while (ADC12CTL1 & ADC12BUSY) ;

	unsigned output = ADC12MEM0;

	ADC12CTL0 &= ~ADC12ENC;                 // Disable conversions
	ADC12CTL0 &= ~(ADC12ON);                // Shutdown ADC12
	REFCTL0 &= ~REFON;
  // 100*Vcap = adc_val * 2.56 * 2 * 100 / 4096	
	// 100 is to not use float
  // 2 is for the Vcap divider
  // 2.56 is Capy's Vdd
  // The absurdly satisfying simplification is below... You're welcome.
  if (scaler == 100) {
    output = output >> 3;
  }
  else if (scaler == 1000) {
    // With scaler == 1000 we're multiplying by 1.25, so 5/4
    output = (output * 5) >> 2;
  }
  // Cheat sheet: 100*Vcap = adc_val/8
	return (unsigned)output;
}
