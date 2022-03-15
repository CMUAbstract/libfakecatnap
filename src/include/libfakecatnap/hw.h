#ifndef _LIBFAKECATNAP_HW_H_
#define _LIBFAKECATNAP_HW_H_

void start_timer(uint16_t time);

void init_comparator();

#ifndef GDB_INT_CFG
#define DISABLE_LFCN_TIMER TA0CCTL0 &= ~CCIE;
#define ENABLE_LFCN_TIMER TA0CCTL0 |= CCIE;
#else

#define DISABLE_LFCN_TIMER P1IE &= ~BIT0;
#define ENABLE_LFCN_TIMER P1IE |= BIT0;

#endif


#endif
