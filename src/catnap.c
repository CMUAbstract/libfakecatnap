// Contains the major pieces of catnap, the specifics live elsewhere, but the
// boot behavior, scheduler, timer and comparator interrupts all live here
#include <stdio.h>
#include <msp430.h>
#include <libcapybara/board.h>
#include <libcapybara/power.h>
#include <libmsp/periph.h>
#include <libio/console.h>
#include <float.h>
#include "print_util.h"
#include "catnap.h"
#include "checkpoint.h"
#include "culpeo.h"
#include "scheduler.h"
#include "events.h"
#include "tasks.h"
#include "hw.h"
#include "comp.h"

//#define LFCN_DBG

#define SLEEP_BITS LPM3_bits


#ifndef BIT_FLIP

#define BIT_FLIP(port,bit) \
	P##port##OUT |= BIT##bit; \
	P##port##DIR |= BIT##bit; \
	P##port##OUT &= ~BIT##bit; \

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
  BIT_FLIP(1,1);
  tasks_ok = 1;
  fail_count++;
  // Init timer
  ENABLE_LFCN_TIMER;
  // Init comparator
  init_comparator();
  // Code to make 3.5 COUT--> remove all BIT_FLIPS(3,5) to run this
  P3SEL1 |= BIT5;
  P3SEL0 &= ~(BIT5);
  P3DIR |= BIT5;

  // on very first boot, pull in starter event
  if (first_boot != MAGIC_NUMBER) {
    add_event(&EVT_FCN_STARTER);
    first_boot = MAGIC_NUMBER;
#ifndef LFCN_CONT_POWER
    capybara_shutdown();
#endif
  }
  // Hardware init after STARTER event
  app_hw_init();
  // Clear a couple variables after a reboot
  vfinal = 0;
  vstart = 0;//TODO should these be NV at all?
  // Run the whole shebang
  scheduler();

  return 0; // Should not get here!
}

void scheduler(void) {
  while(1) {
    LCFN_INTERRUPTS_INTERNAL_DISABLE;
    culpeo_V_t bucket_temp;
    switch (curctx->mode) {
      case EVENT:
        #ifdef CATNAP_FEASIBILITY
        vfinal = turn_on_read_adc(SCALER);
        calculate_energy_use(curctx->active_evt,vstart,vfinal);
        PRINTF("event! %u %u\r\n",cr_window_ready,cr_window_it);
        #else
        vfinal = turn_on_read_adc(SCALER);
        if (new_event) {
          bucket_temp = calc_culpeo_bucket();//TODO only run on changes
          new_event = 0;
        }
        PRINTF("event! %x ",curctx->fifo->tsk_cnt);
        PRINTF("\r\n");
        #endif
        break;
      case TASK:
      case SLEEPING:
      case CHARGING:
        PRINTF("\tnot event! %x\r\n",curctx->fifo->tsk_cnt);
        curctx->mode = CHARGING;
        break;
      default:
        while(1) {
          PRINTF("Shouldn't be here!!! %u\r\n",curctx->mode);
        }
        break;
    }
    // Update feasibility, etc
    update_event_timers(ticks_waited);
    ticks_waited = 0;
    // Schedule something next
    evt_t * nextE = pick_event();
    LFCN_DBG_PRINTF("post pick %x \r\n",curctx->active_evt->valid);
    // If event, measure vcap
    if (nextE != NULL) {
      LFCN_DBG_PRINTF("NEXTE\r\n");
      // Profile if needed
      //TODO make this smarter so we can re-profile easily
      #ifndef CATNAP_FEASIBILITY
      // Culpeo profiling
      #ifndef CULPEO_PROF_DISABLE
      if (nextE->V_final == 0 || nextE->V_min == 0) {
        profile_event(nextE);
        PRINTF("Should not be here!!!\r\n");
        //DOES NOT RETURN
      }
      #else
      //TODO this needs to be more intelligent
      nextE->V_final = 2.258;
      nextE->V_min = 2.128;
      Vmin = nextE->V_min;
      Vfinal = nextE->V_final;
      nextE->V_safe = calc_culpeo_vsafe();
      PRINTF("Vsafe: ");
      print_float(nextE->V_safe);
      #endif//PROF_DISABLE
      #endif//CATNAP_FEASIBILITY
      //TODO streamline the number of adc reads
      uint16_t test = turn_on_read_adc(SCALER);
      // Only run event if it's safe, otherwise charge
      uint16_t V_safe_int = (uint16_t)nextE->V_safe;
      PRINTF("Cur: %u, V_safe=%u\r\n",test,V_safe_int);
      #ifdef LFCN_CONT_POWER
      test = V_safe_int + 10;
      #endif
      if (test > V_safe_int) {
        // Swap context
        context_t *next = (curctx == &context_0 )? &context_1 : &context_0;
        next->active_task = curctx->active_task;
        next->fifo = curctx->fifo;
        next->active_evt = nextE;
        next->mode = EVENT;
        // enable the absolute lowest threshold so we power off if there's a failure
        LFCN_DBG_PRINTF("Set low thresh %x\r\n",DEFAULT_MIN_THRES);
        SET_LOWER_COMP(DEFAULT_MIN_THRES);
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
      else {
        tasks_ok = 0; // No tasks for now
        curctx->active_evt->valid = STARTED;
      }
    }
    // First set up timers to wait for an event
    ticks_to_wait = get_next_evt_time();
    LFCN_DBG_PRINTF("To wait: %u\r\n",ticks_to_wait);
    start_timer(ticks_to_wait);
    // check if we're above
    #ifndef LFCN_CONT_POWER
    uint16_t temp = turn_on_read_adc(SCALER);
    #else
    uint16_t temp = LFCN_STARTER_THRESH + 10;
    #endif
    //LFCN_DBG_PRINTF("ADC: %u %u %u\r\n",temp,event_threshold,tasks_ok);
    LFCN_DBG_PRINTF("Pick task? %u %u %u\r\n",tasks_ok,temp,event_threshold);
    if (temp > event_threshold && tasks_ok) { // Only pick a task if we're above the thresh
      LFCN_DBG_PRINTF("Set thresh %u, lvl %u\r\n",event_threshold,lower_thres);
      SET_LOWER_COMP(lower_thres); // Interrupt if we got below event thresh
      // Check if there's an active task, and jump to it if there is
      if (curctx->active_task != NULL) {
        context_t *next = (curctx == &context_0 )? &context_1 : &context_0;
        next->active_task = curctx->active_task;
        next->fifo = curctx->fifo;
        next->active_evt = NULL;
        next->mode = TASK;
        LFCN_DBG_PRINTF("1mode:%u %u\r\n",curctx->mode,curctx->active_task->valid_chkpt);
        LCFN_INTERRUPTS_ENABLE;
        curctx = next;
        if (next->active_task->valid_chkpt) {
          BIT_FLIP(1,1);
          restore_vol(); // Jump to new task
        }
        else {
          __asm__ volatile ( // volatile because output operands unused by C
              "br %[nt]\n"
              : /* no outputs */
              : [nt] "r" (curctx->active_task->func)
          );
        }
      }
      task_t * nextT = pop_task(); //Start changes
      if ( nextT != NULL) {
        context_t *next = (curctx == &context_0 )? &context_1 : &context_0;
        next->active_task = nextT;
        next->active_evt = NULL;
        next->mode = TASK;
        update_task_fifo(next); // Commit changes since we need to wait until the
                                // task is latched.
        LFCN_DBG_PRINTF("mode:%u\r\n",curctx->mode);
        LCFN_INTERRUPTS_ENABLE;
        curctx = next;
        __asm__ volatile ( // volatile because output operands unused by C
            "br %[nt]\n"
            : /* no outputs */
            : [nt] "r" (curctx->active_task->func)
        );
      }
      else {
      // We only wind up here if there are no active tasks
        if (ticks_to_wait == 0) {
          // No events active either
          ticks_to_wait = DEFAULT_WAIT;//TODO a hack to stop spiraling...
          start_timer(ticks_to_wait);
          LFCN_DBG_PRINTF("Going to sleep now\r\n");
        }
      }
    }
    // Got to sleep
    curctx->mode = SLEEPING;
    if ((TA0CTL & MC) == MC_0) {
      start_timer_running();//Start timer if it's not on
      LFCN_DBG_PRINTF("backup timer on!\r\n");
    }
    else {
      LFCN_DBG_PRINTF("Ta0ctl:%x\r\n",TA0CTL);
    }
    // Grab starting voltage if we're sleeping
    #ifdef CATNAP_FEASIBILITY
    if (curctx->mode != TASK) {
      v_charge_start = turn_on_read_adc(SCALER);
      t_start = TA0R;//TODO is the timer running here?
      LFCN_DBG_PRINTF("Ta0r: %x\r\n",TA0R);
    }
    #endif
    // Else sleep to recharge
    // set compE interrupt/timer
    BIT_FLIP(1,1);
    BIT_FLIP(1,1);
    BIT_FLIP(1,1);
    if(curctx->fifo->tsk_cnt == 0 && curctx->active_task == NULL) {
      LFCN_DBG_PRINTF("Set nothing!\r\n");
    } else {
      LFCN_DBG_PRINTF("Upper thres: %x\r\n",max_thres);
      SET_MAX_UPPER_COMP(); // Set interrupt when we're fully charged too
    }
    // sleep
    LFCN_DBG_PRINTF("Sleep\r\n");
    comp_e_flag = 1; // Flag to get us out
    LCFN_INTERRUPTS_ENABLE;
    __disable_interrupt();
      BIT_FLIP(1,1);
      BIT_FLIP(1,1);
    while((TA0CCTL0 & CCIE) && comp_e_flag) {
      BIT_FLIP(1,1);
      BIT_FLIP(1,1);
      __bis_SR_register(SLEEP_BITS + GIE);
      BIT_FLIP(1,1);
      BIT_FLIP(1,1);
      __disable_interrupt();
    }
    LFCN_DBG_PRINTF("Wake\r\n");
    #ifdef CATNAP_FEASIBILITY
    if (curctx->mode != TASK) {
      v_charge_end = turn_on_read_adc(SCALER);
      if (!comp_e_flag) {
        t_end = TA0R;//we were woken up by the comparator
        LFCN_DBG_PRINTF("after sleep! %x\r\n",TA0R);
      }
      else {
        t_end = ticks_waited - t_start; //woken up by timer which clobbered ta0r
        LFCN_DBG_PRINTF("after sleep timer! %x %x %x\r\n",t_end,ticks_waited,t_start);
      }
      calculate_charge_rate(t_end,t_start);
      t_end = 0;// this is ok because these vars are volatile
      t_start = 0;
    }
    #endif
    __enable_interrupt();
  }

  return; // Should not get here
}

uint16_t store = 0;

__attribute__ ((interrupt(COMP_E_VECTOR)))
void COMP_VBANK_ISR (void) {
  BIT_FLIP(1,1);
  BIT_FLIP(1,1);
  store = CEINT;
  if (CEINT | CERDYIFG) {
    if (!(CEINT & CEIFG) && !(CEINT & CEIIFG)) {
      BIT_FLIP(1,1);
      BIT_FLIP(1,1);
      CEINT &= ~(CERDYIFG | CERDYIE);
      return;
    }
      CEINT &= ~(CERDYIFG | CERDYIE);
  }
  if (ISR_DISABLE) {
    comp_violation = 1;
    CEINT = 0;
    return;
  }
  comp_violation = 0;
  CEINT = 0;
  DISABLE_LFCN_TIMER;// Just in case
  LFCN_DBG_PRINTF("in comp %x,\r\n",store);
  volatile int chkpt_flag = 0;
  if (!(CECTL2 & CERSEL)) {
    if ((LEVEL_MASK & CECTL2) == level_to_reg[DEFAULT_MIN_THRES]) {
      switch (curctx->mode) {
        case CHARGING:
        case EVENT:
        case SLEEPING:{
          PRINTF("shutdown evt %u\r\n",curctx->mode);
          ticks_waited = TA0R;
          capybara_shutdown();
          break;
        }
        case TASK:{
          checkpoint();
          chkpt_flag = 1;
          if (chkpt_flag) {
            ticks_waited = TA0R;
            PRINTF("shutdown tsk\r\n");
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
    LFCN_DBG_PRINTF("comp2 %x\r\n",CECTL2);
      tasks_ok = 0;
      if (curctx->mode != TASK) {
        LFCN_DBG_PRINTF("Mode: %u\r\n",curctx->mode);
        return; // If we're not in a task just return w/out a checkpoint
      }
      if (curctx->active_task->valid_chkpt) {
        LFCN_DBG_PRINTF("Saving us!!\r\n");
        return;
      }
      volatile int chkpt_flag;
      chkpt_flag = 0;
      LFCN_DBG_PRINTF("Chkpt2!\r\n");
      checkpoint();
      chkpt_flag = 1;
      // Jump to scheduler
      if ( chkpt_flag == 1) {
        curctx->mode = CHARGING;
        ticks_waited = TA0R;
        __asm__ volatile ( // volatile because output operands unused by C
            "br %[nt]\n"
            : /* no outputs */
            : [nt] "r" (&scheduler)
        );
      }
      else {
        CEINT = 0;
        
        BIT_FLIP(1,1);
        LFCN_DBG_PRINTF("Ret2 from chkpt %x\r\n",curctx->active_task);
        curctx->active_task->valid_chkpt = 0;
      }
    }
  }
  else {/*Charged to max*/
    /* allow tasks again */ 
    // Switch to worrying about the minimum
    LFCN_DBG_PRINTF("Set min! %x\r\n",LEVEL_MASK & CECTL2);
    SET_LOWER_COMP(DEFAULT_MIN_THRES);
    ticks_waited = TA0R;
    __bic_SR_register_on_exit(SLEEP_BITS); //wake up
    comp_e_flag = 0;
    tasks_ok = 1;
  }
  COMP_VBANK(INT) |= COMP_VBANK(IE);
  ENABLE_LFCN_TIMER;// Just in case
  LFCN_DBG_PRINTF("here\r\n");
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
  ticks_waited = ticks_to_wait;
  TA0R = 0; // Not sure if we need this
  // If we're in a task, checkpoint it and move on
  if ( curctx->mode == TASK) {
    LFCN_DBG_PRINTF("Chkpt3! %u\r\n",curctx->active_task->valid_chkpt);
    if (curctx->active_task->valid_chkpt) {
      LFCN_DBG_PRINTF("Saving us timer\r\n");
      return;
    }
    volatile int chkpt_flag = 0;
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
    BIT_FLIP(1,1);
    LFCN_DBG_PRINTF("Ret3 from chkpt\r\n");
    curctx->active_task->valid_chkpt = 0;
    return;
  }
  __bic_SR_register_on_exit(SLEEP_BITS); //wake up
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
  __bic_SR_register_on_exit(SLEEP_BITS); //wake up
  P1IE |= BIT0;
}
#endif


