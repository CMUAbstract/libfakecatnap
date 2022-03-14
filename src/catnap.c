// Contains the major pieces of catnap, the specifics live elsewhere, but the
// boot behavior, scheduler, timer and comparator interrupts all live here
#include <stdio.h>
#include <msp430.h>
#include "catnap.h"
#include "tasks.h"
#include "events.h"
#include "hw.h"
#include <libcapybara/board.h>
#include <libcapybara/power.h>
#include <libmsp/periph.h>
#include <libio/console.h>

__nv evt_list_t all_events = {.events = {0}, .cur_event = MAX_EVENTS+1}; 
__nv task_fifo_t all_tasks = {.tasks = {0}, .tsk_cnt = 0, .front = 0, .back = 0};

__nv context_t context_0 = {0};
__nv context_t context_1 = {0};
__nv context_t *curctx = &context_0;

// Main function to handle boot and kick us off
int main(void) {
  // Init capy
  capybara_init(); // TODO may want to slim down
  // Init timer
  __delay_cycles(8000000); //TODO remove!
  TA0CCTL0 |= CCIE;
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
      // Sleep done, update timers n'at
      // Event needs to start
      // Event done

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
      task_t * nextT = pop_task();
      if ( nextT != NULL) {
        context_t *next = (curctx == &context_0 )? &context_1 : &context_0;
        next->active_task = nextT;
        next->active_evt = NULL;
        next->mode = TASK;
        // TODO measure Vcap
        curctx = next;
        // Jump may involve resuming from a checkpoint
        if (curctx->active_task->valid_chkpt) {
          //resume_chkpt();
        }
        else {
          __asm__ volatile ( // volatile because output operands unused by C
              "br %[nt]\n"
              : /* no outputs */
              : [nt] "r" (curctx->active_task->func)
          );
        }
      }
    }

    // Else sleep to recharge
      // measure vcap
      // set compE interrupt/timer
      // sleep
    PRINTF("Sleeping for: %u\r\n",ticks_to_wait);
    __disable_interrupt();
    int _flag = 0;
    while(TA0CCTL0 & CCIE) {
      __bis_SR_register(LPM3_bits + GIE);
      __disable_interrupt();
      _flag = 1;
    }
    __enable_interrupt();
    PRINTF("AWAKE! %u\r\n",_flag);
    _flag = 0;
  }

  return 0; // Should not get here
}




void COMP_VBANK_ISR (void)
{
  PRINTF("in comp\r\n");
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
        //checkpoint();//TODO add checkpointing routine
        capybara_shutdown();
      }
      break;
  }
}

void __attribute__((interrupt(TIMER0_A0_VECTOR)))
timerISRHandler(void) {

	TA0CTL = MC_0 | TASSEL_1 | TACLR; // Stop timer
	TA0CCTL0 &= ~(CCIFG | CCIE); // Clear flag and stop int
	CEINT &= ~CEIE; // Disable comp interrupt for our sanity
/*--------------------------------------------------------*/
  ticks_waited = ticks_to_wait;
  // Not totally sure what code goes here yet
  PRINTF("Up!\r\n");
  __bic_SR_register_on_exit(LPM3_bits); //wake up
/*--------------------------------------------------------*/
	CEINT |= CEIE; // Re-enable checkpoint interrupt
  //TA0CCTL0 |= CCIE; // Re-enable timer int.
}


