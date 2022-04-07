#ifndef __LCFN_SCHEDULER_
#define __LCFN_SCHEDULER_

#include <libmsp/mem.h>
#include <stdint.h>

uint32_t get_charge_rate_worst();
void calculate_C(uint32_t charge_rate);
unsigned is_schedulable(uint32_t charge_rate);
unsigned calculate_charge_rate(uint16_t t_charge_end, uint16_t t_charge_start);
void calculate_energy_use(evt_t* e, unsigned v_before_event, unsigned v_after_event);
void update_comp();

// STAY BELOW 2.56V or the squared voltage (translated to 256) doesn't fit in a
// uint16_t!

//TODO set to a real values based on our adc, timer, etc
#define V_THRES  0
#define CHARGE_TIME_THRES 100
#define AMP_FACTOR  100
#define U_AMP_FACTOR  100

#define V_OFF_SQUARED 25600
//#define V_OFF_SQUARED 32400

#define CR_WINDOW_SIZE 3

extern volatile __nv uint32_t cr_window[CR_WINDOW_SIZE];
extern volatile __nv uint8_t cr_window_it;
extern volatile __nv uint8_t cr_window_ready;

extern uint16_t v_charge_start;
extern uint16_t v_charge_end;


#endif
