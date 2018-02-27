﻿#ifndef __FILTER_H
#define __FILTER_H

#include "param_def.h"

#define LPF_1_(hz,t,in,out) ((out) += ( 1 / ( 1 + 1 / ( (hz) *6.28f *(t) ) ) ) *( (in) - (out) ))

#define LPF_1(A,B,C,D) LPF_1_((A),(B),(C),*(D));

typedef struct
{
	float a;
	float b;
	float e_nr;
	float out;
} _filter_1_st;

void anotc_filter_1(float base_hz, float gain_hz, float dT, float in, _filter_1_st *f1);

void Moving_Average(float moavarray[], uint16_t len, uint16_t *fil_cnt,float in,float *out);
int32_t Moving_Median(int32_t moavarray[], uint16_t len, uint16_t *fil_p, int32_t in);
#define _xyz_f_t xyz_f_t
void simple_3d_trans(_xyz_f_t *ref, _xyz_f_t *in, _xyz_f_t *out);

#endif
