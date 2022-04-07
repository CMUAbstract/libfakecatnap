#ifndef _LCFN_CULPEO_H_
#define _LCFN_CULPEO_H_

extern __nv float Vmin;
extern __nv float Vfinal;
extern __nv float glob_sqrt;
float local_sqrt(void);

extern volatile uint8_t culpeo_profiling_flag;

void print_float(float f);

// The following are terms for calculating Vsafe
// We follow Catnap's val*100 protocol to avoid floats, but go up to val*1000

//Note: Vsafe = Vd + VE

// voltage where we multiply by 1000 to get 3 decimal places of precision
typedef uint16_t culpeo_V_t;

#if 1
//VE terms
// VE^2 = Vs_term - n_ratio*Vf**2 + Voff_squared
// TODO actually document this
// These assume Vstart = 2.4, Voff = 1.6
#define Vs_const_float 6.8589815
#define n_ratio_float 1.1907954
#define Voff_sq_float 2.56

//Vd terms
#define Vd_const_float 1.0887018
// Vd = (Vf-Vmin)*(Vmin*m+b)/Vd_const
#else //Terms for Voff = 1.8

#define Voff_sq_float  3.24
#define n_ratio_float  1.1365818
#define Vs_const_float  6.5467110
#define Vd_const_float  1.2832105

#endif

// Booster efficiency terms
//m = 0.16228070175438583; b= 0.4207894736842105
#define m_float 0.1622807
#define b_float 0.4207894


#define CULPEO_SCALE 10000

culpeo_V_t calc_culpeo_vsafe(void);
culpeo_V_t calc_culpeo_bucket(void);

// Other floats
#define V_off_float 1.6
#define V_start_sq_float 5.76

#endif
