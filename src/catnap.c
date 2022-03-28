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

#ifndef BIT_FLIP

#define BIT_FLIP(port,bit) \
	P##port##OUT |= BIT##bit; \
	P##port##DIR |= BIT##bit; \
	P##port##OUT &= ~BIT##bit; \

#endif

__nv evt_list_t all_events = {.events = {0}, .cur_event = MAX_EVENTS+1};
__nv task_fifo_t all_tasks = {.tasks = {0}};

volatile uint8_t test_flag = 0;
__nv volatile context_t context_0 = {NULL,NULL,CHARGING,0,&fifo_0};
__nv volatile context_t context_1 = {NULL,NULL,CHARGING,0,&fifo_0};
__nv volatile context_t *curctx = &context_0;

__nv uint8_t comp_e_flag = 1;
__nv uint8_t tasks_ok = 1;

// Main function to handle boot and kick us off
int main(void) {
  // Init capy
  capybara_init(); // TODO may want to slim down
  PRINTF("Boot\r\n");
  // Init timer
  //__delay_cycles(800000); //TODO take out
  //tasks_ok = 0;
  ENABLE_LFCN_TIMER;
  // Init comparator
  init_comparator();
  //PRINTF("start %x\r\n",CECTL2);

  P3SEL1 |= BIT5;
  P3SEL0 &= ~(BIT5);
  P3DIR |= BIT5;
  //__bis_SR_register(LPM3_bits | GIE);//sleep?
  //PRINTF("Wake!\r\n");
  //SET_LOWER_COMP(DEFAULT_MIN_THRES);//Happens in isr
  // on very first boot, pull in starter event
  add_event(&EVT_FCN_STARTER);
  // Run the whole shebang
  scheduler();

  return 0; // Should not get here!
}

void scheduler(void) {
  while(1) {
    LCFN_INTERRUPTS_DISABLE;
    // Why are we entering the loop?
    switch (curctx->mode) {
      case EVENT:
        vfinal = turn_on_read_adc();
        PRINTF("event! %x\r\n",curctx->active_task);
        /*TODO run feasibility*/
        //TODO fis this setting, we need to account for event time
        //ticks_waited = TA0R;
        break;
      case TASK:
      case SLEEPING:
      case CHARGING:
        PRINTF("not event! %x\r\n",curctx->mode);
        curctx->mode = CHARGING;
        break;
    }
    // Update feasibility, etc
    //PRINTF("ticks:%u\r\n",ticks_waited);
    update_event_timers(ticks_waited);
    // Schedule something next
    evt_t * nextE = pick_event();
    BIT_FLIP(1,1);
    // If event, measure vcap
    if (nextE != NULL) {
      //PRINTF("NEXTE\r\n");
      //__delay_cycles(80000); //TODO remove
      // Swap context
      context_t *next = (curctx == &context_0 )? &context_1 : &context_0;
      next->active_task = curctx->active_task;
      next->active_evt = nextE;
      next->mode = EVENT;
      // enable the absolute lowest threshold so we power off if there's a failure
      //PRINTF("Set low thresh %x\r\n",nextE);
      BIT_FLIP(1,1);
      BIT_FLIP(1,1);
      SET_LOWER_COMP(DEFAULT_MIN_THRES); //TODO may not need this on camaroptera
      vstart = turn_on_read_adc();
      LCFN_INTERRUPTS_ENABLE;
      curctx = next;
      // Jump
      __asm__ volatile ( // volatile because output operands unused by C
          "br %[nt]\n"
          : /* no outputs */
          : [nt] "r" (curctx->active_evt->evt)
      );
    }
    //PRINTF("Pick task?\r\n");
    //else { TODO can we actually leave this out?
      // First set up timers to wait for an event
    ticks_to_wait = get_next_evt_time();
    //PRINTF("To wait: %u\r\n",ticks_to_wait);
    start_timer(ticks_to_wait);
    // check if we're above
    uint16_t temp = turn_on_read_adc();
    //PRINTF("ADC: %u\r\n",temp);
    BIT_FLIP(1,5);
    BIT_FLIP(1,5);
    BIT_FLIP(1,5);
    if (temp > event_threshold && tasks_ok) { // Only pick a task if we're above the thresh
      //PRINTF("Set thresh %u, tasks ok: %u\r\n",event_threshold,tasks_ok);
      SET_LOWER_COMP(DEFAULT_LOWER_THRES); // Interrupt if we got below event thresh
      // Check if there's an active task, and jump to it if there is
      //PRINTF("chkpt: %u,active task? %x, \r\n",curctx->active_task->valid_chkpt,curctx->active_task);
      if (curctx->active_task != NULL && curctx->active_task->valid_chkpt) {
        context_t *next = (curctx == &context_0 )? &context_1 : &context_0;
        next->active_task = curctx->active_task;
        next->fifo = curctx->fifo;
        next->active_evt = NULL;
        BIT_FLIP(1,4);
        BIT_FLIP(1,4);
        BIT_FLIP(1,4);
        next->mode = TASK;
        PRINTF("1mode:%u\r\n",curctx->mode);
        LCFN_INTERRUPTS_ENABLE;
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
        BIT_FLIP(1,4);
        BIT_FLIP(1,4);
        BIT_FLIP(1,4);
        PRINTF("mode:%u\r\n",curctx->mode);
        LCFN_INTERRUPTS_ENABLE;
        curctx = next;
        __asm__ volatile ( // volatile because output operands unused by C
            "br %[nt]\n"
            : /* no outputs */
            : [nt] "r" (curctx->active_task->func)
        );
      }
    }
    //}
    BIT_FLIP(1,1);
    BIT_FLIP(1,1);
    BIT_FLIP(1,1);
    // Else sleep to recharge
    SET_MAX_UPPER_COMP(); // Set interrupt when we're fully charged too
    // measure vcap
    // set compE interrupt/timer
    // sleep
    LCFN_INTERRUPTS_ENABLE;
    //PRINTF("Sleeping for %u = %u",TA0CCR0,ticks_to_wait);
    __disable_interrupt();
    comp_e_flag = 1; // Flag to get us out
    while((TA0CCTL0 & CCIE) && comp_e_flag) { //TODO expand so we can get out with compE
      __bis_SR_register(LPM3_bits + GIE);
      __disable_interrupt();
    }

    __enable_interrupt();
    PRINTF("Wake!\r\n");
  }

  return; // Should not get here
}



__attribute__ ((interrupt(COMP_E_VECTOR)))
void COMP_VBANK_ISR (void) {
  //PRINTF("in comp\r\n");
  COMP_VBANK(INT) &= ~COMP_VBANK(IE);
  //COMP_VBANK(CTL1) &= ~COMP_VBANK(ON);
  DISABLE_LFCN_TIMER;// Just in case
  BIT_FLIP(1,4);
  BIT_FLIP(1,4);
  //PRINTF("comp\r\n");
  volatile int chkpt_flag = 0;
  if (!(CECTL2 & CERSEL)) {
  //PRINTF("comp3\r\n");
    if ((LEVEL_MASK & CECTL2) == level_to_reg[DEFAULT_MIN_THRES]) {
      //PRINTF("comp4\r\n");
      switch (curctx->mode) {
        case CHARGING:
        case EVENT:
        case SLEEPING:{
          BIT_FLIP(1,0);
          BIT_FLIP(1,0);
          BIT_FLIP(1,0);
          BIT_FLIP(1,0);
          ticks_waited = TA0R;
          capybara_shutdown();
          break;
        }
        case TASK:{
          PRINTF("Chkpt!\r\n");
          checkpoint();
          chkpt_flag = 1;
          if (chkpt_flag) {
            ticks_waited = TA0R;
            BIT_FLIP(1,0);
            BIT_FLIP(1,0);
            BIT_FLIP(1,0);
            curctx->mode = CHARGING;
            capybara_shutdown();
          }
          else {
            curctx->mode = TASK;
            //TODO: may need to add ticks here
            return;
          }
          break;
        }
        default:
          break;
      }
    }
    else { /* We hit the event bucket threshold*/ /* Stop task, jump back to scheduler so we start charging*/
    //PRINTF("comp2 %x\r\n",CECTL2);
      tasks_ok = 0;
      if (curctx->mode != TASK) {
        return; // If we're not in a task just return w/out a checkpoint
      }
      volatile int chkpt_flag;
      chkpt_flag = 0;
      PRINTF("Chkpt2!\r\n");
      checkpoint();
      chkpt_flag = 1;
      // Jump to scheduler
      if ( chkpt_flag == 1) {
        BIT_FLIP(1,0);
        BIT_FLIP(1,0);
        curctx->mode = CHARGING;
        ticks_waited = TA0R;
        __asm__ volatile ( // volatile because output operands unused by C
            "br %[nt]\n"
            : /* no outputs */
            : [nt] "r" (&scheduler)
        );
      }
      else {
        PRINTF("Ret2\r\n");
      }
    }
  }
  else {/*Charged to max*/
    /* allow tasks again */ 
    BIT_FLIP(1,4);
    BIT_FLIP(1,4);
    BIT_FLIP(1,4);
    BIT_FLIP(1,4);
    // SWitch to worrying about the minimum
    PRINTF("Set min!\r\n");
    SET_LOWER_COMP(DEFAULT_MIN_THRES);
    ticks_waited = TA0R;
    __bic_SR_register_on_exit(LPM3_bits); //wake up
    comp_e_flag = 0;
    tasks_ok = 1;
  }
  BIT_FLIP(1,0);
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
  PRINTF("Timer %u\r\n",curctx->mode);
/*--------------------------------------------------------*/
  BIT_FLIP(1,5);
  BIT_FLIP(1,5);
  BIT_FLIP(1,5);
  //PRINTF("timera0\r\n");
  // Record wait time
  ticks_waited = ticks_to_wait;
  TA0R = 0; // Not sure if we need this
  // If we're in a task, checkpoint it and move on
  if ( curctx->mode == TASK) {
    volatile int chkpt_flag = 0;
    BIT_FLIP(1,5);
    PRINTF("Chkpt3! %u\r\n",curctx->active_task->valid_chkpt);
    checkpoint();
    chkpt_flag = 1;
    // Jump to scheduler
    if ( chkpt_flag == 1) {
      __asm__ volatile ( // volatile because output operands unused by C
          "br %[nt]\n"
          : /* no outpums */
          : [nt] "r" (&scheduler)
      );
    }
    PRINTF("Ret3\r\n");
    return;
  }
  BIT_FLIP(1,5);
  __bic_SR_register_on_exit(LPM3_bits); //wake up
/*--------------------------------------------------------*/
	CEINT |= CEIE; // Re-enable checkpoint interrupt
  //TA0CCTL0 |= CCIE; // Re-enable timer int.
}
#else
void __attribute__((interrupt(PORT1_VECTOR)))
timerISRHandler(void) {
  P1IE &= ~BIT0;
  BIT_FLIP(1,5);
  BIT_FLIP(1,5);
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


