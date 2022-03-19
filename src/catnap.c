// Contains the major pieces of catnap, the specifics live elsewhere, but the
// boot behavior, scheduler, timer and comparator interrupts all live here
#include <stdio.h>
#include <msp430.h>
#include "catnap.h"
#include "tasks.h"
#include "events.h"
#include "hw.h"
#include "checkpoint.h"
#include <libcapybara/board.h>
#include <libcapybara/power.h>
#include <libmsp/periph.h>
#include <libio/console.h>

__nv evt_list_t all_events = {.events = {0}, .cur_event = MAX_EVENTS+1}; 
__nv task_fifo_t all_tasks = {.tasks = {0}, .tsk_cnt = 0, .front = 0, .back = 0};

__nv volatile context_t context_0 = {0};
__nv volatile context_t context_1 = {0};
__nv volatile context_t *curctx = &context_0;

__nv volatile fifo_meta_t fifos_0;
__nv volatile fifo_meta_t fifos_1;
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


void scheduler(void) {
  while(1) {
    // Why are we entering the loop?
    switch (curctx->mode) {
      case EVENT:
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
      // TODO measure Vcap
      curctx = next;
      // Jump
      __asm__ volatile ( // volatile because output operands unused by C
          "br %[nt]\n"
          : /* no outputs */
          : [nt] "r" (curctx->active_evt->evt)
      );
    }
    else {
      // First set up timers to wait for an event
      ticks_to_wait = get_next_evt_time();
      start_timer(ticks_to_wait);
      // Check if there's an active task, and jump to it if there is
      if (curctx->active_task != NULL && curctx->active_task->valid_chkpt) {
        context_t *next = (curctx == &context_0 )? &context_1 : &context_0;
        next->active_task = curctx->active_task;
        next->fifo = curctx->fifo;
        next->active_evt = NULL;
        next->mode = TASK;
        curctx = next;
        restore_vol();
      }
      task_t * nextT = pop_task();
      if ( nextT != NULL) {
        context_t *next = (curctx == &context_0 )? &context_1 : &context_0;
        next->active_task = nextT;
        next->active_evt = NULL;
        next->mode = TASK;
        // TODO measure Vcap
        curctx = next;
        // Jump may involve resuming from a checkpoint
        __asm__ volatile ( // volatile because output operands unused by C
            "br %[nt]\n"
            : /* no outputs */
            : [nt] "r" (curctx->active_task->func)
        );
      }
    }

    // Else sleep to recharge
      // measure vcap
      // set compE interrupt/timer
      // sleep
    __disable_interrupt();
    int _flag = 0;
    while(TA0CCTL0 & CCIE) {
      __bis_SR_register(LPM3_bits + GIE);
      __disable_interrupt();
      _flag = 1;
    }
    __enable_interrupt();
    _flag = 0;
  }

  return 0; // Should not get here
}




void COMP_VBANK_ISR (void)
{
  PRINTF("in comp\r\n");
  DISABLE_LFCN_TIMER;// Just in case
  switch (__even_in_range(COMP_VBANK(IV), 0x4)) {
    case COMP_INTFLAG2(LIBCAPYBARA_VBANK_COMP_TYPE, IIFG):
        break;
    case COMP_INTFLAG2(LIBCAPYBARA_VBANK_COMP_TYPE, IFG):
      COMP_VBANK(INT) &= ~COMP_VBANK(IE);
      COMP_VBANK(CTL1) &= ~COMP_VBANK(ON);
      if (curctx->mode == EVENT) {
        capybara_shutdown();
      }
      else {
        // Checkpoint needs to be **immediately** followed by capybara
        // shutdown for this to work
        checkpoint();
        capybara_shutdown(); //TODO confirm this is a single instruction call
      }
      break;
  }
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


