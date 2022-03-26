#include "catnap.h"
#include <libio/console.h>

int event_return() {
  PRINTF("evt done: \r\n");
  if (curctx->active_evt->valid != OFF) {
    curctx->active_evt->valid = DONE;
  }
  __asm__ volatile ( // volatile because output operands unused by C
      "br %[nt]\n"
      : /* no outputs */
      : [nt] "r" (&scheduler)
  );

  return 0;
}
