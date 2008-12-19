// -*- tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
// vi: set et ts=4 sw=2 sts=2:
/* This software was developed by Bruce Hendrickson and Robert Leland   *
* at Sandia National Laboratories under US Department of Energy        *
* contract DE-AC04-76DP00789 and is copyrighted by Sandia Corporation. */

#include "config.h"
#include <math.h>

/* Returns 2-norm of an n-vector over range. */
double norm(vec,beg,end)
double *vec;
int beg, end;
{
  double dot();

  return(sqrt(dot(vec,beg,end,vec)));
}
