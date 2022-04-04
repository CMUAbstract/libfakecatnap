#include "catnap.h"
#include "culpeo.h"
#include "hw.h"
#include <msp430.h>
#include <stdio.h>
#include <math.h>
#include <float.h>
#include <libio/console.h>
#include <libmsp/mem.h>
#include <libmsp/sleep.h>
#include <libcapybara/power.h>

// uint32_t max: 4,294,967,295
__nv float Vmin = 2.103;
__nv float Vfinal = 2.238;
__nv float glob_sqrt = .5;

uint16_t culpeo_min_reading = 0xff;

#define cast_out(fval) \
  (unsigned)(fval*SCALER)

float inline local_sqrt(void) {
  float x = glob_sqrt;
	float z = x / 2;
  //PRINTF("In sqrt:\r\n");
	for (unsigned i = 0; i < 1000; ++i) {
		z -= (z*z - x) / (2*z);
		if ((z*z - x) < x * 0.0001) {
      //PRINTF("iterations:%u\r\n",i);
			return z;
		}
	}
	return z;
}

//NOTE: this doesn't zero pad
void print_float(float f)
{
	if (f < 0) {
		PRINTF("-");
		f = -f;
	}

	PRINTF("%u.",(int)f);
	//PRINTF("%u.",(unsigned)f);

	// Print decimal -- 3 digits
	PRINTF("%u", (((int)(f * 1000)) % 1000));
	//PRINTF("%u", (((unsigned)(f * 1000)) % 1000));
  PRINTF(" ");
}


culpeo_V_t calc_culpeo_vsafe(void) {
  float Vd = Vfinal - Vmin;
  float scale = Vmin*m_float + b_float;
  scale /= Vd_const_float;
  float Vd_scaled = Vd*scale;
  scale = 1- Vd_scaled;
  //print_float(scale);
  float Ve_squared = Vs_const_float + Voff_sq_float;
  float temp = Vfinal*Vfinal;
  temp *= n_ratio_float;
  Ve_squared -= temp;
  glob_sqrt = Ve_squared;
  float Ve = local_sqrt();
  //print_float(Ve);
  culpeo_V_t ve_int = cast_out(Ve);
  culpeo_V_t vd_int = cast_out(Vd_scaled);
  //PRINTF("Ve: %u, Vd: %u\r\n",ve_int,vd_int);
  uint16_t Vsafe = ve_int + vd_int;
  return Vsafe;
}

culpeo_V_t calc_culpeo_bucket(void) {
  // Walk through active events
  // Assume they're in order?
  // Start by setting bucket level to Voff
  float Vbucket = V_off_float;
  //TODO reorder these
  for (int i = 0; i < MAX_EVENTS; i++) {
		evt_t* e = all_events.events[i];
    if ( e == NULL || e->valid == OFF) {
      continue;
    }
    for (int j = 0; j < e->burst_num; j++) {
      /*PRINTF("0Bucket:");
      print_float(Vbucket);
      PRINTF("\r\n");*/
      if ((Vbucket - (e->V_final - e->V_min)) < V_off_float) {
        Vbucket = V_off_float + (e->V_final - e->V_min);
        //PRINTF("inner mod\r\n");
      }
      /*PRINTF("1Bucket:");
      print_float(Vbucket);
      PRINTF("\r\n");*/
      //TODO we could optimize and add another variable to store Vbucket_sq since
      //we get it for free
      float Vbucket_sq;
      Vbucket_sq = V_start_sq_float - e->V_final*e->V_final + Vbucket*Vbucket;
      glob_sqrt = Vbucket_sq;
      Vbucket = local_sqrt();
      /*PRINTF("Bucket:");
      print_float(Vbucket);
      PRINTF("\r\n");*/
    }
  }
  culpeo_V_t Vb = cast_out(Vbucket);
  return Vb;
}

#define CULPEO_ADC_ENABLE \
do { \
  P1SEL1 |= BIT2; \
  P1SEL0 |= BIT2;\
  ADC12CTL0 &= ~ADC12ENC; \
  ADC12CTL0 = ADC12SHT0_2 | ADC12ON; \
  ADC12CTL1 = ADC12SHP;\
  ADC12CTL2 = ADC12RES_2; \
  ADC12MCTL0 = ADC12INCH_2;\
  ADC12IER0 &= ~ADC12IE0;\
  __delay_cycles(1000);\
  }while(0);

#define CULPEO_ADC_DISABLE \
		ADC12CTL0 &= ~(ADC12ENC | ADC12ON);

#define CULPEO_PROF_TIMER_ENABLE \
	TA1CCR0 = 8000;\
	TA1CTL = MC__UP | TASSEL__SMCLK;\
  TA1CCTL0 = CCIE;\

#define CULPEO_PROF_TIMER_DISABLE \
  TA1CCTL0 &= ~CCIE;

// Leave in volatile memory
volatile uint8_t culpeo_profiling_flag = 0;

//Look, this really is intrinsice to the calculations we're doing here. It is
//customized to the divider, but meh
#define CULPEO_VHIGH 1875

void culpeo_charging(){ 
  uint16_t adc_reading = 0x00;
  PRINTF("Charging!\r\n"); 
  //Configure P1.2 for ADC
  P1SEL1 |= BIT2;
  P1SEL0 |= BIT2;

  ADC12CTL0 &= ~ADC12ENC;           // Disable ADC
  ADC12CTL0 = ADC12SHT0_2 | ADC12ON;      // Sampling time, S&H=16, ADC12 on
  ADC12CTL1 = ADC12SHP;                   // Use sampling timer
  ADC12CTL2 = ADC12RES_2;                // 12-bit conversion results
  ADC12MCTL0 = ADC12INCH_2; // A2 ADC input select; VR=Vdd
  ADC12IER0 &= ~ADC12IE0;                  // Disable ADC conv complete interrupt
  __delay_cycles(10000);
  int count = 0;
  while( adc_reading < CULPEO_VHIGH ){
    count++;
    // ======== Configure ADC ========
    // Take single sample when timer triggers and compare with threshold

    ADC12IFGR0 &= ~ADC12IFG0;
    ADC12CTL1 |= ADC12SHP | ADC12SHS_0 | ADC12CONSEQ_0 ;      // Use ADC12SC to trigger and single-channel
    ADC12CTL0 |= (ADC12ON + ADC12ENC + ADC12SC);      // Trigger ADC conversion

    while(!(ADC12IFGR0 & ADC12IFG0)); // Wait till conversion over
    adc_reading = ADC12MEM0;          // Read ADC value

    ADC12CTL0 &= ~ADC12ENC;           // Disable ADC
    msp_sleep(10000);
    //__delay_cycles(80000);
    if (count > 2) {
      capybara_shutdown();
    }
  }
  while( adc_reading > CULPEO_VHIGH){
    // ======== Configure ADC ========
    // Take single sample when timer triggers and compare with threshold

    ADC12IFGR0 &= ~ADC12IFG0;
    ADC12CTL1 |= ADC12SHP | ADC12SHS_0 | ADC12CONSEQ_0 ;      // Use ADC12SC to trigger and single-channel
    ADC12CTL0 |= (ADC12ON + ADC12ENC + ADC12SC);      // Trigger ADC conversion

    while(!(ADC12IFGR0 & ADC12IFG0)); // Wait till conversion over
    adc_reading = ADC12MEM0;          // Read ADC value

    ADC12CTL0 &= ~ADC12ENC;           // Disable ADC
    //burn
    __delay_cycles(8000);
  }
  adc_reading = 0;
}




int profile_event(evt_t *ev) {
  //Disable other interrupts
  LCFN_INTERRUPTS_DISABLE;
  // Charge up to 2.4V
  culpeo_charging();
  // Start our timer
  CULPEO_PROF_TIMER_ENABLE;
  // Set flag, set min
  culpeo_min_reading = 0xffff;
  culpeo_profiling_flag = 1;
  // Set up context switch n'at
  context_t *next = (curctx == &context_0 )? &context_1 : &context_0;
  next->active_task = curctx->active_task;
  next->active_evt = ev;
  next->mode = EVENT;
  curctx = next;
  PRINTF("Jump!\r\n");
  // Jump
  __asm__ volatile ( // volatile because output operands unused by C
      "br %[nt]\n"
      : /* no outputs */
      : [nt] "r" (curctx->active_evt->evt)
  );
}

// 

int profile_cleanup(evt_t *ev) {
  CULPEO_PROF_TIMER_DISABLE;
  // sleep
  __delay_cycles(800000);//100ms
  uint16_t temp = read_adc();  
  Vmin = (float)culpeo_min_reading/800.0;
  Vfinal = (float)temp/800.0;
  ev->V_safe = calc_culpeo_vsafe();
  ev->V_min = Vmin;
  ev->V_final = Vfinal;
  culpeo_profiling_flag = 0;
  CULPEO_ADC_DISABLE;
}

void __attribute__((interrupt(TIMER1_A0_VECTOR)))
timerA1ISRHandler(void) {
    uint16_t val;
    //PRINTF("ms timer\r\n");
    TA1R = 0;
    val = read_adc();
    if (culpeo_min_reading > val) {
      culpeo_min_reading = val;
    }
}


