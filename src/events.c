#include "catnap.h"
#include <libio/console.h>
#include "events.h"
#include "culpeo.h"

int event_return() {
  evt_t *evt = curctx->active_evt;
  if (evt->valid != OFF) {
    evt->valid = DONE;
  }
  if (culpeo_profiling_flag) {
    profile_cleanup(evt);
    PRINTF("vmin/Vfinal %x ",evt);
    print_float(evt->V_min);
    print_float(evt->V_final);
    print_float(evt->V_safe);
    for(int i = 0; i < 2; i++) {
      __delay_cycles(8000000);
    }
    capybara_shutdown();
  }
  __asm__ volatile ( // volatile because output operands unused by C
      "br %[nt]\n"
      : /* no outputs */
      : [nt] "r" (&scheduler)
  );

  return 0;
}
