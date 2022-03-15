#include <stdio.h>
#include <msp430.h>
#include <libmsp/periph.h>
#include <libio/console.h>
#include <libmsp/mem.h>
#include "checkpoint.h"
#include "catnap.h"

//Debug logging, not checkpoint logging
#define LFCN_LOGGING 0

__nv unsigned chkpt_finished = 0;

__nv unsigned __numBoots = 0;
__nv unsigned true_first = 1;

__nv unsigned int * SAVED_PC = (unsigned int *) 0xFB80;
__nv unsigned int* CURR_SP = (unsigned int *) 0xFBC4;
__nv unsigned int* DEBUG_LOC = (unsigned int *) 0xFBC0;


void save_stack();
inline void  restore_stack() __attribute__((always_inline));

extern int __stack; //should be the lowest addr in the stack;

/*Function that copies the stack to nvmem*/
void save_stack()
{
  uint16_t *stack_start = (uint16_t*)(&__stack);
#if LFCN_LOGGING
  PRINTF("save: stack is from %u to %u\r\n", stack_start, *CURR_SP);
#endif
  //PRINTF("ss %u sp %u\r\n", stack_start, *CURR_SP);
  unsigned int* save_point = 0xFBC8;
  // Had to reverse direction because MSP430 stack grows down
  if ((unsigned int)stack_start - (unsigned int)(*(CURR_SP)) > 0x400) {
    PRINTF("Stack overflow!\r\n");
  }
  while( (unsigned int)stack_start > *CURR_SP) {
    *save_point = *stack_start;
    save_point++;
    stack_start--;
    //PRINTF("%u = %u\r\n",*stack_start, *save_point);
  }
 // PRINTF("save pt %u\r\n", save_point);
}

/*Function that restores the stack from nvmem
  Note: this NEEDS to be inlined so that the return address pushed to the stack
  doesn't get clobbered. I.e. if the function is inlined there is no return
  address to worry about.*/
void inline restore_stack()
{
  uint16_t *stack_start = (uint16_t*)(&__stack);
  //PRINTF("restore: stack is from %u to %u\r\n", stack_start, *((unsigned int*)CURR_SP));
#if LFCN_LOGGING
  PRINTF("restore: stack is from %u to %u\r\n", stack_start, *CURR_SP);
#endif
  unsigned int* save_point = 0xFBC8;
  while( (unsigned)stack_start > *CURR_SP) {
    *stack_start = *save_point;
    save_point++;
    stack_start--;
    //PRINTF("%u = %u\r\n",*stack_start, *save_point);
  }
}

void checkpoint() {
  //save the registers
  //start with the status reg
  __asm__ volatile ("MOV R2, &0x0FB88");
  // Move this one so we have it
  __asm__ volatile ("MOVX.A R4, &0x0000FB90");

  // This is R1 (SP), but it will be R0 (PC) on restore (since we don't want to
  // resume in chckpnt obvi, but after the return)
  // Need to add 4 to checkpoint 0(R1) so we skip "capybara_shutdown" call in
  // interrupt handler when we restore, but subtract that 4 so we do call
  // capybara shutdown.
  // TODO This means we need a sacrificial instruction after every call to
  // checkpoint under different circumstances... gotta make it less janky
  __asm__ volatile ("MOVX.A 0(R1), R4"); // move old pc to r4
  __asm__ volatile ("ADDA #4, R4"); // inc
  __asm__ volatile ("MOVX.A R4, &0x0000FB80"); //Set pc for after reboot
  __asm__ volatile ("SUBA #4, R4"); // dec
  __asm__ volatile ("MOVX.A R4, 0(R1)"); // write back to stack
  __asm__ volatile ("MOVX.A R1, &0x0000FBC4"); //Curr sp
  __asm__ volatile ("ADDA #4, R1"); // inc cur stack to get rid of calla push
  __asm__ volatile ("MOVX.A R1, &0x0000FB84"); // write to mem
  __asm__ volatile ("SUBA #4, R1"); // put back to waht it should be

  #if LFCN_LOGGING
  unsigned i  = 0;
  while(i < 50) {
    PRINTF("checkpoint: first batch done\r\n");
    PRINTF("old stack val %u new val %u\r\n", *((unsigned int*) 0x0FB80),
    *((unsigned int*)0x0FB84));
    i++;
  }
  i = 0;
#endif
  //R3 is constant generator, doesn't need to be restored

  //the rest are general purpose regs
  __asm__ volatile ("MOVX.A R5, &0x0FB94");
  __asm__ volatile ("MOVX.A R6, &0x0FB98");
  __asm__ volatile ("MOVX.A R7, &0x0fb9c");

  __asm__ volatile ("MOVX.A R8, &0x0FBA0");
  __asm__ volatile ("MOVX.A R9, &0x0FBA4");
  __asm__ volatile ("MOVX.A R10, &0x0FBA8");
  __asm__ volatile ("MOVX.A R11, &0x0fbac");

  __asm__ volatile ("MOVX.A R12, &0x0FBB0");
  __asm__ volatile ("MOVX.A R13, &0x0FBB4");
  __asm__ volatile ("MOVX.A R14, &0x0FBB8");
  __asm__ volatile ("MOVX.A R15, &0x0fbbc");

  save_stack();
  __asm__ volatile ("MOVX.A &0x0FB90, R4");
  curctx->active_task->valid_chkpt = 1;
  chkpt_finished = 1;
}


void restore_vol() {
  //restore the registers

  if (!chkpt_finished) {
    PRINTF("Error! Broken checkpoint\r\n");
    return; // Error
    }
  PRINTF("Restoring\r\n");
  unsigned i = 0;
  chkpt_finished = 0;
  __numBoots +=1;

  __asm__ volatile ("MOVX.A R1, &0x0000FBC0");
  __asm__ volatile ("MOVX.A &0x0FB84, R1");
  restore_stack();

#if LFCN_LOGGING
  while(i < 50) {
    PRINTF("sp done\r\n");
    i++;
  }
  i = 0;
#endif
  __asm__ volatile ("MOVX.A &0x0FB90, R4");
  __asm__ volatile ("MOVX.A &0x0FB94, R5");
  __asm__ volatile ("MOVX.A &0x0FB98, R6");
  __asm__ volatile ("MOVX.A &0x0fb9c, R7");

  #if LFCN_LOGGING
  while(i < 50) {
    PRINTF("first batch done\r\n");
    PRINTF("old stack val %u new val %u\r\n", *(DEBUG_LOC), *((unsigned int*)0xFB84));
    i++;
  }
  i = 0;
  #endif
  __asm__ volatile ("MOVX.A &0x0FBB0, R12");
  __asm__ volatile ("MOVX.A &0x0FBB4, R13");
  __asm__ volatile ("MOVX.A &0x0FBB8, R14");
  __asm__ volatile ("MOVX.A &0x0fbbc, R15");

#if LCFN_LOGGING
  while(i < 50) {
    PRINTF("third batch done\r\n");
    i++;
  }
  i = 0;
#endif
  __asm__ volatile ("MOVX.A &0x0FBA0, R8");
  __asm__ volatile ("MOVX.A &0x0FBA4, R9");
  __asm__ volatile ("MOVX.A &0x0FBA8, R10");
  __asm__ volatile ("MOVX.A &0x0fbac, R11");

  //last but not least, move regs 2 and 0
  __asm__ volatile ("MOVX.A &0x0FB84, R1");
  __asm__ volatile ("MOVX.A &0x0FB88, R2");
  __asm__ volatile ("MOVX.A &0x0FB80, R0");
  //pc has been changed, can't do anything here!!
}

