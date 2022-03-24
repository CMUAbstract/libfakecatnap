// Contains the major pieces of catnap, the specifics live elsewhere, but the
// boot behavior, scheduler, timer and comparator interrupts all live here
#include <stdio.h>
#include <msp430.h>
#include "catnap.h"
#include "tasks.h"
#include "events.h"
#include "hw.h"
#include "comp.h"
#include "checkpoint.h"
#include <libcapybara/board.h>
#include <libcapybara/power.h>
#include <libmsp/periph.h>
#include <libio/console.h>

__nv evt_list_t all_events = {.events = {0}, .cur_event = MAX_EVENTS+1};
__nv task_fifo_t all_tasks = {.tasks = {0}};

__nv volatile context_t context_0 = {NULL,NULL,CHARGING,0,&fifo_0};
__nv volatile context_t context_1 = {NULL,NULL,CHARGING,0,&fifo_0};
__nv volatile context_t *curctx = &context_0;

// Main function to handle boot and kick us off
int main(void) {
  // Init capy
  capybara_init(); // TODO may want to slim down
  __delay_cycles(8000000); //TODO remove!
  PRINTF("Boot\r\n");
  // Init timer
  ENABLE_LFCN_TIMER;
  // Init comparator
  init_comparator();
  // on very first boot, pull in starter event
  add_event(&EVT_FCN_STARTER);
  // Run the whole shebang
  scheduler();

  return 0; // Should not get here!
}

__nv uint16_t vstart = 0;
__nv uint16_t vfinal = 0;

void scheduler(void) {
  while(1) {
    // Why are we entering the loop?
    switch (curctx->mode) {
      case EVENT:
        vfinal = turn_on_read_adc();
        ticks_waited = TA0R;
        break;
      case TASK:
      case SLEEPING:
        break;
    }
    // Update feasibility, etc
    update_event_timers(ticks_waited);
    // Schedule something next
    evt_t * nextE = pick_event();
    // If event, measure vcap
    if (nextE != NULL) {
      // Swap context
      context_t *next = (curctx == &context_0 )? &context_1 : &context_0;
      next->active_task = curctx->active_task;
      next->active_evt = nextE;
      next->mode = EVENT;
      // enable the absolute lowest threshold so we power off if there's a failure
      SET_LOWER_COMP(V_1_12); //TODO may not need this on camaroptera
      vstart = turn_on_read_adc();
      curctx = next;
      // Jump
      __asm__ volatile ( // volatile because output operands unused by C
          "br %[nt]\n"
          : /* no outputs */
          : [nt] "r" (curctx->active_evt->evt)
      );
    }
    //else { TODO can we actually leave this out?
      // First set up timers to wait for an event
      ticks_to_wait = get_next_evt_time();
      start_timer(ticks_to_wait);
      // check if we're above
    uint16_t temp = turn_on_read_adc();
    if (temp > event_threshold) { // Only pick a task if we're above the thresh
      SET_LOWER_COMP(event_bucket); // Interrupt if we got below event thresh
      // Check if there's an active task, and jump to it if there is
      if (curctx->active_task != NULL && curctx->active_task->valid_chkpt) {
        context_t *next = (curctx == &context_0 )? &context_1 : &context_0;
        next->active_task = curctx->active_task;
        next->fifo = curctx->fifo;
        next->active_evt = NULL;
        next->mode = TASK;
        curctx = next;
        restore_vol(); // Jump to new task
      }
      task_t * nextT = pop_task(); //Start changes
      if ( nextT != NULL) {
        context_t *next = (curctx == &context_0 )? &context_1 : &context_0;
        next->active_task = nextT;
        next->active_evt = NULL;
        next->mode = TASK;
        update_task_fifo(next); // Commit changes since we need to wait until the
                                // task is latched.
        curctx = next;
        __asm__ volatile ( // volatile because output operands unused by C
            "br %[nt]\n"
            : /* no outputs */
            : [nt] "r" (curctx->active_task->func)
        );
      }
    }
    //}

    // Else sleep to recharge
      SET_MAX_UPPER_COMP(); // Set interrupt when we're fully charged too
      // measure vcap
      // set compE interrupt/timer
      // sleep
    __disable_interrupt();
    int _flag = 0;
    while(TA0CCTL0 & CCIE) { //TODO expand so we can get out with compE
      __bis_SR_register(LPM3_bits + GIE);
      __disable_interrupt();
      _flag = 1;
    }
    __enable_interrupt();
    _flag = 0;
  }

  return 0; // Should not get here
}



void COMP_VBANK_ISR (void) {
  PRINTF("in comp\r\n");
  DISABLE_LFCN_TIMER;// Just in case
  COMP_VBANK(INT) &= ~COMP_VBANK(IE);
  COMP_VBANK(CTL1) &= ~COMP_VBANK(ON);
  if (/*Failure imminent*/) {
    switch (curctx->mode) {
      case EVENT:
      case SLEEPING:
        capybara_shutdown();
        break;
      case TASK:
        volatile int chkpt_flag = 0;
        checkpoint();
        chkpt_flag = 1;
        if (chkpt_flag) {
          capybara_shutdown();
        }
        break;
      default:
        break;
    }
  }
  else if(/*Discharged to event bucket*/) {
    /* Stop task, start charging*/
  }
  else {/*Charged to max*/
    /* allow tasks again */
  }
  COMP_VBANK(INT) |= COMP_VBANK(IE);
  ENABLE_LFCN_TIMER;// Just in case
}

#ifndef GDB_INT_CFG
void __attribute__((interrupt(TIMER0_A0_VECTOR)))
timerISRHandler(void) {

	TA0CTL = MC_0 | TASSEL_1 | TACLR; // Stop timer
	TA0CCTL0 &= ~(CCIFG | CCIE); // Clear flag and stop int
	//TA0CCTL0 &= ~(CCIFG); // Clear flag and stop int
	CEINT &= ~CEIE; // Disable comp interrupt for our sanity
/*--------------------------------------------------------*/
  PRINTF("timera0\r\n");
  // Record wait time
  //ticks_waited = ticks_to_wait;
  ticks_waited = ticks_to_wait;
  TA0R = 0; // Not sure if we need this
  // If we're in a task, checkpoint it and move on
  if ( curctx->mode == TASK) {
    volatile int chkpt_flag = 0;
    checkpoint();
    chkpt_flag = 1;
    // Jump to scheduler
    if ( chkpt_flag == 1) {
      __asm__ volatile ( // volatile because output operands unused by C
          "br %[nt]\n"
          : /* no outputs */
          : [nt] "r" (&scheduler)
      );
    }
    return;
  }
  __bic_SR_register_on_exit(LPM3_bits); //wake up
/*--------------------------------------------------------*/
	CEINT |= CEIE; // Re-enable checkpoint interrupt
  //TA0CCTL0 |= CCIE; // Re-enable timer int.
}
#else
void __attribute__((interrupt(PORT1_VECTOR)))
timerISRHandler(void) {
  P1IE &= ~BIT0;
  P1IFG = 0;
  // Record wait time
  //ticks_waited = ticks_to_wait;
  // If we're in a task, checkpoint it and move on
  if ( curctx->mode == TASK ) {
    volatile int chkpt_flag = 0;
    checkpoint();
    chkpt_flag = 1;
    if (chkpt_flag) {
      // Jump to scheduler
      __asm__ volatile ( // volatile because output operands unused by C
          "br %[nt]\n"
          : /* no outputs */
          : [nt] "r" (&scheduler)
      );
    }
    return;
  }
  __bic_SR_register_on_exit(LPM3_bits); //wake up
  P1IE |= BIT0;
}
#endif


