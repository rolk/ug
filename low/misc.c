// -*- tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
// vi: set et ts=4 sw=2 sts=2:
/****************************************************************************/
/*	                                                                        */
/* File:      misc.c                                                        */
/*                                                                          */
/* Purpose:   miscellaneous routines                                        */
/*                                                                          */
/* Author:      Klaus Johannsen                                             */
/*              Institut fuer Computeranwendungen                           */
/*              Universitaet Stuttgart                                      */
/*              Pfaffenwaldring 27                                          */
/*              70569 Stuttgart                                             */
/*              internet: ug@ica3.uni-stuttgart.de		                    */
/*                                                                          */
/* History:   08.12.94 begin, ug3-version                                   */
/*                                                                          */
/* Revision:  07.09.95                                                      */
/*                                                                          */
/****************************************************************************/

/****************************************************************************/
/*                                                                          */
/* include files                                                            */
/*              system include files                                        */
/*              application include files                                   */
/*                                                                          */
/****************************************************************************/

#include <math.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stddef.h>
#include <stdio.h>

#define COMPILE_MISC_H
#include "compiler.h"
#include "general.h"
#include "misc.h"

/****************************************************************************/
/*                                                                            */
/* defines in the following order                                            */
/*                                                                            */
/*          compile time constants defining static data size (i.e. arrays)    */
/*          other constants                                                    */
/*          macros                                                            */
/*                                                                            */
/****************************************************************************/

/****************************************************************************/
/*                                                                            */
/* data structures used in this source file (exported data structures are    */
/*          in the corresponding include file!)                                */
/*                                                                            */
/****************************************************************************/



/****************************************************************************/
/*                                                                            */
/* definition of exported global variables                                    */
/*                                                                            */
/****************************************************************************/

int UG_math_error = 0; /* This will be non zero after a math error occured  */
int me = 0;                             /* to have in the serial case this variable as a dummy */

/****************************************************************************/
/*                                                                            */
/* definition of variables global to this source file only (static!)        */
/*                                                                            */
/****************************************************************************/

/* RCS string */
static char RCS_ID("$Header$",UG_RCS_STRING);

/****************************************************************************/
/*                                                                            */
/* forward declarations of macros                                            */
/*                                                                            */
/****************************************************************************/

#define MIN_DETERMINANT                 1e-8

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
/****                                                                     ****/
/****        general routines                                             ****/
/****                                                                     ****/
/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

/****************************************************************************/
/*D
   expandfmt - Expand (make explicit) charset-ranges in scanf

   SYNOPSIS:
   char *expandfmt (const char *fmt)

   PARAMETERS:
   .  fmt - pointer to char (const)

   DESCRIPTION:
   This function expands (make explicit) charset-ranges in scanf.
   For example '%<number>[...a-d...]' --> '%<number>[...abcd...]'.

   RETURN VALUE:
   char*
   .n        new pointer to char
   D*/
/****************************************************************************/

/* install a user math error handler */
int matherr(struct exception *x)
{
  /* your math error handling here */
  UG_math_error = 1;       /* lets be more specific later ... */
  return(0);   /* proceed with standard messages */
}

#define FMTBUFFSIZE         1031

static char newfmt[FMTBUFFSIZE];

char *expandfmt (const char *fmt)
{
  const char *pos;
  char *newpos;
  char leftchar,rightchar;
  int newlen;

  /* calculate min size of newfmt */
  newlen = strlen(fmt);
  assert (newlen<FMTBUFFSIZE-1);

  pos    = fmt;
  newpos = newfmt;

  /* scan fmt for %<number>[ */
  while (*pos!='\0')
  {
    /* copy til '%' */
    while (*pos!='%' && *pos!='\0')
      *(newpos++) = *(pos++);

    if (*pos=='\0')
      break;

    *(newpos++) = *(pos++);

    /* copy til !isdigit */
    while (isdigit(*pos) && *pos!='\0')
      *(newpos++) = *(pos++);

    if (*pos=='\0')
      break;

    if (*pos!='[')
      continue;

    *(newpos++) = *(pos++);

    /* ']' following '[' is included in the charset */
    if ((*pos)==']')
      *(newpos++) = *(pos++);

    /* '^]' following '[' is included in the charset */
    else if ((*pos)=='^' && (*(pos+1))==']')
    {
      *(newpos++) = *(pos++);
      *(newpos++) = *(pos++);
    }

    while (*pos!=']' && *pos!='\0')
    {
      /* now we are inside the charset '[...]': */

      /* treat character ranges indicated by '-' */
      while (*pos!='-' && *pos!=']' && *pos!='\0')
        *(newpos++) = *(pos++);

      if (*pos=='\0')
        break;

      if ((*pos)==']')
        continue;

      /* gotya: is left char < right char? */
      leftchar  = *(pos-1);
      rightchar = *(pos+1);

      if (leftchar=='[' || rightchar==']')
      {
        *(newpos++) = *(pos++);
        continue;
      }

      if (leftchar>=rightchar)
      {
        *(newpos++) = *(pos++);
        continue;
      }

      if (leftchar+1==rightchar)
      {
        /* for example '...b-c...' */
        pos++;
        continue;
      }

      /* calc new size and expand range */
      newlen += rightchar-leftchar-2;
      assert (newlen<FMTBUFFSIZE-1);

      leftchar++;
      pos++;

      while (leftchar<rightchar)
      {
        if (leftchar=='^' || leftchar==']')
        {
          leftchar++;
          continue;
        }
        *(newpos++) = leftchar++;
      }
    }
  }

  *newpos = '\0';

  return (newfmt);
}

/****************************************************************************/
/*D
   StrTok - Copy a token out of a string (ANSI-C)

   SYNOPSIS:
   char *StrTok (char *s, const char *ct)

   PARAMETERS:
   .  s -  pointer to char
   .  ct - pointer to char (const)

   DESCRIPTION:
   This function copies a token out of a string.

   See also ANSI-C for description of this function.

   RETURN VALUE:
   char
   .n    pointer to char, modified string
   D*/
/****************************************************************************/

char *StrTok (char *s, const char *ct)
{
  static char *e;
  char *b;
  INT i, flag;

  if (s != NULL)
    b = s-1;
  else
    b = e+1;

  flag = 0;
  while (flag == 0)
  {
    b += 1;
    for (i=0; i<strlen(ct); i++)
      if ((*(ct+i)) == (*b))
        flag = 1;
  }
  e = b;
  flag = 0;
  while (flag == 0)
  {
    e += 1;
    for (i=0; i<strlen(ct); i++)
      if ((*(ct+i)) == (*e))
        flag = 1;
  }
  *e = '\0';

  return (b);
}

/****************************************************************************/
/*D
   strntok - Split a string into tokens each of maximal length 'n+1'

   SYNOPSIS:
   const char *strntok (const char *str, const char *sep, int n, char *token)

   PARAMETERS:
   .  str -   pointer to char (const)
   .  sep -   pointer to char (const)
   .  n -     integer, number of chars in token
   .  token - pointer to char

   DESCRIPTION:
   This function splits a string into tokens each of maximal length 'n+1'.
   A pointer to the next char following the token (its a sep) is returned.
   NB: possibly check whether the returned char is a sep.
   If not: the token was to long and only the first n chars where copied!

   RETURN VALUE:
   char
   .n     pointer to token
   .n     NULL if token larger than n.
   D*/
/****************************************************************************/

const char *strntok (const char *str, const char *sep, int n, char *token)
{
  int i;

  /* scan while current char is a seperator */
  while ((*str!='\0') && (strchr(sep,*str)!=NULL)) str++;

  /* copy into token */
  for (i=0; i<n; i++,str++)
    if ((*str!='\0') && (strchr(sep,*str)==NULL))
      token[i] = *str;
    else
      break;

  if (strchr(sep,*str)==NULL)
    return (NULL);                    /* ERROR: token too long! */

  /* 0-terminate string */
  token[i] = '\0';

  return (str);
}

/****************************************************************************/

static void Copy (char *a, const char *b, INT size)
{
  INT i;

  for (i=0; i<size; i++)
    a[i] = b[i];
}

/****************************************************************************/
/*D
   QSort - Sorting routine (standard function)

   SYNOPSIS:
   void QSort (void *base,INT n,INT size,int (*cmp)(const void *,const void *))

   PARAMETERS:
   .  base - pointer to void, field to be sorted
   .  n -    integer, length of string
   .  size - integer, number of characters to be sorted
   .  cmp -  pointer to function with two arguments

   DESCRIPTION:
   This function sorts the values returned by a function given as argument.

   See also standard description of 'QSort'.

   RETURN VALUE:
   void
   D*/
/****************************************************************************/
void QSort (void *base, INT n, INT size, int (*cmp)(const void *, const void *))
{
  INT i, j;
  char *Base, v[4], t[4];
  INT cmp1, cmp2, compare,flag;

  if (n<2) return;
  flag=0;
  Base = (char*)base;
  Copy(v,Base+(n-1)*size,size);
  for (i=-1, j=n-1;;)
  {
    while (++i<n-1)
      if ((cmp1=cmp( (void *)v, (void *)(Base+i*size))) <= 0)
        break;
    while ( --j>0)
      if ((cmp2=cmp( (void *)v, (void *)(Base+j*size))) >= 0)
        break;
    if (i>=j)
      break;
    compare  = (cmp1<0);
    compare |= ((cmp2>0)<<1);
    switch (compare)
    {
    case (0) :
      QSort ( (void *)(Base+i*size), j-i+1, size, cmp);
      i--;
      while (++i<n-1)
        if (cmp( (void *)v, (void *)(Base+i*size)) < 0)
          break;
      flag=1;
      break;
    case (1) :
      Copy(t,Base+i*size,size);
      Copy(Base+i*size,Base+j*size,size);
      Copy(Base+j*size,t,size);
      i--;
      break;
    case (2) :
      Copy(t,Base+i*size,size);
      Copy(Base+i*size,Base+j*size,size);
      Copy(Base+j*size,t,size);
      j++;
      break;
    case (3) :
      Copy(t,Base+i*size,size);
      Copy(Base+i*size,Base+j*size,size);
      Copy(Base+j*size,t,size);
      break;
    }
    if (flag) break;
  }
  Copy(t,Base+i*size,size);
  Copy(Base+i*size,v,size);
  Copy(Base+(n-1)*size,t,size);
  QSort ( base, i, size, cmp);
  QSort ( (void *)(Base+(i+1)*size), n-i-1, size, cmp);
}

/****************************************************************************/
/*D
   SelectionSort - Sorting routine (standard)

   SYNOPSIS:
   void SelectionSort (void *base,INT n,INT size,int (*cmp)(const void *,const void *))

   PARAMETERS:
   .  base - pointer to void, field to be sorted
   .  n -    integer, length of string
   .  size - integer, number of characters to be sorted
   .  cmp -  pointer to function with two arguments

   DESCRIPTION:
   This function sorts the arguments of the function.

   See also standard description of 'SelectionSort'.

   RETURN VALUE:
   void
   D*/
/****************************************************************************/

void SelectionSort (void *base, INT n, INT size, int (*cmp)(const void *, const void *))
{
  INT i,j,k1,k2,s;
  char Smallest[4];
  char *Base;

  if (n<2) return;
  Base = (char*)base;

  for (i=0; i<n; i++)
  {
    Copy(Smallest,Base+i*size,size);
    k1=i;
    for (s=0; s<n-i; s++)
    {
      k2=k1;
      for (j=i; j<n; j++)
      {
        if (j==k1) continue;
        if (cmp( (void *)Smallest, (void *)(Base+j*size)) > 0)
        {
          Copy(Smallest,Base+j*size,size);
          k1=j;
        }
      }
      if (k1==k2) break;
    }
    Copy(Smallest,Base+i*size,size);
    Copy(Base+i*size,Base+k1*size,size);
    Copy(Base+k1*size,Smallest,size);
  }
}
