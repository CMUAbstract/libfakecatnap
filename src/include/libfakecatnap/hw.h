#ifndef _LIBFAKECATNAP_HW_H_
#define _LIBFAKECATNAP_HW_H_

void start_timer(uint16_t time);
void start_timer_running(void);

void init_comparator();

uint16_t read_adc();
uint16_t turn_on_read_adc(uint16_t);


extern __nv uint16_t event_threshold;
extern __nv uint16_t vfinal;
extern __nv uint16_t vstart;
extern uint16_t t_start;
extern uint16_t t_end;

extern volatile uint8_t comp_violation;
extern volatile uint8_t  ISR_DISABLE;


#ifndef GDB_INT_CFG

#define DISABLE_LFCN_TIMER TA0CCTL0 &= ~CCIE;
#define ENABLE_LFCN_TIMER TA0CCTL0 |= CCIE;

#define LCFN_INTERRUPTS_DISABLE \
  TA0CCTL0 &= ~CCIE; \
  ISR_DISABLE = 1; \

#define LCFN_INTERRUPTS_INTERNAL_DISABLE \
  TA0CCTL0 &= ~CCIE; \
  CEINT &= ~(CEIE | CEIIE | CERDYIE);


#ifndef LFCN_CONT_POWER

#define LCFN_INTERRUPTS_ENABLE \
  BIT_FLIP(1,1); \
  ISR_DISABLE = 0;\
  /*if (comp_violation | (CECTL1 & CEOUT))*/\
  if (comp_violation || (CECTL1 & CEOUT)) { \
    BIT_FLIP(1,1); \
    CEINT |= CEIFG;\
  }\
	CEINT |= (CEIE | CEIIE);\
  TA0CCTL0 |= CCIE; \

#else//CONT_POWER

#define LCFN_INTERRUPTS_ENABLE \
  TA0CCTL0 |= CCIE; \

#endif//CONT_POWER

#else

#define DISABLE_LFCN_TIMER P1IE &= ~BIT0;
#define ENABLE_LFCN_TIMER P1IE |= BIT0;

#define LCFN_INTERRUPTS_DISABLE \
  P1IE &= ~BIT0;\
	CEINT = ~(CEIE | CEIIE);

#endif//INT

#define ADC_ENABLE \
do { \
  P1SEL1 |= BIT2; \
  P1SEL0 |= BIT2;\
  ADC12CTL0 &= ~ADC12ENC; \
  ADC12CTL0 = ADC12SHT0_2 | ADC12ON; \
  ADC12CTL1 = ADC12SHP;\
  ADC12CTL2 = ADC12RES_2; \
  ADC12MCTL0 = ADC12INCH_2;\
  ADC12IER0 &= ~ADC12IE0;\
  }while(0);


#define ADC_DISABLE \
		ADC12CTL0 &= ~(ADC12ENC | ADC12ON);


// mask for mode control bits in TAxCTL
#define MC_MASK 0x30

#endif
