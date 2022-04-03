// Contains the major pieces of catnap, the specifics live elsewhere, but the
// boot behavior, scheduler, timer and comparator interrupts all live here
#include <stdio.h>
#include <msp430.h>
#include <libcapybara/board.h>
#include <libcapybara/power.h>
#include <libmsp/periph.h>
#include <libio/console.h>
#include <float.h>
#include "catnap.h"
#include "checkpoint.h"
#include "culpeo.h"
#include "scheduler.h"
#include "events.h"
#include "tasks.h"
#include "hw.h"
#include "comp.h"

//#define LFCN_DBG

#ifdef LFCN_DBG
  #define LFCN_DBG_PRINTF PRINTF
#else
  #define LFCN_DBG_PRINTF(...)
#endif

#ifndef BIT_FLIP

#define BIT_FLIP(port,bit) \
	P##port##OUT |= BIT##bit; \
	P##port##DIR |= BIT##bit; \
	P##port##OUT &= ~BIT##bit; \

#endif


#ifdef CATNAP_FEASIBILITY
#define SCALER 100
#else
#define SCALER 1000
#endif

#define MAGIC_NUMBER 0xab03

__nv uint16_t first_boot = 0;
__nv evt_list_t all_events = {.events = {0}, .cur_event = MAX_EVENTS+1};
__nv task_fifo_t all_tasks = {.tasks = {0}};

volatile uint8_t test_flag = 0;
__nv volatile context_t context_0 = {NULL,NULL,CHARGING,0,&fifo_0};
__nv volatile context_t context_1 = {NULL,NULL,CHARGING,0,&fifo_0};
__nv volatile context_t *curctx = &context_0;

__nv uint8_t comp_e_flag = 1;
volatile __nv uint8_t tasks_ok = 1;

__nv uint8_t fail_count = 0;


// Main function to handle boot and kick us off
int main(void) {
  // Init capy
  capybara_init(); // TODO may want to slim down
  PRINTF("Boot\r\n");
  fail_count++;
  app_hw_init();
  // Init timer
  ENABLE_LFCN_TIMER;
  // Init comparator
  init_comparator();
  // Code to make 3.5 COUT--> remove all BIT_FLIPS(3,5) to run this
  /*P3SEL1 |= BIT5;
  P3SEL0 &= ~(BIT5);
  P3DIR |= BIT5;*/
  // on very first boot, pull in starter event
  if (first_boot != MAGIC_NUMBER) {
    add_event(&EVT_FCN_STARTER);
    first_boot = MAGIC_NUMBER;
    capybara_shutdown();
  }
  // Clear a couple variables after a reboot
  vfinal = 0;
  vstart = 0;//TODO should these be NV at all?
  // Run the whole shebang
  scheduler();

  return 0; // Should not get here!
}

void scheduler(void) {
  while(1) {
    LCFN_INTERRUPTS_DISABLE;
    culpeo_V_t bucket_temp;
    switch (curctx->mode) {
      case EVENT:
        #ifdef CATNAP_FEASIBILITY
        vfinal = turn_on_read_adc(SCALER);
        calculate_energy_use(curctx->active_evt,vstart,vfinal);
        PRINTF("event! %u %u\r\n",vstart,vfinal);
        #else
        bucket_temp = calc_culpeo_bucket();//TODO only run on changes
        PRINTF("event! ");
        print_float(curctx->active_evt->V_final);
        print_float(curctx->active_evt->V_min);
        PRINTF("\r\n");
        #endif
        //TODO fix this setting, we need to account for event time
        //ticks_waited = TA0R;
        break;
      case TASK:
      case SLEEPING:
      case CHARGING:
        PRINTF("not event! %x\r\n",curctx->active_task);
        PRINTF("\tnot event! %x\r\n",ticks_waited);
        curctx->mode = CHARGING;
        break;
    }
    //dump_events();
    // Update feasibility, etc
    //PRINTF("ticks:%u\r\n",ticks_waited);
    update_event_timers(ticks_waited);
    ticks_waited = 0;
    // Schedule something next
    evt_t * nextE = pick_event();
      /*PRINTF("post pick %x ",curctx->active_evt);
      print_float(curctx->active_evt->V_final);
      print_float(curctx->active_evt->V_min);
      PRINTF("\r\n");*/
    BIT_FLIP(1,1);
    // If event, measure vcap
    if (nextE != NULL) {
      LFCN_DBG_PRINTF("NEXTE\r\n");
      // Profile if needed
      //TODO make this smarter so we can re-profile easily
      #ifndef CATNAP_FEASIBILITY
      // Culpeo profiling
      if (nextE->V_final == 0 || nextE->V_min == 0) {
        profile_event(nextE);
        PRINTF("Should not be here!!!\r\n");
        //DOES NOT RETURN
      }
      #endif
      // Swap context
      context_t *next = (curctx == &context_0 )? &context_1 : &context_0;
      next->active_task = curctx->active_task;
      next->active_evt = nextE;
      next->mode = EVENT;
      // enable the absolute lowest threshold so we power off if there's a failure
      LFCN_DBG_PRINTF("Set low thresh %x\r\n",nextE);
      BIT_FLIP(1,1);
      BIT_FLIP(1,1);
      SET_LOWER_COMP(DEFAULT_MIN_THRES); //TODO may not need this on camaroptera
      #ifdef CATNAP_FEASIBILITY
      vstart = turn_on_read_adc(SCALER);
      #endif
      LCFN_INTERRUPTS_ENABLE;
      curctx = next;
      LFCN_DBG_PRINTF("Jump!\r\n");
      // Jump
      __asm__ volatile ( // volatile because output operands unused by C
          "br %[nt]\n"
          : /* no outputs */
          : [nt] "r" (curctx->active_evt->evt)
      );
    }
    LFCN_DBG_PRINTF("Pick task? %u\r\n",tasks_ok);
    // First set up timers to wait for an event
    ticks_to_wait = get_next_evt_time();
    //LFCN_DBG_PRINTF("To wait: %u\r\n",ticks_to_wait);
    start_timer(ticks_to_wait);
    // check if we're above
    #ifndef LFCN_CONT_POWER
    uint16_t temp = turn_on_read_adc(SCALER);
    #else
    uint16_t temp = LFCN_STARTER_THRESH + 10;
    #endif
    //LFCN_DBG_PRINTF("ADC: %u %u %u\r\n",temp,event_threshold,tasks_ok);
    BIT_FLIP(1,5);
    BIT_FLIP(1,5);
    BIT_FLIP(1,5);
    if (temp > event_threshold && tasks_ok) { // Only pick a task if we're above the thresh
      LFCN_DBG_PRINTF("Set thresh %u, tasks ok: %u\r\n",event_threshold,tasks_ok);
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
        //LFCN_DBG_PRINTF("1mode:%u\r\n",curctx->mode);
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
        //LFCN_DBG_PRINTF("mode:%u\r\n",curctx->mode);
        LCFN_INTERRUPTS_ENABLE;
        curctx = next;
        __asm__ volatile ( // volatile because output operands unused by C
            "br %[nt]\n"
            : /* no outputs */
            : [nt] "r" (curctx->active_task->func)
        );
      }
    }
    // Got to sleep
    curctx->mode = SLEEPING;
    BIT_FLIP(1,1);
    BIT_FLIP(1,1);
    BIT_FLIP(1,1);
    // Grab starting voltage if we're sleeping
    #ifdef CATNAP_FEASIBILITY
    if (curctx->mode != TASK) {
      v_charge_start = turn_on_read_adc(SCALER);
      t_start = TA0R;//TODO is the timer running here?
      BIT_FLIP(3,5);
    }
    #endif
    // Else sleep to recharge
    SET_MAX_UPPER_COMP(); // Set interrupt when we're fully charged too
    // measure vcap
    // set compE interrupt/timer
    // sleep
    LCFN_INTERRUPTS_ENABLE;
    //LFCN_DBG_PRINTF("Sleeping for %u = %u",TA0CCR0,ticks_to_wait);
    __disable_interrupt();
    comp_e_flag = 1; // Flag to get us out
    BIT_FLIP(1,1);
    BIT_FLIP(1,1);
    BIT_FLIP(1,1);
    BIT_FLIP(1,1);
    BIT_FLIP(1,1);
    BIT_FLIP(1,1);
    while((TA0CCTL0 & CCIE) && comp_e_flag) {
      __bis_SR_register(LPM3_bits + GIE);
      //LFCN_DBG_PRINTF("Woo!\r\n");
      __disable_interrupt();
    }
    //LFCN_DBG_PRINTF("Done?");
    BIT_FLIP(1,1);
    BIT_FLIP(1,1);
    BIT_FLIP(1,1);
    BIT_FLIP(1,1);
    BIT_FLIP(1,1);
    #ifdef CATNAP_FEASIBILITY
    if (curctx->mode != TASK) {
      v_charge_end = turn_on_read_adc(SCALER);
      if (!comp_e_flag) {
        t_end = TA0R;//we were woken up by the comparator
      }
      else {
        t_end = ticks_waited - t_start; //woken up by timer which clobbered ta0r
      }
      BIT_FLIP(3,5);
      BIT_FLIP(3,5);
      calculate_charge_rate(t_end,t_start);
      //PRINTF("Got t_end!\r\n");
      t_end = 0;// this is ok because these vars are volatile
      t_start = 0;
    }
    #endif
    __enable_interrupt();
    //LFCN_DBG_PRINTF("Wake!\r\n");
  }

  return; // Should not get here
}



__attribute__ ((interrupt(COMP_E_VECTOR)))
void COMP_VBANK_ISR (void) {
  COMP_VBANK(INT) &= ~COMP_VBANK(IE);
  //COMP_VBANK(CTL1) &= ~COMP_VBANK(ON);
  DISABLE_LFCN_TIMER;// Just in case
  BIT_FLIP(1,4);
  BIT_FLIP(1,4);
  //PRINTF("in comp\r\n");
  volatile int chkpt_flag = 0;
  if (!(CECTL2 & CERSEL)) {
  //LFCN_DBG_PRINTF("comp3\r\n");
    if ((LEVEL_MASK & CECTL2) == level_to_reg[DEFAULT_MIN_THRES]) {
      //LFCN_DBG_PRINTF("comp4\r\n");
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
          //LFCN_DBG_PRINTF("Chkpt!\r\n");
          checkpoint();
          chkpt_flag = 1;
          if (chkpt_flag) {
            ticks_waited = TA0R;
            BIT_FLIP(1,0);
            BIT_FLIP(1,0);
            BIT_FLIP(1,0);
            curctx->mode = CHARGING;
            //capybara_shutdown();
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
    //LFCN_DBG_PRINTF("comp2 %x\r\n",CECTL2);
      tasks_ok = 0;
      if (curctx->mode != TASK) {
        return; // If we're not in a task just return w/out a checkpoint
      }
      volatile int chkpt_flag;
      chkpt_flag = 0;
      LFCN_DBG_PRINTF("Chkpt2!\r\n");
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
        LFCN_DBG_PRINTF("Ret2\r\n");
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
    //LFCN_DBG_PRINTF("Set min! %x\r\n",LEVEL_MASK & CECTL2);
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
  //LFCN_DBG_PRINTF("Timer %u\r\n",curctx->mode);
/*--------------------------------------------------------*/
  BIT_FLIP(1,5);
  BIT_FLIP(1,5);
  BIT_FLIP(1,5);
  BIT_FLIP(1,5);
  // Record wait time
  ticks_waited = ticks_to_wait;
  TA0R = 0; // Not sure if we need this
  // If we're in a task, checkpoint it and move on
  //PRINTF("timera0 %u\r\n",(TA0CCTL0 & CCIE));
  if ( curctx->mode == TASK) {
    LFCN_DBG_PRINTF("Chkpt3! %u\r\n",curctx->active_task->valid_chkpt);
    volatile int chkpt_flag = 0;
    BIT_FLIP(1,5);
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
    LFCN_DBG_PRINTF("Ret3\r\n");
    return;
  }
  BIT_FLIP(1,5);
  __bic_SR_register_on_exit(LPM3_bits); //wake up
/*--------------------------------------------------------*/
#ifndef LFCN_CONT_POWER
	CEINT |= CEIE; // Re-enable checkpoint interrupt
#endif
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


