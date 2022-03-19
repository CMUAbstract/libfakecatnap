#include "catnap.h"
#include <libio/console.h>

int event_return() {
  if (curctx->active_evt->valid != OFF) {
    curctx->active_evt->valid = DONE;
    //PRINTF("evt done: %x",(uint16_t)curctx->active_evt);
  }
  __asm__ volatile ( // volatile because output operands unused by C
      "br %[nt]\n"
      : /* no outputs */
      : [nt] "r" (&scheduler)
  );

  return 0;
}
