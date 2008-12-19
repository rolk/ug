// -*- tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
// vi: set et ts=4 sw=2 sts=2:
/* This software was developed by Bruce Hendrickson and Robert Leland   *
* at Sandia National Laboratories under US Department of Energy        *
* contract DE-AC04-76DP00789 and is copyrighted by Sandia Corporation. */

#include "config.h"
#include        <stdio.h>
#include        <stdlib.h>
#include        <limits.h>

#ifdef __STDC__
  #include      <float.h>
#endif


/* Returns some machine/compiler specific values. */
/* Note: These values might not be calculated precisely correctly.*/
/*       If you know them for your machine, replace this code! */

void machine_params(double_epsilon, double_min, double_max, rand_max)
double *double_epsilon;         /* returns machine precision */
double *double_max;             /* returns largest double value */
double *double_min;             /* returns smallest double value */
int *rand_max;                  /* returns largest value returnable from rand() */
{
#ifndef DBL_EPSILON
  double eps;
#endif
#ifndef DBL_MIN
  double min, min_prev;
#endif
#ifndef DBL_MAX
  double max;
#endif
#ifndef RAND_MAX
  int rmax, val;
  int n_rand_calls = 100;       /* number of times to call rand to find max */
  int i;                        /* loop counter */
#endif

#ifndef DBL_EPSILON
  eps = 1.0/16.0;
  while (1.0 + eps > 1.0) eps /= 2.0;
  *double_epsilon = eps*2.0;
#else
  *double_epsilon = DBL_EPSILON;
#endif

#ifndef DBL_MIN
  min_prev = min = 1.0;
  while (min*1.0 > 0) {
    min_prev = min;
    min /= 32.0;
  }
  min = min_prev;
  while (min*1.0 > 0) {
    min_prev = min;
    min /= 2.0;
  }
  *double_min = min_prev/(*double_epsilon);
#else
  *double_min = DBL_MIN;
#endif

#ifndef DBL_MAX
  max = 2.0*(1.0 - *double_epsilon)/(*double_min);
  /*
     two_max = max*2.0;
     while (two_max/2.0 == max) {
        max = two_max;
        two_max = max*2.0;
     }
   */
  *double_max = max;
#else
  *double_max = DBL_MAX;
#endif

#ifndef RAND_MAX
  rmax = 32767;
  for (i=n_rand_calls; i; i--) {
    val = rand();
    while (val > rmax) rmax = 2*rmax + 1;
  }
  *rand_max = rmax;
#else
  *rand_max = RAND_MAX;
#endif
}
