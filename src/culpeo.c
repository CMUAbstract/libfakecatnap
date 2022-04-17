#include "catnap.h"
#include "culpeo.h"
#include "comp.h"
#include "hw.h"
#include "print_util.h"
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
__nv uint8_t new_event = 0;

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
  PRINTF("Calc_vsafe:");
  print_float(Vfinal);
  print_float(Vmin);
  float Vd = Vfinal - Vmin;
  if (Vd < 0) {
    Vd = 0;
  }
  float scale = Vmin*(m_float*Vmin + b_float);
  scale /= Vd_const_float;
  float Vd_scaled = Vd*scale;
  // scale = 1- Vd_scaled;
  //print_float(scale);
  float Ve_squared = Vs_const_float + Voff_sq_float;
  float temp = Vfinal*Vfinal;
  temp *= n_ratio_float;
  Ve_squared -= temp;
  glob_sqrt = Ve_squared;
  float Ve = local_sqrt();
  //print_float(Ve+Vd_scaled);
  culpeo_V_t ve_int = cast_out(Ve);
  culpeo_V_t vd_int = cast_out(Vd_scaled);
  LFCN_DBG_PRINTF("Ve: %u, Vd: %u\r\n",ve_int,vd_int);
  uint16_t Vsafe = ve_int + vd_int;
  return Vsafe;
}

unsigned voltage_to_thresh(uint16_t volt) {
  unsigned val;
  if (SCALER == 100) {
    val = (volt - 140)/2;
  }
  else { //SCALER == 1000
    val = (volt - 1400)/20;
    LFCN_DBG_PRINTF("volt: %u val:%u\r\n",volt,val);
  }
  return val;
}

culpeo_V_t calc_culpeo_bucket(void) {
  // Walk through active events
  // Assume they're in order?
  // Start by setting bucket level to Voff
  float Vbucket = V_off_float;
  for (int i = MAX_EVENTS - 1; i >= 0; i--) {
		evt_t* e = all_events.events[i];
    if ( e == NULL || e->V_final == 0 || e == &EVT_FCN_STARTER) {
      continue;
    }
    for (int j = 0; j < e->burst_num; j++) {
      LFCN_DBG_PRINTF("0Bucket:");
      #ifdef LFCN_DBG
      print_float(e->V_final);
      print_float(e->V_min);
      #endif
      LFCN_DBG_PRINTF("\r\n");
      if ((Vbucket - (e->V_final - e->V_min)) < V_off_float) {
        Vbucket = V_off_float + (e->V_final - e->V_min);
        //PRINTF("inner mod\r\n");
      }
      //TODO we could optimize and add another variable to store Vbucket_sq since
      //we get it for free
      float Vbucket_sq;
      Vbucket_sq = V_start_sq_float - e->V_final*e->V_final + Vbucket*Vbucket;
      glob_sqrt = Vbucket_sq;
      Vbucket = local_sqrt();
    }
  }
  culpeo_V_t Vb = cast_out(Vbucket);
  for (int i = 0; i < MAX_EVENTS; i++) {
		evt_t* e = all_events.events[i];
    if ( e == NULL || e == &EVT_FCN_STARTER || e->V_final == 0) {
      continue;
    }
    if (Vb < e->V_safe) {
      Vb = e->V_safe;
    }
  }
  LFCN_DBG_PRINTF("Bucket: %u",Vb);
  if (Vb < 1650) {
    Vb = 1650;
  }
  lower_thres = voltage_to_thresh(Vb); //Should roughly even up the comp and adc
  max_thres = lower_thres + 5; //Corresponds to ~.1V
  if (max_thres >= NUM_LEVEL) {
    PRINTF("Error! max thres too high %u\r\n",max_thres);
    while(1);
  }
  event_threshold = Vb - 2;
  LFCN_DBG_PRINTF("|%u %u %u:thresholds\r\n",lower_thres,max_thres,event_threshold);

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
		ADC12CTL0 &= ~(ADC12ENC | ADC12ON);\
    REFCTL0 &= ~REFON;

#define CULPEO_ADC_FULLY_OFF \
  ADC12CTL0 = 0;\
  ADC12CTL1 = 0;\
  ADC12CTL2 = 0x11;\
  ADC12IER0 = 0;\
  TA1CTL = 0; \
  TA1CCTL0 = 0;

#define CULPEO_PROF_TIMER_ENABLE \
	TA1CCR0 = 8000;\
	TA1CTL = MC__UP | TASSEL__SMCLK;\
  TA1CCTL0 = CCIE;\

#define CULPEO_PROF_TIMER_DISABLE \
  TA1CCTL0 &= ~CCIE;

// Leave in volatile memory
volatile uint8_t culpeo_profiling_flag = 0;

//Look, this really is intrinsic to the calculations we're doing here. It is
//customized to the divider, but meh
#define CULPEO_VHIGH 1930

void culpeo_charging(){ 
  uint16_t adc_reading = 0x00;
  //Configure P1.2 for ADC
  P1SEL1 |= BIT2;
  P1SEL0 |= BIT2;
  PRINTF("Charging!\r\n");
  ADC12CTL0 &= ~ADC12ENC;           // Disable ADC

  while(REFCTL0 & REFGENBUSY);            // If ref generator busy, WAIT
  REFCTL0 |= REFVSEL_2 | REFON;

  ADC12CTL0 = ADC12SHT0_2 | ADC12ON;      // Sampling time, S&H=16, ADC12 on
  ADC12CTL1 = ADC12SHP;                   // Use sampling timer
  ADC12CTL2 = ADC12RES_2;                // 12-bit conversion results
  ADC12MCTL0 = ADC12INCH_2 | ADC12VRSEL_1; // A2 ADC input select; VR=Vdd
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
  // Disable incoming power
  //TODO do we need this?
  //P1OUT |= BIT1;
  //P1DIR |= BIT1;
  __delay_cycles(80000);

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
    for(int i = 0; i < 0xA0;i++){
      //for(int j = 0; j < 0xff;j++){
        volatile temp = i;
      //}
    }
    __delay_cycles(8000);
    //PRINTF("Burn!\r\n");
  }
  LFCN_DBG_PRINTF("Go:%u\r\n",adc_reading);
  //ADC12CTL0 &= ~(ADC12ON);
  //REFTCL0 &= ~REFON;
  // Leave on for profiling
  adc_reading = 0;
}


int profile_event(evt_t *ev) {
  //Disable other interrupts
  LCFN_INTERRUPTS_DISABLE;
  // Charge up to 2.4V
  culpeo_charging();
  if (!ev->no_profile) {
  // Start our timer
    CULPEO_PROF_TIMER_ENABLE;
  }
  else {
    //Turn off adc
  }
  // Set flag, set min
  culpeo_min_reading = 0xffff;
  culpeo_profiling_flag = 1;
  // Set up context switch n'at
  context_t *next = (curctx == &context_0 )? &context_1 : &context_0;
  next->active_task = curctx->active_task;
  next->active_evt = ev;
  next->fifo = curctx->fifo;
  next->mode = EVENT;
  curctx = next;
  PRINTF("Profiling!\r\n");
  BIT_FLIP(1,1); \
  BIT_FLIP(1,1); \
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
  uint16_t temp;
  uint16_t max = 0;
  if (ev->no_profile) {
    // Catch min now
    ADC12CTL0 &= ~ADC12ENC;           // Disable ADC

    while(REFCTL0 & REFGENBUSY);            // If ref generator busy, WAIT
    REFCTL0 |= REFVSEL_2 | REFON;

    ADC12CTL0 = ADC12SHT0_2 | ADC12ON;      // Sampling time, S&H=16, ADC12 on
    ADC12CTL1 = ADC12SHP;                   // Use sampling timer
    ADC12CTL2 = ADC12RES_2;                // 12-bit conversion results
    ADC12MCTL0 = ADC12INCH_2 | ADC12VRSEL_1; // A2 ADC input select; VR=Vdd
    ADC12IER0 &= ~ADC12IE0;                  // Disable ADC conv complete interrupt
    culpeo_min_reading = read_adc();
  }
  for(int i =0; i < 5; i++) {
    __delay_cycles(800000);//100ms
    //temp = turn_on_read_adc(SCALER);
    temp = read_adc();
    LFCN_DBG_PRINTF("%u\r\n",temp);
    if(temp > max) {
      max = temp;
    }
  }
  CULPEO_ADC_DISABLE;
  //CULPEO_ADC_FULLY_OFF;
  // Re-enable incoming power
  //P1OUT &= ~BIT1;
  //__delay_cycles(80);
  //P1DIR &= ~BIT1;
  LFCN_DBG_PRINTF("%u\r\n",temp);
  Vmin = (float)culpeo_min_reading/820.0;
  Vfinal = (float)max/820.0;
  //Vmin = (float)culpeo_min_reading/SCALER;
  //Vfinal = (float)max/SCALER;
  ev->V_safe = calc_culpeo_vsafe();
  ev->V_min = Vmin;
  ev->V_final = Vfinal;
  new_event = 1;
  culpeo_profiling_flag = 0;
  //CULPEO_ADC_DISABLE;
  return 0;
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


