// -*- tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
// vi: set et ts=4 sw=2 sts=2:
/****************************************************************************/
/*																			*/
/* File:	  bullet.h                                                                                                      */
/*																			*/
/* Purpose:   The bullet plotter rasterizes lines and polygons in a pixel   */
/*            buffer using---in 3D---the z buffer algorithm. Suitable       */
/*            output devices can display the pixel buffer in a single       */
/*            operation which may be faster than the standard plotting      */
/*            routine.                                                      */
/*																			*/
/* Author:	  Michael Lampe                                                                                                 */
/*			  Institut fuer Computeranwendungen                                                     */
/*			  Universitaet Stuttgart										*/
/*			  Pfaffenwaldring 27											*/
/*			  70569 Stuttgart												*/
/*			  internet: ug@ica3.uni-stuttgart.de                                                    */
/*																			*/
/* History:   24.2.98 begin, ug3-version									*/
/*																			*/
/* Remarks:                                                                                                                             */
/*																			*/
/****************************************************************************/


/****************************************************************************/
/*                                                                          */
/* auto include mechanism and other include files                           */
/*                                                                          */
/****************************************************************************/

#ifndef __BULLET__
#define __BULLET__

#ifndef __COMPILER__
#include "compiler.h"
#endif

#ifndef __WPM__
#include "wpm.h"
#endif

/****************************************************************************/
/*																			*/
/* constant declarations                                                                        */
/*																			*/
/****************************************************************************/

#define BULLET_OK             0
#define BULLET_CANT           1
#define BULLET_NOMEM          2

/****************************************************************************/
/*																			*/
/* function declarations													*/
/*																			*/
/****************************************************************************/

INT BulletOpen(PICTURE *picture, DOUBLE factor);
void BulletClose(void);
void BulletPlot(void);
void BulletLine(DOUBLE *p1, DOUBLE *p2, long c);
void BulletPolyLine(DOUBLE *p, INT n, long c);
void BulletPolygon(DOUBLE *p, INT n, DOUBLE i, long c);

#endif