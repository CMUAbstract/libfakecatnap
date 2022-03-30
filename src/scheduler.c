#include "catnap.h"
#include "scheduler.h"
#include "comp.h"
#include <libio/console.h>

volatile __nv uint32_t cr_window[CR_WINDOW_SIZE] = {0xffffffff,0xffffffff,0xffffffff};
volatile __nv uint8_t cr_window_it = 0;
volatile __nv uint8_t cr_window_ready = 0;

uint16_t v_charge_start;
uint16_t v_charge_end;

void update_comp()
{
	uint32_t energy_budget = level_to_volt[lower_thres]
		* level_to_volt[lower_thres] - V_OFF_SQUARED;
	uint32_t worst_case_energy = 0;
	for (unsigned i = 0; i < MAX_EVENTS; ++i) {
		// For bursty, this gets a bit conservative
		evt_t* e = all_events.events[i];
    if ( e == NULL || e->valid == OFF) {
      continue;
    }
    //PRINTF("e: %x, energy: %u\r\n",e,e->energy);
		worst_case_energy += e->energy * e->burst_num;
	}
	//PRINTF("Budget: %x %x\r\n", (unsigned)(energy_budget >> 16),
  //(unsigned)(energy_budget & 0xFFFF));
	//PRINTF("Worst: %x %x\r\n", (unsigned)(worst_case_energy >> 16),
  //(unsigned)(worst_case_energy & 0xFFFF));
	if (worst_case_energy > energy_budget) {
		PRINTF("Comp: Event not schedulable!\r\n");
	}
}

void calculate_energy_use(evt_t* e, unsigned v_before_event,
		unsigned v_after_event)
{
	//PRINTF("Energy use vs: %u, ve: %u\r\n", v_before_event, v_after_event);
	uint32_t used_E;
	if (v_before_event < v_after_event) {
		used_E = 0;
	} else {
		used_E = v_before_event*v_before_event
			- v_after_event*v_after_event;
	}
	// Always remember the worst-case energy
	if (used_E > e->energy) {
		// Update all reservedE with the same param with current
		//for (unsigned i = 0; i < MODE_NUM; ++i) {
      // If no param, than update all reservedE
      //e->reservedE[i] = used_E;
      e->energy = used_E;
		//}
		update_comp();
	}
}

unsigned calculate_charge_rate(uint16_t t_charge_end, uint16_t t_charge_start)
{
	unsigned rate_changed = 0;
	uint32_t charge_rate;

	//PRINTF("Chrg: vs: %u, ve: %u\r\n", v_charge_start, v_charge_end);
	//PRINTF("Chrg: Ts: %u, Te: %u\r\n", t_charge_start, t_charge_end);
  //PRINTF("IT:%u %u \r\n",cr_window_it, cr_window_ready);
	if (!v_charge_start) {
		goto calculate_charge_rate_cleanup;
	}
	if (v_charge_start > v_charge_end) {
		goto calculate_charge_rate_cleanup;
	}

	if (t_charge_end < t_charge_start) {
		goto calculate_charge_rate_cleanup;
	}
	unsigned charge_time = t_charge_end - t_charge_start;

	// V does not go up above 1.9xV
	// Thus, it may look like it is not charging properly
	// We consider this case as "LARGE INCOMING ENERGY!"
	if (v_charge_end >= V_NEARLY_MAX) {
		//PRINTF("I think we have high power\r\n");
		charge_rate = 1000;
	} else {
	// if less than 0.05V diff, do not use the result
		if (v_charge_start + V_THRES >= v_charge_end) {
			//PRINTF("charge too small\r\n");
			goto calculate_charge_rate_cleanup;
		}

		// If too short, it is incorrect
		if (charge_time < CHARGE_TIME_THRES) {
			//PRINTF("time too short\r\n");
			goto calculate_charge_rate_cleanup;
		}

		// TODO: Maybe this can get optimized a lot because if v_after_start = 0, the
		// value is always constant unless the thres changes.
		// Calc charge rate
		uint32_t charged_energy = v_charge_end * v_charge_end
			- v_charge_start * v_charge_start;
    //PRINTF("Chgd E: %x %x\r\n", (unsigned)(charged_energy >> 16),
    //(unsigned)(charged_energy & 0xFFFF));
		// Amp factor to avoid charge_rate being 0
		charge_rate = AMP_FACTOR * charged_energy / charge_time;
	}
  //PRINTF("New chrg_rate:%u %u\r\n", (unsigned)(charge_rate >> 16), (unsigned)(charge_rate & 0xFFFF));
	// Push it to averaging window
	cr_window[cr_window_it] = charge_rate;
	if (cr_window_it == CR_WINDOW_SIZE - 1) {
		cr_window_ready = 1;
		cr_window_it = 0;
	} else {
		cr_window_it++;
	}

	//uint32_t avg_charge_rate = get_charge_rate_average();
	uint32_t worst_charge_rate = get_charge_rate_worst();
  //PRINTF("Charge rate: %u\r\n",worst_charge_rate);
	//PRINTF("cr: %u %u\r\n", (unsigned)(worst_charge_rate >> 16), (unsigned)(worst_charge_rate & 0xFFFF));

	// Change mode if necessary
  //TODO use this?
	//change_mode(worst_charge_rate);

	// Calculate schedulability
	unsigned schedulable = is_schedulable(worst_charge_rate);
	while (!schedulable) {
    PRINTF("Not schedulable!\r\n");
	}

calculate_charge_rate_cleanup:
	v_charge_start = 0;
	v_charge_end = 0;
  //PRINTF("We out!\r\n");
	return rate_changed;
}

// Calculate the utilization
// and see if the events are
// schedulable
unsigned is_schedulable(uint32_t charge_rate)
{
	// Calculate charge time
	calculate_C(charge_rate);

	uint32_t U = 0; // Utilization
	for (unsigned i = 0; i < MAX_EVENTS; ++i) {
		uint32_t tmp;
		evt_t* event = all_events.events[i];
    if (event == NULL || event->valid == OFF) {
      continue;
    }
		tmp = event->charge_time * event->burst_num;
		tmp *= U_AMP_FACTOR; // Amplify
		tmp /= (uint32_t)event->period;
		//PRINTF("charge time: %u T: %u\r\n", (unsigned)event->charge_time, event->get_period());
		U += tmp;
	}
	PRINTF("U: %x %x\r\n", (unsigned)(U >> 16), (unsigned)(U & 0xFFFF));
	if (U <= 100) {
		return 1;
	} else {
		return 0;
	}
}

// Calculate C
void calculate_C(uint32_t charge_rate)
{
	for (unsigned i = 0; i < MAX_EVENTS; ++i) {
		evt_t* event = all_events.events[i];
    if (event == NULL || event->valid == OFF) {
      continue;
    }
		event->charge_time = (event->energy*AMP_FACTOR / charge_rate);
		event->charge_time += 1; // Just so that it is never 0 (for degrading)
   // PRINTF("Chrg_time: %u\r\n",event->charge_time);
	}
}

uint32_t get_charge_rate_worst()
{
	// Return the second-worst (heuristic)
	uint32_t worst = UINT32_MAX;
	uint32_t second_worst = UINT32_MAX;
	for (unsigned i = 0; i < CR_WINDOW_SIZE; ++i) {
		if (second_worst > cr_window[i]) {
			if (worst > cr_window[i]) {
				second_worst = worst;
				worst = cr_window[i];	
			} else {
				second_worst = cr_window[i];
			}
		}
	}
	
	//PRINTF("SW: %u\r\n", (unsigned)(second_worst & 0xFFFF));
	//PRINTF("W: %u\r\n", (unsigned)(worst & 0xFFFF));
	if (cr_window_ready) {
		return second_worst;
	}
	return worst;
}

