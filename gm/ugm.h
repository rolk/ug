// -*- tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
// vi: set et ts=4 sw=2 sts=2:
/****************************************************************************/
/*																			*/
/* File:	  ugm.h                                                                                                                 */
/*																			*/
/* Purpose:   unstructured grid manager header file                                             */
/*			  internal interface in grid manager module                                     */
/*																			*/
/* Author:	  Peter Bastian                                                                                                 */
/*			  Interdisziplinaeres Zentrum fuer Wissenschaftliches Rechnen	*/
/*			  Universitaet Heidelberg										*/
/*			  Im Neuenheimer Feld 368										*/
/*			  6900 Heidelberg												*/
/*			  internet: ug@ica3.uni-stuttgart.de			                        */
/*																			*/
/* History:   09.03.92 begin, ug version 2.0								*/
/*			  13.12.94 begin, ug version 3.0								*/
/*																			*/
/* Remarks:                                                                                                                             */
/*																			*/
/****************************************************************************/


/* RCS_ID
   $Header$
 */

/****************************************************************************/
/*																			*/
/* auto include mechanism and other include files							*/
/*																			*/
/****************************************************************************/

#ifndef __UGM__
#define __UGM__

#include "compiler.h"
#include "gm.h"
#include "dlmgr.h"

/****************************************************************************/
/*																			*/
/* defines in the following order											*/
/*																			*/
/*		  compile time constants defining static data size (i.e. arrays)	*/
/*		  other constants													*/
/*		  macros															*/
/*																			*/
/****************************************************************************/

#define MAX_PAR_DIST    1.0E-5          /* max.dist between different parameter */

#ifdef ModelP
#define PutFreeObject(theMG,object,size,type) PutFreeObject_par(MGHEAP(theMG),(object),(size),(type))
#define GetMemoryForObject(theMG,size,type) GetMemoryForObject_par(MGHEAP(theMG),(size),(type))
#else
#define GetMemoryForObject(theMG,size,type) GetFreelistMemory(MGHEAP(theMG),(size))
#define PutFreeObject(theMG,object,size,type) PutFreelistMemory(MGHEAP(theMG),(object),(size))
#endif

/****************************************************************************/
/*																			*/
/* data structures exported by the corresponding source file				*/
/*																			*/
/****************************************************************************/


/****************************************************************************/
/*																			*/
/* definition of exported global variables									*/
/*																			*/
/****************************************************************************/


/****************************************************************************/
/*																			*/
/* function declarations													*/
/*																			*/
/****************************************************************************/

/* init */
INT              InitUGManager                  (void);

/* object handling */
INT              GetFreeOBJT                    (void);
INT              ReleaseOBJT                    (INT type);

/* create basic objects */
#ifdef ModelP
EDGE *CreateEdge (GRID *theGrid, NODE *from, NODE *to, INT with_vector);
#endif
ELEMENT     *CreateElement          (GRID *theGrid, INT tag, INT objtype,
                                     NODE **nodes, ELEMENT *Father);
INT         CreateSonElementSide    (GRID *theGrid, ELEMENT *theElement,
                                     INT side, ELEMENT *theSon, INT son_side);

GRID            *CreateNewLevel                 (MULTIGRID *theMG, INT algebraic);
GRID        *CreateNewLevelAMG      (MULTIGRID *theMG);

/* dispose basic objects */
INT              DisposeElement                 (GRID *theGrid, ELEMENT *theElement, INT dispose_connections);
INT              DisposeTopLevel                (MULTIGRID *theMG);

/* miscellaneous */
ELEMENT     *FindFather             (VERTEX *vptr);
INT              FindNeighborElement    (const ELEMENT *theElement, INT Side, ELEMENT **theNeighbor, INT *NeighborSide);
INT              PointInElement                 (const DOUBLE*, const ELEMENT *theElement);
VIRT_HEAP_MGMT *GetGenMGUDM             (void);
INT             CheckOrientation                (INT n, VERTEX **vertices);

NODE        *CreateSonNode          (GRID *theGrid, NODE *FatherNode);
NODE            *CreateMidNode                  (GRID *theGrid, ELEMENT *theElement, INT edge);
NODE            *GetCenterNode                  (ELEMENT *theElement);
NODE        *CreateCenterNode       (GRID *theGrid, ELEMENT *theElement);

#ifdef __THREEDIM__
NODE            *CreateSideNode                 (GRID *theGrid, ELEMENT *theElement, INT side);
NODE            *GetSideNode                    (ELEMENT *theElement, INT side);
#endif
INT          GetSideIDFromScratch   (ELEMENT *theElement, NODE *theNode);
NODE        *GetMidNode             (ELEMENT *theElement, INT edge);
INT                     GetNodeContext                  (ELEMENT *theElement, NODE **theElementContext);
void            GetNbSideByNodes                (ELEMENT *theNeighbor, INT *nbside, ELEMENT *theElement, INT side);


#ifdef ModelP
void *GetMemoryForObject_par (HEAP *theHeap, INT size, INT type);
INT PutFreeObject_par (HEAP *theHeap, void *object, INT size, INT type);
#endif

#endif
