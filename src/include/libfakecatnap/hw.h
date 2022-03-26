#ifndef _LIBFAKECATNAP_HW_H_
#define _LIBFAKECATNAP_HW_H_

void start_timer(uint16_t time);

void init_comparator();

uint16_t read_adc();
uint16_t turn_on_read_adc(void);


extern __nv uint16_t event_threshold;
extern __nv uint16_t vfinal;
extern __nv uint16_t vstart;
extern __nv uint16_t t_start;
extern __nv uint16_t t_end;


#ifndef GDB_INT_CFG
#define DISABLE_LFCN_TIMER TA0CCTL0 &= ~CCIE;
#define ENABLE_LFCN_TIMER TA0CCTL0 |= CCIE;
#else

#define DISABLE_LFCN_TIMER P1IE &= ~BIT0;
#define ENABLE_LFCN_TIMER P1IE |= BIT0;

#endif

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



#endif
