#include "catnap.h"
#include "culpeo.h"
#include <msp430.h>
#include <stdio.h>
#include <math.h>
#include <float.h>
#include <libio/console.h>
#include <libmsp/mem.h>


// uint32_t max: 4,294,967,295
__nv float Vmin = 2.103;
__nv float Vfinal = 2.238;
__nv float glob_sqrt = .5;

#define cast_out(fval) \
  (unsigned)(fval*1000)

float inline local_sqrt(void) {
  float x = glob_sqrt;
	float z = x / 2;
  //PRINTF("In sqrt:\r\n");
	for (unsigned i = 0; i < 1000; ++i) {
		z -= (z*z - x) / (2*z);
		if ((z*z - x) < x * 0.0001) {
      //PRINTF("iterations:%u\r\n",i);
			return z;
		}
	}
	return z;
}

//NOTE: this doesn't zero pad
void print_float(float f)
{
	if (f < 0) {
		PRINTF("-");
		f = -f;
	}

	PRINTF("%u.",(int)f);
	//PRINTF("%u.",(unsigned)f);

	// Print decimal -- 3 digits
	PRINTF("%u", (((int)(f * 1000)) % 1000));
	//PRINTF("%u", (((unsigned)(f * 1000)) % 1000));
  PRINTF(" ");
}


culpeo_V_t calc_culpeo_vsafe(void) {
  float Vd = Vfinal - Vmin;
  float scale = Vmin*m_float + b_float;
  scale /= Vd_const_float;
  float Vd_scaled = Vd*scale;
  scale = 1- Vd_scaled;
  //print_float(scale);
  float Ve_squared = Vs_const_float + Voff_sq_float;
  float temp = Vfinal*Vfinal;
  temp *= n_ratio_float;
  Ve_squared -= temp;
  glob_sqrt = Ve_squared;
  float Ve = local_sqrt();
  //print_float(Ve);
  culpeo_V_t ve_int = cast_out(Ve);
  culpeo_V_t vd_int = cast_out(Vd_scaled);
  //PRINTF("Ve: %u, Vd: %u\r\n",ve_int,vd_int);
  uint16_t Vsafe = ve_int + vd_int;
  return Vsafe;
}

culpeo_V_t calc_culpeo_bucket(void) {
  // Walk through active events
  // Assume they're in order?
  // Start by setting bucket level to Voff
  float Vbucket = V_off_float;
  //TODO reorder these
  for (int i = 0; i < MAX_EVENTS; i++) {
		evt_t* e = all_events.events[i];
    if ( e == NULL || e->valid == OFF) {
      continue;
    }
    for (int j = 0; j < e->burst_num; j++) {
      /*PRINTF("0Bucket:");
      print_float(Vbucket);
      PRINTF("\r\n");*/
      if ((Vbucket - (e->V_final - e->V_min)) < V_off_float) {
        Vbucket = V_off_float + (e->V_final - e->V_min);
        //PRINTF("inner mod\r\n");
      }
      /*PRINTF("1Bucket:");
      print_float(Vbucket);
      PRINTF("\r\n");*/
      //TODO we could optimize and add another variable to store Vbucket_sq since
      //we get it for free
      float Vbucket_sq;
      Vbucket_sq = V_start_sq_float - e->V_final*e->V_final + Vbucket*Vbucket;
      glob_sqrt = Vbucket_sq;
      Vbucket = local_sqrt();
      /*PRINTF("Bucket:");
      print_float(Vbucket);
      PRINTF("\r\n");*/
    }
  }
  culpeo_V_t Vb = cast_out(Vbucket);
  return Vb;
}

