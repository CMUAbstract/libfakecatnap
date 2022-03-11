// Contains the major pieces of catnap, the specifics live elsewhere, but the
// boot behavior, scheduler, timer and comparator interrupts all live here


// Main function to handle boot and kick us off
int main(void) {
  // Init capy
  capybara_init(); // TODO may want to slim downa
  // Init timer
  TA0CCTL0 |= CCIE;
  // Init comparator
  init_comparator();
  // on very first boot, pull in starter event
  add_event(EVT_FCN_STARTER); 
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

    // Schedule something next
      // If event, measure vcap
      // Jump

    // Else sleep to recharge
      // measure vcap
      // set compE interrupt/timer
      // sleep
  }

  return 0; // Should not get here
}




void COMP_VBANK_ISR (void)
{
  switch (__even_in_range(COMP_VBANK(IV), 0x4)) {
    case COMP_INTFLAG2(LIBCAPYBARA_VBANK_COMP_TYPE, IIFG):
        break;
    case COMP_INTFLAG2(LIBCAPYBARA_VBANK_COMP_TYPE, IFG):
      COMP_VBANK(INT) &= ~COMP_VBANK(IE);
      COMP_VBANK(CTL1) &= ~COMP_VBANK(ON);
      if (curctx->curr_m == Atomic) {
        capybara_shutdown();
      }
      else {
        // Checkpoint needs to be **immediately** followed by capybara
        // shutdown for this to work
        checkpoint();
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
  
  // Not totally sure what code goes here yet

/*--------------------------------------------------------*/
	CEINT |= CEIE; // Re-enable checkpoint interrupt
  TA0CCTL0 |= CCIE; // Re-enable timer int.
}


