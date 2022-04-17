#ifndef _LIBFAKECATNAP_EVENTS_H_
#define _LIBFAKECATNAP_EVENTS_H_
typedef enum evt_valid_ {
  OFF,
  WAITING,
  RDY,
  DONE,
  STARTED
} evt_valid_t;

typedef enum periodicity_ {
  PERIODIC,
  SPORADIC,
  BURSTY
} periodicity_t;

typedef void (evt_func_t)(void);

typedef struct evt_ {
  // Func pointer
  evt_func_t *evt;
  // Starting voltage, may need to be a table
  uint32_t energy;
  uint32_t charge_time;
  uint16_t burst_num;
  int16_t time_rdy;
  evt_valid_t valid;
  uint16_t period;
  uint8_t periodic;
  uint8_t no_profile;
  uint16_t V_safe;
  float V_final;
  float V_min;
} evt_t;

#define EVENT(name) EVT_ ## name

#define DEC_EVT(name, func, per,periodicity)\
  __nv evt_t EVT_ ## name  =  \
  { .evt = &func, \
    .period = per, \
    .periodic = periodicity, \
    .burst_num = 1, /*TODO switch back to 1*/\
    .time_rdy = per, \
    .valid = WAITING, \
    .no_profile = 0, \
    .V_safe = 0, \
    .V_min = 0, \
    .V_final = 0 \
  }

int event_return();

#define EVENT_RETURN() \
  event_return();


#define INT_RETURN \
  __asm__ volatile ("br %[nt]\n" : : [nt] "r" (&scheduler));
  

#endif
