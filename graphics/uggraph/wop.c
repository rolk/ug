// NOTE: The current revision of this file was left untouched when the DUNE source files were reindented!
// NOTE: It contained invalid syntax that could not be processed by uncrustify.

/****************************************************************************/
/*																			*/
/* File:	  wop.c 														*/
/*																			*/
/* Purpose:   work functions on pictures									*/
/*																			*/
/* Author:	  Klaus Johannsen												*/
/*			  Interdisziplinaeres Zentrum fuer Wissenschaftliches Rechnen	*/
/*			  Universitaet Heidelberg										*/
/*			  Im Neuenheimer Feld 368										*/
/*			  6900 Heidelberg												*/
/*			  internet: ug@ica3.uni-stuttgart.de 		            		*/
/*																			*/
/* History:   21.06.93 begin, ug version ug21Xmas3d 						*/
/*																			*/
/* Remarks: 																*/
/*																			*/
/****************************************************************************/

/****************************************************************************/
/*																			*/
/* include files															*/
/*			  system include files											*/
/*			  application include files 									*/
/*																			*/
/****************************************************************************/

#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <memory.h>

#include "compiler.h"
#include "debug.h"
#include "wop.h"
#include "wpm.h"
#include "misc.h"
#include "evm.h"
#include "cw.h"
#include "graph.h"
#include "gm.h"
#include "ugm.h"
#include "num.h"
#include "shapes.h"
#include "general.h"
#include "debug.h"
#ifdef ModelP
#include "parallel.h"
#include "ppif.h"
#endif

/****************************************************************************/
/*																			*/
/* defines in the following order											*/
/*																			*/
/*		  compile time constants defining static data size (i.e. arrays)	*/
/*		  other constants													*/
/*		  macros															*/
/*																			*/
/****************************************************************************/

/* definition of the arrow */
#define ARR_ALPHA						0.7
#define ARR_SIN 						0.5
#define ARR_COS 					   -0.866

/* definition of the dependency */
#define DEP_LEN							8
#define DEP_POS							0.9
#define DEP_SIN 						0.342
#define DEP_COS 					   -0.94

/* definitions for pulling frames by mouse */
#define MOUSE_NOT_MOVED					0
#define MOUSE_MOVED						1
#define REJECTED						2

/* ctrl entries of element ctrl */
INT ce_VSIDES;
#define VSIDES_LEN						6
#define VSIDES(p)						CW_READ(p,ce_VSIDES)
#define SETVSIDES(p,n)					CW_WRITE(p,ce_VSIDES,n)
#define VIEWABLE(p,i)					(VSIDES(p) & (1<<i))

INT ce_NORDER;
#define NORDER_LEN						5
#define NORDER(p)						CW_READ(p,ce_NORDER)
#define SETNORDER(p,n)					CW_WRITE(p,ce_NORDER,n)

INT ce_COUNT;
#define COUNT_LEN						4
#define COUNT(p)						CW_READ(p,ce_COUNT) 
#define SETCOUNT(p,n)					CW_WRITE(p,ce_COUNT,n)

INT ce_CUTMODE;
#define CUTMODE_LEN 					2
#define CUTMODE(p)						CW_READ(p,ce_CUTMODE)
#define SETCUTMODE(p,n) 				CW_WRITE(p,ce_CUTMODE,n)

/* values for CUTMODE */
#define CM_BEHIND						0
#define CM_INTERSECT					1
#define CM_INFRONT						2

/* Macros for Node order */
#define NODE_ORDER(p) 		   ((TAG(theElement) == TETRAHEDRON) ? \
								NORDER(theElement) : (p)->ge.property)
#define SET_NODE_ORDER(p,n)    ((p)->ge.property = n)
#define GETBITS(x,p)           (((x)>>((p)+1-(3)))&~(~0<<(3)))
#define SETBITS(x,p,y)         (((y)<<((p)+1-(3)))|(x))
#define CORNER_OF_SIDE0(t,s,c) (element_descriptors[t]->corner_of_side[(s)][(c)])

/* pixel resolution for inserting boundary nodes */
#define SMALLPIX 			4

/* introduce new coordinate system for matrix plots: 
		(0,0) is in the upper left corner
		x-axis to yhe right
		y-axis down (opposite direction resp. standard ug-coordinate system!)
		unit is 1 matrix entry box
*/
#define MAT_XC(col)	(col)
#define MAT_YC(row)	(MAT_maxrow-(row))

/****************************************************************************/
/*																			*/
/* data structures used in this source file (exported data structures are	*/
/*		  in the corresponding include file!)								*/
/*																			*/
/****************************************************************************/

typedef void (*ProjectionProcPtr) (DOUBLE *, COORD_POINT *); 

/****************************************************************************/
/*																			*/
/* definition of variables global to this source file only (static!)		*/
/*																			*/
/****************************************************************************/

#define WINDOW_TEXT_SIZE		(10*TextFactor)

/* general multiplier for all text sizes */
static DOUBLE TextFactor=1;

/************************************************************************/
/************ ordinary static variables   *******************************/
/************************************************************************/

#ifdef __DO_HEAP_USED__
	static INT		Heap_Used_Min;
	static INT		Heap_Used_Max;
#endif


/* internal function used to create value of NodeOrder from code determined in function 'CalcNodeOrder' */
/* if ">=" means nearer, than OrderIndex[111111] = 0 is the index for x1>=x2>=x3>=x4					*/
static INT OrderIndex[64] = {23,17,-1,15,  21,-1,11,9,	 -1,-1,-1,14,  -1,-1,-1,8,
							 -1,-1,-1,-1,  20,-1,10,-1,  -1,-1,-1,-1,  -1,-1,7,6,
							 22,16,-1,-1,  -1,-1,-1,-1,  -1,13,-1,12,  -1,-1,-1,-1,
							 19,-1,-1,-1,  18,-1,-1,-1,   5,3,-1,2, 	4,-1,1,0	};


/*  CornerIndex[24][MAX_CORNERS_OF_ELEM] */
static INT CornerIndex[24][4] = {
  {0,1,2,3},{0,1,3,2},{0,2,1,3},{0,2,3,1},{0,3,1,2},{0,3,2,1},
  {1,0,2,3},{1,0,3,2},{1,2,0,3},{1,2,3,0},{1,3,0,2},{1,3,2,0},
  {2,0,1,3},{2,0,3,1},{2,1,0,3},{2,1,3,0},{2,3,0,1},{2,3,1,0},
  {3,0,1,2},{3,0,2,1},{3,1,0,2},{3,1,2,0},{3,2,0,1},{3,2,1,0}  
};
							
/*  SideCornerIndex[MAX_SIDES_OF_ELEM][24][MAX_CORNERS_OF_SIDE] */
static INT SideCornerIndex[4][24][3] =
						 {
						  { 
   							{0,1,2},{0,1,2},{0,2,1},{0,2,1},{0,1,2},{0,2,1},
						 	{1,0,2},{1,0,2},{1,2,0},{1,2,0},{1,0,2},{1,2,0},
						 	{2,0,1},{2,0,1},{2,1,0},{2,1,0},{2,0,1},{2,1,0},
						 	{0,1,2},{0,2,1},{1,0,2},{1,2,0},{2,0,1},{2,1,0} 
						  },
						  { 	
   							{1,2,3},{1,3,2},{2,1,3},{2,3,1},{3,1,2},{3,2,1},
							{1,2,3},{1,3,2},{1,2,3},{1,2,3},{1,3,2},{1,3,2},
							{2,1,3},{2,3,1},{2,1,3},{2,1,3},{2,3,1},{2,3,1},
							{3,1,2},{3,2,1},{3,1,2},{3,1,2},{3,2,1},{3,2,1} 
						  },
						  { 	
   							{0,2,3},{0,3,2},{0,2,3},{0,2,3},{0,3,2},{0,3,2},
							{0,2,3},{0,3,2},{2,0,3},{2,3,0},{3,0,2},{3,2,0},
							{2,0,3},{2,0,3},{2,0,3},{2,3,0},{2,3,0},{2,3,0},
							{3,0,2},{3,0,2},{3,0,2},{3,2,0},{3,2,0},{3,2,0} 
						  },
						  { 	
   							{0,1,3},{0,1,3},{0,1,3},{0,3,1},{0,3,1},{0,3,1},
							{1,0,3},{1,0,3},{1,0,3},{1,3,0},{1,3,0},{1,3,0},
							{0,1,3},{0,3,1},{1,0,3},{1,3,0},{3,0,1},{3,1,0},
							{3,0,1},{3,0,1},{3,1,0},{3,1,0},{3,0,1},{3,1,0} 
						  }
						 };
				
static INT NoOfViewableSides[64] =  {0,1,1,2,1,2,2,3,1,2,2,3,2,3,3,4,
									1,2,2,3,2,3,3,4,2,3,3,4,3,4,4,5,
									1,2,3,3,2,3,3,4,2,3,3,4,3,4,4,5,
									2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6};

/* unit vectors */
static DOUBLE				ex[3] = {1.0, 0.0, 0.0};
static DOUBLE				ey[3] = {0.0, 1.0, 0.0};
static DOUBLE				ez[3] = {0.0, 0.0, 1.0};

/*----------- variables describing taransformations --------------------*/
static DOUBLE					ObsTrafo[16], InvObsTrafo[16];
static DOUBLE					CutTrafo[16], InvCutTrafo[16];
static INT						CUT_CutExisting;
static DOUBLE					CUT_CutNormal[3];
static INT						CUT_CutAtFront;
static ProjectionProcPtr		OBS_ProjectProc;	
static INT						OBS_Perspective;
static DOUBLE  					OBS_PerspCorr[2];
static DOUBLE					OBS_ViewDirection[3];
static DOUBLE					OBS_ViewPlaneDist;

/*----------- variables describing phys. reactangle (2D) -------------------*/
static COORD_POINT					PhysRect[4];

/*----------- used by DrawPictureFrame -------------------------------------*/
static INT DoFramePicture=YES;

/****************************************************************************/
/************ variables used for communication of functions *****************/
/****************************************************************************/

/*---------- variables use by OrderElements etc ----------------------------*/
static VIEWEDOBJ			*OE_ViewedObj;

/*---------- variables use by GetFirst/NextElement... ----------------------*/
static MULTIGRID					*GE_MG; 						
static INT							GE_fromLevel,GE_toLevel;		


/*---------- input variables of 'EW_ElementEval2D/3D' ----------------------*/
/* defines for 2D/3D */
#define PLOT_COPY			YELLOW_CLASS		/* values for 'Elem2Plot'		*/
#define PLOT_IRR			GREEN_CLASS
#define PLOT_REG			RED_CLASS
#define PLOT_ALL			(RED_CLASS+1)/* this means all levels		*/

#define COLOR_COPY			YELLOW_CLASS		/* values for 'NoColor' and 	*/
#define COLOR_IRR			GREEN_CLASS		/* 'Color'						*/
#define COLOR_REG			RED_CLASS
#define COLOR_LOWER_LEVEL	(RED_CLASS+1)
#define COLOR_EDGE			(RED_CLASS+2)
#define EE_MAX_PROP		    100

/* defines 2D */
#define EE2D_TEXTSIZE			(8*TextFactor)

#define COLOR_BND			(RED_CLASS+3)
#define COLOR_ELEMID		(RED_CLASS+4)

/* defines 3D */
#define EE3D_TEXTSIZE			(8*TextFactor)

#define COLOR_CUT_EDGE		(RED_CLASS+3)

/* 2D */
static INT	EE2D_Elem2Plot[10];	/* 1 if element has to be plotted			*/
static long EE2D_NoColor[10];	/* 1 if no color (background color) used	*/
static long EE2D_Color[10];		/* colors used								*/
static INT EE2D_MaxLevel;		/* level considered to be the top level 	*/
static INT EE2D_ElemID; 		/* 1 if element ID has to be plotted		*/
static INT EE2D_RefMark;		/* 1 if plot refinement marks				*/
static long EE2D_ColorRefMark;	/* color of refinement marks				*/
static INT EE2D_IndMark;		/* 1 if plot indicator marks				*/
static long EE2D_ColorIndMark;	/* color of indicator marks	     			*/
static DOUBLE EE2D_ShrinkFactor;/* shrink factor, 1.0 if normal plot		*/
#ifdef ModelP
static DOUBLE EE2D_PartShrinkFactor;
								/* part. shrink factor, 1.0 if normal plot	*/
static DOUBLE_VECTOR EE2D_PartMidPoint;
#endif
static INT EE2D_Property;		/* 1 if plot property						*/
static INT EE2D_NProperty;		/* nb of properties							*/
static long EE2D_PropertyColor[EE_MAX_PROP+1];	/* colors used			    */

/* 3D */
static INT	EE3D_Elem2Plot[10];	/* 1 if element has to be plotted			*/
static long EE3D_NoColor[10];	/* 1 if no color (background color) used	*/
static long EE3D_Color[10];		/* colors used								*/
static INT EE3D_MaxLevel;		/* level considered to be the top level 	*/
static DOUBLE EE3D_ShrinkFactor;/* shrink factor, 1.0 if normal plot		*/
static INT EE3D_Property;		/* 1 if plot property						*/
static INT EE3D_NProperty;		/* nb of properties							*/
static long EE3D_PropertyColor[EE_MAX_PROP+1];	/* colors used			    */
#ifdef ModelP
static DOUBLE EE3D_PartShrinkFactor;
								/* part. shrink factor, 1.0 if normal plot	*/
static DOUBLE_VECTOR EE3D_PartMidPoint;
#endif


/*---------- working variables of 'NW_NodesEval2D' -------------------------*/
static long NE_IDColor; 		/* color of node ID's                       */
static long NE_BndMarkerColor;	/* color of bnd marks						*/
static long NE_CornerMarkerColor;/* color of corner marks					*/
static long NE_InnerMarkerColor;/* color of inner marks 					*/
static INT NE_EvalNodeID;		/* 1 if to evaluate 						*/
static INT NE_EvalInnerNode;	/* 1 if to evaluate 						*/
static INT NE_EvalBndNode;		/* 1 if to evaluate 						*/
static short NE_InnerMarker;	/* marker for inner nodes					*/
static short NE_BndMarker;		/* marker for bnd nodes 					*/
static short NE_CornerMarker;	/* marker for corner nodes 					*/
static short NE_InnerMarkerSize;/* markersize for inner nodes				*/
static short NE_BndMarkerSize;	/* markersize for bnd nodes 				*/
static short NE_CornerMarkerSize;/* markersize for corner nodes 			*/
static NODE *NE_Node;			/* node for insert node work				*/

/*---------- working variables of 'EW_MarkElement2D' -----------------------*/
static INT ME2D_found;			/* TRUE if an element found					*/
static ELEMENT *ME2D_elem;		/* pointer to element found					*/
static INT ME2D_pointIn;		/* TRUE if point specified, rectangle else	*/
static DOUBLE_VECTOR ME2D_point;	/* point if ME2D_pointIn TRUE				*/
static DOUBLE ME2D_xmin;			/* search rectangle							*/
static DOUBLE ME2D_xmax;			/* search rectangle							*/
static DOUBLE ME2D_ymin;			/* search rectangle							*/
static DOUBLE ME2D_ymax;			/* search rectangle							*/
static INT ME2D_rule;			/* rule to be used							*/

/*---------- working variables of 'EXT_MoveNodeEval2d' ---------------------*/
static MULTIGRID *MN_MG;		/* multigrid pointer						*/
static NODE *MN_Node;			/* moved node								*/
static DOUBLE MN_pos[2];			/* new pos of the moved node				*/
static DOUBLE MN_lambda;			/* new boundary parameter if boundary node 	*/
static DOUBLE MN_xmin;			/* limits of the picture					*/
static DOUBLE MN_xmax;			/* limits of the picture					*/
static DOUBLE MN_ymin;			/* limits of the picture					*/
static DOUBLE MN_ymax;			/* limits of the picture					*/
static DOUBLE MN_delta;			/* resolution 								*/
static INT MN_accept;			/* indicates whether correctly moved or not	*/
static INT MN_MouseMoved;		/* invert links at last pos if TRUE			*/
static INT MN_LastMousePos[2];	/* store last mouse position				*/
static short MN_Resolution;     /* resolution           					*/

/*---------- working variables of 'EW_BndEval2d' ---------------------------*/
static short BND_PlotBoundary;	/* plot boundary if TRUE					*/
static long BND_BndColor;		/* use this color for the outer boundary	*/
static long BND_InnerBndColor;	/* use this color for interior boundaries	*/
static short BND_BndLineWidth;	/* use this line width						*/
static short BND_Resolution;     /* resolution           					*/
static MULTIGRID *BND_MG;		/* mg pointer								*/

/*---------- working variables of 'VW_VecMatEval' --------------------------*/
#define VM_MARKERSIZE		6
#define VM_TEXTSIZE			(8*TextFactor)
#define VM_VECMAT_TEXTSIZE	(8*TextFactor)
#define VM_LINEFAC			1.2

static INT VM_Marker;			/* plot markers for Vectors					*/
static long VN_MarkerColor[4];	/* colors of Markers (VCLASS dependent)		*/
static INT VM_Type[MAXVECTORS];	/* plot only vectors of TRUE Types			*/
static long VM_MColor;			/* color of connections						*/
static long VM_StrongColor;		/* color of strong connections				*/
static INT VM_Connections;		/* also plot connections					*/
static INT VM_MExtra;			/* also plot extra connections				*/
static long VM_MExtraColor;		/* color of extra connections				*/
static INT VM_Idx;				/* also plot vector indices					*/
static long	VM_IdxColor;		/* color of indices							*/
static INT VM_Order;			/* plot order								*/
static long VM_OrderStart;		/* spectrum start for order					*/
static float VM_OrderDelta;		/* spectrum increment for order				*/
static INT VM_Dependency;		/* plot dependencies						*/
static VECTOR *VM_LastVector;	/* preceding vector							*/
static INT VM_lastind;			/* remember index of last vector			*/
static INT VM_ConnectVectors;	/* connect vectors to show order			*/
static long VM_ConnectColor;	/* color of vector order connections		*/
static long VM_CutColor;		/* color for cut vectors (order=2)			*/
static INT VM_VecData;			/* plot vector data							*/
static INT VM_MatData;			/* plot matrix data							*/
static VECDATA_DESC *VM_tvd;	/* vector descriptor						*/
static MATDATA_DESC *VM_tmd;	/* matrix descriptor						*/
static long VM_VecMatColor;		/* color of vector matrix data				*/

/*---------- working variables of 'Matrix' stuff ---------------------------*/
#define MAT_FRAMESIZE			5
#define MAT_TEXTSIZE			(8*TextFactor)

static MatrixEvalProcPtr MAT_eval;/* evaluation function (if no symbol)		*/
static MATDATA_DESC *MAT_md;	/* matrix descriptor (if symbol)			*/
static INT MAT_conn;			/* plot connections							*/
static INT MAT_extra;			/* plot extra connections					*/
static INT MAT_rel;				/* scale values by inverse diagonal entry	*/
static INT MAT_maxrow;			/* last row									*/
static DOUBLE MAT_factor;		/* color spectrum factor					*/
static long MAT_black;			/* black for frames and values				*/
static long MAT_white;			/* white for frames and values				*/
static long MAT_dark;			/* indicate dark colors (color>MAT_dark)	*/
static INT MAT_frame;			/* frame colored squares					*/
static INT MAT_printsize;		/* minimal square size for printing values	*/
static INT MAT_print;			/* print values								*/
static DOUBLE MAT_offset;		/* color spectrum offset					*/
static INT MAT_log;				/* take log of absolute values				*/
static DOUBLE MAT_thresh;		/* don't plot entries with |.|<thresh		*/

static BLOCKVECTOR *BV_theBV;	/* current bockvector						*/
static long BV_color;			/* black for seperating lines of blocks		*/
static DOUBLE MAT_dash;			/* length of the line segments in dashed lines */
static DOUBLE MAT_space;			/* gap between line segments in dashed lines*/

/*---------- working variables of 'EW_ElementBdryEval2D' -------------------*/
static long EB_ColorGrid;		/* color of the grid plotted with the EScala*/

/*---------- working variables of 'EW_FindElement3D' -----------------------*/
static COORD_POINT FE2D_MousePos;


/*---------- working variables of 'EW_FindNode2D' --------------------------*/
#define FN2D_INVSIZE			3
#define FN2D_ACC				3

static DOUBLE	 	*FN2D_pos;
static DOUBLE		FN2D_xmin;
static DOUBLE		FN2D_xmax;
static DOUBLE		FN2D_ymin;
static DOUBLE		FN2D_ymax;
static INT 			FN2D_found;

/*---------- working variables of 'EW_FindVector2D' ------------------------*/
#define FV2D_INVSIZE			3
#define FV2D_ACC				3

static DOUBLE_VECTOR	FV2D_pos;
static DOUBLE		FV2D_xmin;
static DOUBLE		FV2D_xmax;
static DOUBLE		FV2D_ymin;
static DOUBLE		FV2D_ymax;
static INT 			FV2D_found;

/*---------- working variables of 'EW_FindElement2D' -----------------------*/
static INT			FE2D_found;

/*---------- working variables of 'EW_FindElement3D' -----------------------*/
static COORD_POINT 	FE3D_MousePos;


/*---------- working variables of 'EW_EScalar2D' ---------------------------*/
static PreprocessingProcPtr EScalar2D_PreProcess;
static ElementEvalProcPtr EScalar2D_EvalFct;
static DOUBLE		EScalar2D_V2C_factor;
static DOUBLE		EScalar2D_V2C_offset;
static INT			EScalar2D_mode;
static INT			EScalar2D_depth;
static INT			EScalar2D_numOfContours;
static DOUBLE		EScalar2D_ContValues[PO_MAXCONTOURS];
static DOUBLE		EScalar2D_minValue;
static DOUBLE		EScalar2D_maxValue;


/*---------- working variables of 'EW_EScalar3D' ---------------------------*/
static PreprocessingProcPtr EScalar3D_PreProcess;
static ElementEvalProcPtr EScalar3D_EvalFct;
static DOUBLE		EScalar3D_V2C_factor;
static DOUBLE		EScalar3D_V2C_offset;
static INT			EScalar3D_mode;
static INT			EScalar3D_depth;
static INT			EScalar3D_numOfContours;
static DOUBLE		EScalar3D_ContValues[PO_MAXCONTOURS];
static DOUBLE		EScalar3D_minValue;
static DOUBLE		EScalar3D_maxValue;


/*---------- working variables of 'EW_Line2D' ---------------------------*/
static ElementEvalProcPtr LINE2D_EvalFct;
static DOUBLE		LINE2D_V2Y_factor;
static DOUBLE		LINE2D_V2Y_offset;
static long			LINE2D_Color;
static INT			LINE2D_depth;
static DOUBLE		LINE2D_minValue;
static DOUBLE		LINE2D_maxValue;
static COORD_POINT	LINE2D_Begin;
static COORD_POINT	LINE2D_End;
static COORD_POINT	LINE2D_BeginRot;
static COORD_POINT	LINE2D_EndRot;

/*---------- working variables of 'EW_EVector3D' ---------------------------*/
#define RASTERPOINTS_MAX		200

static ElementVectorProcPtr EVector_EvalFct;
static DOUBLE				EVector_rastersize;
static INT					EVector_cutvector;
static DOUBLE				EVector_V2L_factor;
static DOUBLE				EVector_CutLenFactor;
static long 				EVector_ColorCut;

/* 2D */
static long 				EVector2D_ColorNormal;

/* 3D */
static INT					EVector3D_projectvector;
static DOUBLE				EVector3D_V2C_factor;
static DOUBLE				EVector3D_V2C_offset;


/*---------- working variables of 'GetNode...' routines --------------------*/
static MULTIGRID	*GNode_MG;
static INT			 GNode_fromLevel;
static INT			 GNode_toLevel;

/*---------- working variables of 'GetElement...' routines -----------------*/
static MULTIGRID	*GElem_MG;
static INT			 GElem_fromLevel;
static INT			 GElem_toLevel;

/*---------- working variables of 'FindRange' routines ---------------------*/
static INT 			GEN_FR_put;
static DOUBLE		GEN_FR_min;
static DOUBLE		GEN_FR_max;


/*---------- working variables of 'WorkOnPicture' routines -----------------*/
static OUTPUTDEVICE 				*WOP_OutputDevice;
static PICTURE						*WOP_Picture;					
static PLOTOBJ						*WOP_PlotObj;
static VIEWEDOBJ					*WOP_ViewedObj;
static WORK 						*WOP_Work;
static PLOTOBJHANDLING				*WOP_PlotObjHandling;
static MULTIGRID					*WOP_MG;
static WORKPROCS					*WOP_WorkProcs;
static INT							WOP_ViewDim;
static INT							WOP_WorkMode;
static ELEMENT						*WOP_Element;
static NODE 						*WOP_Node;
static VECTOR						*WOP_Vector;
static DRAWINGOBJ 					WOP_DrawingObject[DO_SIZE];

static GEN_ExecuteProcPtr			WOP_GEN_ExecuteProc;
static GEN_PostProcessProcPtr		WOP_GEN_PostProcessProc;
static GEN_PreProcessProcPtr		WOP_GEN_PreProcessProc;
static EW_GetFirstElementProcPtr	WOP_EW_GetFirstElementProc; 
static EW_GetNextElementProcPtr 	WOP_EW_GetNextElementProc;
static EW_EvaluateProcPtr			WOP_EW_EvaluateProc;
static NW_GetFirstNodeProcPtr		WOP_NW_GetFirstNodeProc;	
static NW_GetNextNodeProcPtr		WOP_NW_GetNextNodeProc;
static NW_EvaluateProcPtr			WOP_NW_EvaluateProc;
static VW_GetFirstVectorProcPtr 	WOP_VW_GetFirstVectorProc;
static VW_GetNextVectorProcPtr		WOP_VW_GetNextVectorProc;
static VW_EvaluateProcPtr			WOP_VW_EvaluateProc;
static EXT_EvaluateProcPtr			WOP_EXT_EvaluateProc;
static RECURSIVE_EvaluateProcPtr	WOP_RECURSIVE_EvaluateProc;

/*---------- variables for parallel extensions ----------------------------*/
#ifdef ModelP
static DRAWINGOBJ          WOP_DO_Buffer[WOP_DOWN_CHANNELS+1]
                                        [DO_BUFFER_SLOTS][DO_SLOT_SIZE];
static VChannelPtr         WOP_UpChannel;
static VChannelPtr         WOP_DownChannel[WOP_DOWN_CHANNELS];
static INT                 WOP_NbDesc[WOP_DOWN_CHANNELS];
static DRAWINGOBJ          *WOP_DObjPnt;

static INT WOP_Sending  [WOP_DOWN_CHANNELS+1]; /* indicates sending from buffer i  */
static INT WOP_Receiving[WOP_DOWN_CHANNELS];   /* indicates receiving in buffer i  */
static INT WOP_NbTokens [WOP_DOWN_CHANNELS];   /* nb of tokens seen from channel i */
static INT WOP_More     [WOP_DOWN_CHANNELS+1]; /* more to receive from channel i   */
static INT WOP_Count    [WOP_DOWN_CHANNELS+1]; /* slots used in buffer i           */
static INT WOP_Front    [WOP_DOWN_CHANNELS+1]; /* next free slot in buffer i       */
static INT WOP_Rear     [WOP_DOWN_CHANNELS+1]; /* first nonempty slot in buffer i  */
static INT WOP_SError   [WOP_DOWN_CHANNELS+1]; /* error for sending on channel i   */
static INT WOP_RError   [WOP_DOWN_CHANNELS];   /* error for receiving on channel i */
static msgid WOP_Outmsg [WOP_DOWN_CHANNELS+1]; /* IDs for messages being sent      */
static msgid WOP_Inmsg  [WOP_DOWN_CHANNELS];   /* IDs for messages being received  */
static INT WOP_CurrDoLen;                      /* length of current DO             */ 

#endif


/* RCS string */
static char RCS_ID("$Header$",UG_RCS_STRING);

/****************************************************************************/
/*																			*/
/* forward declarations of functions used before they are defined			*/
/*																			*/
/****************************************************************************/

static INT PlotContourTriangle3D (ELEMENT *theElement, DOUBLE **CornersOfElem, 
								  DOUBLE *TP0, DOUBLE *TP1, DOUBLE *TP2, 
								  DOUBLE *LTP0, DOUBLE *LTP1, DOUBLE *LTP2, 
								  INT depth, DRAWINGOBJ **theDO);

/****************************************************************************/
/*D
   CreatePlotObjHandling - Allocate a new PLOTOBJHANDLING

   SYNOPSIS:
   static PLOTOBJHANDLING	*CreatePlotObjHandling (char *PlotObjTypeName)

   PARAMETERS:
.  PlotObjTypeName - the name

   DESCRIPTION:
   This function allocates a new 'PLOTOBJHANDLING'.

   RETURN VALUE:
   pointer to 'PLOTOBJHANDLING' 

   NULL
D*/										
/****************************************************************************/

static PLOTOBJHANDLING	*CreatePlotObjHandling (char *PlotObjTypeName)
{
	PLOTOBJHANDLING *poh;
	INT i;
	
	/* allocate PLOTOBJHANDLING envItem */
	poh = (PLOTOBJHANDLING*) CreatePlotObjType (PlotObjTypeName,sizeof(PLOTOBJHANDLING));
	if (poh==NULL)
		return (NULL);
	
	/* init defaults */
	for (i=0; i<nboftools; i++)
		POH_NTOOLFUNC(poh,i) = 0;
	POH_DYNAMIC_INFO(poh) = NULL;
	POH_CLICKACTION(poh)  = NULL;
	
	/* reset all works */
	for (i=0; i<NB_WORK; i++) POH_NBCYCLES(poh,i) = 0;
	
	return (poh);
}

/****************************************************************************/
/*D
   GetPlotObjHandling - Get PLOTOBJHANDLING

   SYNOPSIS:
   static PLOTOBJHANDLING *GetPlotObjHandling (char *PlotObjHandlingName);

   PARAMETERS:
.  PlotObjHandlingName - the name

   DESCRIPTION:
   This function gets 'PLOTOBJHANDLING' by name.

   RETURN VALUE:
   PLOTOBJHANDLING *
.n     pointer to 'PLOTOBJHANDLING' 
.n     NULL if error occured.
D*/
/****************************************************************************/

static PLOTOBJHANDLING	*GetPlotObjHandling (char *PlotObjHandlingName)
{
	return ((PLOTOBJHANDLING*)GetPlotObjType(PlotObjHandlingName));
}

/****************************************************************************/
/*
   PerspectiveProjection - 

   SYNOPSIS:
   static void PerspectiveProjection (DOUBLE *in, COORD_POINT *ScreenPoint);

   PARAMETERS:
.  in - input vect (3d) to transform
.  ScreenPoint - 

   DESCRIPTION:
   This function project a screen vector perspectively onto the (2d) screen.

   RETURN VALUE:
   void
*/
/****************************************************************************/

static void PerspectiveProjection (DOUBLE *in, COORD_POINT *ScreenPoint)
{
	DOUBLE k;

	k = OBS_ViewPlaneDist/(OBS_ViewPlaneDist-in[2]);
	(*ScreenPoint).x = k*in[0] + (1-k)*OBS_PerspCorr[0];
	(*ScreenPoint).y = k*in[1] + (1-k)*OBS_PerspCorr[1];
}

/****************************************************************************/
/*
   NormalProjection - Project a screen vector parallel onto the (2d) screen

   SYNOPSIS:
   static void NormalProjection (DOUBLE *in, COORD_POINT *ScreenPoint);

   PARAMETERS:
.  in - input vect (3d) to transform
.  ScreenPoint - 

   DESCRIPTION:
   This function projects a screen vector parallel onto the (2d) screen.

   RETURN VALUE:
   void											
*/
/****************************************************************************/

static void NormalProjection (DOUBLE *in, COORD_POINT *ScreenPoint)
{
	(*ScreenPoint).x = in[0];
	(*ScreenPoint).y = in[1];
}

/****************************************************************************/
/*
   BuildObsTrafo - Build observer transformation 

   SYNOPSIS:
   static INT BuildObsTrafo (PICTURE *thePicture);

   PARAMETERS:
.  thePicture -

   DESCRIPTION:
   This function builds observer transformation.

   RETURN VALUE:
   INT
.n    0 if ok
.n    1 if error occured.
*/												
/****************************************************************************/

static INT BuildObsTrafo (PICTURE *thePicture)
{
	VIEWEDOBJ *theViewedObj;
	PLOTOBJ *thePlotObj;
	DOUBLE VRS_2_PHS[16], PHS_2_VRS[16], VRS_2_SCS[16];
	DOUBLE pt[2],cpt[2];
	DOUBLE ZD[3];
	DOUBLE *MP, *XD, *YD;
	INT *LL, *UR;
	
	theViewedObj = PIC_VO(thePicture);
	if (VO_STATUS(theViewedObj) != ACTIVE) return (1);
	thePlotObj = VO_PO(theViewedObj);
	LL = PIC_GLL(thePicture);
	UR = PIC_GUR(thePicture);
	MP = VO_PMP(theViewedObj);
	XD = VO_PXD(theViewedObj);
	YD = VO_PYD(theViewedObj);
	
	switch (PO_DIM(thePlotObj))
	{
		case TYPE_2D:
			
			/* set trafo from phys. space to view reference system */
			VRS_2_PHS[0] = 2.0*XD[0];		VRS_2_PHS[3] = 2.0*YD[0];		VRS_2_PHS[6] = MP[0]-XD[0]-YD[0];
			VRS_2_PHS[1] = 2.0*XD[1];		VRS_2_PHS[4] = 2.0*YD[1];		VRS_2_PHS[7] = MP[1]-XD[1]-YD[1];
			VRS_2_PHS[2] = 0.0; 			VRS_2_PHS[5] = 0.0; 			VRS_2_PHS[8] = 1.0;
			if (M3_Invert(PHS_2_VRS,VRS_2_PHS)) return (1);
			
			/* set trafo from view reference system to screen system */
			VRS_2_SCS[0] = UR[0]-LL[0]; 	VRS_2_SCS[3] = 0.0; 			VRS_2_SCS[6] = LL[0];
			VRS_2_SCS[1] = 0.0; 			VRS_2_SCS[4] = UR[1]-LL[1]; 	VRS_2_SCS[7] = LL[1];
			VRS_2_SCS[2] = 0.0; 			VRS_2_SCS[5] = 0.0; 			VRS_2_SCS[8] = 1.0;
			
			/* set ObsTrafo and its inverse */
			M3_TIMES_M3(VRS_2_SCS,PHS_2_VRS,ObsTrafo)	
			if (M3_Invert(InvObsTrafo,ObsTrafo)) return (1);
			OBS_ProjectProc = NormalProjection;
			
			/* calculate phys. rectangle to be plotted */
			pt[_X_] = LL[_X_]; pt[_Y_] = LL[_Y_];
			V2_TRAFOM3_V2(pt,InvObsTrafo,cpt);
			PhysRect[0].x = cpt[_X_]; PhysRect[0].y = cpt[_Y_];
			pt[_X_] = UR[_X_]; pt[_Y_] = LL[_Y_];
			V2_TRAFOM3_V2(pt,InvObsTrafo,cpt);
			PhysRect[1].x = cpt[_X_]; PhysRect[1].y = cpt[_Y_];
			pt[_X_] = UR[_X_]; pt[_Y_] = UR[_Y_];
			V2_TRAFOM3_V2(pt,InvObsTrafo,cpt);
			PhysRect[2].x = cpt[_X_]; PhysRect[2].y = cpt[_Y_];
			pt[_X_] = LL[_X_]; pt[_Y_] = UR[_Y_];
			V2_TRAFOM3_V2(pt,InvObsTrafo,cpt);
			PhysRect[3].x = cpt[_X_]; PhysRect[3].y = cpt[_Y_];
			break;
			
		case TYPE_3D:
			
			/* set trafo from phys. space to view reference system */
			V3_VECTOR_PRODUCT(XD,YD,ZD)
			if (V3_Normalize(ZD)) return (1);
			VRS_2_PHS[0] = XD[0];		VRS_2_PHS[4] = YD[0];		VRS_2_PHS[8] = ZD[0];		VRS_2_PHS[12]= MP[0];
			VRS_2_PHS[1] = XD[1];		VRS_2_PHS[5] = YD[1];		VRS_2_PHS[9] = ZD[1];		VRS_2_PHS[13]= MP[1];
			VRS_2_PHS[2] = XD[2];		VRS_2_PHS[6] = YD[2];		VRS_2_PHS[10]= ZD[2];		VRS_2_PHS[14]= MP[2];
			VRS_2_PHS[3] = 0.0; 		VRS_2_PHS[7] = 0.0; 		VRS_2_PHS[11]= 0.0; 		VRS_2_PHS[15]= 1.0;
			if (M4_Invert(PHS_2_VRS,VRS_2_PHS)) return (1);
			
			/* set trafo from view reference system to screen system */
			VRS_2_SCS[0] = 0.5*(UR[0]-LL[0]); 	VRS_2_SCS[4] = 0.0; 				VRS_2_SCS[8] = 0.0; 		VRS_2_SCS[12]= 0.5*(UR[0]+LL[0]);
			VRS_2_SCS[1] = 0.0; 				VRS_2_SCS[5] = 0.5*(UR[1]-LL[1]); 	VRS_2_SCS[9] = 0.0; 		VRS_2_SCS[13]= 0.5*(UR[1]+LL[1]);
			VRS_2_SCS[2] = 0.0; 				VRS_2_SCS[6] = 0.0; 				VRS_2_SCS[10]= 1.0; 		VRS_2_SCS[14]= 0.0;
			VRS_2_SCS[3] = 0.0; 				VRS_2_SCS[7] = 0.0; 				VRS_2_SCS[11]= 0.0; 		VRS_2_SCS[15]= 1.0;

			/* set ObsTrafo and its inverse */
			M4_TIMES_M4(VRS_2_SCS,PHS_2_VRS,ObsTrafo)	
			if (M4_Invert(InvObsTrafo,ObsTrafo)) return (1);
			if (VO_PERSPECTIVE(theViewedObj) == YES)
			{
				OBS_Perspective = YES;
				OBS_ProjectProc = PerspectiveProjection;
			}
			else
			{
				OBS_Perspective = NO;
				OBS_ProjectProc = NormalProjection;
			}
			OBS_PerspCorr[0] = VRS_2_SCS[12]; OBS_PerspCorr[1] = VRS_2_SCS[13]; 
			V3_SUBTRACT(VO_VP(theViewedObj),VO_VT(theViewedObj),OBS_ViewDirection)
			V3_EUKLIDNORM(OBS_ViewDirection,OBS_ViewPlaneDist)
			if (OBS_ViewPlaneDist<SMALL_C) return (1);
			break;
			
		default:
			RETURN(1);
	}
	M4_COPY(ObsTrafo,VO_TRAFO(theViewedObj));
	M4_COPY(InvObsTrafo,VO_INVTRAFO(theViewedObj));
	
	return (0);
}
	
/****************************************************************************/
/*
   BuildCutTrafo - Build cut transformation 

   SYNOPSIS:
   static INT BuildCutTrafo (CUT *theCut, DOUBLE *theViewDir);

   PARAMETERS:
.  theCut - 
.  theViewDir - 

   DESCRIPTION:
   This function builds cut transformation.

   RETURN VALUE:
   INT
.n    0 if ok
.n    1 if error occured.
*/												
/****************************************************************************/

static INT BuildCutTrafo (CUT *theCut, DOUBLE *theViewDir)
{
	DOUBLE XD[3], YD[3], ZD[3];
	DOUBLE *PN, *PP;
	DOUBLE scpr;
	
	CUT_CutExisting = 0;
	if (theCut == NULL) return (1);
	if (CUT_STATUS(theCut)!=ACTIVE) return (0);
	CUT_CutExisting = 1;
	PN = CUT_PN(theCut);
	PP = CUT_PP(theCut);
	
	/* set trafo from phys. space to view reference system */
	V3_COPY(PN,ZD)
	if (V3_Normalize(ZD)) return (1);
	V3_Orthogonalize(ex,ZD,XD);
	if (V3_Normalize(XD))
	{
		V3_Orthogonalize(ey,ZD,XD);
		if (V3_Normalize(XD)) return (1);
	}
	V3_VECTOR_PRODUCT(ZD,XD,YD)
	if (V3_Normalize(YD)) return (1);
	InvCutTrafo[0] = XD[0]; 	InvCutTrafo[4] = YD[0]; 	InvCutTrafo[8] = ZD[0]; 	InvCutTrafo[12]= PP[0];
	InvCutTrafo[1] = XD[1]; 	InvCutTrafo[5] = YD[1]; 	InvCutTrafo[9] = ZD[1]; 	InvCutTrafo[13]= PP[1];
	InvCutTrafo[2] = XD[2]; 	InvCutTrafo[6] = YD[2]; 	InvCutTrafo[10]= ZD[2]; 	InvCutTrafo[14]= PP[2];
	InvCutTrafo[3] = 0.0;		InvCutTrafo[7] = 0.0;		InvCutTrafo[11]= 0.0;		InvCutTrafo[15]= 1.0;
	if (M4_Invert(CutTrafo,InvCutTrafo)) return (1);
	
	/* is cut plane at front? */
	V3_SCALAR_PRODUCT(theViewDir,PN,scpr)
	CUT_CutAtFront = 0; 
	if (scpr>0.0)	CUT_CutAtFront = 1;
	V3_COPY(ZD,CUT_CutNormal)
				
	return (0);
}

static INT MousePullFrame (PICTURE *thePicture, INT OldMousePos[2], DOUBLE *frame_xmin, DOUBLE *frame_xmax, DOUBLE *frame_ymin, DOUBLE *frame_ymax)
{
	UGWINDOW *ugw;
	COORD_POINT FrameLL,FrameLR,FrameUR,FrameUL;
	DOUBLE xmin,xmax,ymin,ymax;
	INT MousePos[2],LastMousePos[2];
	INT status;
	INT state;
	char buffer[128];
	
	xmin	= MIN(PIC_GLL(thePicture)[_X_],PIC_GUR(thePicture)[_X_]);
	xmax	= MAX(PIC_GLL(thePicture)[_X_],PIC_GUR(thePicture)[_X_]);
	ymin	= MIN(PIC_GLL(thePicture)[_Y_],PIC_GUR(thePicture)[_Y_]);
	ymax	= MAX(PIC_GLL(thePicture)[_Y_],PIC_GUR(thePicture)[_Y_]);
	
	status = MOUSE_NOT_MOVED;
	V2_COPY(OldMousePos,LastMousePos);
	while (MouseStillDown())
	{
		MousePosition(MousePos);
		
		if (V2_ISEQUAL(MousePos,LastMousePos)) continue;
		
		/* inside picture? */
		if ((MousePos[0]<xmin) || (MousePos[0]>xmax) || (MousePos[1]<ymin) || (MousePos[1]>ymax))
		{
			status = REJECTED;
			break;
		}
		
		if (status==MOUSE_MOVED)
		{
			/* invert last frame */
			UgInverseLine(FrameLL,FrameLR);
			UgInverseLine(FrameLR,FrameUR);
			UgInverseLine(FrameUR,FrameUL);
			UgInverseLine(FrameUL,FrameLL);
		}
		
		V2_COPY(MousePos,LastMousePos);
		
		status = MOUSE_MOVED;
		
		/* new frame */
		FrameUL.x = FrameLL.x = MousePos[_X_];
		FrameLR.x = FrameUR.x = OldMousePos[_X_];
		FrameLR.y = FrameLL.y = MousePos[_Y_];
		FrameUL.y = FrameUR.y = OldMousePos[_Y_];
		
		/* invert new frame */
		UgInverseLine(FrameLL,FrameLR);
		UgInverseLine(FrameLR,FrameUR); 		
		UgInverseLine(FrameUR,FrameUL);
		UgInverseLine(FrameUL,FrameLL);
		
		/* print dynamic info iff */
		ugw = PIC_UGW(thePicture);
		if ((VO_STATUS(PIC_VO(thePicture)) == ACTIVE) && POH_DYNAMIC_INFO_AVAIL(PIC_POH(thePicture)))
		{
			if (POH_DYNAMIC_INFO(PIC_POH(thePicture))(thePicture,UGW_CURRTOOL(ugw),UGW_CURRFUNC(ugw),MousePos,buffer)==0)
				state = MOUSE_IN_CURR_PIC;
			else
				state = STATIC_TEXT;
			if (!((state==STATIC_TEXT) && (UGW_BOXSTATE(ugw)==STATIC_TEXT)))
				DrawInfoBox(UGW_IFWINDOW(ugw),buffer);
			UGW_BOXSTATE(ugw) = state;
		}
		else if (UGW_BOXSTATE(ugw)!=STATIC_TEXT)
		{
			sprintf(buffer,"no dynamic info");
			DrawInfoBox(UGW_IFWINDOW(ugw),buffer);
			UGW_BOXSTATE(ugw) = STATIC_TEXT;
		}
		
		UgFlush();
	}
	
	if (status!=MOUSE_NOT_MOVED)
	{
		/* invert old frame */
		UgInverseLine(FrameLL,FrameLR); 		
		UgInverseLine(FrameLR,FrameUR); 		
		UgInverseLine(FrameUR,FrameUL); 		
		UgInverseLine(FrameUL,FrameLL); 		
		UgFlush();
	}
	
	if (status==REJECTED)
		return (REJECTED);
	
	if (status==MOUSE_NOT_MOVED)
	{
		/* copy old position to all corners */
		FrameUL.x = FrameLL.x = OldMousePos[_X_];
		FrameLR.x = FrameUR.x = OldMousePos[_X_];
		FrameLR.y = FrameLL.y = OldMousePos[_Y_];
		FrameUL.y = FrameUR.y = OldMousePos[_Y_];
	}
	
	/* window search rectangle */
	*frame_xmin = MIN(FrameLL.x,FrameUR.x);
	*frame_xmax = MAX(FrameLL.x,FrameUR.x);
	*frame_ymin = MIN(FrameLL.y,FrameUR.y);
	*frame_ymax = MAX(FrameLL.y,FrameUR.y);
	
	return (status);
}

/****************************************************************************/
/*
   EW_GetNextElement_hor_fw_up - Return next Element in horizontal, forward, upward direction

   SYNOPSIS:
   static ELEMENT *EW_GetNextElement_hor_fw_up (ELEMENT *theElement);

   PARAMETERS:
.  theElement - Element for which the successor is to be determined

   DESCRIPTION:
   This function returns next Element in horizontal, forward, upward direction.

   RETURN VALUE:
   ELEMENT *
.n     pointer to
.n     NULL if the argument is the last Element.
*/
/****************************************************************************/

static ELEMENT *EW_GetNextElement_hor_fw_up (ELEMENT *theElement)
{
	INT currLevel;
	
	currLevel = LEVEL(theElement);
	
	do
	{
		if (SUCCE(theElement)==NULL)
		{
			if (++currLevel>GElem_toLevel)
				return (NULL);
			else
				theElement = FIRSTELEMENT(GRID_ON_LEVEL(GElem_MG,currLevel));
		}
		else
			theElement = SUCCE(theElement);
	}
	while (!USED(theElement));
	
	return (theElement);
}

/****************************************************************************/
/*
   EW_GetNextElement_hor_fw_down - Return next Element in horizontal, forward, downward direction

   SYNOPSIS:
   static ELEMENT *EW_GetNextElement_hor_fw_down (ELEMENT *theElement);

   PARAMETERS:
.  theElement - Element for which the successor is to be determined

   DESCRIPTION:
   This function return next Element in horizontal, forward, downward direction.

   RETURN VALUE:
   ELEMENT *
.n      pointer to
.n      NULL if the argument is the last Element.
*/
/****************************************************************************/

static ELEMENT *EW_GetNextElement_hor_fw_down (ELEMENT *theElement)
{
	INT currLevel;
	
	currLevel = LEVEL(theElement);
	
	do
	{
		if (SUCCE(theElement)==NULL)
		{
			if (--currLevel<GElem_fromLevel)
				return (NULL);
			else
				theElement = FIRSTELEMENT(GRID_ON_LEVEL(GElem_MG,currLevel));
		}
		else
			theElement = SUCCE(theElement);
	}
	while (!USED(theElement));
	
	return (theElement);
}

/****************************************************************************/
/*
   EW_GetNextElement_hor_bw_up - Return next Element in horizontal, forward, upward direction

   SYNOPSIS:
   static ELEMENT *EW_GetNextElement_hor_bw_up (ELEMENT *theElement);

   PARAMETERS:
.  theElement - Element for which the successor is to be determined

   DESCRIPTION:
   This function return next Element in horizontal, forward, upward direction.

   RETURN VALUE:
   ELEMENT *
.n     pointer to
.n     NULL if the argument is the last Element.
*/
/****************************************************************************/

static ELEMENT *EW_GetNextElement_hor_bw_up (ELEMENT *theElement)
{
	INT currLevel;
	
	currLevel = LEVEL(theElement);
	
	do
	{
		if (PREDE(theElement)==NULL)
		{
			if (++currLevel>GElem_toLevel)
				return (NULL);
			else
				theElement = LASTELEMENT(GRID_ON_LEVEL(GElem_MG,currLevel));
		}
		else
			theElement = PREDE(theElement);
	}
	while (!USED(theElement));
	
	return (theElement);
}

/****************************************************************************/
/*
   EW_GetNextElement_hor_bw_down - return next Element in horizontal, forward, downward direction

   SYNOPSIS:
   static ELEMENT *EW_GetNextElement_hor_bw_down (ELEMENT *theElement)

   PARAMETERS:
.  theElement - Element for which the successor is to be determined

   DESCRIPTION:
   This function return next Element in horizontal, forward, downward direction.

   RETURN VALUE:
   ELEMENT *
.n     pointer to ELEMENT *
.n     NULL if the argument is the last Element.
*/
/****************************************************************************/

static ELEMENT *EW_GetNextElement_hor_bw_down (ELEMENT *theElement)
{
	INT currLevel;
	
	currLevel = LEVEL(theElement);
	
	do
	{
		if (PREDE(theElement)==NULL)
		{
			if (--currLevel<GElem_fromLevel)
				return (NULL);
			else
				theElement = LASTELEMENT(GRID_ON_LEVEL(GElem_MG,currLevel));
		}
		else
			theElement = PREDE(theElement);
	}
	while (!USED(theElement));
	
	return (theElement);
}
/****************************************************************************/
/*
   EW_GetNextElement_vert_fw_up - Return next Element in vertical, forward, upward direction

   SYNOPSIS:
   static ELEMENT *EW_GetNextElement_vert_fw_up (ELEMENT *theElement);

   PARAMETERS:
.  theElement - Element for which the successor is to be determined

   DESCRIPTION:
   This function return next Element in vertical, forward, upward direction.

   RETURN VALUE:
   ELEMENT *
.n     pointer to ELEMENT *
.n     NULL if the argument is the last Element.
*/
/****************************************************************************/

static ELEMENT *EW_GetNextElement_vert_fw_up (ELEMENT *theElement)
{
	do
	{
		/* something above? */
		if ((LEVEL(theElement)<GElem_toLevel) && (SON(theElement,0)!=NULL))
		{
			/* first go up */
			theElement = SON(theElement,0);
			
			/* go back while father not left */
			while ((PREDE(theElement)!=NULL) && (EFATHER(PREDE(theElement))==EFATHER(theElement)))
				theElement = PREDE(theElement);
		}
		else
		{
			/* descent while all sons of the father are processed */
			while ((LEVEL(theElement)>GElem_fromLevel)
						&& ((SUCCE(theElement)==NULL) || (EFATHER(SUCCE(theElement))!=EFATHER(theElement))))
				theElement = EFATHER(theElement);
			
			theElement = SUCCE(theElement);
			if (theElement==NULL)
				return (NULL);
		}
	}
	while (!USED(theElement));
	
	return (theElement);
}
/****************************************************************************/
/*
   EW_GetNextElement_vert_bw_up - Return next Element in vertical, backward, upward direction 

   SYNOPSIS:
   static ELEMENT *EW_GetNextElement_vert_bw_up (ELEMENT *theElement);

   PARAMETERS:
.  theElement - Element for which the successor is to be determined

   DESCRIPTION:
   This function returns next Element in vertical, backward, upward direction.	

   RETURN VALUE:
   ELEMENT *
.n      pointer to ELEMENT *
.n      NULL if the argument is the last Element.
*/
/****************************************************************************/

static ELEMENT *EW_GetNextElement_vert_bw_up (ELEMENT *theElement)
{
	do
	{
		if ((PREDE(theElement)!=NULL) && (EFATHER(PREDE(theElement))==EFATHER(theElement)))
		{
			/* predecessor has the same father */
			theElement = PREDE(theElement);
			
			/* go up as long as possible */
			while ((LEVEL(theElement)<GElem_toLevel) && (SON(theElement,0)!=NULL))
			{
				theElement = SON(theElement,0);
				
				/* go forward while father not left */
				while ((SUCCE(theElement)!=NULL) && (EFATHER(SUCCE(theElement))==EFATHER(theElement)))
					theElement = SUCCE(theElement);
			}
		}
		else
		{
			/* descent to father */
			if (LEVEL(theElement)>GElem_fromLevel)
				theElement = EFATHER(theElement);
			else
				return (NULL);
		}
	}
	while (!USED(theElement));
	
	return (theElement);
}

/****************************************************************************/
/*																			*/
/* Function:  EW_GetFirstElement_hor_fw_up									*/
/*																			*/
/* Purpose:   return first Element in horizontal, forward, upward direction */
/*																			*/
/* Input:	  the corresponding multigrid, from level, to level 			*/
/*																			*/
/* Output:	  first Element, NULL if there is none to return				*/
/*																			*/
/****************************************************************************/

static ELEMENT *EW_GetFirstElement_hor_fw_up (MULTIGRID *theMG, INT fromLevel, INT toLevel)
{
	ELEMENT *theElement;
	
	if (theMG==NULL)
		return (NULL);
	
	if (fromLevel<0)
		return (NULL);
	
	if (toLevel>TOPLEVEL(theMG))
		return (NULL);
	
	if (toLevel<fromLevel)
		return (NULL);
	
	GElem_MG		= theMG;
	GElem_fromLevel = fromLevel;
	GElem_toLevel	= toLevel;
	
	theElement = FIRSTELEMENT(GRID_ON_LEVEL(GElem_MG,fromLevel));
	
	if (theElement==NULL)
		return (NULL);
	else if (USED(theElement))
		return (theElement);
	else
		return (EW_GetNextElement_hor_fw_up(theElement));
}

/****************************************************************************/
/*
   EW_GetFirstElement_hor_fw_down - return first Element in horizontal, forward, downward direction

   SYNOPSIS:
   static ELEMENT *EW_GetFirstElement_hor_fw_down (MULTIGRID *theMG, INT fromLevel, 
   INT toLevel);

   PARAMETERS:
.  theMG - pointer to multigrid
.  fromLevel - 
.  toLevel -

   DESCRIPTION:
   This function returns first Element in horizontal, forward, downward direction.

   RETURN VALUE:
   ELEMENT *
.n      pointer to first Element
.n      NULL if there is none to return.
*/
/****************************************************************************/

static ELEMENT *EW_GetFirstElement_hor_fw_down (MULTIGRID *theMG, INT fromLevel, INT toLevel)
{
	ELEMENT *theElement;
	
	if (theMG==NULL)
		return (NULL);
	
	if (fromLevel<0)
		return (NULL);
	
	if (toLevel>TOPLEVEL(theMG))
		return (NULL);
	
	if (toLevel<fromLevel)
		return (NULL);
	
	GElem_MG		= theMG;
	GElem_fromLevel = fromLevel;
	GElem_toLevel	= toLevel;
	
	theElement = FIRSTELEMENT(GRID_ON_LEVEL(GElem_MG,toLevel));
	
	if (theElement==NULL)
		return (NULL);
	else if (USED(theElement))
		return (theElement);
	else
		return (EW_GetNextElement_hor_fw_down(theElement));
}
/****************************************************************************/
/*
   EW_GetFirstElement_hor_bw_up	- Return first Element in horizontal, backward, upward direction

   SYNOPSIS:
   static ELEMENT *EW_GetFirstElement_hor_bw_up (MULTIGRID *theMG, 
   INT fromLevel, INT toLevel);

   PARAMETERS:
.  theMG - the pointer to multigrid
.  from Level -
.  toLevel -

   DESCRIPTION:
   This function returns first Element in horizontal, backward, upward direction. 

   RETURN VALUE:
   ELEMENT *
.n     pointer to first Element
.n     NULL if there is none to return. 
*/
/****************************************************************************/

static ELEMENT *EW_GetFirstElement_hor_bw_up (MULTIGRID *theMG, INT fromLevel, INT toLevel)
{
	ELEMENT *theElement;
	
	if (theMG==NULL)
		return (NULL);
	
	if (fromLevel<0)
		return (NULL);
	
	if (toLevel>TOPLEVEL(theMG))
		return (NULL);
	
	if (toLevel<fromLevel)
		return (NULL);
	
	GElem_MG		= theMG;
	GElem_fromLevel = fromLevel;
	GElem_toLevel	= toLevel;
	
	theElement = LASTELEMENT(GRID_ON_LEVEL(GElem_MG,fromLevel));
	
	if (theElement==NULL)
		return (NULL);
	else if (USED(theElement))
		return (theElement);
	else
		return (EW_GetNextElement_hor_bw_up(theElement));
}

/****************************************************************************/
/*
   EW_GetFirstElement_hor_bw_down - Return first Element in horizontal, backward, downward direction

   SYNOPSIS:
   static ELEMENT *EW_GetFirstElement_hor_bw_down (MULTIGRID *theMG, 
   INT fromLevel, INT toLevel);

   PARAMETERS:
.  theMG - pointer to multigrid
.  fromLevel -
.  toLevel -

   DESCRIPTION:
   This function returns first Element in horizontal, backward, downward direction.

   RETURN VALUE:
   ELEMENT *
.n     pointer to first Element
.n     NULL if there is none to return.
*/
/****************************************************************************/

static ELEMENT *EW_GetFirstElement_hor_bw_down (MULTIGRID *theMG, INT fromLevel, INT toLevel)
{
	ELEMENT *theElement;
	
	if (theMG==NULL)
		return (NULL);
	
	if (fromLevel<0)
		return (NULL);
	
	if (toLevel>TOPLEVEL(theMG))
		return (NULL);
	
	if (toLevel<fromLevel)
		return (NULL);
	
	GElem_MG		= theMG;
	GElem_fromLevel = fromLevel;
	GElem_toLevel	= toLevel;
	
	theElement = LASTELEMENT(GRID_ON_LEVEL(GElem_MG,toLevel));
	
	if (theElement==NULL)
		return (NULL);
	else if (USED(theElement))
		return (theElement);
	else
		return (EW_GetNextElement_hor_bw_down(theElement));
}

/****************************************************************************/
/*
   EW_GetFirstElement_vert_fw_up - Return first Element in vertical, forward, upward direction

   SYNOPSIS:
   static ELEMENT *EW_GetFirstElement_vert_fw_up (MULTIGRID *theMG, INT fromLevel,
   INT toLevel)

   PARAMETERS:
.  theMG - 
.  fromLevel -
.  toLevel -

   DESCRIPTION:
   This function  returns first Element in vertical, forward, upward direction.

   RETURN VALUE:
   ELEMENT *
.n      pointer to first Element
.n      NULL if there is none to return. 
*/
/****************************************************************************/

static ELEMENT *EW_GetFirstElement_vert_fw_up (MULTIGRID *theMG, INT fromLevel, INT toLevel)
{
	ELEMENT *theElement;
	
	if (theMG==NULL)
		return (NULL);
	
	if (fromLevel<0)
		return (NULL);
	
	if (toLevel>TOPLEVEL(theMG))
		return (NULL);
	
	if (toLevel<fromLevel)
		return (NULL);
	
	GElem_MG		= theMG;
	GElem_fromLevel = fromLevel;
	GElem_toLevel	= toLevel;
	
	theElement = FIRSTELEMENT(GRID_ON_LEVEL(GElem_MG,fromLevel));
	
	if (theElement==NULL)
		return (NULL);
	else if (USED(theElement))
		return (theElement);
	else
		return (EW_GetNextElement_vert_fw_up(theElement));
}

/****************************************************************************/
/*
   EW_GetFirstElement_vert_bw_up -  Return first Element in vertical, backward, upward direction

   SYNOPSIS:
   static ELEMENT *EW_GetFirstElement_vert_bw_up (MULTIGRID *theMG, 
   INT fromLevel, INT toLevel);

   PARAMETERS:
.  theMG - 
.  fromLevel -
.  toLevel -

   DESCRIPTION:
   This function returns first Element in vertical, backward, upward direction.

   RETURN VALUE:
   ELEMENT *
.n      pointer to first Element
.n      NULL if there is none to return.
*/
/****************************************************************************/

static ELEMENT *EW_GetFirstElement_vert_bw_up (MULTIGRID *theMG, INT fromLevel, INT toLevel)
{
	ELEMENT *theElement;
	
	if (theMG==NULL)
		return (NULL);
	
	if (fromLevel<0)
		return (NULL);
	
	if (toLevel>TOPLEVEL(theMG))
		return (NULL);
	
	if (toLevel<fromLevel)
		return (NULL);
	
	GElem_MG		= theMG;
	GElem_fromLevel = fromLevel;
	GElem_toLevel	= toLevel;
	
	theElement = LASTELEMENT(GRID_ON_LEVEL(GElem_MG,fromLevel));
	
	if (theElement==NULL)
		return (NULL);
	
	/* go up as long as possible */
	while ((LEVEL(theElement)<GElem_toLevel) && (SON(theElement,0)!=NULL))
	{
		theElement = SON(theElement,0);
		
		/* go foreward while father not left */
		while ((SUCCE(theElement)!=NULL) && (EFATHER(SUCCE(theElement))==EFATHER(theElement)))
			theElement = SUCCE(theElement);
	}
	
	if (USED(theElement))
		return (theElement);
	else
		return (EW_GetNextElement_vert_bw_up(theElement));
}

/****************************************************************************/
/*
   CalcCrossingPoint - Calculate crossing point of two lines

   SYNOPSIS:
   static INT CalcCrossingPoint (COORD_POINT P1, COORD_POINT P2, 
   COORD_POINT P3, COORD_POINT P4, DOUBLE *alpha, DOUBLE *beta);

   PARAMETERS:
.  P1 -
.  P2 -
.  P3 -
.  P4 -
.  alpha -
.  beta -

   DESCRIPTION:
   This function calculates crossing point of two lines.

   RETURN VALUE:
   INT
.n    0 if ok
.n    1 if error occured.
*/
/****************************************************************************/

static INT CalcCrossingPoint (COORD_POINT P1, COORD_POINT P2, COORD_POINT P3, COORD_POINT P4, DOUBLE *alpha, DOUBLE *beta)
{
	INT flags1;
	DOUBLE determinante, c1, c2;
	
	/* check if one endpoint of line0 coincide with one endpoint of line1 */
	if (ABS(P1.x - P3.x)<SMALL_C && ABS(P1.y - P3.y)<SMALL_C) return(0);
	if (ABS(P1.x - P4.x)<SMALL_C && ABS(P1.y - P4.y)<SMALL_C) return(0);
	if (ABS(P2.x - P3.x)<SMALL_C && ABS(P2.y - P3.y)<SMALL_C) return(0);
	if (ABS(P2.x - P4.x)<SMALL_C && ABS(P2.y - P4.y)<SMALL_C) return(0);
	
	flags1 = 0;
	if (ABS(P1.x - P2.x)<SMALL_C) flags1 |= 1;
	if (ABS(P1.y - P2.y)<SMALL_C) flags1 |= 2;
	if (ABS(P3.x - P4.x)<SMALL_C) flags1 |= 4;
	if (ABS(P3.y - P4.y)<SMALL_C) flags1 |= 8;
	
	switch (flags1)
	{
		case(0):
			/* the natural case */
			determinante = (P2.y-P1.y)*(P4.x-P3.x) - (P2.x-P1.x)*(P4.y-P3.y);
			if (ABS(determinante)<SMALL_C)
			{
				/* the lines are parallel */
				/* check if P1 (or P2) is on line1 */
				c1 = (P1.y-P3.y)/(P4.y-P3.y);
				c2 = (P2.y-P3.y)/(P4.y-P3.y);
				if (ABS((1.0-c1)*P3.x + c1*P4.x - P1.x)<SMALL_C)
				{
					if (((c1<=0.0) && (c2<=0.0)) || ((1.0<=c1) && (1.0<=c2)))
						return (0);
					if (0.0<c1 && c1<1.0)
					{
						*beta = c1;
						*alpha = 0.0;
						return (1);
					}
					if (0.0<c2 && c2<1.0)
					{
						*beta = c2;
						*alpha = 1.0;
						return (1);
					}
					
					/* line1 is contained in line0: calculate position of P3 on line0 */
					c1 = (P3.y-P1.y)/(P2.y-P1.y);
					if (c1<=0.0 || 1.0<=c1)
						/* this is impossible! */
						return (0);
					*alpha = c1;
					*beta = 0.0;
				}
				return (0);
			}
			else
			{
				/* the lines are not parallel */
				*alpha = ((P4.x-P3.x)*(P3.y-P1.y) - (P4.y-P3.y)*(P3.x-P1.x))/determinante;
				*beta  = ((P2.x-P1.x)*(P3.y-P1.y) - (P2.y-P1.y)*(P3.x-P1.x))/determinante;
				if (0.0<*alpha && *alpha<1.0 && 0.0<*beta && *beta<1.0)
					return (1);
				return (0);
			}
		case(1):
			/* line0 is vertical */
			*beta = (P1.x-P3.x)/(P4.x-P3.x);
			*alpha = (((1.0-(*beta))*P3.y + (*beta)*P4.y)-P1.y)/(P2.y-P1.y);
			if (0.0<*alpha && *alpha<1.0 && 0.0<*beta && *beta<1.0)
				return (1);
			return (0);
		case(2):
			/* line0 is horizontal */
			*beta = (P1.y-P3.y)/(P4.y-P3.y);
			*alpha = (((1.0-(*beta))*P3.x + (*beta)*P4.x)-P1.x)/(P2.x-P1.x);
			if (0.0<*alpha && *alpha<1.0 && 0.0<*beta && *beta<1.0)
				return (1);
			return (0);
		case(3):
			/* line0 is degenerated */
			*beta = (P1.y-P3.y)/(P4.y-P3.y);
			if (ABS((1.0-(*beta))*P3.x + (*beta)*P4.x - P1.x)<SMALL_C)
				if (0.0<*beta && *beta<1.0)
				{
					*alpha = 0.5;
					return (1);
				}
			return (0);
		case(4):
			/* line1 is vertical */
			*alpha = (P3.x-P1.x)/(P2.x-P1.x);
			*beta  = (((1.0-(*alpha))*P1.y + (*alpha)*P2.y)-P3.y)/(P4.y-P3.y);
			if (0.0<*alpha && *alpha<1.0 && 0.0<*beta && *beta<1.0)
				return (1);
			return (0);
		case(5):
			/* both lines are vertical */
			if (ABS(P1.x - P3.x)<SMALL_C)
			{
				c1 = (P1.y-P3.y)/(P4.y-P3.y);
				if (0.0<c1 && c1<1.0)
				{
					*alpha = 0.0;
					*beta  = c1;
					return (1);
				}
				c2 = (P2.y-P3.y)/(P4.y-P3.y);
				if (0.0<c2 && c2<1.0)
				{
					*alpha = 1.0;
					*beta  = c2;
					return (1);
				}
				c1 = (P3.y-P1.y)/(P2.y-P1.y);
				if (0.0<c1 && c1<1.0)
				{
					*alpha = c1;
					*beta  = 0.0;
					return (1);
				}
				c2 = (P4.y-P1.y)/(P2.y-P1.y);
				if (0.0<c2 && c2<1.0)
				{
					*alpha = c2;
					*beta  = 1.0;
					return (1);
				}
				/* impossible case */
				return (0);
			}
			return (0);
		case(6):
			/* line0 is horizontal, line1 is vertical */
			*alpha = (P3.x-P1.x)/(P2.x-P1.x);
			*beta  = (P1.y-P3.y)/(P4.y-P3.y);
			if (0.0<*alpha && *alpha<1.0 && 0.0<*beta && *beta<1.0)
				return (1);
			return (0);
		case(7):
			/* line0 is degenerated, line1 is vertical */
			if (ABS(P1.x - P3.x)<SMALL_C)
			{
				*alpha = 0.5;
				*beta  = (P1.y-P3.y)/(P4.y-P3.y);
				if ( 0.0<*beta && *beta<1.0)
					return (1);
			}
			return (0);
		case(8):
			/* line1 is horizontal */
			*alpha = (P3.y-P1.y)/(P2.y-P1.y);
			*beta  = (((1.0-(*alpha))*P1.x + (*alpha)*P2.x)-P3.x)/(P4.x-P3.x);
			if (0.0<*alpha && *alpha<1.0 && 0.0<*beta && *beta<1.0)
				return (1);
			return (0);
		case(9):
			/* line0 is vertical, line1 is horizontal */
			*alpha = (P3.y-P1.y)/(P2.y-P1.y);
			*beta  = (P1.x-P3.x)/(P4.x-P3.x);
			if (0.0<*alpha && *alpha<1.0 && 0.0<*beta && *beta<1.0)
				return (1);
			return (0);
		case(10):
			/* both lines are horizontal */
			if (ABS(P1.y - P3.y)<SMALL_C)
			{
				c1 = (P1.x-P3.x)/(P4.x-P3.x);
				if (0.0<c1 && c1<1.0)
				{
					*alpha = 0.0;
					*beta  = c1;
					return (1);
				}
				c2 = (P2.x-P3.x)/(P4.x-P3.x);
				if (0.0<c2 && c2<1.0)
				{
					*alpha = 1.0;
					*beta  = c2;
					return (1);
				}
				c1 = (P3.x-P1.x)/(P2.x-P1.x);
				if (0.0<c1 && c1<1.0)
				{
					*alpha = c1;
					*beta  = 0.0;
					return (1);
				}
				c2 = (P4.x-P1.x)/(P2.x-P1.x);
				if (0.0<c2 && c2<1.0)
				{
					*alpha = c2;
					*beta  = 1.0;
					return (1);
				}
				/* impossible case */
				return (0);
			}
			return (0);
		case(11):
			/* line0 is degenerated, line1 is horizontal */
			if (ABS(P1.y - P3.y)<SMALL_C)
			{
				*alpha = 0.5;
				*beta  = (P1.x-P3.x)/(P4.x-P3.x);
				if ( 0.0<*beta && *beta<1.0)
					return (1);
			}
			return (0);
		case(12):
			/* line1 is degenerated */
			*alpha = (P3.y-P1.y)/(P2.y-P1.y);
			if (ABS((1.0-(*alpha))*P1.x + (*alpha)*P2.x - P3.x)<SMALL_C)
				if (0.0<*alpha && *alpha<1.0)
				{
					*beta = 0.5;
					return (1);
				}
			return (0);
		case(13):
			/* line0 is vertical, lin1 is degenerated */
			if (ABS(P1.x - P3.x)<SMALL_C)
			{
				*alpha = (P3.y-P1.y)/(P2.y-P1.y);
				*beta  = 0.5;
				if ( 0.0<*alpha && *alpha<1.0)
					return (1);
			}
			return (0);
		case(14):
			/* line0 is horizontal, lin1 is degenerated */
			if (ABS(P1.y - P3.y)<SMALL_C)
			{
				*alpha = (P3.x-P1.x)/(P2.x-P1.x);
				*beta  = 0.5;
				if ( 0.0<*alpha && *alpha<1.0)
					return (1);
			}
			return (0);
		case(15):
			/* both lines are degenerated */
			if (ABS(P1.x - P3.x)<SMALL_C && ABS(P1.y - P3.y)<SMALL_C)
			{
				*alpha = 0.5;
				*beta  = 0.5;
				return (1);
			}
			return (0);
	}
	return (-1);
}

/****************************************************************************/
/*
   MarkElements_MGS - Mark elements on surface of multigrid

   SYNOPSIS:
   static INT MarkElements_MGS (MULTIGRID *theMG, INT fromLevel, INT toLevel);

   PARAMETERS:
.  theMG - pointer to multigrid
.  fromLevel -
.  toLevel -

   DESCRIPTION:
   This function marks elements on surface of multigrid. 

   RETURN VALUE:
   INT
.n    0 if ok
.n    1 if error occured. 												
*/
/****************************************************************************/

static INT MarkElements_MGS (MULTIGRID *theMG, INT fromLevel, INT toLevel)
{
	ELEMENT *theElement;
	INT i;
	
	fromLevel = MAX(fromLevel,0);
	toLevel = MIN(toLevel,CURRENTLEVEL(theMG));
	
	for (i=fromLevel; i<toLevel; i++)
		for (theElement=FIRSTELEMENT(theMG->grids[i]); theElement!=NULL; theElement=SUCCE(theElement))
			if (NSONS(theElement)==0)
				SETUSED(theElement,1);
			else
				SETUSED(theElement,0);
	
	for (theElement=FIRSTELEMENT(theMG->grids[toLevel]); theElement!=NULL; theElement=SUCCE(theElement))
		SETUSED(theElement,1);
				
	return (0);
}

/****************************************************************************/
/*
   MarkElements_MGS_On_Line - Mark elements on surface of multigrid on a line

   SYNOPSIS:
   static INT MarkElements_MGS_On_Line (MULTIGRID *theMG, INT fromLevel, INT toLevel, DOUBLE *p1, DOUBLE *p2);

   PARAMETERS:
.  theMG - pointer to multigrid
.  fromLevel -
.  toLevel -
.  p1, p2 - begin and end of the line

   DESCRIPTION:
   This function marks elements on surface of multigrid lying on a line. 

   RETURN VALUE:
   INT
.n    0 if ok
.n    1 if error occured. 												
*/
/****************************************************************************/

static INT ElementISLine2D (ELEMENT *theElement, DOUBLE *p1, DOUBLE *p2)
{
	INT i, n;
	COORD_POINT P1, P2, P3, P4;
	DOUBLE alpha, beta;
	
	P1.x=p1[0]; P1.y=p1[1]; P2.x=p2[0]; P2.y=p2[1];
	n = CORNERS_OF_ELEM(theElement);
	P3.x=CVECT(MYVERTEX(CORNER(theElement,n-1)))[0]; P3.y=CVECT(MYVERTEX(CORNER(theElement,n-1)))[1];
	for (i=0; i<n; i++)
	{
		P4.x=CVECT(MYVERTEX(CORNER(theElement,i)))[0]; P4.y=CVECT(MYVERTEX(CORNER(theElement,i)))[1];
		if (CalcCrossingPoint(P1,P2,P3,P4,&alpha,&beta)) return (1);
		P3.x=P4.x; P3.y=P4.y;
	}
	return (0);
}

static INT MarkElements_MGS_On_Line (MULTIGRID *theMG, INT fromLevel, INT toLevel, DOUBLE *p1, DOUBLE *p2)
{
	ELEMENT *theElement;
	INT i;
	
	fromLevel = MAX(fromLevel,0);
	toLevel = MIN(toLevel,CURRENTLEVEL(theMG));
	
	for (i=fromLevel; i<toLevel; i++)
		for (theElement=FIRSTELEMENT(theMG->grids[i]); theElement!=NULL; theElement=SUCCE(theElement))
			if (NSONS(theElement)==0 && ElementISLine2D(theElement,p1,p2))
				SETUSED(theElement,1);
			else
				SETUSED(theElement,0);
	
	for (theElement=FIRSTELEMENT(theMG->grids[toLevel]); theElement!=NULL; theElement=SUCCE(theElement))
		if (ElementISLine2D(theElement,p1,p2))
			SETUSED(theElement,1);
		else
			SETUSED(theElement,0);
				
	return (0);
}

/****************************************************************************/
/*
   MarkElements_MGS - Mark elements on surface of multigrid 

   SYNOPSIS:
   static INT MarkElements_ID (MULTIGRID *theMG, INT id);

   PARAMETERS:
.  theMG - pointer to multigrid
.  id -
 
   DESCRIPTION:
   This function marks elements on surface of multigrid.

   RETURN VALUE:
   INT
.n     0 if ok
.n     1 if error occured.
*/
/****************************************************************************/

static INT MarkElements_ID (MULTIGRID *theMG, INT id)
{
	ELEMENT *theElement;
	INT i;
	
	for (i=0; i<=TOPLEVEL(theMG); i++)
		for (theElement=FIRSTELEMENT(theMG->grids[i]); theElement!=NULL; theElement=SUCCE(theElement))
			if (ID(theElement)==id)
				SETUSED(theElement,1);
			else
				SETUSED(theElement,0);
	
	return (0);
}

/****************************************************************************/
/*
   MarkElements2D - Mark elements on surface of multigrid specified by EE2D_Elem2P

   SYNOPSIS:
   static INT MarkElements2D (MULTIGRID *theMG, INT fromLevel, INT toLevel);

   PARAMETERS:
.  theMG -
.  fromlevel -
.  toLevel -

   DESCRIPTION:
   This function marks elements on surface of multigrid specified by EE2D_Elem2P.

   RETURN VALUE:
   INT
.n    0 if ok
.n    1 if error occured.
*/
/****************************************************************************/

static INT MarkElements2D (MULTIGRID *theMG, INT fromLevel, INT toLevel)
{
	ELEMENT *theElement;
	INT i;
	
	fromLevel = MAX(fromLevel,0);
	toLevel = MIN(toLevel,CURRENTLEVEL(theMG));
	
	if (EE2D_Elem2Plot[PLOT_ALL])
	{
		for (i=fromLevel; i<toLevel; i++)
			for (theElement=FIRSTELEMENT(theMG->grids[i]); theElement!=NULL; theElement=SUCCE(theElement))
				if (NSONS(theElement)==0 && EE2D_Elem2Plot[ECLASS(theElement)])
					SETUSED(theElement,1);
				else
					SETUSED(theElement,0);
	}
	else
	{
		for (i=fromLevel; i<toLevel; i++)
			for (theElement=FIRSTELEMENT(theMG->grids[i]); theElement!=NULL; theElement=SUCCE(theElement))
				SETUSED(theElement,0);
	}
	
	for (theElement=FIRSTELEMENT(theMG->grids[toLevel]); theElement!=NULL; theElement=SUCCE(theElement))
		if (EE2D_Elem2Plot[ECLASS(theElement)])
			SETUSED(theElement,1);
		else
			SETUSED(theElement,0);
				
	for (i=toLevel+1; i<=TOPLEVEL(theMG); i++)
		for (theElement=FIRSTELEMENT(theMG->grids[i]); theElement!=NULL; theElement=SUCCE(theElement))
			SETUSED(theElement,0);

	return (0);
}

/****************************************************************************/
/*
   MarkElements3D - Mark elements on surface of multigrid specified by EE2D_Elem2P

   SYNOPSIS:
   static INT MarkElements3D (MULTIGRID *theMG, INT fromLevel, INT toLevel);

   PARAMETERS:
.  theMG -
.  fromLevel -
.  toLevel -

   DESCRIPTION:
   This function marks elements on surface of multigrid specified by EE2D_Elem2P.

   RETURN VALUE:
   INT
.n   0 if ok
.n   1 if error occured.
*/
/****************************************************************************/

static INT MarkElements3D (MULTIGRID *theMG, INT fromLevel, INT toLevel)
{
	ELEMENT *theElement;
	INT i;
	
	fromLevel = MAX(fromLevel,0);
	toLevel = MIN(toLevel,CURRENTLEVEL(theMG));
	
	if (EE3D_Elem2Plot[PLOT_ALL])
	{
		for (i=fromLevel; i<toLevel; i++)
			for (theElement=FIRSTELEMENT(theMG->grids[i]); theElement!=NULL; theElement=SUCCE(theElement))
				if (NSONS(theElement)==0 && EE3D_Elem2Plot[ECLASS(theElement)])
					SETUSED(theElement,1);
				else
					SETUSED(theElement,0);
	}
	else
	{
		for (i=fromLevel; i<toLevel; i++)
			for (theElement=FIRSTELEMENT(theMG->grids[i]); theElement!=NULL; theElement=SUCCE(theElement))
				SETUSED(theElement,0);
	}
	
	for (theElement=FIRSTELEMENT(theMG->grids[toLevel]); theElement!=NULL; theElement=SUCCE(theElement))
		if (EE3D_Elem2Plot[ECLASS(theElement)])
			SETUSED(theElement,1);
		else
			SETUSED(theElement,0);
				
	for (i=toLevel+1; i<=TOPLEVEL(theMG); i++)
		for (theElement=FIRSTELEMENT(theMG->grids[i]); theElement!=NULL; theElement=SUCCE(theElement))
			SETUSED(theElement,0);

	return (0);
}

/****************************************************************************/
/*
   MarkElements_Level -  Mark elements on level of multigrid	

   SYNOPSIS:
   static INT MarkElements_Level (MULTIGRID *theMG, INT level);

   PARAMETERS:
.  theMG - pointer to multigrid
.  level -

   DESCRIPTION:
   This function marks elements on level of multigrid.

   RETURN VALUE:
   INT
.n    0 if ok
.n    1 when error occured.
*/
/****************************************************************************/

static INT MarkElements_Level (MULTIGRID *theMG, INT level)
{
	ELEMENT *theElement;
	INT i;
	
	level = MAX(level,0);
	level = MIN(level,CURRENTLEVEL(theMG));
	
	for (i=0; i<level; i++)
		for (theElement=FIRSTELEMENT(theMG->grids[i]); theElement!=NULL; theElement=SUCCE(theElement))
			SETUSED(theElement,0);
	
	for (theElement=FIRSTELEMENT(theMG->grids[level]); theElement!=NULL; theElement=SUCCE(theElement))
		SETUSED(theElement,1);
				
	return (0);
}

/****************************************************************************/
/*
   MarkElements_MGS_Bnd - Mark bnd elements on surface of multigrid

   SYNOPSIS:
   static INT MarkElements_MGS_Bnd (MULTIGRID *theMG, INT fromLevel, INT toLevel);

   PARAMETERS:
.  theMG - pointer to multigrid
.  fromLevel -
.  toLevel -

   DESCRIPTION:
   This function marks bnd elements on surface of multigrid.

   RETURN VALUE:
   INT
.n    0 if ok
.n    1 if error occured.
*/
/****************************************************************************/

static INT MarkElements_MGS_Bnd (MULTIGRID *theMG, INT fromLevel, INT toLevel)
{
	ELEMENT *theElement;
	INT i;
	
	fromLevel = MAX(fromLevel,0);
	toLevel = MIN(toLevel,CURRENTLEVEL(theMG));
	
	for (i=fromLevel; i<toLevel; i++)
		for (theElement=FIRSTELEMENT(theMG->grids[i]); theElement!=NULL; theElement=SUCCE(theElement))
			if (NSONS(theElement)==0 && OBJT(theElement)==BEOBJ)
				SETUSED(theElement,1);
			else
				SETUSED(theElement,0);
	
	for (theElement=FIRSTELEMENT(theMG->grids[toLevel]); theElement!=NULL; theElement=SUCCE(theElement))
			if (OBJT(theElement)==BEOBJ)
				SETUSED(theElement,1);
			else
				SETUSED(theElement,0);
				
	return (0);
}

/****************************************************************************/
/*
   MarkElements_MGS_Bnd_Cut - Mark elements on surface of multigrid lying on bnd or cut	

   SYNOPSIS:
   static INT MarkElements_MGS_Bnd_Cut (MULTIGRID *theMG, INT fromLevel, 
   INT toLevel);

   PARAMETERS:
.  theMG - pointer to multigrid
.  fromLevel -
.  toLevel -

   DESCRIPTION:
   This function marks elements on surface of multigrid lying on bnd or cut.

   RETURN VALUE:
   INT
.n    0 if ok
.n    1 if error occured.
*/
/****************************************************************************/

static INT MarkElements_MGS_Bnd_Cut (MULTIGRID *theMG, INT fromLevel, INT toLevel)
{
	ELEMENT *theElement;
	INT i;
	
	fromLevel = MAX(fromLevel,0);
	toLevel = MIN(toLevel,CURRENTLEVEL(theMG));
	
	for (i=fromLevel; i<toLevel; i++)
		for (theElement=FIRSTELEMENT(theMG->grids[i]); theElement!=NULL; theElement=SUCCE(theElement))
			if (NSONS(theElement)==0 && (OBJT(theElement)==BEOBJ || CUTMODE(theElement)==CM_INTERSECT))
				SETUSED(theElement,1);
			else
				SETUSED(theElement,0);
	
	for (theElement=FIRSTELEMENT(theMG->grids[toLevel]); theElement!=NULL; theElement=SUCCE(theElement))
			if (OBJT(theElement)==BEOBJ || CUTMODE(theElement)==CM_INTERSECT)
				SETUSED(theElement,1);
			else
				SETUSED(theElement,0);
				
	return (0);
}

/****************************************************************************/
/*
   MarkElements_MGS_Bnd_and_Cut	- Mark elements on surface of multigrid lying on bnd or cut

   SYNOPSIS:
   static INT MarkElements_MGS_Bnd_and_Cut (MULTIGRID *theMG, INT fromLevel, 
   INT toLevel);

   PARAMETERS:
.  theMG - pointer to multigrid
.  fromLevel -
.  toLevel -

   DESCRIPTION:
   This function marks elements on surface of multigrid lying on bnd or cut.

   RETURN VALUE:
   INT
.n    0 if ok
.n    1 if error occured.
*/
/****************************************************************************/

static INT MarkElements_MGS_Bnd_and_Cut (MULTIGRID *theMG, INT fromLevel, INT toLevel)
{
	ELEMENT *theElement;
	INT i;
	
	fromLevel = MAX(fromLevel,0);
	toLevel = MIN(toLevel,CURRENTLEVEL(theMG));
	
	for (i=fromLevel; i<toLevel; i++)
		for (theElement=FIRSTELEMENT(theMG->grids[i]); theElement!=NULL; theElement=SUCCE(theElement))
			if (NSONS(theElement)==0 && OBJT(theElement)==BEOBJ && CUTMODE(theElement)==CM_INTERSECT)
				SETUSED(theElement,1);
			else
				SETUSED(theElement,0);
	
	for (theElement=FIRSTELEMENT(theMG->grids[toLevel]); theElement!=NULL; theElement=SUCCE(theElement))
			if (OBJT(theElement)==BEOBJ && CUTMODE(theElement)==CM_INTERSECT)
				SETUSED(theElement,1);
			else
				SETUSED(theElement,0);
				
	return (0);
}

/****************************************************************************/
/*
   MarkElements_MGS_Cut - Mark elements on surface of multigrid being intersected

   SYNOPSIS:
   static INT MarkElements_MGS_Cut (MULTIGRID *theMG, INT fromLevel, INT toLevel);
 
   PARAMETERS:
.  theMG - pointer to multigrid
.  fromLevel -
.  toLevel -

   DESCRIPTION:
   This function marks elements on surface of multigrid being intersected.

   RETURN VALUE:
   INT
.n    0 if ok
.n    1 if error occured.
*/
/****************************************************************************/

static INT MarkElements_MGS_Cut (MULTIGRID *theMG, INT fromLevel, INT toLevel)
{
	ELEMENT *theElement;
	INT i;
	
	fromLevel = MAX(fromLevel,0);
	toLevel = MIN(toLevel,CURRENTLEVEL(theMG));
	
	for (i=fromLevel; i<toLevel; i++)
		for (theElement=FIRSTELEMENT(theMG->grids[i]); theElement!=NULL; theElement=SUCCE(theElement))
			if (NSONS(theElement)==0 && CUTMODE(theElement)==CM_INTERSECT)
				SETUSED(theElement,1);
			else
				SETUSED(theElement,0);
	
	for (theElement=FIRSTELEMENT(theMG->grids[toLevel]); theElement!=NULL; theElement=SUCCE(theElement))
		if (CUTMODE(theElement)==CM_INTERSECT)
			SETUSED(theElement,1);
		else
			SETUSED(theElement,0);
				
	return (0);
}

/****************************************************************************/
/*
   EW_GetFirstElement_vert_fw_up_Proc  - Get the GetFirstElementProc	

   SYNOPSIS:
   static EW_GetFirstElementProcPtr EW_GetFirstElement_vert_fw_up_Proc
   (VIEWEDOBJ *theViewedObj);

   PARAMETERS:
.  theViewedObj -

   DESCRIPTION:
   This function gets the GetFirstElementProc.

   RETURN VALUE:
   EW_GetFirstElementProcPtr 
.n    pointer
.n    NULL
*/
/****************************************************************************/

static EW_GetFirstElementProcPtr EW_GetFirstElement_vert_fw_up_Proc (VIEWEDOBJ *theViewedObj)
{
	return (EW_GetFirstElement_vert_fw_up);
}
/****************************************************************************/
/*
   EW_GetFirstElement_vert_bw_up_Proc - Get the GetFirstElementProc	

   SYNOPSIS:
   static EW_GetFirstElementProcPtr EW_GetFirstElement_vert_bw_up_Proc 
   (VIEWEDOBJ *theViewedObj);

   PARAMETERS:
.  theViewedObj -

   DESCRIPTION:
   This function gets the GetFirstElementProc.

   RETURN VALUE:
   EW_GetFirstElementProcPtr
.n      pointer to 
.n      Null if error occured.
*/
/****************************************************************************/
static EW_GetFirstElementProcPtr EW_GetFirstElement_vert_bw_up_Proc (VIEWEDOBJ *theViewedObj)
{
	return (EW_GetFirstElement_vert_bw_up);
}
/****************************************************************************/
/*
   EW_GetFirstElement_hor_fw_up_Proc - Get the GetFirstElementProc	

   SYNOPSIS:
   static EW_GetFirstElementProcPtr EW_GetFirstElement_hor_fw_up_Proc 
   (VIEWEDOBJ *theViewedObj);

   PARAMETERS:
.  theViewedObj -

   DESCRIPTION:
   This function gets the GetFirstElementProc.

   RETURN VALUE:
   EW_GetFirstElementProcPtr
.n      pointer to 
.n      Null if error occured.
*/
/****************************************************************************/
static EW_GetFirstElementProcPtr EW_GetFirstElement_hor_fw_up_Proc (VIEWEDOBJ *theViewedObj)
{
	return (EW_GetFirstElement_hor_fw_up);
}
/****************************************************************************/
/*
   EW_GetFirstElement_hor_fw_down_Proc - Get the GetFirstElementProc	

   SYNOPSIS:
   static EW_GetFirstElementProcPtr EW_GetFirstElement_hor_fw_down_Proc 
   (VIEWEDOBJ *theViewedObj);

   PARAMETERS:
.  theViewedObj -

   DESCRIPTION:
   This function gets the GetFirstElementProc.

   RETURN VALUE:
   EW_GetFirstElementProcPtr
.n      pointer to 
.n      Null if error occured
*/
/****************************************************************************/
static EW_GetFirstElementProcPtr EW_GetFirstElement_hor_fw_down_Proc (VIEWEDOBJ *theViewedObj)
{
	return (EW_GetFirstElement_hor_fw_down);
}
/****************************************************************************/
/*
   EW_GetFirstElement_hor_bw_up_Proc  - Get the GetFirstElementProc	

   SYNOPSIS:
   static EW_GetFirstElementProcPtr EW_GetFirstElement_hor_bw_up_Proc 
   (VIEWEDOBJ *theViewedObj);

   PARAMETERS:
.  theViewedObj -

   DESCRIPTION:
   This function gets the GetFirstElementProc.

   RETURN VALUE:
   EW_GetFirstElementProcPtr
.n      pointer to 
.n      Null if error occured.
*/
/****************************************************************************/
static EW_GetFirstElementProcPtr EW_GetFirstElement_hor_bw_up_Proc (VIEWEDOBJ *theViewedObj)
{
	return (EW_GetFirstElement_hor_bw_up);
}
/****************************************************************************/
/*
   EW_GetFirstElement_hor_bw_down_Proc  - Get the GetFirstElementProc	

   SYNOPSIS:
   static EW_GetFirstElementProcPtr EW_GetFirstElement_hor_bw_down_Proc 
   (VIEWEDOBJ *theViewedObj);

   PARAMETERS:
.  theViewedObj -

   DESCRIPTION:
   This function gets the GetFirstElementProc.

   RETURN VALUE:
   EW_GetFirstElementProcPtr
.n       pointer to 
.n       Null if error occured.
*/
/****************************************************************************/
static EW_GetFirstElementProcPtr EW_GetFirstElement_hor_bw_down_Proc (VIEWEDOBJ *theViewedObj)
{
	return (EW_GetFirstElement_hor_bw_down);
}

/****************************************************************************/
/*
   EW_GetNextElement_vert_fw_up_Proc - Get the GetNextElementProc

   SYNOPSIS:
   static EW_GetNextElementProcPtr EW_GetNextElement_vert_fw_up_Proc 
   (VIEWEDOBJ *theViewedObj);

   PARAMETERS:
.  theViewedObj - 

   DESCRIPTION:
   This function gets the GetNextElementProc.

   RETURN VALUE:
   EW_GetNextElementProcPtr
.n      pointer to 
.n      NULL, when error occured.
*/											
/****************************************************************************/

static EW_GetNextElementProcPtr EW_GetNextElement_vert_fw_up_Proc (VIEWEDOBJ *theViewedObj)
{
	return (EW_GetNextElement_vert_fw_up);
}
/****************************************************************************/
/*
   EW_GetNextElement_vert_bw_up_Proc - Get the GetNextElementProc

   SYNOPSIS:
   static EW_GetNextElementProcPtr EW_GetNextElement_vert_bw_up_Proc 
   (VIEWEDOBJ *theViewedObj);

   PARAMETERS:
.  theViewedObj - 

   DESCRIPTION:
   This function gets the GetNextElementProc.

   RETURN VALUE:
   EW_GetNextElementProcPtr
.n      pointer to 
.n      NULL, when error occured.
*/											
/****************************************************************************/
static EW_GetNextElementProcPtr EW_GetNextElement_vert_bw_up_Proc (VIEWEDOBJ *theViewedObj)
{
	return (EW_GetNextElement_vert_bw_up);
}
/****************************************************************************/
/*
   EW_GetNextElement_hor_fw_up_Proc - Get the GetNextElementProc

   SYNOPSIS:
   static EW_GetNextElementProcPtr EW_GetNextElement_hor_fw_up_Proc 
   (VIEWEDOBJ *theViewedObj);

   PARAMETERS:
.  theViewedObj - 

   DESCRIPTION:
   This function gets the GetNextElementProc.

   RETURN VALUE:
   EW_GetNextElementProcPtr
.n      pointer to 
.n      NULL if error occured.
*/											
/****************************************************************************/
static EW_GetNextElementProcPtr EW_GetNextElement_hor_fw_up_Proc (VIEWEDOBJ *theViewedObj)
{
	return (EW_GetNextElement_hor_fw_up);
}
/****************************************************************************/
/*
   EW_GetNextElement_hor_fw_down_Proc - Get the GetNextElementProc

   SYNOPSIS:
   static EW_GetNextElementProcPtr EW_GetNextElement_hor_fw_down_Proc 
   (VIEWEDOBJ *theViewedObj);

   PARAMETERS:
.  theViewedObj - 

   DESCRIPTION:
   This function gets the GetNextElementProc.

   RETURN VALUE:
   EW_GetNextElementProcPtr
.n     pointer to 
.n     NULL if error occured.
*/
/****************************************************************************/											
static EW_GetNextElementProcPtr EW_GetNextElement_hor_fw_down_Proc (VIEWEDOBJ *theViewedObj)
{
	return (EW_GetNextElement_hor_fw_down);
}

/****************************************************************************/
/*
   EW_GetNextElement_hor_bw_up_Proc - Get the GetNextElementProc

   SYNOPSIS:
   static EW_GetNextElementProcPtr EW_GetNextElement_hor_bw_up_Proc 
   (VIEWEDOBJ *theViewedObj);

   PARAMETERS:
.  theViewedObj - 

   DESCRIPTION:
   This function gets the GetNextElementProc.

   RETURN VALUE:
   EW_GetNextElementProcPtr
.n     pointer to 
.n     NULL if error occured.
*/
/****************************************************************************/		
static EW_GetNextElementProcPtr EW_GetNextElement_hor_bw_up_Proc (VIEWEDOBJ *theViewedObj)
{
	return (EW_GetNextElement_hor_bw_up);
}

/****************************************************************************/
/*
   EW_GetNextElement_hor_bw_down_Proc - Get the GetNextElementProc

   SYNOPSIS:
   static EW_GetNextElementProcPtr EW_GetNextElement_hor_bw_down_Proc 
   (VIEWEDOBJ *theViewedObj);

   PARAMETERS:
.  theViewedObj - 

   DESCRIPTION:
   This function gets the GetNextElementProc.

   RETURN VALUE:
   EW_GetNextElementProcPtr
.n      pointer to 
.n      NULL if error occured.
*/
/****************************************************************************/

static EW_GetNextElementProcPtr EW_GetNextElement_hor_bw_down_Proc (VIEWEDOBJ *theViewedObj)
{
	return (EW_GetNextElement_hor_bw_down);
}

/****************************************************************************/
/*																			*/
/* Function:  MarkNodes_MGS 												*/
/*																			*/
/* Purpose:   mark nodes on surface of multigrid							*/
/*																			*/
/* Input:	  MULTIGRID *theMG												*/
/*																			*/
/* Output:	  INT 0: ok 													*/
/*				  1: error													*/
/*																			*/
/****************************************************************************/

static INT MarkNodes_MGS (MULTIGRID *theMG, INT fromLevel, INT toLevel)
{
	NODE *theNode;
	INT i;
	
	fromLevel = MAX(fromLevel,0);
	toLevel = MIN(toLevel,CURRENTLEVEL(theMG));
	
	for (i=fromLevel; i<toLevel; i++)
		for (theNode=FIRSTNODE(theMG->grids[i]); theNode!=NULL; theNode=SUCCN(theNode))
			if (SONNODE(theNode)==NULL)
				SETUSED(theNode,1);
			else
				SETUSED(theNode,0);
	
	for (theNode=FIRSTNODE(theMG->grids[toLevel]); theNode!=NULL; theNode=SUCCN(theNode))
		SETUSED(theNode,1);
				
	return (0);
}

/****************************************************************************/
/*																			*/
/* Function:  MarkNodes_OfMarkedElem										*/
/*																			*/
/* Purpose:   mark nodes on level of multigrid								*/
/*																			*/
/* Input:	  MULTIGRID *theMG												*/
/*																			*/
/* Output:	  INT 0: ok 													*/
/*				  1: error													*/
/*																			*/
/****************************************************************************/

#define MARKMODE_ALL		0
#define MARKMODE_COPY		1
#define MARKMODE_IRREG		2
#define MARKMODE_REG		3

static INT MarkNodes_OfMarkedElem (MULTIGRID *theMG, INT fromLevel, INT toLevel, INT mode)
{
	NODE *theNode;
	ELEMENT *theElement;
	INT i, j, limit;
	
	fromLevel = MAX(fromLevel,0);
	fromLevel = MIN(fromLevel,CURRENTLEVEL(theMG));
	toLevel = MAX(toLevel,0);
	toLevel = MIN(toLevel,CURRENTLEVEL(theMG));
	
	/* set marks from elements */
	limit = 0;
	switch (mode)
	{
		case MARKMODE_ALL:
			for (i=fromLevel; i<=toLevel; i++)
				for (theNode=FIRSTNODE(theMG->grids[i]); theNode!=NULL; theNode=SUCCN(theNode))
					SETUSED(theNode,1);
			break;
		case MARKMODE_REG:
			limit = MAX(limit,RED_CLASS);
		case MARKMODE_IRREG:
			limit = MAX(limit,GREEN_CLASS);
		case MARKMODE_COPY:
			limit = MAX(limit,YELLOW_CLASS);
			for (i=fromLevel; i<=toLevel; i++)
				for (theNode=FIRSTNODE(theMG->grids[i]); theNode!=NULL; theNode=SUCCN(theNode))
					SETUSED(theNode,0);
			for (theElement=FIRSTELEMENT(theMG->grids[toLevel]); theElement!=NULL; theElement=SUCCE(theElement))
				if (ECLASS(theElement)>=limit)
					for (j=0; j<CORNERS_OF_ELEM(theElement); j++)
						SETUSED(CORNER(theElement,j),1);
			break;
		default:
			RETURN(1);
	}
	
	/* only marks on surface count */
	for (i=fromLevel; i<toLevel; i++)
		for (theNode=FIRSTNODE(theMG->grids[i]); theNode!=NULL; theNode=SUCCN(theNode))
			if (SONNODE(theNode)!=NULL)
				SETUSED(theNode,0);
	
	/* skip upper levels */
	for (i=toLevel+1; i<=TOPLEVEL(theMG); i++)
		for (theNode=FIRSTNODE(theMG->grids[i]); theNode!=NULL; theNode=SUCCN(theNode))
			SETUSED(theNode,0);
	
	return (0);
}

/****************************************************************************/
/*																			*/
/* Function:  NW_GetNextNode_hor_fw_up										*/
/*																			*/
/* Purpose:   return next Node in horizontal, forward, upward direction 	*/
/*																			*/
/* Input:	  Node for which the successor is to be determined				*/
/*																			*/
/* Output:	  next Node, NULL if the argument is the last Node				*/
/*																			*/
/****************************************************************************/

static NODE *NW_GetNextNode_hor_fw_up (NODE *theNode)
{
	INT currLevel;
	
	currLevel = LEVEL(theNode);
	
	do
	{
		if (SUCCN(theNode)==NULL)
		{
			if (++currLevel>GNode_toLevel)
				return (NULL);
			else
				theNode = FIRSTNODE(GRID_ON_LEVEL(GNode_MG,currLevel));
		}
		else
			theNode = SUCCN(theNode);
	}
	while (!USED(theNode));
	
	return (theNode);
}

/****************************************************************************/
/*																			*/
/* Function:  NW_GetNextNode_hor_fw_down									*/
/*																			*/
/* Purpose:   return next Node in horizontal, forward, downward direction	*/
/*																			*/
/* Input:	  Node for which the successor is to be determined				*/
/*																			*/
/* Output:	  next Node, NULL if the argument is the last Node				*/
/*																			*/
/****************************************************************************/

static NODE *NW_GetNextNode_hor_fw_down (NODE *theNode)
{
	INT currLevel;
	
	currLevel = LEVEL(theNode);
	
	do
	{
		if (SUCCN(theNode)==NULL)
		{
			if (--currLevel<GNode_fromLevel)
				return (NULL);
			else
				theNode = FIRSTNODE(GRID_ON_LEVEL(GNode_MG,currLevel));
		}
		else
			theNode = SUCCN(theNode);
	}
	while (!USED(theNode));
	
	return (theNode);
}

/****************************************************************************/
/*																			*/
/* Function:  NW_GetNextNode_hor_bw_up										*/
/*																			*/
/* Purpose:   return next Node in horizontal, forward, upward direction 	*/
/*																			*/
/* Input:	  Node for which the successor is to be determined				*/
/*																			*/
/* Output:	  next Node, NULL if the argument is the last Node				*/
/*																			*/
/****************************************************************************/

static NODE *NW_GetNextNode_hor_bw_up (NODE *theNode)
{
	INT currLevel;
	
	currLevel = LEVEL(theNode);
	
	do
	{
		if (PREDN(theNode)==NULL)
		{
			if (++currLevel>GNode_toLevel)
				return (NULL);
			else
				theNode = LASTNODE(GRID_ON_LEVEL(GNode_MG,currLevel));
		}
		else
			theNode = PREDN(theNode);
	}
	while (!USED(theNode));
	
	return (theNode);
}

/****************************************************************************/
/*																			*/
/* Function:  NW_GetNextNode_hor_bw_down									*/
/*																			*/
/* Purpose:   return next Node in horizontal, forward, downward direction	*/
/*																			*/
/* Input:	  Node for which the successor is to be determined				*/
/*																			*/
/* Output:	  next Node, NULL if the argument is the last Node				*/
/*																			*/
/****************************************************************************/

static NODE *NW_GetNextNode_hor_bw_down (NODE *theNode)
{
	INT currLevel;
	
	currLevel = LEVEL(theNode);
	
	do
	{
		if (PREDN(theNode)==NULL)
		{
			if (--currLevel<GNode_fromLevel)
				return (NULL);
			else
				theNode = LASTNODE(GRID_ON_LEVEL(GNode_MG,currLevel));
		}
		else
			theNode = PREDN(theNode);
	}
	while (!USED(theNode));
	
	return (theNode);
}

/****************************************************************************/
/*																			*/
/* Function:  NW_GetNextNode_leave_fw										*/
/*																			*/
/* Purpose:   return next leave-Node in forward direction					*/
/*																			*/
/* Input:	  Node for which the successor is to be determined				*/
/*																			*/
/* Output:	  next Node, NULL if the argument is the last Node				*/
/*																			*/
/****************************************************************************/

static NODE *NW_GetNextNode_leave_fw (NODE *theNode)
{
	INT currLevel;
	VERTEX *theVertex;
	
	do
	{
		if ((theVertex=SUCCV(MYVERTEX(theNode)))==NULL)
		{
			currLevel = LEVEL(MYVERTEX(theNode));
			if (++currLevel>GNode_toLevel)
				return (NULL);
			else
				theNode = TOPNODE(FIRSTVERTEX(GRID_ON_LEVEL(GNode_MG,currLevel)));
		}
		else
			theNode=TOPNODE(theVertex);
		ASSERT(theNode!=NULL);
		while (LEVEL(theNode)>GNode_toLevel && theNode!=NULL) theNode=NFATHER(theNode);
		if (theNode==NULL) return (NULL);
	}
	while (!USED(theNode));
	
	return (theNode);
}

/****************************************************************************/
/*																			*/
/* Function:  NW_GetNextNode_leave_bw										*/
/*																			*/
/* Purpose:   return next leave-Node in backward direction					*/
/*																			*/
/* Input:	  Node for which the successor is to be determined				*/
/*																			*/
/* Output:	  next Node, NULL if the argument is the last Node				*/
/*																			*/
/****************************************************************************/

static NODE *NW_GetNextNode_leave_bw (NODE *theNode)
{
	INT currLevel;
	VERTEX *theVertex;
	
	do
	{
		if ((theVertex=PREDV(MYVERTEX(theNode)))==NULL)
		{
			currLevel = LEVEL(MYVERTEX(theNode));
			if (--currLevel<GNode_fromLevel)
				return (NULL);
			else
				theNode = TOPNODE(LASTVERTEX(GRID_ON_LEVEL(GNode_MG,currLevel)));
		}
		else
			theNode = TOPNODE(theVertex); 
		while (LEVEL(theNode)>GNode_toLevel && theNode!=NULL) theNode=NFATHER(theNode);
		if (theNode==NULL) return (NULL);
	}
	while (!USED(theNode));
	
	return (theNode);
}

/****************************************************************************/
/*																			*/
/* Function:  NW_GetFirstNode_hor_fw_up 									*/
/*																			*/
/* Purpose:   return first Node in horizontal, forward, upward direction	*/
/*																			*/
/* Input:	  the corresponding multigrid, from level, to level 			*/
/*																			*/
/* Output:	  first Node, NULL if there is none to return					*/
/*																			*/
/****************************************************************************/

static NODE *NW_GetFirstNode_hor_fw_up (MULTIGRID *theMG, INT fromLevel, INT toLevel)
{
	NODE *theNode;
	
	if (theMG==NULL)
		return (NULL);
	
	if (fromLevel<0)
		return (NULL);
	
	if (toLevel>TOPLEVEL(theMG))
		return (NULL);
	
	if (toLevel<fromLevel)
		return (NULL);
	
	GNode_MG		= theMG;
	GNode_fromLevel = fromLevel;
	GNode_toLevel	= toLevel;
	
	theNode = FIRSTNODE(GRID_ON_LEVEL(GNode_MG,fromLevel));
	
	if (theNode==NULL)
		return (NULL);
	else if (USED(theNode))
		return (theNode);
	else
		return (NW_GetNextNode_hor_fw_up(theNode));
}

/****************************************************************************/
/*																			*/
/* Function:  NW_GetFirstNode_hor_fw_down									*/
/*																			*/
/* Purpose:   return first Node in horizontal, forward, downward directio	*/
/*																			*/
/* Input:	  the corresponding multigrid, from level, to level 			*/
/*																			*/
/* Output:	  first Node, NULL if there is none to return					*/
/*																			*/
/****************************************************************************/

static NODE *NW_GetFirstNode_hor_fw_down (MULTIGRID *theMG, INT fromLevel, INT toLevel)
{
	NODE *theNode;
	
	if (theMG==NULL)
		return (NULL);
	
	if (fromLevel<0)
		return (NULL);
	
	if (toLevel>TOPLEVEL(theMG))
		return (NULL);
	
	if (toLevel<fromLevel)
		return (NULL);
	
	GNode_MG		= theMG;
	GNode_fromLevel = fromLevel;
	GNode_toLevel	= toLevel;
	
	theNode = FIRSTNODE(GRID_ON_LEVEL(GNode_MG,toLevel));
	
	if (theNode==NULL)
		return (NULL);
	else if (USED(theNode))
		return (theNode);
	else
		return (NW_GetNextNode_hor_fw_down(theNode));
}

/****************************************************************************/
/*																			*/
/* Function:  NW_GetFirstNode_hor_bw_up 									*/
/*																			*/
/* Purpose:   return first Node in horizontal, backward, upward direction	*/
/*																			*/
/* Input:	  the corresponding multigrid, from level, to level 			*/
/*																			*/
/* Output:	  first Node, NULL if there is none to return					*/
/*																			*/
/****************************************************************************/

static NODE *NW_GetFirstNode_hor_bw_up (MULTIGRID *theMG, INT fromLevel, INT toLevel)
{
	NODE *theNode;
	
	if (theMG==NULL)
		return (NULL);
	
	if (fromLevel<0)
		return (NULL);
	
	if (toLevel>TOPLEVEL(theMG))
		return (NULL);
	
	if (toLevel<fromLevel)
		return (NULL);
	
	GNode_MG		= theMG;
	GNode_fromLevel = fromLevel;
	GNode_toLevel	= toLevel;
	
	theNode = LASTNODE(GRID_ON_LEVEL(GNode_MG,fromLevel));
	
	if (theNode==NULL)
		return (NULL);
	else if (USED(theNode))
		return (theNode);
	else
		return (NW_GetNextNode_hor_bw_up(theNode));
}

/****************************************************************************/
/*																			*/
/* Function:  NW_GetFirstNode_hor_bw_down									*/
/*																			*/
/* Purpose:   return first Node in horizontal, backward, downward directi	*/
/*																			*/
/* Input:	  the corresponding multigrid, from level, to level 			*/
/*																			*/
/* Output:	  first Node, NULL if there is none to return					*/
/*																			*/
/****************************************************************************/

static NODE *NW_GetFirstNode_hor_bw_down (MULTIGRID *theMG, INT fromLevel, INT toLevel)
{
	NODE *theNode;
	
	if (theMG==NULL)
		return (NULL);
	
	if (fromLevel<0)
		return (NULL);
	
	if (toLevel>TOPLEVEL(theMG))
		return (NULL);
	
	if (toLevel<fromLevel)
		return (NULL);
	
	GNode_MG		= theMG;
	GNode_fromLevel = fromLevel;
	GNode_toLevel	= toLevel;
	
	theNode = LASTNODE(GRID_ON_LEVEL(GNode_MG,toLevel));
	
	if (theNode==NULL)
		return (NULL);
	else if (USED(theNode))
		return (theNode);
	else
		return (NW_GetNextNode_hor_bw_down(theNode));
}

/****************************************************************************/
/*																			*/
/* Function:  NW_GetFirstNode_leave_fw										*/
/*																			*/
/* Purpose:   return first leave-Node in forward direction					*/
/*																			*/
/* Input:	  the corresponding multigrid, from level, to level 			*/
/*																			*/
/* Output:	  first Node, NULL if there is none to return					*/
/*																			*/
/****************************************************************************/

static NODE *NW_GetFirstNode_leave_fw (MULTIGRID *theMG, INT fromLevel, INT toLevel)
{
	NODE *theNode;
	
	if (theMG==NULL)
		return (NULL);
	
	if (fromLevel<0)
		return (NULL);
	
	if (toLevel>TOPLEVEL(theMG))
		return (NULL);
	
	if (toLevel<fromLevel)
		return (NULL);
	
	GNode_MG		= theMG;
	GNode_fromLevel = fromLevel;
	GNode_toLevel	= toLevel;
	
	theNode = TOPNODE(FIRSTVERTEX(GRID_ON_LEVEL(theMG,fromLevel)));
	while (LEVEL(theNode)>GNode_toLevel && theNode!=NULL) theNode=NFATHER(theNode);
	if (theNode==NULL) return (NULL);
	
	if (theNode==NULL)
		return (NULL);
	else if (USED(theNode))
		return (theNode);
	else
		return (NW_GetNextNode_leave_fw(theNode));
}

/****************************************************************************/
/*																			*/
/* Function:  NW_GetFirstNode_leave_bw										*/
/*																			*/
/* Purpose:   return first leave-Node in backward direction 				*/
/*																			*/
/* Input:	  the corresponding multigrid, from level, to level 			*/
/*																			*/
/* Output:	  first Node, NULL if there is none to return					*/
/*																			*/
/****************************************************************************/

static NODE *NW_GetFirstNode_leave_bw (MULTIGRID *theMG, INT fromLevel, INT toLevel)
{
	NODE *theNode;
	
	if (theMG==NULL)
		return (NULL);
	
	if (fromLevel<0)
		return (NULL);
	
	if (toLevel>TOPLEVEL(theMG))
		return (NULL);
	
	if (toLevel<fromLevel)
		return (NULL);
	
	GNode_MG		= theMG;
	GNode_fromLevel = fromLevel;
	GNode_toLevel	= toLevel;
	
	theNode = LASTNODE(GRID_ON_LEVEL(GNode_MG,fromLevel));
	while (LEVEL(theNode)>GNode_toLevel && theNode!=NULL) theNode=NFATHER(theNode);
	if (theNode==NULL) return (NULL);
	
	if (theNode==NULL)
		return (NULL);
	
	theNode = TOPNODE(LASTVERTEX(GRID_ON_LEVEL(theMG,toLevel)));
	
	if (USED(theNode))
		return (theNode);
	else
		return (NW_GetNextNode_leave_bw(theNode));
}

/****************************************************************************/
/*																			*/
/* Function:  GetFirstNode-Proc 											*/
/*																			*/
/* Purpose:   get the GetFirstNodeProc										*/
/*																			*/
/* Input:	  NODE *theNode, char *theDrawingObject 						*/
/*																			*/
/* Output:	  NULL: error													*/
/*																			*/
/****************************************************************************/

static NW_GetFirstNodeProcPtr NW_GetFirstNode_leave_fw_Proc (VIEWEDOBJ *theViewedObj)
{
	return (NW_GetFirstNode_leave_fw);
}

static NW_GetFirstNodeProcPtr NW_GetFirstNode_leave_bw_Proc (VIEWEDOBJ *theViewedObj)
{
	return (NW_GetFirstNode_leave_bw);
}

static NW_GetFirstNodeProcPtr NW_GetFirstNode_hor_fw_up_Proc (VIEWEDOBJ *theViewedObj)
{
	return (NW_GetFirstNode_hor_fw_up);
}

static NW_GetFirstNodeProcPtr NW_GetFirstNode_hor_fw_down_Proc (VIEWEDOBJ *theViewedObj)
{
	return (NW_GetFirstNode_hor_fw_down);
}

static NW_GetFirstNodeProcPtr NW_GetFirstNode_hor_bw_up_Proc (VIEWEDOBJ *theViewedObj)
{
	return (NW_GetFirstNode_hor_bw_up);
}

static NW_GetFirstNodeProcPtr NW_GetFirstNode_hor_bw_down_Proc (VIEWEDOBJ *theViewedObj)
{
	return (NW_GetFirstNode_hor_bw_down);
}

/****************************************************************************/
/*																			*/
/* Function:  GetNextNode-Proc												*/
/*																			*/
/* Purpose:   get the GetNextNodeProc										*/
/*																			*/
/* Input:	  NODE *theNode, char *theDrawingObject 						*/
/*																			*/
/* Output:	  NULL: error													*/
/*																			*/
/****************************************************************************/

static NW_GetNextNodeProcPtr NW_GetNextNode_leave_fw_Proc (VIEWEDOBJ *theViewedObj)
{
	return (NW_GetNextNode_leave_fw);
}

static NW_GetNextNodeProcPtr NW_GetNextNode_leave_bw_Proc (VIEWEDOBJ *theViewedObj)
{
	return (NW_GetNextNode_leave_bw);
}

static NW_GetNextNodeProcPtr NW_GetNextNode_hor_fw_up_Proc (VIEWEDOBJ *theViewedObj)
{
	return (NW_GetNextNode_hor_fw_up);
}

static NW_GetNextNodeProcPtr NW_GetNextNode_hor_fw_down_Proc (VIEWEDOBJ *theViewedObj)
{
	return (NW_GetNextNode_hor_fw_down);
}

static NW_GetNextNodeProcPtr NW_GetNextNode_hor_bw_up_Proc (VIEWEDOBJ *theViewedObj)
{
	return (NW_GetNextNode_hor_bw_up);
}

static NW_GetNextNodeProcPtr NW_GetNextNode_hor_bw_down_Proc (VIEWEDOBJ *theViewedObj)
{
	return (NW_GetNextNode_hor_bw_down);
}

/****************************************************************************/
/*																			*/
/* Function:  VW_GetFirstVector												*/
/*																			*/
/* Purpose:   return first Vector of toLevel								*/
/*																			*/
/* Input:	  the corresponding multigrid, from level, to level 			*/
/*																			*/
/* Output:	  first Vector, NULL if there is none to return					*/
/*																			*/
/****************************************************************************/

static VECTOR *VW_GetFirstVector (MULTIGRID *theMG, INT fromLevel, INT toLevel)
{
	return (FIRSTVECTOR(GRID_ON_LEVEL(theMG,toLevel)));
}

/****************************************************************************/
/*																			*/
/* Function:  VW_GetNextVector												*/
/*																			*/
/* Purpose:   return SUCCVC of the argument									*/
/*																			*/
/* Input:	  Vector for which the successor is to be determined			*/
/*																			*/
/* Output:	  next Vector, NULL if the argument is the last Vector			*/
/*																			*/
/****************************************************************************/

static VECTOR *VW_GetNextVector (VECTOR *theVec)
{
	return (SUCCVC(theVec));
}

/****************************************************************************/
/*																			*/
/* Function:  GetFirstVector-Proc 											*/
/*																			*/
/* Purpose:   get the GetFirstVectorProc									*/
/*																			*/
/* Input:	  VIEWEDOBJ *theViewedObj										*/
/*																			*/
/* Output:	  NULL: error													*/
/*																			*/
/****************************************************************************/

static VW_GetFirstVectorProcPtr VW_GetFirstVector_Proc (VIEWEDOBJ *theViewedObj)
{
	return (VW_GetFirstVector);
}

/****************************************************************************/
/*																			*/
/* Function:  GetNextVector-Proc											*/
/*																			*/
/* Purpose:   get the GetNextVectorProc										*/
/*																			*/
/* Input:	  VIEWEDOBJ *theViewedObj				 						*/
/*																			*/
/* Output:	  NULL: error													*/
/*																			*/
/****************************************************************************/

static VW_GetNextVectorProcPtr VW_GetNextVector_Proc (VIEWEDOBJ *theViewedObj)
{
	return (VW_GetNextVector);
}

/****************************************************************************/
/*
   EW_DoNothing0D - Do nothing	

   SYNOPSIS:
   static INT EW_DoNothing0D (DRAWINGOBJ *q)

   PARAMETERS:
.  q - the drawing object

   DESCRIPTION:
   This function does nothing (just for programming new ones).

   RETURN VALUE:
   INT

   0 when ok

   1 when erro occured
   */
/****************************************************************************/

static INT EW_DoNothing0D (DRAWINGOBJ *q)
{
	INT end;
	
	end = 0;
	while (!end)
	{
		switch (DO_2c(q))
		{
			case DO_NO_INST:
				end = 1;
				break;
			case DO_RANGE:
				DO_inc_RANGE(q);
				break;
			case DO_LINE:
				DO_inc_LINE(q,0);
				break;
			case DO_STYLED_LINE:
				DO_inc_STYLED_LINE(q,0);
				break;
			case DO_ARROW:
				DO_inc_ARROW(q,0);
				break;
			case DO_DEPEND:
				DO_inc_DEPEND(q,0);
				break;
			case DO_INVERSE_LINE:
				DO_inc_INVERSE_LINE(q,0);
				break;
			case DO_INVERSE_POLYLINE:
				DO_inc_INVERSE_POLYLINE(q,0);
				break;
			case DO_POLYLINE:
				DO_inc_POLYLINE(q,0);
				break;
			case DO_TEXT:
				DO_inc_TEXT(q,0);
				break;
			case DO_POLYMARK:
				DO_inc_POLYMARK(q,0);
				break;
			case DO_POLYGON:
				DO_inc_POLYGON(q,0);
				break;
			case DO_ERASE_SURRPOLYGON:
				DO_inc_ERASE_SURRPOLYGON(q,0);
				break;
			case DO_INVERSE_POLYGON:
				DO_inc_INVERSE_POLYGON(q,0);
				break;
			case DO_ERASE_POLYGON:
				DO_inc_ERASE_POLYGON(q,0);
				break;
			case DO_SURRPOLYGON:
				DO_inc_SURRPOLYGON(q,0);
				break;
			default:
				RETURN(1);
		}
		
	}
	
	return (0);
}

/****************************************************************************/
/*
   Draw2D - Draw content of a 2D drawing object 

   SYNOPSIS:
   static INT Draw2D (DRAWINGOBJ *q)

   PARAMETERS:
.  q - the drawing object

   DESCRIPTION:
   This function draws content of a 2D drawing object.

   RETURN VALUE:
   INT

   0 when ok

   1 when error occured
   */
/****************************************************************************/

static INT Draw2D (DRAWINGOBJ *q)
{
	INT j, n, centered, end, mode;
	DOUBLE help[2],norm;
	COORD_POINT a, b, point[MAX_POINTS_OF_POLY];
	long color;
	
	end = 0;
	while (!end)
	{
		switch (DO_2c(q))
		{
			case DO_NO_INST:
				end = 1;
				break;
			case DO_WAIT:
				DO_inc(q);
				UgFlush();
				UgWait(WAIT_001);
				break;
			case DO_RANGE:
				DO_inc_RANGE(q);
				break;
			case DO_LINE:
				DO_inc(q)
				UgSetColor(DO_2l(q)); DO_inc(q);
				V2_TRAFOM3_V2(DO_2Cp(q),ObsTrafo,help); DO_inc_n(q,2);
				(*OBS_ProjectProc)(help,&a);
				UgMove(a);
				V2_TRAFOM3_V2(DO_2Cp(q),ObsTrafo,help); DO_inc_n(q,2);
				(*OBS_ProjectProc)(help,&a);
				UgDraw(a);
				break;
			case DO_STYLED_LINE:
				DO_inc(q)
				UgSetColor(DO_2l(q)); DO_inc(q);
				V2_TRAFOM3_V2(DO_2Cp(q),ObsTrafo,help); DO_inc_n(q,2);
				(*OBS_ProjectProc)(help,&a);
				V2_TRAFOM3_V2(DO_2Cp(q),ObsTrafo,help); DO_inc_n(q,2);
				(*OBS_ProjectProc)(help,&b);
				help[0] = DO_2C(q); DO_inc_n(q,1);
				help[1] = DO_2C(q); DO_inc_n(q,1);
				UgStyledLine(a,b,help[0],help[1]);
				break;
			case DO_ARROW:
				DO_inc(q)
				UgSetColor(DO_2l(q)); DO_inc(q);
				V2_TRAFOM3_V2(DO_2Cp(q),ObsTrafo,help); DO_inc_n(q,2);
				(*OBS_ProjectProc)(help,point);
				V2_TRAFOM3_V2(DO_2Cp(q),ObsTrafo,help); DO_inc_n(q,2);
				(*OBS_ProjectProc)(help,point+3);
				point[1].x = ARR_ALPHA*point[3].x + (1.0-ARR_ALPHA)*point[0].x;
				point[1].y = ARR_ALPHA*point[3].y + (1.0-ARR_ALPHA)*point[0].y;
				a.x = point[3].x-point[1].x; a.y = point[3].y-point[1].y;
				point[2].x = point[1].x + ARR_COS*a.x - ARR_SIN*a.y;
				point[2].y = point[1].y + ARR_SIN*a.x + ARR_COS*a.y;
				point[4].x = point[1].x + ARR_COS*a.x + ARR_SIN*a.y;
				point[4].y = point[1].y - ARR_SIN*a.x + ARR_COS*a.y;
				point[5].x = point[1].x; point[5].y = point[1].y;
				UgPolyLine(point,6);
				break;
			case DO_DEPEND:
				DO_inc(q)
				UgSetColor(DO_2l(q)); DO_inc(q);
				V2_TRAFOM3_V2(DO_2Cp(q),ObsTrafo,help); DO_inc_n(q,2);
				(*OBS_ProjectProc)(help,point);
				V2_TRAFOM3_V2(DO_2Cp(q),ObsTrafo,help); DO_inc_n(q,2);
				(*OBS_ProjectProc)(help,point+1);
				UgPolyLine(point,2);
				point[1].x = DEP_POS*point[1].x + (1.0-DEP_POS)*point[0].x;
				point[1].y = DEP_POS*point[1].y + (1.0-DEP_POS)*point[0].y;
				a.x = point[1].x-point[0].x; a.y = point[1].y-point[0].y;
				norm = sqrt(a.x*a.x+a.y*a.y);
				a.x *= DEP_LEN/norm; a.y *= DEP_LEN/norm;
				point[0].x = point[1].x + DEP_COS*a.x - DEP_SIN*a.y;
				point[0].y = point[1].y + DEP_SIN*a.x + DEP_COS*a.y;
				point[2].x = point[1].x + DEP_COS*a.x + DEP_SIN*a.y;
				point[2].y = point[1].y - DEP_SIN*a.x + DEP_COS*a.y;
				UgPolyLine(point,3);
				break;
			case DO_INVERSE_LINE:
				DO_inc(q)
				V2_TRAFOM3_V2(DO_2Cp(q),ObsTrafo,help); DO_inc_n(q,2);
				(*OBS_ProjectProc)(help,&a);
				V2_TRAFOM3_V2(DO_2Cp(q),ObsTrafo,help); DO_inc_n(q,2);
				(*OBS_ProjectProc)(help,&b);
				UgInverseLine(a,b); 		
				break;
			case DO_INVERSE_POLYLINE:
				DO_inc(q)
				n = DO_2c(q); DO_inc(q)
				V2_TRAFOM3_V2(DO_2Cp(q),ObsTrafo,help); DO_inc_n(q,2);
				(*OBS_ProjectProc)(help,&a);
				for (j=1; j<n; j++)
				{
					V2_TRAFOM3_V2(DO_2Cp(q),ObsTrafo,help); DO_inc_n(q,2);
					(*OBS_ProjectProc)(help,&b);
					UgInverseLine(a,b);
					a = b;
				}
				break;
			case DO_POLYLINE:
				DO_inc(q)
				n = DO_2c(q); DO_inc(q)
				UgSetColor(DO_2l(q)); DO_inc(q)
				V2_TRAFOM3_V2(DO_2Cp(q),ObsTrafo,help); DO_inc_n(q,2);
				(*OBS_ProjectProc)(help,&a);
				UgMove(a);
				for (j=1; j<n; j++)
				{
					V2_TRAFOM3_V2(DO_2Cp(q),ObsTrafo,help); DO_inc_n(q,2);
					(*OBS_ProjectProc)(help,&a);
					UgDraw(a);
				}
				break;
			case DO_POLYGON:
				DO_inc(q)
				n = DO_2c(q); DO_inc(q)
				UgSetColor(DO_2l(q)); DO_inc(q);
				for (j=0; j<n; j++)
				{
					V2_TRAFOM3_V2(DO_2Cp(q),ObsTrafo,help); DO_inc_n(q,2);
					(*OBS_ProjectProc)(help,point+j);
				}
				UgPolygon(point,j);
				break;
			case DO_ERASE_POLYGON:
				DO_inc(q)
				n = DO_2c(q); DO_inc(q)
				for (j=0; j<n; j++)
				{
					V2_TRAFOM3_V2(DO_2Cp(q),ObsTrafo,help); DO_inc_n(q,2);
					(*OBS_ProjectProc)(help,point+j);
				}
				UgErasePolygon(point,j);
				break;
			case DO_INVERSE_POLYGON:
				DO_inc(q)
				n = DO_2c(q); DO_inc(q)
				for (j=0; j<n; j++)
				{
					V2_TRAFOM3_V2(DO_2Cp(q),ObsTrafo,help); DO_inc_n(q,2);
					(*OBS_ProjectProc)(help,point+j);
				}
				UgInversePolygon(point,j);
				break;
			case DO_ERASE_SURRPOLYGON:
				DO_inc(q)
				n = DO_2c(q); DO_inc(q)
				UgSetColor(DO_2l(q)); DO_inc(q);
				for (j=0; j<n; j++)
				{
					V2_TRAFOM3_V2(DO_2Cp(q),ObsTrafo,help); DO_inc_n(q,2);
					(*OBS_ProjectProc)(help,point+j);
				}
				UgErasePolygon(point,j);
				point[j].x=point[0].x; point[j].y=point[0].y;
				UgPolyLine(point,j+1);
				break;
			case DO_SURRPOLYGON:
				DO_inc(q)
				n = DO_2c(q); DO_inc(q)
				UgSetColor(DO_2l(q)); DO_inc(q);
				color = DO_2l(q); DO_inc(q);
				for (j=0; j<n; j++)
				{
					V2_TRAFOM3_V2(DO_2Cp(q),ObsTrafo,help); DO_inc_n(q,2);
					(*OBS_ProjectProc)(help,point+j);
				}
				UgPolygon(point,j);
				UgSetColor(color);
				point[j].x=point[0].x; point[j].y=point[0].y;
				UgPolyLine(point,j+1);
				break;
			case DO_TEXT:
				DO_inc(q)
				UgSetColor(DO_2l(q)); DO_inc(q);
				mode = DO_2c(q); DO_inc(q)
				centered = DO_2c(q); DO_inc(q)
				UgSetTextSize(DO_2s(q)); DO_inc(q);
				V2_TRAFOM3_V2(DO_2Cp(q),ObsTrafo,help); DO_inc_n(q,2);
				(*OBS_ProjectProc)(help,&a);
				if (centered)
					UgCenteredText(a,(char*)q,mode);
				else
				{
					UgMove(a);
					UgText((char*)q,mode);
				}
				DO_inc_str(q);
				break;
			case DO_POLYMARK:
				DO_inc(q)
				n = DO_2c(q); DO_inc(q)
				UgSetColor(DO_2l(q)); DO_inc(q);
				UgSetMarker(DO_2s(q)); DO_inc(q);
				UgSetMarkerSize(DO_2s(q)); DO_inc(q);
				for (j=0; j<n; j++)
				{
					V2_TRAFOM3_V2(DO_2Cp(q),ObsTrafo,help); DO_inc_n(q,2);
					(*OBS_ProjectProc)(help,point+j);
				}
				UgPolymark(point,n);
				break;
			default:
				RETURN(1);
		}
	}
	
	return (0);
}

/****************************************************************************/
/*																			*/
/* Function:  NW_SelectNode2D												*/
/*																			*/
/* Purpose:   select a node 												*/
/*																			*/
/* Input:	  char *q: the drawing object									*/
/*																			*/
/* Output:	  INT 0: ok 													*/
/*				  1: error													*/
/*																			*/
/****************************************************************************/

static INT NW_SelectNode2D (DRAWINGOBJ *q)
{
	DOUBLE help[2];
	COORD_POINT a, point[4];
	
	V2_TRAFOM3_V2(FN2D_pos,ObsTrafo,help);
	(*OBS_ProjectProc)(help,&a);
	
	/* in rectangle? */
	if ((FN2D_xmin<=a.x) && (a.x<=FN2D_xmax))
		if ((FN2D_ymin<=a.y) && (a.y<=FN2D_ymax))
		{
			if (FN2D_found>=MAXSELECTION)
				return (1);
			
			/* if found, put in/delete from selection list and invert */
			if (SELECTIONMODE(WOP_MG)!=nodeSelection)
				ClearSelection(WOP_MG); 	
			if (AddNodeToSelection(WOP_MG,WOP_Node) == GM_ERROR)
				if (RemoveNodeFromSelection(WOP_MG,WOP_Node) == GM_ERROR)
					return (1);
			
			/* invert surrounding of node */
			point[0].x = point[3].x = a.x-FN2D_INVSIZE;
			point[0].y = point[1].y = a.y-FN2D_INVSIZE;
			point[2].x = point[1].x = a.x+FN2D_INVSIZE;
			point[2].y = point[3].y = a.y+FN2D_INVSIZE;
			UgInversePolygon(point,4);
			
			/* we have found a node */
			FN2D_found++;
		}
	
	return (0);
}

/****************************************************************************/
/*
   Draw3D - Draw content of a 2D drawing object	

   SYNOPSIS:
   static INT Draw3D (DRAWINGOBJ *q);

   PARAMETERS:
.  q - the drawing object

   DESCRIPTION:
   This function draws content of a 2D drawing object.	

   RETURN VALUE:
   INT
.n    0 if ok
.n    1 if error occured.
*/												
/****************************************************************************/

static INT Draw3D (DRAWINGOBJ *q)
{
	INT j, n, centered, end, mode;
	DOUBLE help[3];
	COORD_POINT a, b, point[MAX_POINTS_OF_POLY];
	long color;
	
	end = 0;
	while (!end)
	{
		switch (DO_2c(q))
		{
			case DO_NO_INST:
				end = 1;
				break;
			case DO_WAIT:
				DO_inc(q);
				UgFlush();
				UgWait(WAIT_001);
				break;
			case DO_RANGE:
				DO_inc_RANGE(q);
				break;
			case DO_LINE:
				DO_inc(q)
				UgSetColor(DO_2l(q)); DO_inc(q);
				V3_TRAFOM4_V3(DO_2Cp(q),ObsTrafo,help); DO_inc_n(q,3);
				(*OBS_ProjectProc)(help,&a);
				UgMove(a);
				V3_TRAFOM4_V3(DO_2Cp(q),ObsTrafo,help); DO_inc_n(q,3);
				(*OBS_ProjectProc)(help,&a);
				UgDraw(a);
				break;
			case DO_STYLED_LINE:
				DO_inc(q)
				UgSetColor(DO_2l(q)); DO_inc(q);
				V3_TRAFOM4_V3(DO_2Cp(q),ObsTrafo,help); DO_inc_n(q,3);
				(*OBS_ProjectProc)(help,&a);
				V3_TRAFOM4_V3(DO_2Cp(q),ObsTrafo,help); DO_inc_n(q,3);
				(*OBS_ProjectProc)(help,&b);
				help[0] = DO_2C(q); DO_inc_n(q,1);
				help[1] = DO_2C(q); DO_inc_n(q,1);
				UgStyledLine(a,b,help[0],help[1]);
				break;
			case DO_ARROW:
				DO_inc(q)
				UgSetColor(DO_2l(q)); DO_inc(q);
				V3_TRAFOM4_V3(DO_2Cp(q),ObsTrafo,help); DO_inc_n(q,3);
				(*OBS_ProjectProc)(help,point);
				V3_TRAFOM4_V3(DO_2Cp(q),ObsTrafo,help); DO_inc_n(q,3);
				(*OBS_ProjectProc)(help,point+3);
				point[1].x = ARR_ALPHA*point[3].x + (1.0-ARR_ALPHA)*point[0].x;
				point[1].y = ARR_ALPHA*point[3].y + (1.0-ARR_ALPHA)*point[0].y;
				a.x = point[3].x-point[1].x; a.y = point[3].y-point[1].y;
				point[2].x = point[1].x + ARR_COS*a.x - ARR_SIN*a.y;
				point[2].y = point[1].y + ARR_SIN*a.x + ARR_COS*a.y;
				point[4].x = point[1].x + ARR_COS*a.x + ARR_SIN*a.y;
				point[4].y = point[1].y - ARR_SIN*a.x + ARR_COS*a.y;
				point[5].x = point[1].x; point[5].y = point[1].y;
				UgPolyLine(point,6);
				break;
			case DO_DEPEND:
				DO_inc_DEPEND(q,3);
				break;
			case DO_INVERSE_LINE:
				DO_inc(q)
				UgSetColor(DO_2l(q)); DO_inc(q);
				V3_TRAFOM4_V3(DO_2Cp(q),ObsTrafo,help); DO_inc_n(q,3);
				(*OBS_ProjectProc)(help,&a);
				V3_TRAFOM4_V3(DO_2Cp(q),ObsTrafo,help); DO_inc_n(q,3);
				(*OBS_ProjectProc)(help,&b);
				UgInverseLine(a,b); 		
				break;
			case DO_INVERSE_POLYLINE:
				DO_inc(q)
				n = DO_2c(q); DO_inc(q)
				V3_TRAFOM4_V3(DO_2Cp(q),ObsTrafo,help); DO_inc_n(q,3);
				(*OBS_ProjectProc)(help,&a);
				for (j=1; j<n; j++)
				{
					V3_TRAFOM4_V3(DO_2Cp(q),ObsTrafo,help); DO_inc_n(q,3);
					(*OBS_ProjectProc)(help,&b);
					UgInverseLine(a,b);
					b = a;
				}
				break;
			case DO_POLYLINE:
				DO_inc(q)
				n = DO_2c(q); DO_inc(q)
				UgSetColor(DO_2l(q)); DO_inc(q);
				V3_TRAFOM4_V3(DO_2Cp(q),ObsTrafo,help); DO_inc_n(q,3);
				(*OBS_ProjectProc)(help,&a);
				UgMove(a);
				for (j=1; j<n; j++)
				{
					V3_TRAFOM4_V3(DO_2Cp(q),ObsTrafo,help); DO_inc_n(q,3);
					(*OBS_ProjectProc)(help,&a);
					UgDraw(a);
				}
				break;
			case DO_POLYGON:
				DO_inc(q)
				n = DO_2c(q); DO_inc(q)
				UgSetColor(DO_2l(q)); DO_inc(q);
				for (j=0; j<n; j++)
				{
					V3_TRAFOM4_V3(DO_2Cp(q),ObsTrafo,help); DO_inc_n(q,3);
					(*OBS_ProjectProc)(help,point+j);
				}
				UgPolygon(point,j);
				break;
			case DO_ERASE_POLYGON:
				DO_inc(q)
				n = DO_2c(q); DO_inc(q)
				for (j=0; j<n; j++)
				{
					V3_TRAFOM4_V3(DO_2Cp(q),ObsTrafo,help); DO_inc_n(q,3);
					(*OBS_ProjectProc)(help,point+j);
				}
				UgErasePolygon(point,j);
				break;
			case DO_INVERSE_POLYGON:
				DO_inc(q)
				n = DO_2c(q); DO_inc(q)
				for (j=0; j<n; j++)
				{
					V3_TRAFOM4_V3(DO_2Cp(q),ObsTrafo,help); DO_inc_n(q,3);
					(*OBS_ProjectProc)(help,point+j);
				}
				UgInversePolygon(point,j);
				break;
			case DO_ERASE_SURRPOLYGON:
				DO_inc(q)
				n = DO_2c(q); DO_inc(q)
				UgSetColor(DO_2l(q)); DO_inc(q);
				for (j=0; j<n; j++)
				{
					V3_TRAFOM4_V3(DO_2Cp(q),ObsTrafo,help); DO_inc_n(q,3);
					(*OBS_ProjectProc)(help,point+j);
				}
				UgErasePolygon(point,j);
				point[j].x=point[0].x; point[j].y=point[0].y;
				UgPolyLine(point,j+1);
				break;
			case DO_SURRPOLYGON:
				DO_inc(q)
				n = DO_2c(q); DO_inc(q)
				UgSetColor(DO_2l(q)); DO_inc(q);
				color = DO_2l(q); DO_inc(q);
				for (j=0; j<n; j++)
				{
					V3_TRAFOM4_V3(DO_2Cp(q),ObsTrafo,help); DO_inc_n(q,3);
					(*OBS_ProjectProc)(help,point+j);
				}
				UgPolygon(point,j);
				UgSetColor(color);
				point[j].x=point[0].x; point[j].y=point[0].y;
				UgPolyLine(point,j+1);
				break;
			case DO_TEXT:
				DO_inc(q)
				UgSetColor(DO_2l(q)); DO_inc(q);
				mode = DO_2c(q); DO_inc(q)
				centered = DO_2c(q); DO_inc(q)
				UgSetTextSize(DO_2s(q)); DO_inc(q);
				if ((DO_2c(q)) == TEXT_CENTERED)
				V3_TRAFOM4_V3(DO_2Cp(q),ObsTrafo,help); DO_inc_n(q,3);
				(*OBS_ProjectProc)(help,&a);
				if (centered)
					UgCenteredText(a,(char*)q,mode);
				else
				{
					UgMove(a);
					UgText((char*)q,mode);
				}
				DO_inc_str(q);
				break;
			case DO_POLYMARK:
				DO_inc(q)
				n = DO_2c(q); DO_inc(q)
				UgSetColor(DO_2l(q)); DO_inc(q);
				UgSetMarker(DO_2s(q)); DO_inc(q);
				UgSetMarkerSize(DO_2s(q)); DO_inc(q);
				for (j=0; j<n; j++)
				{
					V3_TRAFOM4_V3(DO_2Cp(q),ObsTrafo,help); DO_inc_n(q,3);
					(*OBS_ProjectProc)(help,point+j);
				}
				UgPolymark(point,n);
				break;
			default:
				RETURN(1);
		}
	}
	
	return (0);
}

/****************************************************************************/
/*																			*/
/* Function:  EW_FindElement2D												*/
/*																			*/
/* Purpose:   find element in 2D drawing object 							*/
/*																			*/
/* Input:	  DRAWINGOBJ *q: the drawing object							    */
/*																			*/
/* Output:	  INT 0: ok 													*/
/*				  1: error													*/
/*																			*/
/****************************************************************************/

static INT EW_SelectElement2D (DRAWINGOBJ *q)
{
	INT j, n, end, found;
	DOUBLE help[3];
	COORD_POINT point[MAX_POINTS_OF_POLY];
	
	end = 0;
	while (!end)
	{
		switch (DO_2c(q))
		{
			case DO_NO_INST:
				end = 1;
				break;
			case DO_RANGE:
				DO_inc_RANGE(q);
				break;
			case DO_LINE:
				DO_inc_LINE(q,2);
				break;
			case DO_STYLED_LINE:
				DO_inc_STYLED_LINE(q,2);
				break;
			case DO_ARROW:
				DO_inc_ARROW(q,2);
				break;
			case DO_DEPEND:
				DO_inc_DEPEND(q,2);
				break;
			case DO_INVERSE_LINE:
				DO_inc_INVERSE_LINE(q,2);
				break;
			case DO_INVERSE_POLYLINE:
				DO_inc_INVERSE_POLYLINE(q,2);
				break;
			case DO_POLYLINE:
				DO_inc_POLYLINE(q,2);
				break;
			case DO_TEXT:
				DO_inc_TEXT(q,2);
				break;
			case DO_POLYMARK:
				DO_inc_POLYMARK(q,2);
				break;
			case DO_POLYGON:
			case DO_ERASE_SURRPOLYGON:
				DO_inc(q)
				n = DO_2c(q); DO_inc_n(q,2)
				for (j=0; j<n; j++)
				{
					V2_TRAFOM3_V2(DO_2Cp(q),ObsTrafo,help); DO_inc_n(q,2);
					(*OBS_ProjectProc)(help,point+j);
				}
				found = PointInPolygon(point,j,FE2D_MousePos);
				break;
			case DO_INVERSE_POLYGON:
			case DO_ERASE_POLYGON:
				DO_inc(q)
				n = DO_2c(q); DO_inc(q)
				for (j=0; j<n; j++)
				{
					V2_TRAFOM3_V2(DO_2Cp(q),ObsTrafo,help); DO_inc_n(q,2);
					(*OBS_ProjectProc)(help,point+j);
				}
				found = PointInPolygon(point,j,FE2D_MousePos);
				break;
			case DO_SURRPOLYGON:
				DO_inc(q)
				n = DO_2c(q); DO_inc_n(q,3)
				for (j=0; j<n; j++)
				{
					V2_TRAFOM3_V2(DO_2Cp(q),ObsTrafo,help); DO_inc_n(q,2);
					(*OBS_ProjectProc)(help,point+j);
				}
				found = PointInPolygon(point,j,FE2D_MousePos);
				break;
			default:
				RETURN(1);
		}
		
	}
	
	/* if found ... */
	if (found)
	{
		/* put in/delete from selection list */
		if (SELECTIONMODE(WOP_MG)!=elementSelection)
			ClearSelection(WOP_MG); 	
		if (AddElementToSelection(WOP_MG,WOP_Element) == GM_ERROR)
			if (RemoveElementFromSelection(WOP_MG,WOP_Element) == GM_ERROR)
				return (1);
		
		/* invert element */
		UgInversePolygon(point,n);
		
		/* something (i.e. an element) was found */
		FE2D_found = 1;	
	}
	
	return (0);
}


/****************************************************************************/
/*
   FindRange2D - Find range for 2D

   SYNOPSIS:
   static INT FindRange2D (DRAWINGOBJ *q);

   PARAMETERS:
   q - the drawing object

   DESCRIPTION:
   This function finds range for 2D.

   RETURN VALUE:
   INT
.n    0 if ok
.n    1 if error occured.
*/
/****************************************************************************/

static INT FindRange2D (DRAWINGOBJ *q)
{
	INT end;
	
	end = 0;
	while (!end)
	{
		switch (DO_2c(q))
		{
			case DO_NO_INST:
				end = 1;
				break;
			case DO_RANGE:
				DO_inc(q)
				GEN_FR_min = MIN(GEN_FR_min,DO_2C(q)); DO_inc(q);
				GEN_FR_max = MAX(GEN_FR_max,DO_2C(q)); DO_inc(q);
				end = 1;
				break;
			case DO_LINE:
				DO_inc_LINE(q,2);
				break;
			case DO_STYLED_LINE:
				DO_inc_STYLED_LINE(q,2);
				break;
			case DO_ARROW:
				DO_inc_ARROW(q,2);
				break;
			case DO_DEPEND:
				DO_inc_DEPEND(q,2);
				break;
			case DO_INVERSE_LINE:
				DO_inc_INVERSE_LINE(q,2);
				break;
			case DO_INVERSE_POLYLINE:
				DO_inc_INVERSE_POLYLINE(q,2);
				break;
			case DO_POLYLINE:
				DO_inc_POLYLINE(q,2);
				break;
			case DO_TEXT:
				DO_inc_TEXT(q,2);
				break;
			case DO_POLYMARK:
				DO_inc_POLYMARK(q,2);
				break;
			case DO_POLYGON:
				DO_inc_POLYGON(q,2);
				break;
			case DO_ERASE_SURRPOLYGON:
				DO_inc_ERASE_SURRPOLYGON(q,2);
				break;
			case DO_INVERSE_POLYGON:
				DO_inc_INVERSE_POLYGON(q,2);
				break;
			case DO_ERASE_POLYGON:
				DO_inc_ERASE_POLYGON(q,2);
				break;
			case DO_SURRPOLYGON:
				DO_inc_SURRPOLYGON(q,2);
				break;
			default:
				RETURN(1);
		}
		
	}
	
	return (0);
}

/****************************************************************************/
/*
   FindRange3D -  Find range for 3D 

   SYNOPSIS:
   static INT FindRange3D (DRAWINGOBJ *q);

   PARAMETERS:
.  q - the drawing object

   DESCRIPTION:
   This function finds range for 3D.

   RETURN VALUE:
   INT
.n    0 if ok
.n    1 if error occured.
*/   													
/****************************************************************************/

static INT FindRange3D (DRAWINGOBJ *q)
{
	INT end;
	
	end = 0;
	while (!end)
	{
		switch (DO_2c(q))
		{
			case DO_NO_INST:
				end = 1;
				break;
			case DO_RANGE:
				DO_inc(q)
				GEN_FR_min = MIN(GEN_FR_min,DO_2C(q)); DO_inc(q);
				GEN_FR_max = MAX(GEN_FR_max,DO_2C(q)); DO_inc(q);
				break;
			case DO_LINE:
				DO_inc_LINE(q,3);
				break;
			case DO_STYLED_LINE:
				DO_inc_STYLED_LINE(q,3);
				break;
			case DO_ARROW:
				DO_inc_ARROW(q,3);
				break;
			case DO_DEPEND:
				DO_inc_DEPEND(q,3);
				break;
			case DO_INVERSE_LINE:
				DO_inc_INVERSE_LINE(q,3);
				break;
			case DO_INVERSE_POLYLINE:
				DO_inc_INVERSE_POLYLINE(q,3);
				break;
			case DO_POLYLINE:
				DO_inc_POLYLINE(q,3);
				break;
			case DO_TEXT:
				DO_inc_TEXT(q,3);
				break;
			case DO_POLYMARK:
				DO_inc_POLYMARK(q,3);
				break;
			case DO_POLYGON:
				DO_inc_POLYGON(q,3);
				break;
			case DO_ERASE_SURRPOLYGON:
				DO_inc_ERASE_SURRPOLYGON(q,3);
				break;
			case DO_INVERSE_POLYGON:
				DO_inc_INVERSE_POLYGON(q,3);
				break;
			case DO_ERASE_POLYGON:
				DO_inc_ERASE_POLYGON(q,3);
				break;
			case DO_SURRPOLYGON:
				DO_inc_SURRPOLYGON(q,3);
				break;
			default:
				RETURN(1);
		}
		
	}
	
	return (0);
}

/****************************************************************************/
/*																			*/
/* Function:  GEN_PostProcess_Scalar_FR 									*/
/*																			*/
/* Purpose:   postprocess for findrange of scalar plot						*/
/*																			*/
/* Input:	  PICTURE *thePicture, WORK *theWork							*/
/*																			*/
/* Return:	  INT 0: ok 													*/
/*			  INT 1: an error occurred										*/
/*																			*/
/****************************************************************************/

static INT GEN_PostProcess_Scalar_FR (PICTURE *thePicture, WORK *theWork)
{
	struct FindRange_Work *FR_Work;
	DOUBLE m,l;
	INT i;
	
	FR_Work = W_FINDRANGE_WORK(theWork);
	
	#ifdef ModelP
	Broadcast(&GEN_FR_min,sizeof(double));
	Broadcast(&GEN_FR_max,sizeof(double));
	#endif
	if (GEN_FR_min>GEN_FR_max)
	{
		UserWrite("findrange failed\n");
		return (0);
	}
	
	/* postprocess findrange */
	if (FR_Work->symmetric==YES)
	{
		GEN_FR_max = MAX(ABS(GEN_FR_min),ABS(GEN_FR_max));
		GEN_FR_min = -GEN_FR_max;
	}
	if (FR_Work->zoom!=1.0)
	{
		m = 0.5*(GEN_FR_max + GEN_FR_min);
		l = 0.5*(GEN_FR_max - GEN_FR_min);
		GEN_FR_min = m - FR_Work->zoom*l;
		GEN_FR_max = m + FR_Work->zoom*l;
	}
	FR_Work->min = GEN_FR_min;
	FR_Work->max = GEN_FR_max;
	
	/* store if */
	if (GEN_FR_put == YES)
	{
		PIC_PO(thePicture)->theEspo.min = GEN_FR_min;
		PIC_PO(thePicture)->theEspo.max = GEN_FR_max;
		if (PIC_PO(thePicture)->theEspo.mode==PO_CONTOURS_EQ)
		{
			for (i=0; i<PIC_PO(thePicture)->theEspo.numOfContours; i++)
				PIC_PO(thePicture)->theEspo.contValues[i] = GEN_FR_min + (DOUBLE)i * (GEN_FR_max - GEN_FR_min) / (DOUBLE)(PIC_PO(thePicture)->theEspo.numOfContours-1);
		}
	}
	
	return (0);
}
	
/**********************************************************************************************************/
/************************************ Part for 2D and 3D Version ******************************************/
/**********************************************************************************************************/

/****************************************************************************/
/*
   DynInfo_Grid2D - print dynamic info for Matrix for infobox of ugwindow

   SYNOPSIS:
   INT DynInfo_Matrix (PICTURE *pic, INT tool, INT fct, const INT mp[2], char *text)

   PARAMETERS:
.  pic  - print info for this picture
.  tool - current tool
.  fct  - currenr tool function
.  mp   - mouse position in window
.  text - resulting info text

   DESCRIPTION:
   The position of the mouse is given as index of matrix entry.

   RETURN VALUE:
   INT
.n   0 if text will change with mouse position
.n   1 if text is static
*/
/****************************************************************************/

static INT DynInfo_Matrix (PICTURE *pic, INT tool, INT fct, const INT mp[2], char *text)
{
	struct MatrixPlotObj *theMpo;
	MULTIGRID *theMG;
	GRID *theGrid;
	VIEWEDOBJ *vo;
	DOUBLE cpt[2];
	INT maxrow;
	
	if (PIC_VALID(pic) == NO)
	{
		strcpy(text,"pic invalid");
		return (1);
	}
	
	vo	   = PIC_VO(pic);
	theMpo = &(PIC_PO(pic)->theMpo);
	theMG  = PO_MG(PIC_PO(pic));
	theGrid= GRID_ON_LEVEL(theMG,CURRENTLEVEL(theMG));
	maxrow = NVEC(theGrid);
	
	V2_TRAFOM3_V2(mp,InvObsTrafo,cpt);
	
	cpt[0] = floor(cpt[0]) +1;
	cpt[1] = floor(maxrow - cpt[1]) +1;
	
	sprintf(text,"(%5d,%5d)",(int)cpt[1],(int)cpt[0]);
	
	return (0);
}

static INT RECURSIVE_BVPreProcess (PICTURE *thePicture, WORK *theWork)
{
	struct MatrixPlotObj *theMpo;
	OUTPUTDEVICE *theOD;
	MULTIGRID *theMG;
	GRID *theGrid;

	theMpo = &(PIC_PO(thePicture)->theMpo);
	
	if (!theMpo->BV)
		return (1);
	
	theOD  = PIC_OUTPUTDEV(thePicture);
	theMG  = PO_MG(PIC_PO(thePicture));
	theGrid= GRID_ON_LEVEL(theMG,CURRENTLEVEL(theMG));
	
	BV_color	= theOD->black;
	BV_theBV	= GFIRSTBV(theGrid);
	
	if (BV_theBV==NULL)
		return (2);
	
	return (0);
}


static INT BVEval_recurse( DRAWINGOBJ *theDO, GEN_ExecuteProcPtr ExecuteProc, BLOCKVECTOR *bv, INT pos_parent, INT width_parent, INT pos_own )
/* called from RECURSIVE_BVEval; does the recursive plotting */
{
	INT width_own, pos_child;
	BLOCKVECTOR *bv_child;
	DRAWINGOBJ *theStartDO = theDO;
	
	width_own = BVNUMBEROFVECTORS( bv );
printf( "%d %d %d %d\n", pos_parent,width_parent,	pos_own,width_own);

	/* hor line at BVFIRSTVECTOR(BV_theBV) */
	DO_2c(theDO) = DO_LINE; DO_inc(theDO) 
	DO_2l(theDO) = BV_color; DO_inc(theDO)
	DO_2C(theDO) = MAT_XC(pos_parent);   			DO_inc(theDO); DO_2C(theDO) = MAT_YC(pos_own); DO_inc(theDO)
	DO_2C(theDO) = MAT_XC(pos_parent+width_parent); DO_inc(theDO); DO_2C(theDO) = MAT_YC(pos_own); DO_inc(theDO)

	/* vert line at BVFIRSTVECTOR(BV_theBV) */
	DO_2c(theDO) = DO_LINE; DO_inc(theDO);
	DO_2l(theDO) = BV_color; DO_inc(theDO);
	DO_2C(theDO) = MAT_XC(pos_own);   DO_inc(theDO); DO_2C(theDO) = MAT_YC(pos_parent);				 DO_inc(theDO);
	DO_2C(theDO) = MAT_XC(pos_own);   DO_inc(theDO); DO_2C(theDO) = MAT_YC(pos_parent+width_parent); DO_inc(theDO);

	if ( MAT_dash != 0.0 )
	{
		/* hor dashed line before BVFIRSTVECTOR(BV_theBV) */
		DO_2c(theDO) = DO_STYLED_LINE; DO_inc(theDO); 
		DO_2l(theDO) = BV_color; DO_inc(theDO);
		DO_2C(theDO) = MAT_XC(pos_parent); DO_inc(theDO); DO_2C(theDO) = MAT_YC(pos_own); DO_inc(theDO);
		DO_2C(theDO) = MAT_XC(0); 		   DO_inc(theDO); DO_2C(theDO) = MAT_YC(pos_own); DO_inc(theDO);
		DO_2C(theDO) = MAT_dash;  DO_inc(theDO)
		DO_2C(theDO) = MAT_space; DO_inc(theDO)
		
		/* hor dashed line after BVFIRSTVECTOR(BV_theBV) */
		DO_2c(theDO) = DO_STYLED_LINE; DO_inc(theDO); 
		DO_2l(theDO) = BV_color; DO_inc(theDO);
		DO_2C(theDO) = MAT_XC(MAT_maxrow); 				DO_inc(theDO); DO_2C(theDO) = MAT_YC(pos_own); DO_inc(theDO);
		DO_2C(theDO) = MAT_XC(pos_parent+width_parent); DO_inc(theDO); DO_2C(theDO) = MAT_YC(pos_own); DO_inc(theDO);
		DO_2C(theDO) = MAT_dash;  DO_inc(theDO);
		DO_2C(theDO) = MAT_space; DO_inc(theDO);

		/* vert line above BVFIRSTVECTOR(BV_theBV) */
		DO_2c(theDO) = DO_STYLED_LINE; DO_inc(theDO) ;
		DO_2l(theDO) = BV_color; DO_inc(theDO);
		DO_2C(theDO) = MAT_XC(pos_own);   DO_inc(theDO); DO_2C(theDO) = MAT_YC(pos_parent);	DO_inc(theDO);
		DO_2C(theDO) = MAT_XC(pos_own);   DO_inc(theDO); DO_2C(theDO) = MAT_YC(0); 			DO_inc(theDO);
		DO_2C(theDO) = MAT_dash;  DO_inc(theDO);
		DO_2C(theDO) = MAT_space; DO_inc(theDO);
	
		/* vert line above BVFIRSTVECTOR(BV_theBV) */
		DO_2c(theDO) = DO_STYLED_LINE; DO_inc(theDO);
		DO_2l(theDO) = BV_color; DO_inc(theDO);
		DO_2C(theDO) = MAT_XC(pos_own); DO_inc(theDO); DO_2C(theDO) = MAT_YC(pos_parent+width_parent); DO_inc(theDO);
		DO_2C(theDO) = MAT_XC(pos_own); DO_inc(theDO); DO_2C(theDO) = MAT_YC(MAT_maxrow); 			   DO_inc(theDO);
		DO_2C(theDO) = MAT_dash;  DO_inc(theDO);
		DO_2C(theDO) = MAT_space; DO_inc(theDO);
	}
	
	DO_2c(theDO) = DO_NO_INST;
	
	if ((*ExecuteProc)(theStartDO)) 
		return (1);
	
/* TODO: does not work in parallel (s.l.)
	#ifdef ModelP
	WOP_DObjPnt = theDO;
	#endif
*/
		
	if ( BV_IS_LEAF_BV( bv ) )
		return (0);
	
	pos_child = pos_own;
	for ( bv_child = BVDOWNBV(bv); bv_child != BVDOWNBVEND(bv); bv_child = BVSUCC(bv_child) )
	{
		if (BVEval_recurse( theDO, ExecuteProc, bv_child, pos_own, width_own, pos_child ))
			return (1);
		pos_child += BVNUMBEROFVECTORS( bv_child );
	}
	
	return (0);
}


static INT RECURSIVE_BVEval (DRAWINGOBJ *theDO, GEN_ExecuteProcPtr ExecuteProc)
{
	DRAWINGOBJ *theStartDO = theDO;
	INT pos_child;
	BLOCKVECTOR *bv_child;
	
	if ( BV_theBV == NULL )
		return (0);
	
	/* process the blockvector list following GFIRSTBV(grid) */
	pos_child = 0;
	for ( bv_child = BV_theBV; bv_child != NULL; bv_child = BVSUCC(bv_child) )
	{
		if (BVEval_recurse( theDO, ExecuteProc, bv_child, 0, MAT_maxrow, pos_child ))
			return (1);
		pos_child += BVNUMBEROFVECTORS(bv_child);
	}
	
	/* hor line after last vector */
	DO_2c(theDO) = DO_LINE; DO_inc(theDO) 
	DO_2l(theDO) = BV_color; DO_inc(theDO);
	DO_2C(theDO) = MAT_XC(0);   		DO_inc(theDO); DO_2C(theDO) = MAT_YC(MAT_maxrow);   DO_inc(theDO);
	DO_2C(theDO) = MAT_XC(MAT_maxrow);	DO_inc(theDO); DO_2C(theDO) = MAT_YC(MAT_maxrow);   DO_inc(theDO);
		
	/* vert line after last vector */
	DO_2c(theDO) = DO_LINE; DO_inc(theDO) 
	DO_2l(theDO) = BV_color; DO_inc(theDO);
	DO_2C(theDO) = MAT_XC(MAT_maxrow);  DO_inc(theDO); DO_2C(theDO) = MAT_YC(0);   			DO_inc(theDO);
	DO_2C(theDO) = MAT_XC(MAT_maxrow);  DO_inc(theDO); DO_2C(theDO) = MAT_YC(MAT_maxrow);   DO_inc(theDO);
		
	DO_2c(theDO) = DO_NO_INST;

/* TODO: does not work in parallel
	#ifdef ModelP
	WOP_DObjPnt = theDO;
	#endif
*/
	
	if ((*ExecuteProc)(theStartDO)) 
		return (1);
	
	return (0);
}

static INT VW_MatrixPreProcess (PICTURE *thePicture, WORK *theWork)
{
	struct MatrixPlotObj *theMpo;
	OUTPUTDEVICE *theOD;
	MULTIGRID *theMG;
	GRID *theGrid;
	COORD_POINT point0,point1;
	DOUBLE x0[2],x1[2],help[2];
	DOUBLE d;
	INT mtp,n;

	theMpo = &(PIC_PO(thePicture)->theMpo);
	theOD  = PIC_OUTPUTDEV(thePicture);
	theMG  = PO_MG(PIC_PO(thePicture));
	theGrid= GRID_ON_LEVEL(theMG,CURRENTLEVEL(theMG));
	
	MAT_conn			= theMpo->conn;
	MAT_extra			= theMpo->extra;
	MAT_dash			= theMpo->dash;
	MAT_space			= theMpo->space;
	MAT_black			= theOD->black;
	MAT_white			= theOD->white;
	
	/* set globals for eval function */
	if (theMpo->Matrix!=NULL)
	{
		MAT_eval		= NULL;
		MAT_md			= theMpo->Matrix;
	}
	else
	{
		MAT_eval		= theMpo->EvalFct->EvalProc;
	}
	MAT_maxrow			= NVEC(theGrid);
	MAT_log				= theMpo->log;
	MAT_thresh			= theMpo->thresh;
	MAT_rel				= theMpo->rel;
	
	/* color range */
	if (theMpo->max - theMpo->min < SMALL_D)
		if (W_ID(theWork) != FINDRANGE_WORK)
		{
			UserWrite("maxValue has to be larger than minValue\n");
			return (1);
		}
	if ((theMpo->max - theMpo->min)==0) 
		MAT_factor = 0;
	else
		MAT_factor = (theOD->spectrumEnd - theOD->spectrumStart)/(theMpo->max - theMpo->min);
	MAT_offset = theOD->spectrumStart - MAT_factor*theMpo->min;
	MAT_dark   = theOD->spectrumStart + 0.2*(theOD->spectrumEnd - theOD->spectrumStart);
	
	/* smallest square needed */
	n = 0;
	for (mtp=0; mtp<NMATTYPES; mtp++)
		n = MAX(n,MAX(MD_ROWS_IN_MTYPE(MAT_md,mtp),MD_COLS_IN_MTYPE(MAT_md,mtp)));
	
	/* compute size of squares in pixel coordinates */
	x0[0] = 0.0;			x0[1] = 0.0;
	x1[0] = 1.0/(DOUBLE)n;	x1[1] = 0.0;
	V2_TRAFOM3_V2(x0,ObsTrafo,help); (*OBS_ProjectProc)(help,&point0);
	V2_TRAFOM3_V2(x1,ObsTrafo,help); (*OBS_ProjectProc)(help,&point1);
	
	d = sqrt((point0.x-point1.x)*(point0.x-point1.x)+(point0.y-point1.y)*(point0.y-point1.y));
	
	if (d>MAT_FRAMESIZE)
		MAT_frame = TRUE;
	else
		MAT_frame = FALSE;
	
	if (d>4*MAT_TEXTSIZE)
		MAT_print = TRUE;
	else
		MAT_print = FALSE;
	
	l_setindex(theGrid);
	
	return (0);
}

static INT PlotMatrixEntry (
				DOUBLE rowi, DOUBLE coli,
				DOUBLE row, DOUBLE col,
				DOUBLE w, DOUBLE h,
				DOUBLE value, DOUBLE printvalue,
				DRAWINGOBJ **DOhandle)
{
	long Color;
	char valtext[16],*p;
	
	Color = (long)(MAT_factor*value+MAT_offset);
	Color = MIN(Color,WOP_OutputDevice->spectrumEnd);
	if (Color<WOP_OutputDevice->spectrumStart)
		return (0);
	
	/* draw */
	DO_2c(*DOhandle) = DO_POLYGON; DO_inc(*DOhandle) 
	DO_2c(*DOhandle) = 4; DO_inc(*DOhandle) 
	DO_2l(*DOhandle) = Color; DO_inc(*DOhandle);
	DO_2C(*DOhandle) = col;   DO_inc(*DOhandle); DO_2C(*DOhandle) = row;   DO_inc(*DOhandle);
	DO_2C(*DOhandle) = col+w; DO_inc(*DOhandle); DO_2C(*DOhandle) = row;   DO_inc(*DOhandle);
	DO_2C(*DOhandle) = col+w; DO_inc(*DOhandle); DO_2C(*DOhandle) = row+h; DO_inc(*DOhandle);
	DO_2C(*DOhandle) = col;   DO_inc(*DOhandle); DO_2C(*DOhandle) = row+h; DO_inc(*DOhandle);
	
	if (MAT_frame)
	{
		DO_2c(*DOhandle) = DO_POLYLINE; DO_inc(*DOhandle) 
		DO_2c(*DOhandle) = 5; DO_inc(*DOhandle) 
		DO_2l(*DOhandle) = MAT_black; DO_inc(*DOhandle);
		DO_2C(*DOhandle) = col;   DO_inc(*DOhandle); DO_2C(*DOhandle) = row;   DO_inc(*DOhandle);
		DO_2C(*DOhandle) = col+w; DO_inc(*DOhandle); DO_2C(*DOhandle) = row;   DO_inc(*DOhandle);
		DO_2C(*DOhandle) = col+w; DO_inc(*DOhandle); DO_2C(*DOhandle) = row+h; DO_inc(*DOhandle);
		DO_2C(*DOhandle) = col;   DO_inc(*DOhandle); DO_2C(*DOhandle) = row+h; DO_inc(*DOhandle);
		DO_2C(*DOhandle) = col;   DO_inc(*DOhandle); DO_2C(*DOhandle) = row;   DO_inc(*DOhandle);
	}
	if (MAT_print)
	{
		if (Color<MAT_dark)
			Color = MAT_white;
		else
			Color = MAT_black;
		
		sprintf(valtext,"%.3e",printvalue);
		
		/* exponent */
		p = strchr(valtext,'e');
		DO_2c(*DOhandle) = DO_TEXT; DO_inc(*DOhandle) 
		DO_2l(*DOhandle) = Color; DO_inc(*DOhandle);
		DO_2c(*DOhandle) = TEXT_REGULAR; DO_inc(*DOhandle) 
		DO_2c(*DOhandle) = TEXT_CENTERED; DO_inc(*DOhandle) 
		DO_2s(*DOhandle) = MAT_TEXTSIZE; DO_inc(*DOhandle);
		DO_2C(*DOhandle) = col+0.5*w;   DO_inc(*DOhandle); DO_2C(*DOhandle) = row+0.5*h;   DO_inc(*DOhandle);
		strcpy(DO_2cp(*DOhandle),p); DO_inc_str(*DOhandle);
		
		/* mantisse */
		*p = '\0';
		DO_2c(*DOhandle) = DO_TEXT; DO_inc(*DOhandle) 
		DO_2l(*DOhandle) = Color; DO_inc(*DOhandle);
		DO_2c(*DOhandle) = TEXT_REGULAR; DO_inc(*DOhandle) 
		DO_2c(*DOhandle) = TEXT_CENTERED; DO_inc(*DOhandle) 
		DO_2s(*DOhandle) = MAT_TEXTSIZE; DO_inc(*DOhandle);
		DO_2C(*DOhandle) = col+0.5*w;   DO_inc(*DOhandle); DO_2C(*DOhandle) = row+0.75*h;   DO_inc(*DOhandle);
		strcpy(DO_2cp(*DOhandle),valtext); DO_inc_str(*DOhandle);
		
		/* index */
		sprintf(valtext,"%ld,%ld",(long)rowi,(long)coli);
		DO_2c(*DOhandle) = DO_TEXT; DO_inc(*DOhandle) 
		DO_2l(*DOhandle) = Color; DO_inc(*DOhandle);
		DO_2c(*DOhandle) = TEXT_REGULAR; DO_inc(*DOhandle) 
		DO_2c(*DOhandle) = TEXT_CENTERED; DO_inc(*DOhandle) 
		DO_2s(*DOhandle) = MAT_TEXTSIZE; DO_inc(*DOhandle);
		DO_2C(*DOhandle) = col+0.5*w;   DO_inc(*DOhandle); DO_2C(*DOhandle) = row+0.25*h;   DO_inc(*DOhandle);
		strcpy(DO_2cp(*DOhandle),valtext); DO_inc_str(*DOhandle);
	}
	
	return (0);
}

static PlotPointBlockMatrixEntry (
				DOUBLE rowi, DOUBLE coli,
				DOUBLE row, DOUBLE col,
				INT nr, INT nc,
				const DOUBLE *values,
				DOUBLE *min, DOUBLE *max,
				DRAWINGOBJ **DOhandle)
{
	DOUBLE value,printvalue;
	DOUBLE w,h;
	INT i,j;
	
	w = 1.0/(DOUBLE)nc;
	h = 1.0/(DOUBLE)nr;
	
	for (i=0; i<nr; i++)
		for (j=0; j<nc; j++)
		{
			printvalue = value = values[i*nc+j];
			
			if (fabs(value)<=MAT_thresh)
				continue;
			if (MAT_log)
				value = log(fabs(value));
			
			/* store range */
			*max = MAX(*max,value);
			*min = MIN(*min,value);
			
			PlotMatrixEntry(rowi,coli,row+1-(i+1)*h,col+j*w,w,h,value,printvalue,DOhandle);
		}
	if (!MAT_frame)
		return (0);
	
	/* frame whole block */
	DO_2c(*DOhandle) = DO_POLYLINE; DO_inc(*DOhandle) 
	DO_2c(*DOhandle) = 5; DO_inc(*DOhandle) 
	DO_2l(*DOhandle) = MAT_black; DO_inc(*DOhandle);
	DO_2C(*DOhandle) = col;   DO_inc(*DOhandle); DO_2C(*DOhandle) = row;   DO_inc(*DOhandle);
	DO_2C(*DOhandle) = col+1; DO_inc(*DOhandle); DO_2C(*DOhandle) = row;   DO_inc(*DOhandle);
	DO_2C(*DOhandle) = col+1; DO_inc(*DOhandle); DO_2C(*DOhandle) = row+1; DO_inc(*DOhandle);
	DO_2C(*DOhandle) = col;   DO_inc(*DOhandle); DO_2C(*DOhandle) = row+1; DO_inc(*DOhandle);
	DO_2C(*DOhandle) = col;   DO_inc(*DOhandle); DO_2C(*DOhandle) = row;   DO_inc(*DOhandle);
	
	return (0);
}

static INT VW_MatrixEval (VECTOR *vec, DRAWINGOBJ *theDO)
{
	MATRIX *mat;
	DOUBLE values[MAX_MAT_COMP],invdiag[MAX_MAT_COMP],min,max;
	SHORT *cmp;
	INT row,col,nr,nc,rt,mtp,i;
	
	/* origin is UPPER left corner */
	row = MAT_maxrow-VINDEX(vec);
	rt  = VTYPE(vec);
	
	min =  MAX_D;
	max = -MAX_D;
	
	for (i=0; i<MAX_MAT_COMP; i++) invdiag[i] = 1.0;
	for (mat=VSTART(vec); mat!=NULL; mat=MNEXT(mat))
	{
		if (CEXTRA(MMYCON(mat)))
		{
			if (!MAT_extra) continue;
		}
		else
			if (!MAT_conn) continue;
		
		col = VINDEX(MDEST(mat))-1;	/* -1 yields correct adjustment */
		
		if (MAT_eval==NULL)
		{
			mtp = MTP(rt,VTYPE(MDEST(mat)));
			
			if (!MD_ISDEF_IN_MTYPE(MAT_md,mtp)) continue;
			
			nr  = MD_ROWS_IN_MTYPE(MAT_md,mtp);
			nc  = MD_COLS_IN_MTYPE(MAT_md,mtp);
			cmp = MD_MCMPPTR_OF_MTYPE(MAT_md,mtp);
			if (MDIAG(mat) && MAT_rel)
				for (i=0; i<nr; i++)
					invdiag[i] = 1.0/MVALUE(mat,cmp[i*nc+i]);
			for (i=0; i<nr*nc; i++)
				values[i] = invdiag[i/nc] * MVALUE(mat,cmp[i]);
		}
		else
		{
			nr = nc = 1;
			values[0] = (*MAT_eval)(mat);
		}
		
		PlotPointBlockMatrixEntry(VINDEX(vec),VINDEX(MDEST(mat)),row,col,nr,nc,values,&min,&max,&theDO);
	}
	
	/* fill range */
	if ((max!=-MAX_D) && (min!=MAX_D))
	{
		DO_2c(theDO) = DO_RANGE; DO_inc(theDO);
		DO_2C(theDO) = min; DO_inc(theDO);
		DO_2C(theDO) = max; DO_inc(theDO);
	}
	DO_2c(theDO) = DO_NO_INST;

	#ifdef ModelP
	WOP_DObjPnt = theDO;
	#endif
	
	return (0);
}

static INT VW_PreProcess_Matrix_FR (PICTURE *thePicture, WORK *theWork)
{
	if (VW_MatrixPreProcess (thePicture,theWork)) 
		return (1);

	/* reset min and max values */
	GEN_FR_put = W_FINDRANGE_WORK(theWork)->put;
	GEN_FR_min =  MAX_D;
	GEN_FR_max = -MAX_D;	
	
	return (0);
}

static INT GEN_PostProcess_Matrix_FR (PICTURE *thePicture, WORK *theWork)
{
	struct FindRange_Work *FR_Work;
	DOUBLE m,l;
	
	FR_Work = W_FINDRANGE_WORK(theWork);
	
	if (GEN_FR_min>GEN_FR_max)
	{
		UserWrite("findrange failed\n");
		return (0);
	}
	
	/* postprocess findrange */
	if (FR_Work->symmetric==YES)
	{
		GEN_FR_max = MAX(ABS(GEN_FR_min),ABS(GEN_FR_max));
		GEN_FR_min = -GEN_FR_max;
	}
	if (FR_Work->zoom!=1.0)
	{
		m = 0.5*(GEN_FR_max + GEN_FR_min);
		l = 0.5*(GEN_FR_max - GEN_FR_min);
		GEN_FR_min = m - FR_Work->zoom*l;
		GEN_FR_max = m + FR_Work->zoom*l;
	}
	FR_Work->min = GEN_FR_min;
	FR_Work->max = GEN_FR_max;
	
	/* store if */
	if (GEN_FR_put == YES)
	{
		PIC_PO(thePicture)->theMpo.min = GEN_FR_min;
		PIC_PO(thePicture)->theMpo.max = GEN_FR_max;
	}
	
	return (0);
}
	
/****************************************************************************/
/*																			*/
/* Function:  EW_PostProcess_EVector_FR 									*/
/*																			*/
/* Purpose:   postprocess for findrange of vector plot						*/
/*																			*/
/* Input:	  PICTURE *thePicture, WORK *theWork							*/
/*																			*/
/* Return:	  INT 0: ok 													*/
/*			  INT 1: an error occurred										*/
/*																			*/
/****************************************************************************/

static INT GEN_PostProcess_Vector_FR (PICTURE *thePicture, WORK *theWork)
{
	struct FindRange_Work *FR_Work;
	
	FR_Work = W_FINDRANGE_WORK(theWork);
	
	/* postprocess findrange */
		FR_Work->min = 0.0;
	if (FR_Work->zoom>0.0)
		FR_Work->max = GEN_FR_max*FR_Work->zoom;
	else
		FR_Work->max = GEN_FR_max;
	
	/* store if */
	if (GEN_FR_put == YES)
		PIC_PO(thePicture)->theEvpo.max = FR_Work->max;
	
	return (0);
}
	
/**********************************************************************************************************/
/************************************ Part only for 2D Version ********************************************/
/**********************************************************************************************************/

#ifdef __TWODIM__

/****************************************************************************/
/*
   OrderElements - Order elements w.r.t. theViewedObject 

   SYNOPSIS:
   static INT OrderElements_2D (MULTIGRID *theMG, VIEWEDOBJ *theViewedObj)

   PARAMETERS:
.  theMG - pointer to multigrid
.  heViewedObj - 

   DESCRIPTION:
   This function order elements w.r.t. theViewedObject.

   RETURN VALUE:
   INT

   0 when ok

   1 when error occured
   */
/****************************************************************************/

static INT OrderElements_2D (MULTIGRID *theMG, VIEWEDOBJ *theViewedObj)
{
	return (0);
}

/****************************************************************************/
/*
   DynInfo_Grid2D - print dynamic info for 2D Grid for infobox of ugwindow

   SYNOPSIS:
   INT DynInfo_Grid2D (PICTURE *pic, INT tool, INT fct, const INT mp[2], char *text)

   PARAMETERS:
.  pic  - print info for this picture
.  tool - current tool
.  fct  - currenr tool function
.  mp   - mouse position in window
.  text - resulting info text

   DESCRIPTION:
   The position of the mouse is given in physical coordinates.

   RETURN VALUE:
   INT
.n   0 if text will change with mouse position
.n   1 if text is static
*/
/****************************************************************************/

static INT DynInfo_Grid2D (PICTURE *pic, INT tool, INT fct, const INT mp[2], char *text)
{
	VIEWEDOBJ *vo;
	DOUBLE cpt[2];
	
	if (PIC_VALID(pic) == NO)
	{
		strcpy(text,"pic invalid");
		return (1);
	}
	
	vo = PIC_VO(pic);
	V2_TRAFOM3_V2(mp,InvObsTrafo,cpt);
	
	sprintf(text,"(%5.3e,%5.3e)",cpt[0],cpt[1]);
	
	return (0);
}

/****************************************************************************/
/*
   ClickAct_Grid2D - tool dependend act on click for 2D Grid

   SYNOPSIS:
   INT ClickAct_Grid2D (PICTURE *pic, INT tool, INT fct, const INT mp[2])

   PARAMETERS:
.  pic  - print info for this picture
.  tool - current tool
.  fct  - currenr tool function
.  mp   - mouse position in window

   DESCRIPTION:
.  cross - insert boundary node
.  choice - move node
.  circle - insert inner node
.  hand - select node
.  heart - select element
.  gnoedel - unmark, mark copy, mark red, mark blue depending on fct

   RETURN VALUE:
   INT
.n   0 if ok
.n   1 if action for this tool is not available
.n   __LINE__ if an error occured
*/
/****************************************************************************/

static INT ClickAct_Grid2D (PICTURE *pic, INT tool, INT fct, const INT mp[2])
{
	WORK theWork;
	
	switch (tool)
	{
		case crossTool:
			W_ID(&theWork) = INSERTBNDNODE_WORK;
			W_INSERTBNDNODE_WORK(&theWork)->PixelX = mp[0];
			W_INSERTBNDNODE_WORK(&theWork)->PixelY = mp[1];
			break;
		case choiceTool:
			W_ID(&theWork) = MOVENODE_WORK;
			W_MOVENODE_WORK(&theWork)->PixelX = mp[0];
			W_MOVENODE_WORK(&theWork)->PixelY = mp[1];
			break;
		case circleTool:
			W_ID(&theWork) = INSERTNODE_WORK;
			W_INSERTNODE_WORK(&theWork)->PixelX = mp[0];
			W_INSERTNODE_WORK(&theWork)->PixelY = mp[1];
			break;
		case handTool:
			W_ID(&theWork) = SELECTNODE_WORK;
			W_SELECTNODE_WORK(&theWork)->PixelX = mp[0];
			W_SELECTNODE_WORK(&theWork)->PixelY = mp[1];
			break;
		case heartTool:
			W_ID(&theWork) = SELECTELEMENT_WORK;
			W_SELECTELEMENT_WORK(&theWork)->PixelX = mp[0];
			W_SELECTELEMENT_WORK(&theWork)->PixelY = mp[1];
			break;
		case gnoedelTool:
			W_ID(&theWork) = MARKELEMENT_WORK;
			W_MARKELEMENT_WORK(&theWork)->PixelX = mp[0];
			W_MARKELEMENT_WORK(&theWork)->PixelY = mp[1];
			W_MARKELEMENT_WORK(&theWork)->rule   = fct;
			break;
		default:
			return (1);
	}
	if (WorkOnPicture(pic,&theWork))
		return (__LINE__);
	
	return (0);
}
	
/****************************************************************************/
/*
   EW_PreProcess_PlotBndOfElem2D - Initialize input variables of EW_BndOfElemEval2D

   SYNOPSIS:
   static INT EW_PreProcess_PlotBndOfElem2D (PICTURE *thePicture, WORK *theWork);

   PARAMETERS:
.  thePicture - 
.  theWork - 

   DESCRIPTION:
   This function initializes input variables of EW_BndOfElemEval2D.

   RETURN VALUE:
   INT
.n    0 if ok
.n    1 if error occured.
*/
/****************************************************************************/

static INT EW_PreProcess_PlotBndOfElem2D (PICTURE *thePicture, WORK *theWork)
{
	struct GridPlotObj2D *theGpo;
	OUTPUTDEVICE *theOD;
	MULTIGRID *theMG;

	theGpo = &(PIC_PO(thePicture)->theGpo);
	theOD  = PIC_OUTPUTDEV(thePicture);
	theMG  = PO_MG(PIC_PO(thePicture));
	
	/* see if boundary has to be plotted */
	if (theGpo->PlotBoundary == NO)
		return (1); 
	
	EE2D_NoColor[COLOR_BND] 		= 0;	
	EE2D_Color[COLOR_BND]			= theOD->blue;
	EE2D_Elem2Plot[PLOT_ALL]		= 1;
	EE2D_Elem2Plot[PLOT_COPY]		= 1;
	EE2D_Elem2Plot[PLOT_IRR]		= 1;
	EE2D_Elem2Plot[PLOT_REG]		= 1;
	
	/* mark surface elements */
	if (MarkElements_MGS_Bnd(theMG,0,CURRENTLEVEL(theMG))) return (1);
	
	return (0);
}
	
/****************************************************************************/
/*
   EW_PreProcess_PlotBlackBnd2D	- Initialize input variables of EW_BndOfElemEval2D

   SYNOPSIS:
   static INT EW_PreProcess_PlotBlackBnd2D (PICTURE *thePicture, WORK *theWork);

   PARAMETERS:
.  thePicture - 
.  theWork - 

   DESCRIPTION:
   This function initializes input variables of EW_BndOfElemEval2D.

   RETURN VALUE:
   INT
.n    0 if ok
.n    1 if error occured.
*/
/****************************************************************************/

static INT EW_PreProcess_PlotBlackBnd2D (PICTURE *thePicture, WORK *theWork)
{
	OUTPUTDEVICE *theOD;
	MULTIGRID *theMG;

	theOD  = PIC_OUTPUTDEV(thePicture);
	theMG  = PO_MG(PIC_PO(thePicture));
	
	EE2D_NoColor[COLOR_BND] 		= 0;	
	EE2D_Color[COLOR_BND]			= theOD->black;
	EE2D_Elem2Plot[PLOT_ALL]		= 1;
	EE2D_Elem2Plot[PLOT_COPY]		= 1;
	EE2D_Elem2Plot[PLOT_IRR]		= 1;
	EE2D_Elem2Plot[PLOT_REG]		= 1;
	
	/* mark surface elements */
	if (MarkElements_MGS_Bnd(theMG,0,CURRENTLEVEL(theMG))) return (1);
	
	return (0);
}

/****************************************************************************/
/*
   EW_PreProcess_PlotElements2D - Initialize input variables of EW_ElementEval2D for GridPlot2D	

   SYNOPSIS:
   static INT EW_PreProcess_PlotElements2D (PICTURE *thePicture, WORK *theWork);

   PARAMETERS:
.  thePicture -
.  theWork -
  
   DESCRIPTION:
   This function initializes input variables of EW_ElementEval2D for GridPlot2D.

   RETURN VALUE:
   INT
.n    0 if ok
.n    1 if error occured.
*/
/****************************************************************************/

static INT EW_PreProcess_PlotElements2D (PICTURE *thePicture, WORK *theWork)
{
	struct GridPlotObj2D *theGpo;
	OUTPUTDEVICE *theOD;
	MULTIGRID *theMG;
	INT i;
	
	theGpo = &(PIC_PO(thePicture)->theGpo);
	theOD  = PIC_OUTPUTDEV(thePicture);
	theMG  = PO_MG(PIC_PO(thePicture));
	
	if (theGpo->WhichElem == PO_NO && theGpo->PlotElemID == NO)
		return (1);
	
	EE2D_NoColor[COLOR_EDGE]			= 0;	
	EE2D_NoColor[COLOR_LOWER_LEVEL] 	= 1;
	EE2D_NoColor[COLOR_BND] 			= 1;	
	if (theGpo->ElemColored == 1)
	{
		EE2D_NoColor[COLOR_COPY]		= 0;	
		EE2D_NoColor[COLOR_IRR] 		= 0;	
		EE2D_NoColor[COLOR_REG] 		= 0;	
	}
	else
	{
		EE2D_NoColor[COLOR_COPY]		= 1;	
		EE2D_NoColor[COLOR_IRR] 		= 1;	
		EE2D_NoColor[COLOR_REG] 		= 1;	
	}
	if (theGpo->PlotBoundary == YES)
		EE2D_NoColor[COLOR_BND] 		= 0;	
	
	
	EE2D_Color[COLOR_COPY]			= theOD->yellow;
	EE2D_Color[COLOR_IRR]			= theOD->green;
	EE2D_Color[COLOR_REG]			= theOD->red;
	EE2D_Color[COLOR_LOWER_LEVEL]	= theOD->white;
	EE2D_Color[COLOR_EDGE]			= theOD->black;
	EE2D_Color[COLOR_BND]			= theOD->blue;
	EE2D_Color[COLOR_ELEMID]		= theOD->orange;
		
	EE2D_Elem2Plot[PLOT_ALL]		= 0;
	EE2D_Elem2Plot[PLOT_COPY]		= 0;
	EE2D_Elem2Plot[PLOT_IRR]		= 0;
	EE2D_Elem2Plot[PLOT_REG]		= 0;
	
		
	switch (theGpo->WhichElem)
	{
		case PO_NO:
			break;
		case PO_ALL:
			EE2D_Elem2Plot[PLOT_ALL] = 1;
		case PO_COPY:
			EE2D_Elem2Plot[PLOT_COPY] = 1;
		case PO_IRR:
			EE2D_Elem2Plot[PLOT_IRR] = 1;
		case PO_REG:
			EE2D_Elem2Plot[PLOT_REG] = 1;
	}
	EE2D_RefMark					= theGpo->PlotRefMarks;
	EE2D_ColorRefMark				= theOD->magenta;
	EE2D_IndMark					= theGpo->PlotIndMarks;
	EE2D_ColorIndMark				= theOD->red;
	EE2D_ElemID 					= 0;
	if (theGpo->PlotElemID == YES)
	EE2D_ElemID 				= 1;
	EE2D_ShrinkFactor				= theGpo->ShrinkFactor;
	#ifdef ModelP
	{
		GRID *theGrid = GRID_ON_LEVEL(theMG,0);
		NODE *theNode;
		INT  nodes = 0;
		
		EE2D_PartShrinkFactor		= theGpo->PartShrinkFactor;
		if (EE2D_PartShrinkFactor < 1.0)
		{
			nodes = 0;
			V2_CLEAR(EE2D_PartMidPoint)
			for (theNode=PFIRSTNODE(theGrid); theNode!=NULL; theNode=SUCCN(theNode)) {
				V2_ADD(EE2D_PartMidPoint,CVECT(MYVERTEX(theNode)),EE2D_PartMidPoint)
				nodes++;
			}
			if (nodes > 0)
				V2_SCALE(1.0/(DOUBLE)nodes,EE2D_PartMidPoint)
		}
	}
	#endif
	EE2D_Property = 0;
	if (theGpo->ElemColored==2)
	{
		EE2D_NProperty = MG_NPROPERTY(theMG);
		if (EE2D_NProperty>0 && EE2D_NProperty<EE_MAX_PROP)
		{
		    EE2D_Property = 1;
			for (i=0; i<=EE2D_NProperty; i++)
			EE2D_PropertyColor[i] = theOD->spectrumStart 
			  + i*(DOUBLE)(theOD->spectrumEnd - theOD->spectrumStart)
				/ (DOUBLE)EE2D_NProperty;
		}
		else
		{
			theGpo->ElemColored = 1;
			EE2D_Property = 0;
			UserWrite("wrong NProperty, switch back to standard mode\n");
		}
	}
	
	/* mark surface elements */
	EE2D_MaxLevel = CURRENTLEVEL(theMG);
	if (MarkElements2D(theMG,0,EE2D_MaxLevel)) return (1);

	return (0);
}

static INT EW_PreProcess_PlotGridBefore2D (PICTURE *thePicture, WORK *theWork)
{
	struct ElemScalarPlotObj2D *theEspo;
	OUTPUTDEVICE *theOD;
	MULTIGRID *theMG;
	
	theEspo = &(PIC_PO(thePicture)->theEspo);
	theOD  = PIC_OUTPUTDEV(thePicture);
	theMG  = PO_MG(PIC_PO(thePicture));
	
	if ((theEspo->mode!=PO_CONTOURS_EQ) || !theEspo->PlotGrid)
		return (1);

	EB_ColorGrid = theOD->black;
	
	/* mark surface elements */
	if (MarkElements2D(theMG,0,CURRENTLEVEL(theMG))) return (1);

	return (0);
}

static INT EW_PreProcess_PlotGridAfter2D (PICTURE *thePicture, WORK *theWork)
{
	struct ElemScalarPlotObj2D *theEspo;
	OUTPUTDEVICE *theOD;
	MULTIGRID *theMG;
	
	theEspo = &(PIC_PO(thePicture)->theEspo);
	theOD  = PIC_OUTPUTDEV(thePicture);
	theMG  = PO_MG(PIC_PO(thePicture));
	
	if ((theEspo->mode!=PO_COLOR) || !theEspo->PlotGrid)
		return (1);

	EB_ColorGrid = theOD->white;
	
	/* mark surface elements */
	if (MarkElements2D(theMG,0,CURRENTLEVEL(theMG))) return (1);

	return (0);
}

static DRAWINGOBJ * InvertRefinementMark2D (ELEMENT *theElement, DRAWINGOBJ *theDO)
{
	DOUBLE *x[MAX_CORNERS_OF_ELEM];
	DOUBLE_VECTOR MidPoint,sidemid[MAX_SIDES_OF_ELEM];
	INT i,coe,mark,side;
	
	GetRefinementMark (theElement,&mark,&side);
	
	if (mark==NO_REFINEMENT)
		return (theDO);
	
	/* get coordinates of corners of the element */
	coe = CORNERS_OF_ELEM(theElement);
	for (i=0; i<coe; i++)
		x[i] = CVECT(MYVERTEX(CORNER(theElement,i)));
	
	V2_CLEAR(MidPoint)
	for (i=0; i<coe; i++)
		V2_ADD(MidPoint,x[i],MidPoint)
	V2_SCALE(1.0/(DOUBLE)coe,MidPoint)
	
	for (i=0; i<coe; i++)
		V2_LINCOMB(0.5,x[i],0.5,x[(i+1)%coe],sidemid[i]);
	
	switch (mark)
	{
		case COPY:
			DO_2c(theDO) = DO_TEXT; DO_inc(theDO) 
			DO_2l(theDO) = 0; DO_inc(theDO);
			DO_2c(theDO) = TEXT_INVERSE; DO_inc(theDO) 
			DO_2c(theDO) = TEXT_CENTERED; DO_inc(theDO) 
			DO_2s(theDO) = EE2D_TEXTSIZE; DO_inc(theDO);
			V2_COPY(MidPoint,DO_2Cp(theDO)); DO_inc_n(theDO,2);
			strcpy(DO_2cp(theDO),"COPY"); DO_inc_str(theDO);
			break;
		
		case COARSE:
			DO_2c(theDO) = DO_TEXT; DO_inc(theDO) 
			DO_2l(theDO) = 0; DO_inc(theDO);
			DO_2c(theDO) = TEXT_INVERSE; DO_inc(theDO) 
			DO_2c(theDO) = TEXT_CENTERED; DO_inc(theDO) 
			DO_2s(theDO) = EE2D_TEXTSIZE; DO_inc(theDO);
			V2_COPY(MidPoint,DO_2Cp(theDO)); DO_inc_n(theDO,2);
			strcpy(DO_2cp(theDO),"COARSEN"); DO_inc_str(theDO);
			break;
		
		case RED:
			if (coe==TRIANGLE)
			{
				DO_2c(theDO) = DO_INVERSE_POLYLINE; DO_inc(theDO) 
				DO_2c(theDO) = 4; DO_inc(theDO) 
				V2_COPY(sidemid[0],DO_2Cp(theDO)); DO_inc_n(theDO,2);
				V2_COPY(sidemid[1],DO_2Cp(theDO)); DO_inc_n(theDO,2);
				V2_COPY(sidemid[2],DO_2Cp(theDO)); DO_inc_n(theDO,2);
				V2_COPY(sidemid[0],DO_2Cp(theDO)); DO_inc_n(theDO,2);
			}
			else
			{
				DO_2c(theDO) = DO_INVERSE_LINE; DO_inc(theDO) 
				V2_COPY(sidemid[0],DO_2Cp(theDO)); DO_inc_n(theDO,2);
				V2_COPY(sidemid[2],DO_2Cp(theDO)); DO_inc_n(theDO,2);
				DO_2c(theDO) = DO_INVERSE_LINE; DO_inc(theDO) 
				V2_COPY(sidemid[1],DO_2Cp(theDO)); DO_inc_n(theDO,2);
				V2_COPY(sidemid[3],DO_2Cp(theDO)); DO_inc_n(theDO,2);
			}
			break;
		
		case BLUE:
			if (coe==QUADRILATERAL)
			{
				DO_2c(theDO) = DO_INVERSE_LINE; DO_inc(theDO) 
				V2_COPY(sidemid[side],DO_2Cp(theDO)); DO_inc_n(theDO,2);
				V2_COPY(sidemid[(side+2)%coe],DO_2Cp(theDO)); DO_inc_n(theDO,2);
			}
			else
			{
				DO_2c(theDO) = DO_INVERSE_LINE; DO_inc(theDO) 
				V2_COPY(sidemid[side],DO_2Cp(theDO)); DO_inc_n(theDO,2);
				V2_COPY(x[(side+2)%coe],DO_2Cp(theDO)); DO_inc_n(theDO,2);
			}
			break;
		
		default:
			DO_2c(theDO) = DO_TEXT; DO_inc(theDO) 
			DO_2l(theDO) = 0; DO_inc(theDO);
			DO_2c(theDO) = TEXT_INVERSE; DO_inc(theDO) 
			DO_2c(theDO) = TEXT_CENTERED; DO_inc(theDO) 
			DO_2s(theDO) = EE2D_TEXTSIZE; DO_inc(theDO);
			V2_COPY(MidPoint,DO_2Cp(theDO)); DO_inc_n(theDO,2);
			strcpy(DO_2cp(theDO),"?"); DO_inc_str(theDO);
			break;
	}
	return (theDO);
}

/****************************************************************************/
/*
   EW_PreProcess_MarkElement2D - Initialize input variables marking elements 2D 

   SYNOPSIS:
   static INT EW_PreProcess_MarkElement2D (PICTURE *thePicture, WORK *theWork);

   PARAMETERS:
.  thePicture -
.  theWork -
  
   DESCRIPTION:
   This function initializes input variables marking elements 2D.

   RETURN VALUE:
   INT
.n    0 if ok
.n    1 if error occured.
*/
/****************************************************************************/

static INT EW_PreProcess_MarkElement2D (PICTURE *thePicture, WORK *theWork)
{
	struct GridPlotObj2D *theGpo;
	OUTPUTDEVICE *theOD;
	MULTIGRID *theMG;
	DOUBLE_VECTOR point;
	INT OldMousePos[2],status;
	
	theGpo = &(PIC_PO(thePicture)->theGpo);
	theOD  = PIC_OUTPUTDEV(thePicture);
	theMG  = PO_MG(PIC_PO(thePicture));
	
	if (!theGpo->PlotRefMarks)
	{
		PrintErrorMessage('E',"mark","first switch r option on in grid object");
		return (1);
	}
	
	/* store mouse position */
	OldMousePos[_X_] = W_MARKELEMENT_WORK(theWork)->PixelX;
	OldMousePos[_Y_] = W_MARKELEMENT_WORK(theWork)->PixelY;
	
	/* set globals */
	ME2D_rule						= W_MARKELEMENT_WORK(theWork)->rule;
	
	EE2D_Elem2Plot[PLOT_ALL]		= 0;
	EE2D_Elem2Plot[PLOT_COPY]		= 0;
	EE2D_Elem2Plot[PLOT_IRR]		= 0;
	EE2D_Elem2Plot[PLOT_REG]		= 0;
	switch (theGpo->WhichElem)
	{
		case PO_NO:
			break;
		case PO_ALL:
			EE2D_Elem2Plot[PLOT_ALL] = 1;
		case PO_COPY:
			EE2D_Elem2Plot[PLOT_COPY] = 1;
		case PO_IRR:
			EE2D_Elem2Plot[PLOT_IRR] = 1;
		case PO_REG:
			EE2D_Elem2Plot[PLOT_REG] = 1;
	}
	
	/* CAUTION: using EE2D_MaxLevel (i.e. the last setting used) can be wrong
	   actually one should use the level chosen for the last plot of THIS picture */
	if (MarkElements2D(theMG,0,EE2D_MaxLevel)) return (1);
	
	/* get search rectangle */
	status = MousePullFrame(thePicture,OldMousePos,&ME2D_xmin,&ME2D_xmax,&ME2D_ymin,&ME2D_ymax);
	
	if (status==REJECTED)
		return (1);
	
	if (status==MOUSE_NOT_MOVED)
	{
		/* mark element containing the mouse point */
		ME2D_pointIn = TRUE;
		point[_X_] = ME2D_xmin;
		point[_Y_] = ME2D_ymin;
		
		/* transform into physical coordinates */
		V2_TRAFOM3_V2(point,InvObsTrafo,ME2D_point);
	}
	else
		/* mark elements with center of mass contained in rectangle */
		ME2D_pointIn = FALSE;
	
	return (0);
}

static INT EW_MarkElementEval2D (ELEMENT *theElement, DRAWINGOBJ *theDO)
{
	DOUBLE help[2],*corner;
	DOUBLE_VECTOR cm;
	COORD_POINT a;
	INT i,coe;
	
	ME2D_found = FALSE;
	ME2D_elem  = theElement;
	
	/* check element */
	if (ME2D_pointIn)
	{
		if (PointInElement(ME2D_point,theElement))
			ME2D_found = TRUE;
	}
	else
	{
		/* calculate center of mass */
		coe = CORNERS_OF_ELEM(theElement);
		V2_CLEAR(cm);
		for (i=0; i<coe; i++)
		{
			corner = CVECT(MYVERTEX(CORNER(theElement,i)));
			V2_ADD(cm,corner,cm);
		}
		V2_SCALE(1.0/coe,cm);
		
		V2_TRAFOM3_V2(cm,ObsTrafo,help);
		(*OBS_ProjectProc)(help,&a);
		
		/* in rectangle? */
		if ((ME2D_xmin<=a.x) && (a.x<=ME2D_xmax))
			if ((ME2D_ymin<=a.y) && (a.y<=ME2D_ymax))
				ME2D_found = TRUE;
	}
	
	return (0);
}

static INT EW_MarkElement2D (DRAWINGOBJ *q)
{
	DRAWINGOBJ *qstart;
	INT rule,side;
	
	if (!ME2D_found) return (0);
	
	if (!EstimateHere(ME2D_elem))
		return (0);
	
	qstart = q;
	
	/* invert old mark */
	q = InvertRefinementMark2D(ME2D_elem,q);
	
	GetRefinementMark (ME2D_elem,&rule,&side);
	
	/* mark */
	if ((ME2D_rule==BLUE)&&(rule==ME2D_rule))
		side = (side+1)%CORNERS_OF_ELEM(ME2D_elem);
	else
		side = 0;
	MarkForRefinement(ME2D_elem,ME2D_rule,(void *)side);
	
	/* invert new mark */
	q = InvertRefinementMark2D(ME2D_elem,q);
	
	/* terminate plot commands and plot */
	DO_2c(q) = DO_NO_INST;
	Draw2D(qstart);
	
	if (ME2D_pointIn)
		return (1);		/* mark ONE element, we are done */
	
	return (0);
}

/****************************************************************************/
/*
   EW_PreProcess_SelectElement2D - Initialize input variables finding element 2D 

   SYNOPSIS:
   static INT EW_PreProcess_SelectElement2D (PICTURE *thePicture, WORK *theWork);

   PARAMETERS:
.  thePicture -
.  theWork -
  
   DESCRIPTION:
   This function initializes input variables finding element 2D.

   RETURN VALUE:
   INT
.n    0 if ok
.n    1 if error occured.
*/
/****************************************************************************/

static INT EW_PreProcess_SelectElement2D (PICTURE *thePicture, WORK *theWork)
{
	struct GridPlotObj2D *theGpo;
	OUTPUTDEVICE *theOD;
	MULTIGRID *theMG;
	
	theGpo = &(PIC_PO(thePicture)->theGpo);
	theOD  = PIC_OUTPUTDEV(thePicture);
	theMG  = PO_MG(PIC_PO(thePicture));
	
	EE2D_NoColor[COLOR_EDGE]			= 1;	
	EE2D_NoColor[COLOR_LOWER_LEVEL] 	= 1;
	EE2D_NoColor[COLOR_BND] 			= 1;	
	EE2D_NoColor[COLOR_COPY]			= 1;	
	EE2D_NoColor[COLOR_IRR] 			= 1;	
	EE2D_NoColor[COLOR_REG] 			= 1;	
	EE2D_NoColor[COLOR_BND] 			= 1;	
	
	EE2D_Elem2Plot[PLOT_ALL]		= 0;
	EE2D_Elem2Plot[PLOT_COPY]		= 0;
	EE2D_Elem2Plot[PLOT_IRR]		= 0;
	EE2D_Elem2Plot[PLOT_REG]		= 0;
	switch (theGpo->WhichElem)
	{
		case PO_NO:
			break;
		case PO_ALL:
			EE2D_Elem2Plot[PLOT_ALL] = 1;
		case PO_COPY:
			EE2D_Elem2Plot[PLOT_COPY] = 1;
		case PO_IRR:
			EE2D_Elem2Plot[PLOT_IRR] = 1;
		case PO_REG:
			EE2D_Elem2Plot[PLOT_REG] = 1;
	}
	
	FE2D_found							= 0;
	
	/* store mouse position */
	FE2D_MousePos.x = W_SELECTELEMENT_WORK(theWork)->PixelX;
	FE2D_MousePos.y = W_SELECTELEMENT_WORK(theWork)->PixelY;
	
	/* CAUTION: using EE2D_MaxLevel (i.e. the last setting used) can be wrong
	   actually one should use the level chosen for the last plot of THIS picture */
	if (MarkElements2D(theMG,0,EE2D_MaxLevel)) return (1);

	return (0);
}
	
/****************************************************************************/
/*
   EW_BndOfElemEval2D -  Evaluate bnd of 2D element

   SYNOPSIS:
   static INT EW_BndOfElemEval2D (ELEMENT *theElement, DRAWINGOBJ *theDO);

   PARAMETERS:
.  theElement - 
.  theDO -

   DESCRIPTION:
   This function evaluates bnd of 2D element (triangle/quadrilateral). 

   RETURN VALUE:
   INT
.n    0 if ok
.n    1 if error occured.
*/ 
/****************************************************************************/

static INT EW_BndOfElemEval2D (ELEMENT *theElement, DRAWINGOBJ *theDO)
{
	INT i, n;
	DOUBLE *x[MAX_CORNERS_OF_ELEM];
	
	if (OBJT(theElement)==BEOBJ)
	{
		/* get coordinates of corners of the element */
		for (i=0; i<CORNERS_OF_ELEM(theElement); i++)
			x[i] = CVECT(MYVERTEX(CORNER(theElement,i)));
	
		/* store bnd sides on drawing obj */
		n = CORNERS_OF_ELEM(theElement);
		for (i=0; i<n; i++)
		{
			if (INNER_SIDE(theElement,i)) continue;
			DO_2c(theDO) = DO_LINE; DO_inc(theDO) 
			DO_2l(theDO) = EE2D_Color[COLOR_BND]; DO_inc(theDO);
			V2_COPY(x[i],DO_2Cp(theDO)); DO_inc_n(theDO,2);
			V2_COPY(x[(i+1)%n],DO_2Cp(theDO)); DO_inc_n(theDO,2);
		}
	}
		
	DO_2c(theDO) = DO_NO_INST;

	#ifdef ModelP
	WOP_DObjPnt = theDO;
	#endif
	
	return (0);
}

static INT EW_PreProcess_Bnd2D (PICTURE *thePicture, WORK *theWork)
{
	struct GridPlotObj2D *theGpo;
	OUTPUTDEVICE *theOD;
	MULTIGRID *theMG;

	theGpo = &(PIC_PO(thePicture)->theGpo);
	theOD  = PIC_OUTPUTDEV(thePicture);
	theMG  = PO_MG(PIC_PO(thePicture));
	
	BND_PlotBoundary				= theGpo->PlotBoundary;
	BND_BndColor					= theOD->blue;
	BND_InnerBndColor			= theOD->cyan;
	BND_Resolution       		= 10;
	
	BND_MG						= theMG;
	UgSetLineWidth (2);

	/* mark surface elements */
	if (MarkElements_MGS_Bnd(theMG,0,CURRENTLEVEL(theMG))) return (1);
	
	return (0);
}

static INT EW_PreProcess_VecMatBnd2D (PICTURE *thePicture, WORK *theWork)
{
	struct VecMatPlotObj2D *theVmo;
	OUTPUTDEVICE *theOD;
	MULTIGRID *theMG;

	theVmo = &(PIC_PO(thePicture)->theVmo);
	theOD  = PIC_OUTPUTDEV(thePicture);
	theMG  = PO_MG(PIC_PO(thePicture));
	
	BND_PlotBoundary			= theVmo->Boundary;
	BND_BndColor				= theOD->blue;
	BND_InnerBndColor			= theOD->cyan;
	
	BND_MG						= theMG;
	BND_Resolution				= 10;
	UgSetLineWidth (2);
	
	return (0);
}

static INT EW_BndEval2D (ELEMENT *theElement, DRAWINGOBJ *theDO)
{
	DOUBLE alpha,beta,delta,lambda;
	INT res;
	DOUBLE_VECTOR x0,x1;
	long Color;
	BNDS *theSide;
	INT i,j,left,right;

	if (!BND_PlotBoundary || OBJT(theElement)==IEOBJ)
	{
		DO_2c(theDO) = DO_NO_INST;
		#ifdef ModelP
	        WOP_DObjPnt = theDO;
	        #endif
		return (0);
	}
	
	/* plot boundary segments and their ids (if) */
	for (i=0; i<SIDES_OF_ELEM(theElement); i++)
	  {
		theSide = ELEM_BNDS(theElement,i);
		if (theSide == NULL)
		  continue;
		BNDS_BndSDesc(theSide,&left,&right);
		if ((left==0)||(right==0))
			Color = BND_BndColor;
		else
			Color = BND_InnerBndColor;
		alpha  = 0.0;
		beta   = 1.0;
		res    = BND_Resolution;
		delta  = (beta - alpha) / ((DOUBLE)res);

		/* plot boundary with resolution */
		lambda = alpha;
		if (BNDS_Global(theSide,&lambda,x0)) 
		  return (1);
		for (j=1; j<=res; j++)
		  {
			lambda += delta;
			if (j==res) 
			  lambda = beta;
			if (BNDS_Global(theSide,&lambda,x1)) 
			  return (1);
			DO_2c(theDO) = DO_LINE; DO_inc(theDO) 
			  DO_2l(theDO) = Color; DO_inc(theDO);
			V2_COPY(x0,DO_2Cp(theDO)); DO_inc_n(theDO,2);
			V2_COPY(x1,DO_2Cp(theDO)); DO_inc_n(theDO,2);	
			V2_COPY(x1,x0);
		  }
	  }

	DO_2c(theDO) = DO_NO_INST;

        #ifdef ModelP
	WOP_DObjPnt = theDO;
	#endif

	return (0);
}

static INT EW_PostProcess_Bnd2D (PICTURE *thePicture, WORK *theWork)
{
	/* reset standard line width */
	UgSetLineWidth (1);
	
	return (0);
}

/****************************************************************************/
/*																			*/
/* Function:  NW_PreProcess_PlotNodes2D 									*/
/*																			*/
/* Purpose:   initialize input variables of NW_PlotNodes2D					*/
/*																			*/
/* Input:	  PICTURE *thePicture, WORK *theWork							*/
/*																			*/
/* Return:	  INT 0: ok 													*/
/*			  INT 1: an error occurred										*/
/*																			*/
/****************************************************************************/

static INT NW_PreProcess_PlotNodes2D (PICTURE *thePicture, WORK *theWork)
{
	struct GridPlotObj2D *theGpo;
	OUTPUTDEVICE *theOD;
	MULTIGRID *theMG;
	INT mode;

	theGpo = &(PIC_PO(thePicture)->theGpo);
	theOD  = PIC_OUTPUTDEV(thePicture);
	theMG  = PO_MG(PIC_PO(thePicture));
	
	NE_IDColor					= theOD->black;
	NE_BndMarkerColor			= theOD->red;
	NE_CornerMarkerColor		= theOD->red;
	NE_InnerMarkerColor 		= theOD->red;
	NE_InnerMarker				= FILLED_CIRCLE_MARKER;
	NE_BndMarker				= FILLED_SQUARE_MARKER;
	NE_CornerMarker				= FILLED_RHOMBUS_MARKER;
	NE_InnerMarkerSize			= 4;
	NE_BndMarkerSize			= 4;
	NE_CornerMarkerSize			= 4;
	
	NE_EvalNodeID				= 0;
	NE_EvalInnerNode			= 0;
	NE_EvalBndNode				= 0;
	if (theGpo->PlotNodeID == YES)
		NE_EvalNodeID			= 1;
	if (theGpo->PlotNodes == YES)
	{
		NE_EvalInnerNode		= 1;
		NE_EvalBndNode			= 1;
	}
	
	/* mark nodes */
	switch (theGpo->WhichElem)
	{
		case PO_ALL:
			mode = MARKMODE_ALL;
			break;
		case PO_COPY:
			mode = MARKMODE_COPY;
			break;
		case PO_IRR:
			mode = MARKMODE_IRREG;
			break;
		case PO_REG:
			mode = MARKMODE_REG;
			break;
		default:
			RETURN(1);
	}
	if (MarkNodes_OfMarkedElem(theMG,0,CURRENTLEVEL(theMG),mode)) return (1);
	
	return (0);
}

/****************************************************************************/
/*
   DynInfo_VecMat2D - print dynamic info for 2D VecMat for infobox of ugwindow

   SYNOPSIS:
   INT DynInfo_VecMat2D (PICTURE *pic, INT tool, INT fct, const INT mp[2], char *text)

   PARAMETERS:
.  pic  - print info for this picture
.  tool - current tool
.  fct  - currenr tool function
.  mp   - mouse position in window
.  text - resulting info text

   DESCRIPTION:
   The position of the mouse is given in physical coordinates.

   RETURN VALUE:
   INT
.n   0 if text will change with mouse position
.n   1 if text is static
*/
/****************************************************************************/

static INT DynInfo_VecMat2D (PICTURE *pic, INT tool, INT fct, const INT mp[2], char *text)
{
	VIEWEDOBJ *vo;
	DOUBLE cpt[2];
	
	if (PIC_VALID(pic) == NO)
	{
		strcpy(text,"pic invalid");
		return (1);
	}
	
	vo = PIC_VO(pic);
	V2_TRAFOM3_V2(mp,InvObsTrafo,cpt);
	
	sprintf(text,"(%5.3e,%5.3e)",cpt[0],cpt[1]);
	
	return (0);
}

/****************************************************************************/
/*
   ClickAct_VecMat2D - tool dependend act on click for 2D VecMat

   SYNOPSIS:
   INT ClickAct_VecMat2D (PICTURE *pic, INT tool, INT fct, const INT mp[2])

   PARAMETERS:
.  pic  - print info for this picture
.  tool - current tool
.  fct  - currenr tool function
.  mp   - mouse position in window

   DESCRIPTION:
.  hand - select vector

   RETURN VALUE:
   INT
.n   0 if ok
.n   1 if action for this tool is not available
.n   __LINE__ if an error occured
*/
/****************************************************************************/

static INT ClickAct_VecMat2D (PICTURE *pic, INT tool, INT fct, const INT mp[2])
{
	WORK theWork;
	
	switch (tool)
	{
		case handTool:
			W_ID(&theWork) = SELECTVECTOR_WORK;
			W_SELECTNODE_WORK(&theWork)->PixelX = mp[0];
			W_SELECTNODE_WORK(&theWork)->PixelY = mp[1];
			break;
		default:
			return (1);
	}
	if (WorkOnPicture(pic,&theWork))
		return (__LINE__);
	
	return (0);
}

/****************************************************************************/
/*																			*/
/* Function:  VW_VecMatPreProcess		 									*/
/*																			*/
/* Purpose:   initialize input variables of NW_PlotNodes2D					*/
/*																			*/
/* Input:	  PICTURE *thePicture, WORK *theWork							*/
/*																			*/
/* Return:	  INT 0: ok 													*/
/*			  INT 1: an error occurred										*/
/*																			*/
/****************************************************************************/

static INT VW_VecMatPreProcess (PICTURE *thePicture, WORK *theWork)
{
	struct VecMatPlotObj2D *theVmo;
	OUTPUTDEVICE *theOD;
	MULTIGRID *theMG;
	GRID *theGrid;
	VECTOR *vec;
	BLOCKVECTOR  *theBV;
	INT nColors,BVn;

	theVmo = &(PIC_PO(thePicture)->theVmo);
	theOD  = PIC_OUTPUTDEV(thePicture);
	theMG  = PO_MG(PIC_PO(thePicture));
	theGrid = GRID_ON_LEVEL(theMG,CURRENTLEVEL(theMG));
	
	/* set globals for eval function */
	VM_Marker					= theVmo->Marker;
	VN_MarkerColor[0]			= theOD->magenta;
	VN_MarkerColor[1]			= theOD->black;
	VN_MarkerColor[2]			= theOD->yellow;
	VN_MarkerColor[3]			= theOD->red;
	VM_Type[NODEVECTOR]			= theVmo->Type[NODEVECTOR];
	VM_Type[EDGEVECTOR]			= theVmo->Type[EDGEVECTOR];
	VM_Type[ELEMVECTOR]			= theVmo->Type[ELEMVECTOR];
	VM_Connections				= theVmo->Connections;
	VM_MColor					= theOD->red;
	VM_MExtra					= theVmo->Extra;
	VM_MExtraColor				= theOD->cyan;
	VM_Idx						= theVmo->Idx;
	VM_IdxColor					= theOD->blue;
	VM_Order					= theVmo->Order;
	VM_Dependency				= theVmo->Dependency;
	VM_StrongColor				= theOD->green;
	VM_ConnectVectors			= theVmo->ConnectVectors;
	VM_ConnectColor				= theOD->red;
	VM_CutColor					= theOD->black;
	VM_VecData					= (theVmo->vd!=NULL);
	VM_MatData					= (theVmo->md!=NULL);
	VM_tvd						= theVmo->vd;
	VM_tmd						= theVmo->md;
	VM_VecMatColor				= theOD->black;
	
	VM_LastVector				= NULL;
	
	/* check if ordered */
	if (VM_Order && GFIRSTBV(theGrid)==NULL)
	{
		VM_Order=NO;
		UserWrite("grid is not ordered: switch back to non-ordered mode\n");
	}
		
	if (VM_Order)
	{
		nColors = 0;
		switch (VM_Order)
		{
			case 1:
				for (theBV=GFIRSTBV(theGrid); theBV!=NULL; theBV=BVSUCC(theBV))
				{
					for (vec=BVFIRSTVECTOR(theBV); vec!=BVENDVECTOR(theBV); vec=SUCCVC(vec))
						VINDEX(vec) = nColors;
					nColors++;
				}
				break;
			
			case 2:
				for (theBV=GFIRSTBV(theGrid); theBV!=NULL; theBV=BVSUCC(theBV))
				{
					BVn = BVNUMBER(theBV);
					nColors = MAX(ORD_CYC(BVn),nColors);
					for (vec=BVFIRSTVECTOR(theBV); vec!=BVENDVECTOR(theBV); vec=SUCCVC(vec))
						VINDEX(vec) = BVn;
				}
				nColors++;
				break;
			
			case 3:
				for (theBV=GFIRSTBV(theGrid); theBV!=NULL; theBV=BVSUCC(theBV))
				{
					BVn = BVNUMBER(theBV);
					nColors = MAX(ORD_LIN(BVn),nColors);
					for (vec=BVFIRSTVECTOR(theBV); vec!=BVENDVECTOR(theBV); vec=SUCCVC(vec))
						VINDEX(vec) = BVn;
				}
				break;
		}
		
		if (nColors<=0)
			return (1);
		
		VM_OrderStart			= theOD->spectrumStart;
		VM_OrderDelta			= (theOD->spectrumEnd - theOD->spectrumStart) / (float) nColors;
	}
	
	return (0);
}

static INT VW_MatEval (VECTOR *vec, DRAWINGOBJ *theDO)
{
	MATRIX *mat;
	DOUBLE_VECTOR mypos,nbpos;
	long color;
	
	if (!VM_Type[VTYPE(vec)])
	{
		DO_2c(theDO) = DO_NO_INST;

                #ifdef ModelP
	        WOP_DObjPnt = theDO;
	        #endif

		return (0);
	}
	
	VectorPosition(vec,mypos);
	
	if (VM_ConnectVectors)
	{
		if (VM_LastVector!=NULL)
			if (!(VM_Order && (VINDEX(VM_LastVector)!=VINDEX(vec))))
			{
				VectorPosition(VM_LastVector,nbpos);
				
				DO_2c(theDO) = DO_LINE; DO_inc(theDO) 
				DO_2l(theDO) = VM_ConnectColor; DO_inc(theDO);
				V2_COPY(mypos,DO_2Cp(theDO)); DO_inc_n(theDO,2);
				V2_COPY(nbpos,DO_2Cp(theDO)); DO_inc_n(theDO,2);
				
			}
		VM_LastVector = vec;
	}
	else if (VM_Dependency)
	{
		/* plot dependencies */
		for (mat=MNEXT(VSTART(vec)); mat!=NULL; mat=MNEXT(mat))
			if (!CEXTRA(MMYCON(mat)))
			{
				if (!VM_Type[VTYPE(MDEST(mat))]) continue;
				
				VectorPosition(MDEST(mat),nbpos);

				color = MSTRONG(mat) ? VM_StrongColor:VM_MColor;
				if (MDOWN(mat))
				{
					DO_2c(theDO) = DO_DEPEND; DO_inc(theDO) 
					DO_2l(theDO) = VM_MColor; DO_inc(theDO);
					V2_COPY(mypos,DO_2Cp(theDO)); DO_inc_n(theDO,2);
					V2_COPY(nbpos,DO_2Cp(theDO)); DO_inc_n(theDO,2); 	
				}
				if (MUP(mat))
				{
					DO_2c(theDO) = DO_DEPEND; DO_inc(theDO) 
					DO_2l(theDO) = VM_MColor; DO_inc(theDO);
					V2_COPY(nbpos,DO_2Cp(theDO)); DO_inc_n(theDO,2);
					V2_COPY(mypos,DO_2Cp(theDO)); DO_inc_n(theDO,2); 	
				}
			}
	}
	else if (VM_Connections || VM_MExtra)
	{
		/* plot connections */
		for (mat=MNEXT(VSTART(vec)); mat!=NULL; mat=MNEXT(mat))
			if (VM_Type[VTYPE(MDEST(mat))])
			{
				if (CEXTRA(MMYCON(mat)))
				{
					if (!VM_MExtra) continue;
				}
				else if (!VM_Connections) continue;
				
				VectorPosition(MDEST(mat),nbpos);

				DO_2c(theDO) = DO_LINE; DO_inc(theDO) 
				DO_2l(theDO) = CEXTRA(MMYCON(mat)) ? VM_MExtraColor:VM_MColor; DO_inc(theDO);
				V2_COPY(mypos,DO_2Cp(theDO)); DO_inc_n(theDO,2);
				V2_COPY(nbpos,DO_2Cp(theDO)); DO_inc_n(theDO,2);
			}
	}
	
	DO_2c(theDO) = DO_NO_INST;

        #ifdef ModelP
	WOP_DObjPnt = theDO;
	#endif
	
	return (0);
}

static INT VW_VecEval (VECTOR *vec, DRAWINGOBJ *theDO)
{
	DOUBLE_VECTOR mypos;
	INT markertype,cycle,gen,line;
	long color;
	char setchar;
	static INT number;
	
	if (!VM_Type[VTYPE(vec)])
	{
		DO_2c(theDO) = DO_NO_INST;

                #ifdef ModelP
	        WOP_DObjPnt = theDO;
	        #endif

		return (0);
	}
	
	VectorPosition(vec,mypos);
	
	if (VM_Order)
	{
		cycle = ORD_CYC(VINDEX(vec));
		gen   = ORD_GEN(VINDEX(vec));
		line  = ORD_LIN(VINDEX(vec));
		if 		(gen==0) setchar = 'F';
		else if (gen==1) setchar = 'L';
		else if (gen==2) setchar = 'C';
	}
	
	/* plot markers */
	if (VM_Marker)
	{
		switch (VM_Order)
		{
			case 0:
				color = VN_MarkerColor[VCLASS(vec)];
				markertype = VTYPE(vec);
				break;
			case 1:
				color = VM_OrderStart+VINDEX(vec)*VM_OrderDelta;
				markertype = 0;
				break;
			case 2:
				if (gen!=BV_GEN_C)
					color = VM_OrderStart+cycle*VM_OrderDelta;
				else
					color = VM_CutColor;
				markertype = gen;
				break;
			case 3:
				if (gen!=BV_GEN_C)
					color = VM_OrderStart+line*VM_OrderDelta;
				else
					color = VM_CutColor;
				markertype = gen;
				break;
		}
		
		DO_2c(theDO) = DO_POLYMARK; DO_inc(theDO) 
		DO_2c(theDO) = 1; DO_inc(theDO) 
		DO_2l(theDO) = color; DO_inc(theDO);
		switch (markertype)
		{
			case 0:
				DO_2s(theDO) = FILLED_CIRCLE_MARKER;
				break;
			case 1:
				DO_2s(theDO) = FILLED_RHOMBUS_MARKER;
				break;
			case 2:
				switch (VM_Order)
				{
					case 0:
						DO_2s(theDO) = FILLED_SQUARE_MARKER;
						break;
					case 2:
					case 3:
						DO_2s(theDO) = FILLED_SQUARE_MARKER;
						break;
				}
				break;
		}
		DO_inc(theDO);
		DO_2s(theDO) = VM_MARKERSIZE; DO_inc(theDO);
		V2_COPY(mypos,DO_2Cp(theDO)); DO_inc_n(theDO,2);
	}
	
	/* plot index */
	if (VM_Idx)
	{
		DO_2c(theDO) = DO_TEXT; DO_inc(theDO);
		DO_2l(theDO) = VM_IdxColor; DO_inc(theDO);
		if (VM_Order>1)	
			DO_2c(theDO) = TEXT_INDEXED;
		else
			DO_2c(theDO) = TEXT_REGULAR;
		DO_inc(theDO);
		DO_2c(theDO) = TEXT_NOT_CENTERED; DO_inc(theDO);
		DO_2s(theDO) = VM_TEXTSIZE; DO_inc(theDO);
		V2_COPY(mypos,DO_2Cp(theDO)); DO_inc_n(theDO,2);
		switch (VM_Order)
		{
			case 0:
			case 1:
				sprintf(DO_2cp(theDO),"%d",(int)VINDEX(vec));
				break;
			case 2:
				sprintf(DO_2cp(theDO),"%c|/T%d",(int)setchar,(int)cycle);
				break;
			case 3:
				if (VINDEX(vec)==VM_lastind)
					number++;
				else
				{
					VM_lastind = VINDEX(vec);
					number = 0;
				}
				sprintf(DO_2cp(theDO),"%c|/T%d,%d/H%d",(int)setchar,(int)cycle,(int)line,(int)number);
				break;
		}
		DO_inc_str(theDO);
	}
	
	DO_2c(theDO) = DO_NO_INST;

        #ifdef ModelP
	WOP_DObjPnt = theDO;
	#endif
	
	return (0);
}

static INT EXT_PreProcess_InsertNode2D (PICTURE *thePicture, WORK *theWork)
{
		return (1);
}

static INT EXT_PreProcess_MoveNode2D (PICTURE *thePicture, WORK *theWork)
{
	struct GridPlotObj2D *theGpo;
	OUTPUTDEVICE *theOD;
	MULTIGRID *theMG;
	GRID *theGrid;
	VERTEX *theVertex;
	NODE *theNode;
	DOUBLE pt[2],pos[2],del;
	DOUBLE deltaScreen[2],zeroScreen[2],deltaVector[2],zeroVector[2];
	INT k;
	
	theGpo = &(PIC_PO(thePicture)->theGpo);
	theOD  = PIC_OUTPUTDEV(thePicture);
	theMG  = PO_MG(PIC_PO(thePicture));
	
	MN_xmin	= MIN(PIC_GLL(thePicture)[_X_],PIC_GUR(thePicture)[_X_]);
	MN_xmax	= MAX(PIC_GLL(thePicture)[_X_],PIC_GUR(thePicture)[_X_]);
	MN_ymin	= MIN(PIC_GLL(thePicture)[_Y_],PIC_GUR(thePicture)[_Y_]);
	MN_ymax	= MAX(PIC_GLL(thePicture)[_Y_],PIC_GUR(thePicture)[_Y_]);
	MN_MG	= theMG;
	MN_accept= FALSE;
	MN_MouseMoved = FALSE;
	MN_LastMousePos[0] = W_INSERTNODE_WORK(theWork)->PixelX;
	MN_LastMousePos[1] = W_INSERTNODE_WORK(theWork)->PixelY;
	MN_Resolution = 100;
	
	/* get physical position */
	pt[0] = W_INSERTNODE_WORK(theWork)->PixelX;
	pt[1] = W_INSERTNODE_WORK(theWork)->PixelY;
	V2_TRAFOM3_V2(pt,InvObsTrafo,pos);
	
	/* transform pixel box to physical coordinates */
	zeroScreen[0] = 0.0;	deltaScreen[0] = SMALLPIX;
	zeroScreen[1] = 0.0;	deltaScreen[1] = SMALLPIX;
	V2_TRAFOM3_V2(zeroScreen,InvObsTrafo,zeroVector);
	V2_TRAFOM3_V2(deltaScreen,InvObsTrafo,deltaVector);
	V2_EUKLIDNORM_OF_DIFF(deltaVector,zeroVector,MN_delta);
	
	/* find node */
	for (k=0; k<=CURRENTLEVEL(theMG); k++)
	{
		theGrid = GRID_ON_LEVEL(theMG,k);
		for (theNode=FIRSTNODE(theGrid); theNode!=NULL; theNode=SUCCN(theNode))
		{
			theVertex = MYVERTEX(theNode);
			V2_EUKLIDNORM_OF_DIFF(CVECT(theVertex),pos,del);
			if (del<MN_delta)
			{
				/* gotcha! */
				if (MOVE(theVertex)==0)
				{
					PrintErrorMessage('E',"work","you cannot move corner vertices");
					return (1);
				}
				if ((MOVE(theVertex)==1) && (OBJT(theVertex)!=BVOBJ))
				{
					PrintErrorMessage('E',"work","Move type 1, but no boundary vertex");
					return(1);
				}
				if ((MOVE(theVertex)==2) && (OBJT(theVertex)!=IVOBJ))
				{
					PrintErrorMessage('E',"work","Move type 1, but no interior vertex");
					return(1);
				}
				MN_Node = theNode;
				
				InvalidatePicturesOfMG(MN_MG);
				
				return (0);
			}
		}
	}
	PrintErrorMessage('E',"work","no matching vertex found");
	return (1);
}

static INT EXT_MoveNodeEval2D (DRAWINGOBJ *theDO, INT *end)
{
	VERTEX *theVertex,*nbVertex;
	LINK *theLink;
	DOUBLE nbpos[2];
	DOUBLE pos[2];
	DOUBLE len,l,la,le,dl,bestDist2;
	INT MousePos[2];
	ELEMENT *theElement;
	BNDS *theSide;
	INT i;
	
	if (MouseStillDown())
	{
		MousePosition(MousePos);
		
		if (V2_ISEQUAL(MousePos,MN_LastMousePos))
		{
			DO_2c(theDO) = DO_NO_INST;

                        #ifdef ModelP
	                WOP_DObjPnt = theDO;
	                #endif

			return (0);
		}
		
		/* inside picture? */
		if ((MousePos[0]<MN_xmin) || (MousePos[0]>MN_xmax))
		{
			DO_2c(theDO) = DO_NO_INST;

			#ifdef ModelP
	                WOP_DObjPnt = theDO;
	                #endif

			MN_accept = FALSE;
			return (0);
		}
		if ((MousePos[1]<MN_ymin) || (MousePos[1]>MN_ymax))
		{
			DO_2c(theDO) = DO_NO_INST;

                        #ifdef ModelP
	                WOP_DObjPnt = theDO;
	                #endif

			MN_accept = FALSE;
			return (0);
		}
		
		V2_COPY(MousePos,MN_LastMousePos);
		
		if (MN_MouseMoved)
		{
			/* plot links at last postion inverse */
			for (theLink=START(MN_Node); theLink!=NULL; theLink=NEXT(theLink))
			  {
				nbVertex = MYVERTEX(NBNODE(theLink));
				nbpos[0] = XC(nbVertex);	nbpos[1] = YC(nbVertex);
				DO_2c(theDO) = DO_INVERSE_LINE; DO_inc(theDO) 
				V2_COPY(MN_pos,DO_2Cp(theDO)); DO_inc_n(theDO,2);
				V2_COPY(nbpos,DO_2Cp(theDO)); DO_inc_n(theDO,2);
			  }
		}
		MN_MouseMoved = TRUE;
		
		/* mouse position in the physical system */
		V2_TRAFOM3_V2(MousePos,InvObsTrafo,MN_pos);
		
		theVertex = MYVERTEX(MN_Node);
		
		/* boundary vertex? */
		if (MOVE(theVertex)==1)
		{
		    theElement = VFATHER(theVertex);
		    if (theElement == NULL)
			  {
				DO_2c(theDO) = DO_NO_INST;

                                #ifdef ModelP
	                        WOP_DObjPnt = theDO;
	                        #endif

				MN_accept = FALSE;
				return(0);
			  }
			/* scan resolution points of the segment */
			bestDist2 = MAX_C;
			for (i=0; i<SIDES_OF_ELEM(theElement); i++)
			  {
				theSide = ELEM_BNDS(theElement,i);
				if (theSide == NULL)
				  continue;
				la = 0.0;
				le = 1.0;
				dl = (le - la) / MN_Resolution;

				l = la;
				for (i=1; i<MN_Resolution; i++)
				  {
					l += dl;
					if (BNDS_Global(theSide,&l,pos)) return(1);
					V2_EUKLIDNORM_OF_DIFF(pos,MN_pos,len);
					if (len < bestDist2)
					  {
						bestDist2 = len;
						MN_lambda = (l-la) / (le-la);
					  }
				  }
			  }
		}

		if (bestDist2 == MAX_C)
		  {
			DO_2c(theDO) = DO_NO_INST;

                        #ifdef ModelP
	                WOP_DObjPnt = theDO;
                        #endif

			MN_accept = FALSE;
			return(0);
		  }
		
		/* plot links inverse */
		for (theLink=START(MN_Node); theLink!=NULL; theLink=NEXT(theLink))
		  {
			nbVertex = MYVERTEX(NBNODE(theLink));
			nbpos[0] = XC(nbVertex);	nbpos[1] = YC(nbVertex);
			DO_2c(theDO) = DO_INVERSE_LINE; DO_inc(theDO) 
			V2_COPY(MN_pos,DO_2Cp(theDO)); DO_inc_n(theDO,2);
			V2_COPY(nbpos,DO_2Cp(theDO)); DO_inc_n(theDO,2);
		  }
		
		DO_2c(theDO) = DO_NO_INST;

                #ifdef ModelP
	        WOP_DObjPnt = theDO;
	        #endif
		
		MN_accept = TRUE;
		
		return (0);
	}
	if (MN_MouseMoved)
	{
		/* plot links at last postion inverse */
		for (theLink=START(MN_Node); theLink!=NULL; theLink=NEXT(theLink))
		  {
			nbVertex = MYVERTEX(NBNODE(theLink));
			nbpos[0] = XC(nbVertex);	nbpos[1] = YC(nbVertex);
			DO_2c(theDO) = DO_INVERSE_LINE; DO_inc(theDO) 
		  	V2_COPY(MN_pos,DO_2Cp(theDO)); DO_inc_n(theDO,2);
			V2_COPY(nbpos,DO_2Cp(theDO)); DO_inc_n(theDO,2);
		  }
		DO_2c(theDO) = DO_NO_INST;

                #ifdef ModelP
	        WOP_DObjPnt = theDO;
	        #endif

	}
	*end = TRUE;
	return (0);
}

static INT EXT_PostProcess_MoveNode2D (PICTURE *thePicture, WORK *theWork)
{
	VERTEX *theVertex;

	if (!MN_accept)	return (0);
	
	/* now we have to calculate the moved positions */
	theVertex = MYVERTEX(MN_Node);
	
	if (OBJT(theVertex)==IVOBJ) {
		if (MoveNode(MN_MG,MN_Node,MN_pos)!=GM_OK)
			return (1);
		return (0);
	}
	else {
		if (NTYPE(MN_Node) != MID_NODE) {
			PrintErrorMessage('E',"EXT_PostProcess_MoveNode2D",
							  "on the boundary only midnodes can be moved");
			return (1);
		}
		if (MoveMidNode (MN_MG,MN_Node,MN_lambda))
			return (1);
	}

	return (0);
}

/****************************************************************************/
/*
   NW_PreProcess_SelectNode2D - Initialize input variables of EW_BndOfElemEval2D

   SYNOPSIS:
   static INT NW_PreProcess_SelectNode2D (PICTURE *thePicture, WORK *theWork);

   PARAMETERS:
.  thePicture - 
.  theWork -

   DESCRIPTION:
   This function initializes input variables of EW_BndOfElemEval2D.

   RETURN VALUE:
   INT
.n    0 if ok
.n    1 if error occured.
*/										
/****************************************************************************/

static INT NW_PreProcess_SelectNode2D (PICTURE *thePicture, WORK *theWork)
{
	struct GridPlotObj2D *theGpo;
	OUTPUTDEVICE *theOD;
	MULTIGRID *theMG;
	DOUBLE x,y;
	INT mode,status,OldMousePos[2];

	theGpo = &(PIC_PO(thePicture)->theGpo);
	theOD  = PIC_OUTPUTDEV(thePicture);
	theMG  = PO_MG(PIC_PO(thePicture));
	
	FN2D_found = 0;
	
	/* store mouse position */
	OldMousePos[_X_] = W_SELECTNODE_WORK(theWork)->PixelX;
	OldMousePos[_Y_] = W_SELECTNODE_WORK(theWork)->PixelY;
	
	/* mark nodes */
	switch (theGpo->WhichElem)
	{
		case PO_ALL:
			mode = MARKMODE_ALL;
			break;
		case PO_COPY:
			mode = MARKMODE_COPY;
			break;
		case PO_IRR:
			mode = MARKMODE_IRREG;
			break;
		case PO_REG:
			mode = MARKMODE_REG;
			break;
		default:
			RETURN(1);
	}
	if (MarkNodes_OfMarkedElem(theMG,0,CURRENTLEVEL(theMG),mode)) return (1);
	
	/* get search rectangle */
	status = MousePullFrame(thePicture,OldMousePos,&FN2D_xmin,&FN2D_xmax,&FN2D_ymin,&FN2D_ymax);
	
	if (status==REJECTED)
		return (1);
	
	/* if rectangle < FN2D_ACC enlarge it */
	if (FN2D_xmax-FN2D_xmin < 2*FN2D_ACC)
	{
		x = 0.5*(FN2D_xmax+FN2D_xmin);
		FN2D_xmin = x-FN2D_ACC;
		FN2D_xmax = x+FN2D_ACC;
	}
	if (FN2D_ymax-FN2D_ymin < 2*FN2D_ACC)
	{
		y = 0.5*(FN2D_ymax+FN2D_ymin);
		FN2D_ymin = y-FN2D_ACC;
		FN2D_ymax = y+FN2D_ACC;
	}
	
	return (0);
}

static INT NW_SelectNodeEval2D (NODE *theNode, DRAWINGOBJ *theDO)
{
	/* get node position */
	FN2D_pos = CVECT(MYVERTEX(theNode));
	
	return (0);
}

/****************************************************************************/
/*
   VW_PreProcess_SelectVector2D - Initialize input variables of VW_SelectVectorEval2D

   SYNOPSIS:
   static INT VW_PreProcess_SelectVector2D (PICTURE *thePicture, WORK *theWork);

   PARAMETERS:
.  thePicture - 
.  theWork -

   DESCRIPTION:
   This function initializes input variables of VW_SelectVectorEval2D.

   RETURN VALUE:
   INT
.n    0 if ok
.n    1 if error occured.
*/										
/****************************************************************************/

static INT VW_PreProcess_SelectVector2D (PICTURE *thePicture, WORK *theWork)
{
	struct VecMatPlotObj2D *theVmo;
	OUTPUTDEVICE *theOD;
	MULTIGRID *theMG;
	DOUBLE x,y;
	INT status,OldMousePos[2];

	theVmo = &(PIC_PO(thePicture)->theVmo);
	theOD  = PIC_OUTPUTDEV(thePicture);
	theMG  = PO_MG(PIC_PO(thePicture));
	
	FV2D_found = 0;
	
	/* store mouse position */
	OldMousePos[_X_] = W_SELECTNODE_WORK(theWork)->PixelX;
	OldMousePos[_Y_] = W_SELECTNODE_WORK(theWork)->PixelY;
	
	/* get search rectangle */
	status = MousePullFrame(thePicture,OldMousePos,&FV2D_xmin,&FV2D_xmax,&FV2D_ymin,&FV2D_ymax);
	
	if (status==REJECTED)
		return (1);
	
	/* if rectangle < FV2D_ACC enlarge it */
	if (FV2D_xmax-FV2D_xmin < 2*FV2D_ACC)
	{
		x = 0.5*(FV2D_xmax+FV2D_xmin);
		FV2D_xmin = x-FV2D_ACC;
		FV2D_xmax = x+FV2D_ACC;
	}
	if (FV2D_ymax-FV2D_ymin < 2*FV2D_ACC)
	{
		y = 0.5*(FV2D_ymax+FV2D_ymin);
		FV2D_ymin = y-FV2D_ACC;
		FV2D_ymax = y+FV2D_ACC;
	}
	
	return (0);
}

static INT VW_SelectVectorEval2D (VECTOR *theVector, DRAWINGOBJ *theDO)
{
	/* get vector position */
	VectorPosition(theVector,FV2D_pos);
	
	return (0);
}

static INT VW_SelectVector2D (DRAWINGOBJ *q)
{
	DOUBLE help[2];
	COORD_POINT a, point[4];
	
	if (!VM_Type[VTYPE(WOP_Vector)]) return (0);
	
	V2_TRAFOM3_V2(FV2D_pos,ObsTrafo,help);
	(*OBS_ProjectProc)(help,&a);
	
	/* in rectangle? */
	if ((FV2D_xmin<=a.x) && (a.x<=FV2D_xmax))
		if ((FV2D_ymin<=a.y) && (a.y<=FV2D_ymax))
		{
			if (FV2D_found>=MAXSELECTION)
				return (1);
			
			/* if found, put in/delete from selection list and invert */
			if (SELECTIONMODE(WOP_MG)!=vectorSelection)
				ClearSelection(WOP_MG); 	
			if (AddVectorToSelection(WOP_MG,WOP_Vector) == GM_ERROR)
				if (RemoveVectorFromSelection(WOP_MG,WOP_Vector) == GM_ERROR)
					return (1);
			
			/* invert surrounding of node */
			point[0].x = point[3].x = a.x-FV2D_INVSIZE;
			point[0].y = point[1].y = a.y-FV2D_INVSIZE;
			point[2].x = point[1].x = a.x+FV2D_INVSIZE;
			point[2].y = point[3].y = a.y+FV2D_INVSIZE;
			UgInversePolygon(point,4);
			
			/* we have found a node */
			FV2D_found++;
		}
	
	return (0);
}

/****************************************************************************/
/*
   InvertElementSelection2D - invert element selection 

   SYNOPSIS:
   static INT InvertElementSelection2D (PICTURE *thePicture, WORK *theWork);

   PARAMETERS:
.  thePicture -
.  theWork -

   DESCRIPTION:
   This function inverts element selection.

   RETURN VALUE:
   INT
.n    0 if ok
.n    1 if error occured.
*/
/****************************************************************************/

static INT InvertElementSelection2D (PICTURE *thePicture, WORK *theWork)
{
	ELEMENT *theElement;
	DOUBLE_VECTOR help;
	COORD_POINT points[4];
	INT i, j;
	
	/* evaluate and execute */
	if (SELECTIONMODE(WOP_MG)==elementSelection)
		for (i=0; i<SELECTIONSIZE(WOP_MG); i++)
		{
			theElement = (ELEMENT *)SELECTIONOBJECT(WOP_MG,i);
			if (!USED(theElement)) continue;
			for (j=0; j<CORNERS_OF_ELEM(theElement); j++)
			{
				V2_TRAFOM3_V2(CVECT(MYVERTEX(CORNER(theElement,j))),ObsTrafo,help);
				(*OBS_ProjectProc)(help,points+j);
			}
			UgInversePolygon(points,j);
		}

	return (0);
}
	
/****************************************************************************/
/*                                                                          */
/* Function: InvertNodeSelection2D 											*/
/*																			*/
/* Purpose:   invert node selection 										*/
/*																			*/
/* Input:	  PICTURE *thePicture, WORK *theWork							*/
/*																			*/
/* Output:	  INT 0: ok 													*/
/*				  1: error													*/
/*																			*/
/****************************************************************************/

static INT InvertNodeSelection2D (PICTURE *thePicture, WORK *theWork)
{
	NODE *theNode;
	DOUBLE_VECTOR help;
	COORD_POINT a, point[4];
	INT i;
	
	/* evaluate and execute */
	if (SELECTIONMODE(WOP_MG)==nodeSelection)
		for (i=0; i<SELECTIONSIZE(WOP_MG); i++)
		{
			theNode = (NODE *)SELECTIONOBJECT(WOP_MG,i);
			if (!USED(theNode)) continue;
			V2_TRAFOM3_V2(CVECT(MYVERTEX(theNode)),ObsTrafo,help);
			(*OBS_ProjectProc)(help,&a);
			
			/* invert surrounding of node */
			point[0].x = point[3].x = a.x-FN2D_INVSIZE;
			point[0].y = point[1].y = a.y-FN2D_INVSIZE;
			point[2].x = point[1].x = a.x+FN2D_INVSIZE;
			point[2].y = point[3].y = a.y+FN2D_INVSIZE;
			UgInversePolygon(point,4);
		}
	
	return (0);
}

/****************************************************************************/
/*																			*/
/* Function: InvertNodeSelection2D 											*/
/*																			*/
/* Purpose:   invert node selection 										*/
/*																			*/
/* Input:	  PICTURE *thePicture, WORK *theWork							*/
/*																			*/
/* Output:	  INT 0: ok 													*/
/*				  1: error													*/
/*																			*/
/****************************************************************************/

static INT EW_PostProcess_Line2D (PICTURE *thePicture, WORK *theWork)
{
	OUTPUTDEVICE *theOD;
	struct LinePlotObj2D *theLpo;
	DOUBLE_VECTOR p;
	DRAWINGOBJ *theDO;
	
	theOD  = PIC_OUTPUTDEV(thePicture);
	theLpo = &(PIC_PO(thePicture)->theLpo);

	/* draw y-axis */
	theDO = WOP_DrawingObject;
	DO_2c(theDO) = DO_LINE; DO_inc(theDO) 
	DO_2l(theDO) = theOD->black; DO_inc(theDO);
	p[0] = 0.0; p[1] = 0.0;
	V2_COPY(p,DO_2Cp(theDO)); DO_inc_n(theDO,2);
	p[0] = 0.0; p[1] = theLpo->aspectratio;
	V2_COPY(p,DO_2Cp(theDO)); DO_inc_n(theDO,2);
	Draw2D(WOP_DrawingObject);	

	if (LINE2D_V2Y_offset<0.0)
	{
		/* draw x-axis */
		theDO = WOP_DrawingObject;
		DO_2c(theDO) = DO_LINE; DO_inc(theDO) 
		DO_2l(theDO) = theOD->red; DO_inc(theDO);
		p[0] = 0.0; p[1] = 0.0;
		V2_COPY(p,DO_2Cp(theDO)); DO_inc_n(theDO,2);
		p[0] = 1.0; p[1] = 0.0;
		V2_COPY(p,DO_2Cp(theDO)); DO_inc_n(theDO,2);
		Draw2D(WOP_DrawingObject);
	}
	else if (LINE2D_V2Y_offset>theLpo->aspectratio)
	{
		/* draw x-axis */
		theDO = WOP_DrawingObject;
		DO_2c(theDO) = DO_LINE; DO_inc(theDO) 
		DO_2l(theDO) = theOD->red; DO_inc(theDO);
		p[0] = 0.0; p[1] = theLpo->aspectratio;
		V2_COPY(p,DO_2Cp(theDO)); DO_inc_n(theDO,2);
		p[0] = 1.0; p[1] = theLpo->aspectratio;
		V2_COPY(p,DO_2Cp(theDO)); DO_inc_n(theDO,2);
		Draw2D(WOP_DrawingObject);
	}
	else
	{
		/* draw zero-axis */
		theDO = WOP_DrawingObject;
		DO_2c(theDO) = DO_LINE; DO_inc(theDO) 
		DO_2l(theDO) = theOD->black; DO_inc(theDO);
		p[0] = 0.0; p[1] = LINE2D_V2Y_offset;
		V2_COPY(p,DO_2Cp(theDO)); DO_inc_n(theDO,2);
		p[0] = 1.0; p[1] = LINE2D_V2Y_offset;
		V2_COPY(p,DO_2Cp(theDO)); DO_inc_n(theDO,2);
		Draw2D(WOP_DrawingObject);
	}

	return (0);
}

/****************************************************************************/
/*																			*/
/* Function:  GEN_PostProcess_Line_FR	 									*/
/*																			*/
/* Purpose:   postprocess for findrange of scalar plot						*/
/*																			*/
/* Input:	  PICTURE *thePicture, WORK *theWork							*/
/*																			*/
/* Return:	  INT 0: ok 													*/
/*			  INT 1: an error occurred										*/
/*																			*/
/****************************************************************************/

static INT GEN_PostProcess_Line_FR (PICTURE *thePicture, WORK *theWork)
{
	struct FindRange_Work *FR_Work;
	DOUBLE m,l;
	
	FR_Work = W_FINDRANGE_WORK(theWork);
	
	if (GEN_FR_min>GEN_FR_max)
	{
		UserWrite("findrange failed\n");
		return (0);
	}
	
	/* postprocess findrange */
	if (FR_Work->symmetric==YES)
	{
		GEN_FR_max = MAX(ABS(GEN_FR_min),ABS(GEN_FR_max));
		GEN_FR_min = -GEN_FR_max;
	}
	if (FR_Work->zoom!=1.0)
	{
		m = 0.5*(GEN_FR_max + GEN_FR_min);
		l = 0.5*(GEN_FR_max - GEN_FR_min);
		GEN_FR_min = m - FR_Work->zoom*l;
		GEN_FR_max = m + FR_Work->zoom*l;
	}
	FR_Work->min = GEN_FR_min;
	FR_Work->max = GEN_FR_max;
	
	/* store if */
	if (GEN_FR_put == YES)
	{
		PIC_PO(thePicture)->theLpo.min = GEN_FR_min;
		PIC_PO(thePicture)->theLpo.max = GEN_FR_max;
	}
	
	return (0);
}

/****************************************************************************/
/*																			*/
/* Function:  PlotVecMatData2D				 								*/
/*																			*/
/* Purpose:   plot user data of a vector and his matrices 					*/
/*																			*/
/* Input:	  VECTOR *theVector												*/
/*																			*/
/* Output:	  INT 0: ok 													*/
/*				  1: error													*/
/*																			*/
/****************************************************************************/

static INT PlotVecMatData2D (PICTURE *thePicture, VECTOR *vec)
{
	DOUBLE_VECTOR pos, nbpos, help;
	COORD_POINT a,b;
	VECTOR *nbvec;
	MATRIX *mat;
	INT rt,ct,i,n,nlines,line,nc;
	char buffer[256];
	
	VectorPosition(vec,pos);
	V2_TRAFOM3_V2(pos,ObsTrafo,help);
	(*OBS_ProjectProc)(help,&a);
	
	rt = VTYPE(vec);
	if (VM_MatData)
		nlines = MD_ROWS_IN_RT_CT(VM_tmd,rt,rt);
	else
		nlines = VD_NCMPS_IN_TYPE(VM_tvd,rt);
	a.y += PIC_SIGN_Y(thePicture) * 0.5*nlines * VM_VECMAT_TEXTSIZE*VM_LINEFAC;
	
	for (line=0; line<nlines; line++)
	{
		n = 0;
		if (VM_MatData)
		{
			mat = VSTART(vec);
			nc = MD_COLS_IN_RT_CT(VM_tmd,rt,rt);
			for (i=0; i<nc; i++)
				n += sprintf(buffer+n,"%9.2e ",MVALUE(mat,MD_MCMP_OF_RT_CT(VM_tmd,rt,rt,line*nc+i)));
			buffer[--n] = '\0';		/* cut off last blank */
		}
		if (VM_VecData && VM_MatData)
			n += sprintf(buffer+n,"   ");
		if (VM_VecData)
			n += sprintf(buffer+n,"%9.2e",VVALUE(vec,VD_CMP_OF_TYPE(VM_tvd,rt,line)));
		
		UgCenteredText(a,buffer,TEXT_REGULAR);
		
		/* increment line position */
		a.y -= PIC_SIGN_Y(thePicture) * VM_VECMAT_TEXTSIZE*VM_LINEFAC;
	}
	
	if (VM_MatData)
		for (mat=MNEXT(VSTART(vec)); mat!=NULL; mat=MNEXT(mat))
		{
			nbvec = MDEST(mat);
			ct = VTYPE(nbvec);
			if (!VM_Type[ct]) continue;
			
			VectorPosition(nbvec,nbpos);
			V2_TRAFOM3_V2(nbpos,ObsTrafo,help);
			(*OBS_ProjectProc)(help,&b);
			
			b.y += PIC_SIGN_Y(thePicture) * 0.5*nlines * VM_VECMAT_TEXTSIZE*VM_LINEFAC;
			
			/* print data */
			for (line=0; line<nlines; line++)
			{
				n = 0;
				nc = MD_COLS_IN_RT_CT(VM_tmd,rt,ct);
				for (i=0; i<nc; i++)
					n += sprintf(buffer+n,"%9.2e ",MVALUE(mat,MD_MCMP_OF_RT_CT(VM_tmd,rt,ct,line*nc+i)));
				buffer[--n] = '\0';		/* cut off last blank */
				if (VM_VecData)
					n += sprintf(buffer+n,"   %9.2e",VVALUE(nbvec,VD_CMP_OF_TYPE(VM_tvd,ct,line)));
				
				UgCenteredText(b,buffer,TEXT_REGULAR);
				
				/* increment line position */
				b.y -= PIC_SIGN_Y(thePicture) * VM_VECMAT_TEXTSIZE*VM_LINEFAC;
			}
		}
	
	return (0);
}

/****************************************************************************/
/*																			*/
/* Function:  InvertVectorSelectionOrPlotVMData2D 							*/
/*																			*/
/* Purpose:   invert vector selection 										*/
/*																			*/
/* Input:	  PICTURE *thePicture, WORK *theWork							*/
/*																			*/
/* Output:	  INT 0: ok 													*/
/*				  1: error													*/
/*																			*/
/****************************************************************************/

static INT InvertVectorSelectionOrPlotVMData2D (PICTURE *thePicture, WORK *theWork)
{
	MULTIGRID *theMG;
	GRID *theGrid;
	VECTOR *theVector;
	DOUBLE_VECTOR pos,help;
	COORD_POINT a, point[4];
	INT i;
	
	UgSetTextSize(VM_VECMAT_TEXTSIZE);
	UgSetColor(VM_VecMatColor);
	theMG  = PO_MG(PIC_PO(thePicture));
	theGrid = GRID_ON_LEVEL(theMG,CURRENTLEVEL(theMG));
	
	/* evaluate and execute */
	if (SELECTIONMODE(WOP_MG)==vectorSelection)
		for (i=0; i<SELECTIONSIZE(WOP_MG); i++)
		{
			theVector = (VECTOR *)SELECTIONOBJECT(WOP_MG,i);
			
			if (VM_VecData || VM_MatData)
			{
				if (PlotVecMatData2D(thePicture,theVector)!=0)
					return (1);
			}
			else
			{
				VectorPosition(theVector,pos);
				V2_TRAFOM3_V2(pos,ObsTrafo,help);
				(*OBS_ProjectProc)(help,&a);
				
				/* invert surrounding of vector */
				point[0].x = point[3].x = a.x-FV2D_INVSIZE;
				point[0].y = point[1].y = a.y-FV2D_INVSIZE;
				point[2].x = point[1].x = a.x+FV2D_INVSIZE;
				point[2].y = point[3].y = a.y+FV2D_INVSIZE;
				UgInversePolygon(point,4);
			}
		}
	
	/* reset indices of vectors */
	i = 1;
	for (theVector=FIRSTVECTOR(theGrid); theVector!= NULL; theVector=SUCCVC(theVector))
	   	VINDEX(theVector) = i++;	
	
	return (0);
}
	
/****************************************************************************/
/*
   EW_ElementEval2D - 

   SYNOPSIS:
   static INT EW_ElementEval2D (ELEMENT *theElement, DRAWINGOBJ *theDO);

   PARAMETERS:
.  theElement -
.  theDO -
  
   DESCRIPTION:
   This function evaluates geometry of 2D element (triangle/quadrilateral).

   RETURN VALUE:
   INT
.n    0 if ok
.n    1 if error occured.
*/
/****************************************************************************/

static INT EW_ElementEval2D (ELEMENT *theElement, DRAWINGOBJ *theDO)
{
	INT i, j;
	DOUBLE *x[MAX_CORNERS_OF_ELEM];
	DOUBLE_VECTOR MidPoint, help;
	INT coe,rule;
	void *data;

	coe = CORNERS_OF_ELEM(theElement);
	
	/* get coordinates of corners of the element */
	for (i=0; i<coe; i++)
		x[i] = CVECT(MYVERTEX(CORNER(theElement,i)));

	if (EE2D_IndMark)
	  GetRefinementMark ((const ELEMENT *)theElement,&rule,&data);

	/* store viewable sides on drawing obj */
	if (EE2D_ShrinkFactor==1.0)
	{
		if (EE2D_Property)
		{
			DO_2c(theDO) = DO_SURRPOLYGON; DO_inc(theDO); 
			DO_2c(theDO) = coe; DO_inc(theDO) ;
			if (PROP(theElement)<1 || PROP(theElement)>EE2D_NProperty) return (1);
			DO_2l(theDO) = EE2D_PropertyColor[(int)PROP(theElement)];
			DO_inc(theDO);
			DO_2l(theDO) = EE2D_Color[COLOR_EDGE]; DO_inc(theDO);
			for (j=0; j<coe; j++)
			{
				V2_COPY(x[j],DO_2Cp(theDO));
				DO_inc_n(theDO,2);
			}
		}
		else if (LEVEL(theElement)<EE2D_MaxLevel)
		{
			if (((EE2D_NoColor[COLOR_LOWER_LEVEL] && !EE2D_IndMark)) ||
				(((rule != RED) && EE2D_IndMark)) )
			{
				DO_2c(theDO) = DO_ERASE_SURRPOLYGON; DO_inc(theDO) 
				DO_2c(theDO) = coe; DO_inc(theDO) 
			}
			else
			{
				DO_2c(theDO) = DO_SURRPOLYGON; DO_inc(theDO) 
				DO_2c(theDO) = coe; DO_inc(theDO) 
				if (EE2D_IndMark)
				  DO_2l(theDO) = EE2D_ColorIndMark;
				else
				  DO_2l(theDO) = EE2D_Color[COLOR_LOWER_LEVEL]; 
				DO_inc(theDO);
			}
			DO_2l(theDO) = EE2D_Color[COLOR_EDGE]; DO_inc(theDO);
			#ifdef ModelP
			for (j=0; j<coe; j++)
			{
				V2_LINCOMB(EE2D_PartShrinkFactor,x[j],1.0-EE2D_PartShrinkFactor,EE2D_PartMidPoint,help)
				V2_COPY(help,DO_2Cp(theDO));
				DO_inc_n(theDO,2);
			}
			#else
			for (j=0; j<coe; j++)
			{
				V2_COPY(x[j],DO_2Cp(theDO));
				DO_inc_n(theDO,2);
			}
			#endif
		}
		else
		{
			if (((EE2D_NoColor[ECLASS(theElement)] && !EE2D_IndMark)) ||
				(((rule != RED) && EE2D_IndMark)) )
			{
				DO_2c(theDO) = DO_ERASE_SURRPOLYGON; DO_inc(theDO) 
				DO_2c(theDO) = coe; DO_inc(theDO) 
			}
			else
			{
				DO_2c(theDO) = DO_SURRPOLYGON; DO_inc(theDO) 
				DO_2c(theDO) = coe; DO_inc(theDO) 
				if (EE2D_IndMark)
				  DO_2l(theDO) = EE2D_ColorIndMark;
				else
				  DO_2l(theDO) = EE2D_Color[ECLASS(theElement)]; 
				DO_inc(theDO);
			}
			DO_2l(theDO) = EE2D_Color[COLOR_EDGE]; DO_inc(theDO);
			#ifdef ModelP
			for (j=0; j<coe; j++)
			{
				V2_LINCOMB(EE2D_PartShrinkFactor,x[j],1.0-EE2D_PartShrinkFactor,EE2D_PartMidPoint,help)
				V2_COPY(help,DO_2Cp(theDO));
				DO_inc_n(theDO,2);
			}
			#else
			for (j=0; j<coe; j++)
			{
				V2_COPY(x[j],DO_2Cp(theDO));
				DO_inc_n(theDO,2);
			}
			#endif
		}
	}
	else
	{
		V2_CLEAR(MidPoint)
		for (i=0; i<coe; i++)
			V2_ADD(MidPoint,x[i],MidPoint)
		V2_SCALE(1.0/(DOUBLE)i,MidPoint)
					
		if (EE2D_Property)
		{
			DO_2c(theDO) = DO_SURRPOLYGON; DO_inc(theDO); 
			DO_2c(theDO) = coe; DO_inc(theDO) ;
			if (PROP(theElement)<1 || PROP(theElement)>EE2D_NProperty) return (1);
			DO_2l(theDO) = EE2D_PropertyColor[(int)PROP(theElement)]; 
			DO_inc(theDO);
			DO_2l(theDO) = EE2D_Color[COLOR_EDGE]; DO_inc(theDO);
			for (j=0; j<coe; j++)
			{
				V2_LINCOMB(EE2D_ShrinkFactor,x[j],1.0-EE2D_ShrinkFactor,MidPoint,help)
				V2_COPY(help,DO_2Cp(theDO));
				DO_inc_n(theDO,2);
			}
		}
		else if (LEVEL(theElement)<EE2D_MaxLevel)
		{
			if (((EE2D_NoColor[COLOR_LOWER_LEVEL] && !EE2D_IndMark)) ||
				(((rule != RED) && EE2D_IndMark)) )
			{
				DO_2c(theDO) = DO_ERASE_SURRPOLYGON; DO_inc(theDO) 
				DO_2c(theDO) = coe; DO_inc(theDO) 
			}
			else
			{
				DO_2c(theDO) = DO_SURRPOLYGON; DO_inc(theDO) 
				DO_2c(theDO) = coe; DO_inc(theDO) 
				if (EE2D_IndMark)
				  DO_2l(theDO) = EE2D_ColorIndMark;
				else
				  DO_2l(theDO) = EE2D_Color[COLOR_LOWER_LEVEL]; 
				DO_inc(theDO);
			}
			DO_2l(theDO) = EE2D_Color[COLOR_EDGE]; DO_inc(theDO);
			for (j=0; j<coe; j++)
			{
				V2_LINCOMB(EE2D_ShrinkFactor,x[j],1.0-EE2D_ShrinkFactor,MidPoint,help)
				V2_COPY(help,DO_2Cp(theDO));
				DO_inc_n(theDO,2);
			}
		}
		else
		{
			if (((EE2D_NoColor[ECLASS(theElement)] && !EE2D_IndMark)) ||
				(((rule != RED) && EE2D_IndMark)) )
			{
				DO_2c(theDO) = DO_ERASE_SURRPOLYGON; DO_inc(theDO) 
				DO_2c(theDO) = coe; DO_inc(theDO) 
			}
			else
			{
				DO_2c(theDO) = DO_SURRPOLYGON; DO_inc(theDO) 
				DO_2c(theDO) = coe; DO_inc(theDO) 
				if (EE2D_IndMark)
				  DO_2l(theDO) = EE2D_ColorIndMark;
				else
				  DO_2l(theDO) = EE2D_Color[ECLASS(theElement)]; 
				DO_inc(theDO);
			}
			DO_2l(theDO) = EE2D_Color[COLOR_EDGE]; DO_inc(theDO);
			for (j=0; j<coe; j++)
			{
				V2_LINCOMB(EE2D_ShrinkFactor,x[j],1.0-EE2D_ShrinkFactor,MidPoint,help)
				V2_COPY(help,DO_2Cp(theDO));
				DO_inc_n(theDO,2);
			}
		}
	}
	
	/* plot refinement mark */
	if (EE2D_RefMark)
		theDO = InvertRefinementMark2D(theElement,theDO);
	
	/* plot element ID */
	if (EE2D_ElemID)
	{
		V2_CLEAR(MidPoint)
		for (i=0; i<coe; i++)
			V2_ADD(MidPoint,x[i],MidPoint)
		V2_SCALE(1.0/(DOUBLE)i,MidPoint)
		
		DO_2c(theDO) = DO_TEXT; DO_inc(theDO) 
		DO_2l(theDO) = EE2D_Color[COLOR_ELEMID]; DO_inc(theDO);
		DO_2c(theDO) = TEXT_REGULAR; DO_inc(theDO) 
		DO_2c(theDO) = TEXT_CENTERED; DO_inc(theDO) 
		DO_2s(theDO) = EE2D_TEXTSIZE; DO_inc(theDO);
		V2_COPY(MidPoint,DO_2Cp(theDO)); DO_inc_n(theDO,2);
		#ifdef ModelP
			sprintf(DO_2cp(theDO),"%d/%x",
				(int)ID(theElement),
				(long)DDD_InfoGlobalId(PARHDRE(theElement)));
			DO_inc_str(theDO);
		#else
			sprintf(DO_2cp(theDO),"%d",(int)ID(theElement));
			DO_inc_str(theDO);
		#endif
	}

	DO_2c(theDO) = DO_NO_INST;

        #ifdef ModelP
	WOP_DObjPnt = theDO;
	#endif
	
	return (0);
}

static INT EW_ElementBdryEval2D (ELEMENT *theElement, DRAWINGOBJ *theDO)
{
	INT i, n;
	DOUBLE *x[MAX_CORNERS_OF_ELEM];
	
	/* get coordinates of corners of the element */
	for (i=0; i<CORNERS_OF_ELEM(theElement); i++)
		x[i] = CVECT(MYVERTEX(CORNER(theElement,i)));

	/* store element sides on drawing obj */
	n = CORNERS_OF_ELEM(theElement);
	for (i=0; i<n; i++)
	{
		DO_2c(theDO) = DO_LINE; DO_inc(theDO) 
		DO_2l(theDO) = EB_ColorGrid; DO_inc(theDO);
		V2_COPY(x[i],DO_2Cp(theDO)); DO_inc_n(theDO,2);
		V2_COPY(x[(i+1)%n],DO_2Cp(theDO)); DO_inc_n(theDO,2);
	}
		
	DO_2c(theDO) = DO_NO_INST;

        #ifdef ModelP
	WOP_DObjPnt = theDO;
	#endif
	
	return (0);
}

/****************************************************************************/
/*																			*/
/* Function:  NW_NodesEval2D												*/
/*																			*/
/* Purpose:   evaluate node 												*/
/*																			*/
/* Input:	  NODE *theNode, char *theDrawingObject 						*/
/*																			*/
/* Output:	  INT 0: ok 													*/
/*				  1: error													*/
/*																			*/
/****************************************************************************/

static INT NW_NodesEval2D (NODE *theNode, DRAWINGOBJ *theDO)
{
	if (OBJT(MYVERTEX(theNode))==BVOBJ)
	{
		/* plot marks of boundary nodes */
		if (NE_EvalBndNode)
		{
			DO_2c(theDO) = DO_POLYMARK; DO_inc(theDO) 
			DO_2c(theDO) = 1; DO_inc(theDO) 
			if (MOVE(MYVERTEX(theNode)))
			{
				DO_2l(theDO) = NE_BndMarkerColor; DO_inc(theDO);
				DO_2s(theDO) = NE_BndMarker; DO_inc(theDO);
				DO_2s(theDO) = NE_BndMarkerSize; DO_inc(theDO);
			}
			else
			{
				DO_2l(theDO) = NE_CornerMarkerColor; DO_inc(theDO);
				DO_2s(theDO) = NE_CornerMarker; DO_inc(theDO);
				DO_2s(theDO) = NE_CornerMarkerSize; DO_inc(theDO);
			}
			V2_COPY(CVECT(MYVERTEX(theNode)),DO_2Cp(theDO)); DO_inc_n(theDO,2);
		}
	}
	else
		/* plot marks of inner nodes */
		if (NE_EvalInnerNode)
		{
			DO_2c(theDO) = DO_POLYMARK; DO_inc(theDO) 
			DO_2c(theDO) = 1; DO_inc(theDO) 
			DO_2l(theDO) = NE_InnerMarkerColor; DO_inc(theDO);
			DO_2s(theDO) = NE_InnerMarker; DO_inc(theDO);
			DO_2s(theDO) = NE_InnerMarkerSize; DO_inc(theDO);
			V2_COPY(CVECT(MYVERTEX(theNode)),DO_2Cp(theDO)); DO_inc_n(theDO,2);
		}
	
	/* plot node ID */
	if (NE_EvalNodeID)
	{
		DO_2c(theDO) = DO_TEXT; DO_inc(theDO) 
		DO_2l(theDO) = NE_IDColor; DO_inc(theDO)
		DO_2c(theDO) = TEXT_REGULAR; DO_inc(theDO) 
		DO_2c(theDO) = TEXT_NOT_CENTERED; DO_inc(theDO) 
		DO_2s(theDO) = EE2D_TEXTSIZE; DO_inc(theDO);
		V2_COPY(CVECT(MYVERTEX(theNode)),DO_2Cp(theDO)); DO_inc_n(theDO,2);
		#ifdef ModelP
			sprintf(DO_2cp(theDO),"%d/%x",
				(int)ID(theNode),
				(long)DDD_InfoGlobalId(PARHDR(theNode)));
			DO_inc_str(theDO);
		#else
			sprintf(DO_2cp(theDO),"%d",(int)ID(theNode)); DO_inc_str(theDO);
		#endif
	}

	DO_2c(theDO) = DO_NO_INST;

        #ifdef ModelP
	WOP_DObjPnt = theDO;
	#endif
	
	return (0);
}

static INT EXT_NodesEval2D (DRAWINGOBJ *theDO, INT *end)
{
	if (OBJT(MYVERTEX(NE_Node))==BVOBJ)
	{
		/* plot marks of boundary nodes */
		if (NE_EvalBndNode)
		{
			DO_2c(theDO) = DO_POLYMARK; DO_inc(theDO) 
			DO_2c(theDO) = 1; DO_inc(theDO) 
			if (MOVE(MYVERTEX(NE_Node)))
			{
				DO_2l(theDO) = NE_BndMarkerColor; DO_inc(theDO);
				DO_2s(theDO) = NE_BndMarker; DO_inc(theDO);
				DO_2s(theDO) = NE_BndMarkerSize; DO_inc(theDO);
			}
			else
			{
				DO_2l(theDO) = NE_CornerMarkerColor; DO_inc(theDO);
				DO_2s(theDO) = NE_CornerMarker; DO_inc(theDO);
				DO_2s(theDO) = NE_CornerMarkerSize; DO_inc(theDO);
			}
			V2_COPY(CVECT(MYVERTEX(NE_Node)),DO_2Cp(theDO)); DO_inc_n(theDO,2);
		}
	}
	else
		/* plot marks of inner nodes */
		if (NE_EvalInnerNode)
		{
			DO_2c(theDO) = DO_POLYMARK; DO_inc(theDO) 
			DO_2c(theDO) = 1; DO_inc(theDO) 
			DO_2l(theDO) = NE_InnerMarkerColor; DO_inc(theDO);
			DO_2s(theDO) = NE_InnerMarker; DO_inc(theDO);
			DO_2s(theDO) = NE_InnerMarkerSize; DO_inc(theDO);
			V2_COPY(CVECT(MYVERTEX(NE_Node)),DO_2Cp(theDO)); DO_inc_n(theDO,2);
		}
	
	/* plot node ID */
	if (NE_EvalNodeID)
	{
		DO_2c(theDO) = DO_TEXT; DO_inc(theDO) 
		DO_2l(theDO) = NE_IDColor; DO_inc(theDO)
		DO_2c(theDO) = TEXT_REGULAR; DO_inc(theDO) 
		DO_2c(theDO) = TEXT_NOT_CENTERED; DO_inc(theDO) 
		DO_2s(theDO) = EE2D_TEXTSIZE; DO_inc(theDO);
		V2_COPY(CVECT(MYVERTEX(NE_Node)),DO_2Cp(theDO)); DO_inc_n(theDO,2);
		#ifdef ModelP
			sprintf(DO_2cp(theDO),"%d/%x",
				(int)ID(NE_Node),
				(long)DDD_InfoGlobalId(PARHDR(NE_Node)));
			DO_inc_str(theDO);
		#else
			sprintf(DO_2cp(theDO),"%d",(int)ID(NE_Node)); DO_inc_str(theDO);
		#endif
	}

	DO_2c(theDO) = DO_NO_INST;

        #ifdef ModelP
	WOP_DObjPnt = theDO;
	#endif
	
	*end = TRUE;
	
	return (0);
}

/****************************************************************************/
/*
   DynInfo_EScalar2D - print dynamic info for 2D EScalar for infobox of ugwindow

   SYNOPSIS:
   INT DynInfo_EScalar2D (PICTURE *pic, INT tool, INT fct, const INT mp[2], char *text)

   PARAMETERS:
.  pic  - print info for this picture
.  tool - current tool
.  fct  - currenr tool function
.  mp   - mouse position in window
.  text - resulting info text

   DESCRIPTION:
   The position of the mouse is given in physical coordinates.

   RETURN VALUE:
   INT
.n   0 if text will change with mouse position
.n   1 if text is static
*/
/****************************************************************************/

static INT DynInfo_EScalar2D (PICTURE *pic, INT tool, INT fct, const INT mp[2], char *text)
{
	VIEWEDOBJ *vo;
	DOUBLE cpt[2];
	
	if (PIC_VALID(pic) == NO)
	{
		strcpy(text,"pic invalid");
		return (1);
	}
	
	vo = PIC_VO(pic);
	V2_TRAFOM3_V2(mp,InvObsTrafo,cpt);
	
	sprintf(text,"(%5.3e,%5.3e)",cpt[0],cpt[1]);
	
	return (0);
}

/****************************************************************************/
/*
   EW_PreProcess_EScalar2D - Initialize for C(olor)C(ontour) plot

   SYNOPSIS:
   static INT EW_PreProcess_EScalar2D (PICTURE *thePicture, WORK *theWork);

   PARAMETERS:
.  thePicture -
.  theWork -

   DESCRIPTION:
   This function initializes for C(olor)C(ontour) plot.	

   RETURN VALUE:
   INT
.n    0 if ok
.n    1 if error occured.
*/
/****************************************************************************/

static INT EW_PreProcess_EScalar2D (PICTURE *thePicture, WORK *theWork)
{
	struct ElemScalarPlotObj2D *theEspo;
	OUTPUTDEVICE *theOD;
	MULTIGRID *theMG;
	INT i;
	
	theEspo = &(PIC_PO(thePicture)->theEspo);
	theOD  = PIC_OUTPUTDEV(thePicture);
	theMG  = PO_MG(PIC_PO(thePicture));

	/* set value->color fct, eval fct */
	if (theEspo->max - theEspo->min < SMALL_D*(ABS(theEspo->max) + ABS(theEspo->min)))
		if (W_ID(theWork) != FINDRANGE_WORK)
		{
			UserWrite("maxValue has to be larger than minValue\n");
			return (1);
		}
	EScalar2D_EvalFct	 = theEspo->EvalFct->EvalProc;
	if ((theEspo->max - theEspo->min)==0) 
	  EScalar2D_V2C_factor = 0;
	else
	  EScalar2D_V2C_factor = (theOD->spectrumEnd - theOD->spectrumStart)/(theEspo->max - theEspo->min);
	EScalar2D_V2C_offset = theOD->spectrumStart - EScalar2D_V2C_factor*theEspo->min;	
	EScalar2D_mode		 = theEspo->mode;
	if (EScalar2D_mode == PO_CONTOURS_EQ)
	{
		EScalar2D_numOfContours = theEspo->numOfContours;
		for (i=0; i<theEspo->numOfContours; i++)
			EScalar2D_ContValues[i] = theEspo->contValues[i];
	}
	EScalar2D_depth 		= theEspo->depth;
	
	/* mark suface elements on boundary */
	if (MarkElements_MGS(theMG,0,CURRENTLEVEL(theMG))) return (1);
	
	/* prepare evaluation routine */
	if (theEspo->EvalFct->PreprocessProc!=NULL)
		if ((*theEspo->EvalFct->PreprocessProc)(PO_NAME(theEspo),theMG)) 
			return (1);;

	return (0);
}
	
/****************************************************************************/
/*
   EW_PreProcess_Line2D - Initialize for line plot

   SYNOPSIS:
   static INT EW_PreProcess_Line2D (PICTURE *thePicture, WORK *theWork);

   PARAMETERS:
.  thePicture -
.  theWork -

   DESCRIPTION:
   This function initializes for line plot.	

   RETURN VALUE:
   INT
.n    0 if ok
.n    1 if error occured.
*/
/****************************************************************************/

static INT EW_PreProcess_Line2D (PICTURE *thePicture, WORK *theWork)
{
	struct LinePlotObj2D *theLpo;
	OUTPUTDEVICE *theOD;
	MULTIGRID *theMG;
	
	theLpo = &(PIC_PO(thePicture)->theLpo);
	theOD  = PIC_OUTPUTDEV(thePicture);
	theMG  = PO_MG(PIC_PO(thePicture));

	/* set value->color fct, eval fct */
	if (theLpo->max - theLpo->min <= 0.0)
		if (W_ID(theWork) != FINDRANGE_WORK)
		{
			UserWrite("maxValue has to be larger than minValue\n");
			return (1);
		}
	LINE2D_EvalFct	  = theLpo->EvalFct->EvalProc;
	LINE2D_V2Y_factor = theLpo->aspectratio/(theLpo->max - theLpo->min);
	LINE2D_V2Y_offset = - LINE2D_V2Y_factor * theLpo->min;
	LINE2D_depth 	  = theLpo->depth;
	LINE2D_Color	  = (long)theOD->spectrumStart + theLpo->color*(theOD->spectrumEnd - theOD->spectrumStart);
	LINE2D_Begin.x	  = theLpo->left[0];  LINE2D_Begin.y	  = theLpo->left[1];
	LINE2D_End.x	  = theLpo->right[0]; LINE2D_End.y	  	  = theLpo->right[1];
	LINE2D_BeginRot.x = theLpo->left[0];  LINE2D_BeginRot.y	  = 1.0001*theLpo->left[1];
	LINE2D_EndRot.x	  = theLpo->right[0]; LINE2D_EndRot.y	  = 1.0001*theLpo->right[1];
	
	/* mark suface elements on boundary */
	if (MarkElements_MGS_On_Line(theMG,0,CURRENTLEVEL(theMG),theLpo->left,theLpo->right)) return (1);
	
	/* prepare evaluation routine */
	if (theLpo->EvalFct->PreprocessProc!=NULL)
		if ((*theLpo->EvalFct->PreprocessProc)(PO_NAME(theLpo),theMG)) 
			return (1);

	return (0);
}
	
/****************************************************************************/
/*
   PlotColorTriangle2D -  Plot on triangle color(2D coord) with depth

   SYNOPSIS:
   static INT PlotColorTriangle2D (ELEMENT *theElement, 
   DOUBLE **CornersOfElem, DOUBLE *TP0, DOUBLE *TP1, DOUBLE *TP2, 
   INT depth, DRAWINGOBJ **PtrDO);

   PARAMETERS:
.  theElement - 
.  CornersOfElem - its corners
.  TP0 - 
.  TP1 -
.  TP2 - 
.  depth - 
.  PtrDO - 

   DESCRIPTION:
   This function plots on triangle color(2D coord) with depth.

   RETURN VALUE:
   INT
.n    0 if ok
.n    1 if error occured.
*/
/****************************************************************************/

static INT PlotColorTriangle2D (ELEMENT *theElement, const DOUBLE **CornersOfElem, const DOUBLE *TP0, const DOUBLE *TP1, const DOUBLE *TP2, INT depth, DRAWINGOBJ **PtrDO)
{
	DOUBLE_VECTOR EvalPoint, LocalCoord, MP0, MP1, MP2;
	INT i;
	long Color;
	DOUBLE value;

	if (depth<=0)
	{
		/* get values */
		for (i=0; i<DIM; i++)
			EvalPoint[i] = (TP0[i]+TP1[i]+TP2[i])/3.0;
		if (UG_GlobalToLocal(3,CornersOfElem,EvalPoint,LocalCoord)) return (1);
		value = (*EScalar2D_EvalFct)(theElement,CornersOfElem,LocalCoord);
		Color = (long)(EScalar2D_V2C_factor*value+EScalar2D_V2C_offset);
		Color = MIN(Color,WOP_OutputDevice->spectrumEnd);
		Color = MAX(Color,WOP_OutputDevice->spectrumStart);

		/* draw */
		DO_2c(*PtrDO) = DO_POLYGON; DO_inc(*PtrDO) 
		DO_2c(*PtrDO) = 3; DO_inc(*PtrDO) 
		 DO_2l(*PtrDO) = Color; DO_inc(*PtrDO);
		V2_COPY(TP0,DO_2Cp(*PtrDO)); DO_inc_n(*PtrDO,2);
		V2_COPY(TP1,DO_2Cp(*PtrDO)); DO_inc_n(*PtrDO,2);
		V2_COPY(TP2,DO_2Cp(*PtrDO)); DO_inc_n(*PtrDO,2);

		/* store range */
		EScalar2D_minValue = MIN(EScalar2D_minValue,value);
		EScalar2D_maxValue = MAX(EScalar2D_maxValue,value);
	}
	else
	{
		/* find corners of subdivided triangles */
		for (i=0; i<DIM; i++)
		{
			MP0[i] = 0.5*(TP0[i]+TP1[i]);
			MP1[i] = 0.5*(TP1[i]+TP2[i]);
			MP2[i] = 0.5*(TP2[i]+TP0[i]);
		}
		if (PlotColorTriangle2D(theElement,CornersOfElem,TP0,MP0,MP2,depth-1,PtrDO)) return (1);	
		if (PlotColorTriangle2D(theElement,CornersOfElem,MP0,TP1,MP1,depth-1,PtrDO)) return (1);	
		if (PlotColorTriangle2D(theElement,CornersOfElem,TP2,MP2,MP1,depth-1,PtrDO)) return (1);	
		if (PlotColorTriangle2D(theElement,CornersOfElem,MP0,MP1,MP2,depth-1,PtrDO)) return (1);	
	}
	return (0);
}

/****************************************************************************/
/*
   PlotColorQuadrilateral2D - Plot on quadrilateral color(2D coord) with depth 

   SYNOPSIS:
   static INT PlotColorQuadrilateral2D (ELEMENT *theElement,
   DOUBLE **CornersOfElem, DOUBLE *QP0, DOUBLE *QP1, DOUBLE *QP2,
   DOUBLE *QP3, INT depth, DRAWINGOBJ **PtrDO);

   PARAMETERS:
.  theElement - 
.  CornersOfElem - 
.  QP0 - 
.  QP1 -
.  QP2 - 
.  QP3 - 
.  depth -
.  PtrDO - the drawing object to draw on

   DESCRIPTION:
   This function plots on quadrilateral color(2D coord) with depth.

   RETURN VALUE:
   INT
.n    0 if ok
.n    1 if error occured.
*/
/****************************************************************************/
	
static INT PlotColorQuadrilateral2D (ELEMENT *theElement, const DOUBLE **CornersOfElem, const DOUBLE *QP0, const DOUBLE *QP1, const DOUBLE *QP2, const DOUBLE *QP3, INT depth, DRAWINGOBJ **PtrDO)
{
	DOUBLE_VECTOR EVP, LocalCoord, MP0, MP1, MP2, MP3;
	INT i;
	long Color;
	DOUBLE value;

	for (i=0; i<DIM; i++)
		EVP[i] = (QP0[i]+QP1[i]+QP2[i]+QP3[i])*0.25;
	if (depth<=0)
	{
		/* get values */		
		if (UG_GlobalToLocal(4,CornersOfElem,EVP,LocalCoord)) return (1);
		value = (*EScalar2D_EvalFct)(theElement,CornersOfElem,LocalCoord);
		Color = (long)(EScalar2D_V2C_factor*value+EScalar2D_V2C_offset);
		Color = MIN(Color,WOP_OutputDevice->spectrumEnd);
		Color = MAX(Color,WOP_OutputDevice->spectrumStart);

		/* draw */
		DO_2c(*PtrDO) = DO_POLYGON; DO_inc(*PtrDO) 
		DO_2c(*PtrDO) = 4; DO_inc(*PtrDO) 
		DO_2l(*PtrDO) = Color; DO_inc(*PtrDO);
		V2_COPY(QP0,DO_2Cp(*PtrDO)); DO_inc_n(*PtrDO,2);
		V2_COPY(QP1,DO_2Cp(*PtrDO)); DO_inc_n(*PtrDO,2);
		V2_COPY(QP2,DO_2Cp(*PtrDO)); DO_inc_n(*PtrDO,2);
		V2_COPY(QP3,DO_2Cp(*PtrDO)); DO_inc_n(*PtrDO,2);

		/* store range */
		EScalar2D_minValue = MIN(EScalar2D_minValue,value);
		EScalar2D_maxValue = MAX(EScalar2D_maxValue,value);
	}
	else
	{
		/* find corners of subdivided quadrilaterals */
		for (i=0; i<DIM; i++)
		{
			MP0[i] = 0.5*(QP0[i]+QP1[i]);
			MP1[i] = 0.5*(QP1[i]+QP2[i]);
			MP2[i] = 0.5*(QP2[i]+QP3[i]);
			MP3[i] = 0.5*(QP3[i]+QP0[i]);
		}
		if (PlotColorQuadrilateral2D(theElement,CornersOfElem,QP0,MP0,EVP,MP3,depth-1,PtrDO)) return (1);	
		if (PlotColorQuadrilateral2D(theElement,CornersOfElem,MP0,QP1,MP1,EVP,depth-1,PtrDO)) return (1);	
		if (PlotColorQuadrilateral2D(theElement,CornersOfElem,EVP,MP1,QP2,MP2,depth-1,PtrDO)) return (1);	
		if (PlotColorQuadrilateral2D(theElement,CornersOfElem,EVP,MP2,QP3,MP3,depth-1,PtrDO)) return (1);	
	}
	return (0);
}

/****************************************************************************/
/*
   PointOnLine2D - Cals point between two points with contourValue

   SYNOPSIS:
   static INT PointOnLine2D (DOUBLE contourValue, DOUBLE value0, DOUBLE value1, const DOUBLE *vec0, const DOUBLE *vec1, DOUBLE *p);
   
   PARAMETERS:
.  contourValue - 
.  value0 -
.  value1 - 
.  vec0 -
.  vec1 - 
.  VECTOR p - 

   DESCRIPTION:
   This function cals point between two points with contourValue.

   RETURN VALUE:
   INT
.n    0 if ok
.n    1 if error occured.
*/
/****************************************************************************/

static INT PointOnLine2D (DOUBLE contourValue, DOUBLE value0, DOUBLE value1, const DOUBLE *vec0, const DOUBLE *vec1, DOUBLE *p)
{
	DOUBLE alpha;

	if (value0==value1)
	{
		if (value0==contourValue)
		{
			V2_LINCOMB(0.5,vec0,0.5,vec1,p);
			return (1);
		}
		return (0);
	}
	else
	{
		alpha = (contourValue-value0)/(value1-value0);
		if (alpha<0.0 || alpha>1.0) return (0);
		V2_LINCOMB(1.0-alpha,vec0,alpha,vec1,p);
		return (1);
	}
}

/****************************************************************************/
/*
   PlotContourTriangle2D - plot on triangle contourlines (2D coord) with depth

   SYNOPSIS:
   static INT PlotContourTriangle2D (ELEMENT *theElement, 
   DOUBLE **CornersOfElem, DOUBLE *TP0, DOUBLE *TP1, DOUBLE *TP2, 
   INT depth, DRAWINGOBJ **PtrDO);

   PARAMETER:
.  theElement -
.  CornersOfElem - 
.  TP0 - 
.  TP1 -
.  TP2 -
.  depth -
.  PtrDO - the drawing object to draw on

   DESCRIPTION:
   This function plots on triangle contourlines (2D coord) with depth.

   RETURN VALUE:
   INT
.n    0 if ok
.n    1 if error occured.
*/
/****************************************************************************/

static INT PlotContourTriangle2D (ELEMENT *theElement, const DOUBLE **CornersOfElem, const DOUBLE *TP0, const DOUBLE *TP1, const DOUBLE *TP2, INT depth, DRAWINGOBJ **PtrDO)
{
	DOUBLE_VECTOR LocalCoord, MP0, MP1, MP2, PointMid, Point[3];
	INT i, j, n, min, max;
	long Color;
	DOUBLE v0, v1, v2, vmin, vmax;

	if (depth<=0)
	{
		/* get values at the corners */ 	
		if (UG_GlobalToLocal(3,CornersOfElem,(DOUBLE *)TP0,LocalCoord)) 
		  return (1);
		v0	= (*EScalar2D_EvalFct)(theElement,CornersOfElem,LocalCoord);
		if (UG_GlobalToLocal(3,CornersOfElem,(DOUBLE *)TP1,LocalCoord))
		  return (1);
		v1	= (*EScalar2D_EvalFct)(theElement,CornersOfElem,LocalCoord);
		if (UG_GlobalToLocal(3,CornersOfElem,(DOUBLE *)TP2,LocalCoord)) 
		  return (1);
		v2	= (*EScalar2D_EvalFct)(theElement,CornersOfElem,LocalCoord);
		vmin = MIN(v0,v1); vmin = MIN(vmin,v2);
		vmax = MAX(v0,v1); vmax = MAX(vmax,v2);

		/* store range */
		EScalar2D_minValue = MIN(EScalar2D_minValue,vmin);
		EScalar2D_maxValue = MAX(EScalar2D_maxValue,vmax);

		/* find contours to be plotted */
		for (min=0; min<EScalar2D_numOfContours; min++)
			if (EScalar2D_ContValues[min]>=vmin)
				break;
		for (max=EScalar2D_numOfContours-1; max>=0; max--)
			if (EScalar2D_ContValues[max]<=vmax)
				break;

		/* draw contours */
		for (i=min; i<=max; i++)
		{
			/* set color */
			Color = (long)(EScalar2D_V2C_factor*EScalar2D_ContValues[i]+EScalar2D_V2C_offset);
			Color = MIN(Color,WOP_OutputDevice->spectrumEnd);
			Color = MAX(Color,WOP_OutputDevice->spectrumStart);

			/* calculate points on each side of triangle having the right value */
			n=0;
			if (PointOnLine2D(EScalar2D_ContValues[i],v0,v1,TP0,TP1,Point[n]))
			  n++; 
			if (PointOnLine2D(EScalar2D_ContValues[i],v1,v2,TP1,TP2,Point[n]))
			  n++; 
			if (PointOnLine2D(EScalar2D_ContValues[i],v2,v0,TP2,TP0,Point[n]))
			  n++; 
			
			/* draw */
			switch (n)
			{
				case 2:
					DO_2c(*PtrDO) = DO_LINE; DO_inc(*PtrDO) 
					DO_2l(*PtrDO) = Color; DO_inc(*PtrDO);
					V2_COPY(Point[0],DO_2Cp(*PtrDO)); DO_inc_n(*PtrDO,2);
					V2_COPY(Point[1],DO_2Cp(*PtrDO)); DO_inc_n(*PtrDO,2);
					break;
				case 3:
					DO_2c(*PtrDO) = DO_POLYLINE; DO_inc(*PtrDO) 
					DO_2c(*PtrDO) = 5; DO_inc(*PtrDO) 
					DO_2l(*PtrDO) = Color; DO_inc(*PtrDO);
					for (j=0; j<DIM; j++)
						PointMid[j] = (Point[0][j]+Point[1][j]+Point[2][j])/3.0;
					V2_COPY(Point[0],DO_2Cp(*PtrDO)); DO_inc_n(*PtrDO,2);
					V2_COPY(PointMid,DO_2Cp(*PtrDO)); DO_inc_n(*PtrDO,2);
					V2_COPY(Point[1],DO_2Cp(*PtrDO)); DO_inc_n(*PtrDO,2);
					V2_COPY(PointMid,DO_2Cp(*PtrDO)); DO_inc_n(*PtrDO,2);
					V2_COPY(Point[2],DO_2Cp(*PtrDO)); DO_inc_n(*PtrDO,2);
					break;
			}
		}
	}
	else
	{
		/* find corners of subdivided triangles */
		for (i=0; i<DIM; i++)
		{
			MP0[i] = 0.5*(TP0[i]+TP1[i]);
			MP1[i] = 0.5*(TP1[i]+TP2[i]);
			MP2[i] = 0.5*(TP2[i]+TP0[i]);
		}
		if (PlotContourTriangle2D(theElement,CornersOfElem,TP0,MP0,MP2,depth-1,PtrDO)) return (1);	
		if (PlotContourTriangle2D(theElement,CornersOfElem,MP0,TP1,MP1,depth-1,PtrDO)) return (1);	
		if (PlotContourTriangle2D(theElement,CornersOfElem,TP2,MP2,MP1,depth-1,PtrDO)) return (1);	
		if (PlotContourTriangle2D(theElement,CornersOfElem,MP0,MP1,MP2,depth-1,PtrDO)) return (1);	
	}
	return (0);
}

/****************************************************************************/
/*
   PlotContourQuadrilateral2D - Plot on quadrilateral contourlines (2D coord) with depth 

   SYNOPSIS:
   static INT PlotContourQuadrilateral2D (ELEMENT *theElement,
   DOUBLE **CornersOfElem, DOUBLE *QP0, DOUBLE *QP1, DOUBLE *QP2,
   DOUBLE *QP3, INT depth, DRAWINGOBJ **PtrDO);

   PARAMETERS:
.  theElement - 
.  CornersOfElem - 
.  QP0 - 
.  QP1 -
.  QP2 -
.  QP3 - 
.  depth -
.  PtrDO -  the drawing object to draw on
 
   DESCRIPTION:
   This function plots on quadrilateral contourlines (2D coord) with depth.

   RETURN VALUE:
   INT
.n    0 if ok
.n    1 if error occured.
*/
/****************************************************************************/
	
static INT PlotContourQuadrilateral2D (ELEMENT *theElement, const DOUBLE **CornersOfElem, const DOUBLE *QP0, const DOUBLE *QP1, const DOUBLE *QP2, const DOUBLE *QP3, INT depth, DRAWINGOBJ **PtrDO)
{
	DOUBLE_VECTOR EVP, LocalCoord, MP0, MP1, MP2, MP3, PointMid, Point[4];
	INT i, j, n, min, max;
	long Color;
	DOUBLE v0, v1, v2, v3, vmin, vmax;

	for (i=0; i<DIM; i++)
		EVP[i] = (QP0[i]+QP1[i]+QP2[i]+QP3[i])*0.25;
	if (depth<=0)
	{
		/* get values at the corners */ 	
		if (UG_GlobalToLocal(4,CornersOfElem,(DOUBLE *)QP0,LocalCoord)) 
		  return (1);
		v0	= (*EScalar2D_EvalFct)(theElement,CornersOfElem,LocalCoord);
		if (UG_GlobalToLocal(4,CornersOfElem,(DOUBLE *)QP1,LocalCoord)) 
		  return (1);
		v1	= (*EScalar2D_EvalFct)(theElement,CornersOfElem,LocalCoord);
		if (UG_GlobalToLocal(4,CornersOfElem,(DOUBLE *)QP2,LocalCoord)) 
		  return (1);
		v2	= (*EScalar2D_EvalFct)(theElement,CornersOfElem,LocalCoord);
		if (UG_GlobalToLocal(4,CornersOfElem,(DOUBLE *)QP3,LocalCoord)) 
		  return (1);
		v3	= (*EScalar2D_EvalFct)(theElement,CornersOfElem,LocalCoord);
		vmin = MIN(v0,v1); vmin = MIN(vmin,v2); vmin = MIN(vmin,v3);
		vmax = MAX(v0,v1); vmax = MAX(vmax,v2); vmax = MAX(vmax,v3);

		/* store range */
		EScalar2D_minValue = MIN(EScalar2D_minValue,vmin);
		EScalar2D_maxValue = MAX(EScalar2D_maxValue,vmax);

		/* find contours to be plotted */
		for (min=0; min<EScalar2D_numOfContours; min++)
			if (EScalar2D_ContValues[min]>=vmin)
				break;
		for (max=EScalar2D_numOfContours-1; max>=0; max--)
			if (EScalar2D_ContValues[max]<=vmax)
				break;

		/* draw contours */
		for (i=min; i<=max; i++)
		{
			/* set color */
			Color = (long)(EScalar2D_V2C_factor*EScalar2D_ContValues[i]+EScalar2D_V2C_offset);
			Color = MIN(Color,WOP_OutputDevice->spectrumEnd);
			Color = MAX(Color,WOP_OutputDevice->spectrumStart);

			/* calculate points on each side of triangle having the right value */
			n=0;
			if (PointOnLine2D(EScalar2D_ContValues[i],v0,v1,QP0,QP1,
							  Point[n])) n++; 
			if (PointOnLine2D(EScalar2D_ContValues[i],v1,v2,QP1,QP2,
							  Point[n])) n++; 
			if (PointOnLine2D(EScalar2D_ContValues[i],v2,v3,QP2,QP3,
							  Point[n])) n++; 
			if (PointOnLine2D(EScalar2D_ContValues[i],v3,v0,QP3,QP0,
							  Point[n])) n++; 
			
			/* draw */
			switch (n)
			{
				case 1:
					DO_2c(*PtrDO) = DO_LINE; DO_inc(*PtrDO) 
					DO_2l(*PtrDO) = WOP_OutputDevice->black; DO_inc(*PtrDO);
					V2_COPY(Point[0],DO_2Cp(*PtrDO)); DO_inc_n(*PtrDO,2);
					V2_COPY(Point[0],DO_2Cp(*PtrDO)); DO_inc_n(*PtrDO,2);
					break;
				case 2:
					DO_2c(*PtrDO) = DO_LINE; DO_inc(*PtrDO) 
					DO_2l(*PtrDO) = Color; DO_inc(*PtrDO);
					V2_COPY(Point[0],DO_2Cp(*PtrDO)); DO_inc_n(*PtrDO,2);
					V2_COPY(Point[1],DO_2Cp(*PtrDO)); DO_inc_n(*PtrDO,2);
					break;
				case 3:
					DO_2c(*PtrDO) = DO_POLYLINE; DO_inc(*PtrDO) 
					DO_2c(*PtrDO) = 5; DO_inc(*PtrDO) 
					DO_2l(*PtrDO) = Color; DO_inc(*PtrDO);
					for (j=0; j<DIM; j++)
						PointMid[j] = (Point[0][j]+Point[1][j]+Point[2][j])/3.0;
					V2_COPY(Point[0],DO_2Cp(*PtrDO)); DO_inc_n(*PtrDO,2);
					V2_COPY(PointMid,DO_2Cp(*PtrDO)); DO_inc_n(*PtrDO,2);
					V2_COPY(Point[1],DO_2Cp(*PtrDO)); DO_inc_n(*PtrDO,2);
					V2_COPY(PointMid,DO_2Cp(*PtrDO)); DO_inc_n(*PtrDO,2);
					V2_COPY(Point[2],DO_2Cp(*PtrDO)); DO_inc_n(*PtrDO,2);
					break;
				case 4:
					DO_2c(*PtrDO) = DO_LINE; DO_inc(*PtrDO) 
					DO_2l(*PtrDO) = Color; DO_inc(*PtrDO);
					V2_COPY(Point[0],DO_2Cp(*PtrDO)); DO_inc_n(*PtrDO,2);
					V2_COPY(Point[2],DO_2Cp(*PtrDO)); DO_inc_n(*PtrDO,2);
					
					DO_2c(*PtrDO) = DO_LINE; DO_inc(*PtrDO) 
					DO_2l(*PtrDO) = Color; DO_inc(*PtrDO);
					V2_COPY(Point[1],DO_2Cp(*PtrDO)); DO_inc_n(*PtrDO,2);
					V2_COPY(Point[3],DO_2Cp(*PtrDO)); DO_inc_n(*PtrDO,2);
			}
		}
	}
	else
	{
		/* find corners of subdivided quadrilaterals */
		for (i=0; i<DIM; i++)
		{
			MP0[i] = 0.5*(QP0[i]+QP1[i]);
			MP1[i] = 0.5*(QP1[i]+QP2[i]);
			MP2[i] = 0.5*(QP2[i]+QP3[i]);
			MP3[i] = 0.5*(QP3[i]+QP0[i]);
		}
		if (PlotContourQuadrilateral2D(theElement,CornersOfElem,QP0,MP0,EVP,MP3,depth-1,PtrDO)) return (1); 
		if (PlotContourQuadrilateral2D(theElement,CornersOfElem,MP0,QP1,MP1,EVP,depth-1,PtrDO)) return (1); 
		if (PlotContourQuadrilateral2D(theElement,CornersOfElem,EVP,MP1,QP2,MP2,depth-1,PtrDO)) return (1); 
		if (PlotContourQuadrilateral2D(theElement,CornersOfElem,EVP,MP2,QP3,MP3,depth-1,PtrDO)) return (1); 
	}
	return (0);
}

/****************************************************************************/
/*
   EW_EScalar2D	- C(olor)C(ontour) plot 

   SYNOPSIS:
   static INT EW_EScalar2D (ELEMENT *theElement, DRAWINGOBJ *theDO);

   PARAMETERS:
.  theElement - 
.  theDO - 

   DESCRIPTION:
   This function plots C(olor)C(ontour).

   RETURN VALUE:
   INT
.n    0 if ok
.n    1 if error occured.
*/
/****************************************************************************/

static INT EW_EScalar2D (ELEMENT *theElement, DRAWINGOBJ *theDO)
{
	INT i, n, found;
	COORD_POINT corners[MAX_CORNERS_OF_ELEM];
	const DOUBLE *x[MAX_CORNERS_OF_ELEM];
	DRAWINGOBJ *p, *range;
	
	n = CORNERS_OF_ELEM(theElement);
	
	/* get coordinates of corners of the element */
	found = FALSE;
	n = CORNERS_OF_ELEM(theElement);
	for (i=0; i<n; i++)
	{
		x[i] = CVECT(MYVERTEX(CORNER(theElement,i)));
		if (!found)
		{
			corners[i].x = x[i][_X_]; corners[i].y = x[i][_Y_];
			if (PointInPolygon(PhysRect,4,corners[i])) found=TRUE;
		}
	}
	
	if (!found)
	{
		/* no corner of the element lies inside the phys rectangle */
		/* vice versa? */
		for (i=0; i<4; i++)
			if (PointInPolygon(corners,n,PhysRect[i]))
				break;
		if (i>=4)
		{
			/* element and phys rect don't intersect: nothing to do */
			DO_2c(theDO) = DO_NO_INST;

                        #ifdef ModelP
	                WOP_DObjPnt = theDO;
	                #endif
			
			return (0);
		}
	}
		
	/* draw polygon with depth */
	p = theDO;
	EScalar2D_minValue = MAX_D; EScalar2D_maxValue = -MAX_D; 
    DO_2c(theDO) = DO_RANGE; DO_inc(theDO); range = theDO; DO_inc_n(theDO,2);
	switch (EScalar2D_mode)
	{
		case PO_COLOR:
			if (n==TRIANGLE)
			{
				if (PlotColorTriangle2D(theElement,x,x[0],x[1],x[2],EScalar2D_depth,&theDO)) return (1);
			}
			else
			{
				if (PlotColorQuadrilateral2D(theElement,x,x[0],x[1],x[2],x[3],EScalar2D_depth,&theDO)) return (1);
			}
			break;
		case PO_CONTOURS_EQ:
			if (n==TRIANGLE)
			{
				if (PlotContourTriangle2D(theElement,x,x[0],x[1],x[2],EScalar2D_depth,&theDO)) return (1);
			}
			else
			{
				if (PlotContourQuadrilateral2D(theElement,x,x[0],x[1],x[2],x[3],EScalar2D_depth,&theDO)) return (1);
			}
			break;
		default:
			return (1);
	}
	
	DO_2c(theDO) = DO_NO_INST;

        #ifdef ModelP
	WOP_DObjPnt = theDO;
	#endif

	DO_2C(range) = EScalar2D_minValue; DO_inc(range);
	DO_2C(range) = EScalar2D_maxValue;
	
#ifdef __DO_HEAP_USED__
	n = (INT)theDO - (INT)p;
	Heap_Used_Min = MIN(Heap_Used_Min,n);
	Heap_Used_Max = MAX(Heap_Used_Max,n);
#endif
	
	return (0);
}

/****************************************************************************/
/*
   EW_LineElement2D	- line plot 

   SYNOPSIS:
   static INT EW_LineElement2D (ELEMENT *theElement, DRAWINGOBJ *theDO);

   PARAMETERS:
.  theElement - 
.  theDO - 

   DESCRIPTION:
   This function plots line plot.

   RETURN VALUE:
   INT
.n    0 if ok
.n    1 if error occured.
*/
/****************************************************************************/

static INT EW_LineElement2D (ELEMENT *theElement, DRAWINGOBJ *theDO)
{
	INT i, n, m, found;
	COORD_POINT P1, P2;
	const DOUBLE *x[MAX_CORNERS_OF_ELEM];
	DRAWINGOBJ *range;
	DOUBLE alpha[MAX_CORNERS_OF_ELEM], beta;
	DOUBLE_VECTOR LocalCoord, P[MAX_CORNERS_OF_ELEM], PEval, A, B;
	DOUBLE v;
	#ifdef __DO_HEAP_USED__
	DRAWINGOBJ *p;
	#endif
	
	n = CORNERS_OF_ELEM(theElement);
	
	/* get coordinates of corners of the element */
	n = CORNERS_OF_ELEM(theElement);
	for (i=0; i<n; i++)
		x[i] = CVECT(MYVERTEX(CORNER(theElement,i)));
	
	/* draw polygon with depth */
	LINE2D_minValue = MAX_D; LINE2D_maxValue = -MAX_D; 
	found = 0;
	P1.x=x[n-1][0]; P1.y=x[n-1][1];
	for (i=0; i<n; i++)
	{
		P2.x=x[i][0]; P2.y=x[i][1];
		if (CalcCrossingPoint(LINE2D_Begin,LINE2D_End,P1,P2,alpha+found,&beta))
		{
			P[found][0] = (1.0-beta)*P1.x + beta*P2.x;
			P[found][1] = (1.0-beta)*P1.y + beta*P2.y;
			found++;
		}
		P1.x=P2.x; P1.y=P2.y;
	}
	if (found!=2)
	{
		found = 0;
		P1.x=x[n-1][0]; P1.y=x[n-1][1];
		for (i=0; i<n; i++)
		{
			P2.x=x[i][0]; P2.y=x[i][1];
			if (CalcCrossingPoint(LINE2D_BeginRot,LINE2D_EndRot,P1,P2,alpha+found,&beta))
			{
				P[found][0] = (1.0-beta)*P1.x + beta*P2.x;
				P[found][1] = (1.0-beta)*P1.y + beta*P2.y;
				found++;
			}
			P1.x=P2.x; P1.y=P2.y;
		}
	}
	if (found==2)
	{
    	DO_2c(theDO) = DO_RANGE; DO_inc(theDO); range = theDO; DO_inc_n(theDO,2);
		
		if (UG_GlobalToLocal(n,x,P[0],LocalCoord)) return (1);
		v = (*LINE2D_EvalFct)(theElement,x,LocalCoord);
		LINE2D_minValue = MIN(LINE2D_minValue,v);	LINE2D_maxValue = MAX(LINE2D_maxValue,v);
		A[0] = alpha[0];
		A[1] = LINE2D_V2Y_factor*v + LINE2D_V2Y_offset;
		m = POW(2,LINE2D_depth);
		for (i=1; i<=m; i++)
		{
			beta = (DOUBLE)i/(DOUBLE)m;
			V2_LINCOMB(1.0-beta,P[0],beta,P[1],PEval)
			if (UG_GlobalToLocal(n,x,PEval,LocalCoord)) return (1);
			v = (*LINE2D_EvalFct)(theElement,x,LocalCoord);
			LINE2D_minValue = MIN(LINE2D_minValue,v);	LINE2D_maxValue = MAX(LINE2D_maxValue,v);

			B[0] = (1.0-beta)*alpha[0] + beta*alpha[1] ;
			B[1] = LINE2D_V2Y_factor*v + LINE2D_V2Y_offset;
			DO_2c(theDO) = DO_LINE; DO_inc(theDO) 
			DO_2l(theDO) = LINE2D_Color; DO_inc(theDO);
			V2_COPY(A,DO_2Cp(theDO)); DO_inc_n(theDO,2);
			V2_COPY(B,DO_2Cp(theDO)); DO_inc_n(theDO,2);
			
			V2_COPY(B,A)
		}
		
		DO_2C(range) = LINE2D_minValue; DO_inc(range);
		DO_2C(range) = LINE2D_maxValue;
	
	}
	DO_2c(theDO) = DO_NO_INST;

        #ifdef ModelP
	WOP_DObjPnt = theDO;
	#endif
	
#ifdef __DO_HEAP_USED__
		n = (INT)theDO - (INT)p;
		Heap_Used_Min = MIN(Heap_Used_Min,n);
		Heap_Used_Max = MAX(Heap_Used_Max,n);
#endif
	return (0);
}

/****************************************************************************/
/*
   EW_PreProcess_EScalar2D_FR - Initialize for findrange of scalar plot	

   SYNOPSIS:
   static INT EW_PreProcess_EScalar2D_FR (PICTURE *thePicture, WORK *theWork);

   PARAMETERS:
.  thePicture - 
.  theWork -

   DESCRIPTION:
   This function initializes for findrange of scalar plot.

   RETURN VALUE:
   INT
.n    0 if ok
.n    1 if error occured.
*/							
/****************************************************************************/

static INT EW_PreProcess_EScalar2D_FR (PICTURE *thePicture, WORK *theWork)
{
	if (EW_PreProcess_EScalar2D (thePicture,theWork)) 
		return (1);

	/* reset min and max values */
	GEN_FR_put = W_FINDRANGE_WORK(theWork)->put;
	GEN_FR_min = MAX_D;
	GEN_FR_max = -MAX_D;	
	
	return (0);
}
	
/****************************************************************************/
/*
   EW_PreProcess_Line2D_FR - Initialize for findrange of line plot	

   SYNOPSIS:
   static INT EW_PreProcess_Line2D_FR (PICTURE *thePicture, WORK *theWork);

   PARAMETERS:
.  thePicture - 
.  theWork -

   DESCRIPTION:
   This function initializes for findrange of scalar plot.

   RETURN VALUE:
   INT
.n    0 if ok
.n    1 if error occured.
*/							
/****************************************************************************/

static INT EW_PreProcess_Line2D_FR (PICTURE *thePicture, WORK *theWork)
{
	if (EW_PreProcess_Line2D (thePicture,theWork)) 
		return (1);

	/* reset min and max values */
	GEN_FR_put = W_FINDRANGE_WORK(theWork)->put;
	GEN_FR_min = MAX_D;
	GEN_FR_max = -MAX_D;	
	
	return (0);
}
	
/****************************************************************************/
/*
   EW_PreProcess_EVector2D - Initialize for vector plot 2D

   SYNOPSIS:
   static INT EW_PreProcess_EVector2D (PICTURE *thePicture, WORK *theWork);

   PARAMETERS:
.  thePicture -
.  theWork -
   
   DESCRIPTION:
   This function initializes for vector plot 2D.

   RETURN VALUE:
   INT
.n    0 if ok
.n    1 if error occured.
*/
/****************************************************************************/

static INT EW_PreProcess_EVector2D (PICTURE *thePicture, WORK *theWork)
{
	struct ElemVectorPlotObj2D *theEvpo;
	OUTPUTDEVICE *theOD;
	MULTIGRID *theMG;
	
	theEvpo = &(PIC_PO(thePicture)->theEvpo);
	theOD  = PIC_OUTPUTDEV(thePicture);
	theMG  = PO_MG(PIC_PO(thePicture));

	/* set value->length fct, eval fct */
	if (theEvpo->max < SMALL_D)
		if (W_ID(theWork) != FINDRANGE_WORK)
		{
			UserWrite("maxValue has to be larger than zero\n");
			return (1);
		}
	EVector_rastersize	  = theEvpo->RasterSize;
	EVector_cutvector	  = theEvpo->CutVectors;
	EVector_EvalFct 	  = theEvpo->EvalFct->EvalProc;
	EVector_V2L_factor	  = EVector_rastersize/theEvpo->max;				/* scale length of vectors			*/
	EVector_CutLenFactor  = theEvpo->CutLenFactor;
	EVector_ColorCut	  = theOD->red;
	EVector2D_ColorNormal = theOD->black;
	
	/* mark suface elements on boundary */
	if (MarkElements_MGS(theMG,0,CURRENTLEVEL(theMG))) return (1);
	
	/* prepare evaluation routine */
	if (theEvpo->EvalFct->PreprocessProc!=NULL)
		if ((*theEvpo->EvalFct->PreprocessProc)(PO_NAME(theEvpo),theMG))
			return (1);

	return (0);
}
	
/****************************************************************************/
/*
   FindRasterPoints2D - Find rasterpoints in 2D	

   SYNOPSIS:
   static INT FindRasterPoints2D (DOUBLE RasterSize, DOUBLE **Polygon, 
   INT Number, DOUBLE_VECTOR *RasterPoints, INT *RPNumber);

   PARAMETERS:
.  RssterSize -
.  Polygon -
.  Number -
.  RasterPoints -
.  RPNumber -
  
   DESCRIPTION:
   This function finds rasterpoints in 2D.

   RETURN VALUE:
   INT
.n    0 if ok
.n    1 if error occured.
*/
/****************************************************************************/

static INT FindRasterPoints2D (DOUBLE RasterSize, const DOUBLE **Polygon, INT Number, DOUBLE_VECTOR *RasterPoints, INT *RPNumber)
{
	INT i, j, k, i0, i1, j0, j1, c0, c1;
	DOUBLE xmin, xmax, ymin, ymax;
	DOUBLE diff[MAX_POINTS_OF_POLY][2], test[2];
	
	*RPNumber = 0;
	if (Number<2) return (0);
	
	xmin = ymin = MAX_C;
	xmax = ymax = -MAX_C;
	for (i=0; i<Number; i++)
	{
		xmin = MIN(xmin,Polygon[i][0]);
		xmax = MAX(xmax,Polygon[i][0]);
		ymin = MIN(ymin,Polygon[i][1]);
		ymax = MAX(ymax,Polygon[i][1]);
		diff[i][0] = Polygon[(i+1)%Number][0] - Polygon[i][0];
		diff[i][1] = Polygon[(i+1)%Number][1] - Polygon[i][1];
	}
	i0 = (INT)ceil(xmin/RasterSize);
	i1 = (INT)floor(xmax/RasterSize);
	j0 = (INT)ceil(ymin/RasterSize);
	j1 = (INT)floor(ymax/RasterSize);
	
	for (i=i0; i<=i1; i++)
		for (j=j0; j<=j1; j++)
		{
			c0 = c1 = 0;
			for (k=0; k<Number; k++)
			{
				test[0] = RasterSize*(DOUBLE)(i) - Polygon[k][0];
				test[1] = RasterSize*(DOUBLE)(j) - Polygon[k][1];
				if (diff[k][0]*test[1]>=diff[k][1]*test[0]) c0++;
				if (diff[k][0]*test[1]<=diff[k][1]*test[0]) c1++;
			}
			if (c0==Number || c1==Number)
			{
				RasterPoints[*RPNumber][0] = RasterSize*(DOUBLE)(i);
				RasterPoints[*RPNumber][1] = RasterSize*(DOUBLE)(j);
				(*RPNumber)++;
			}
			if (*RPNumber>=RASTERPOINTS_MAX)
				return (0);
		}
	
	return (0);
}

static INT EW_EVectorPreProcess_PlotGridBefore2D (PICTURE *thePicture, WORK *theWork)
{
	struct ElemVectorPlotObj2D *theEvpo;
	OUTPUTDEVICE *theOD;
	MULTIGRID *theMG;
	
	theEvpo = &(PIC_PO(thePicture)->theEvpo);
	theOD  = PIC_OUTPUTDEV(thePicture);
	theMG  = PO_MG(PIC_PO(thePicture));
	
	if (!theEvpo->PlotGrid)
		return (1);

	EB_ColorGrid = theOD->black;
	
	/* mark surface elements */
	if (MarkElements2D(theMG,0,CURRENTLEVEL(theMG))) return (1);

	return (0);
}

/****************************************************************************/
/*
   EW_EVector2D	- Evaluate elements for vector drawing	

   SYNOPSIS:
   static INT EW_EVector2D (ELEMENT *theElement, DRAWINGOBJ *theDO);

   PARAMETERS:
.  theElement -
.  theDO -

   DESCRIPTION:
   This function evaluates elements for vector drawing. 

   RETURN VALUE:
   INT
.n    0 if ok
.n    1 if error occured.
*/
/****************************************************************************/

static INT EW_EVector2D (ELEMENT *theElement, DRAWINGOBJ *theDO)
{
	INT i, nr, n, found;
	DOUBLE_VECTOR LocalCoord, Poly[MAX_POINTS_OF_POLY], RasterPoint[RASTERPOINTS_MAX];
	COORD_POINT corners[MAX_CORNERS_OF_ELEM];
	const DOUBLE *x[MAX_CORNERS_OF_ELEM];
	DOUBLE norm;
	long Color;
	DOUBLE min, max;
	DOUBLE_VECTOR Arrow;
	
	/* get coordinates of corners of the element and their z coordinates in cut system */
	found = FALSE;
	n = CORNERS_OF_ELEM(theElement);
	for (i=0; i<n; i++)
	{
		x[i] = CVECT(MYVERTEX(CORNER(theElement,i)));
		if (!found)
		{
			corners[i].x = x[i][_X_]; corners[i].y = x[i][_Y_];
			if (PointInPolygon(PhysRect,4,corners[i])) found=TRUE;
		}
		V2_COPY(x[i],Poly[i]);
	}
	
	if (!found)
	{
		/* no corner of the element lies inside the phys rectangle */
		/* vice versa? */
		for (i=0; i<4; i++)
			if (PointInPolygon(corners,n,PhysRect[i]))
				break;
		if (i>=4)
		{
			/* element and phys rect don't intersect: nothing to do */
			DO_2c(theDO) = DO_NO_INST;

                        #ifdef ModelP
	                WOP_DObjPnt = theDO;
	                #endif
			
			return (0);
		}
	}
	
	/* get arrows with rastersize */
	if (FindRasterPoints2D(EVector_rastersize,x,i,RasterPoint,&nr)) return (1);
			
	/* handle arrows */
	min = MAX_D; max = -MAX_D;
	for (i=0; i<nr; i++)
	{
		if (UG_GlobalToLocal(CORNERS_OF_ELEM(theElement),x,RasterPoint[i],LocalCoord)) return (1);
		(*EVector_EvalFct)(theElement,x,LocalCoord,Arrow);
		V2_SCALE(EVector_V2L_factor,Arrow)
				
		/* find color and size of arrow, define its endpoint on the cutplane */
		V2_EUKLIDNORM(Arrow,norm)
		max = MAX(max,norm/EVector_V2L_factor); min = MIN(min,norm/EVector_V2L_factor);
			
		if ((norm>EVector_rastersize*EVector_CutLenFactor) && EVector_cutvector)
		{
			Color = EVector_ColorCut;
			V2_SCALE(EVector_rastersize*EVector_CutLenFactor/norm,Arrow)
		}
		else
			Color = EVector2D_ColorNormal;
		V2_ADD(RasterPoint[i],Arrow,Arrow);
		
		/* draw arrow */
		DO_2c(theDO) = DO_ARROW; DO_inc(theDO) 
									 DO_2l(theDO) = Color; DO_inc(theDO);
		V2_COPY(RasterPoint[i],DO_2Cp(theDO)); DO_inc_n(theDO,2);
		V2_COPY(Arrow,DO_2Cp(theDO)); DO_inc_n(theDO,2); 	
	}

	if (nr>0)
	{
		/* store range */
		DO_2c(theDO) = DO_RANGE; DO_inc(theDO) 
									 DO_2C(theDO) = min; DO_inc(theDO);
									 DO_2C(theDO) = max; DO_inc(theDO);
	}
			
	DO_2c(theDO) = DO_NO_INST;

        #ifdef ModelP
	WOP_DObjPnt = theDO;
	#endif
	
	return (0);
}

/****************************************************************************/
/*
   DynInfo_EVector2D - print dynamic info for 2D EVector for infobox of ugwindow

   SYNOPSIS:
   INT DynInfo_EVector2D (PICTURE *pic, INT tool, INT fct, const INT mp[2], char *text)

   PARAMETERS:
.  pic  - print info for this picture
.  tool - current tool
.  fct  - currenr tool function
.  mp   - mouse position in window
.  text - resulting info text

   DESCRIPTION:
   The position of the mouse is given in physical coordinates.

   RETURN VALUE:
   INT
.n   0 if text will change with mouse position
.n   1 if text is static
*/
/****************************************************************************/

static INT DynInfo_EVector2D (PICTURE *pic, INT tool, INT fct, const INT mp[2], char *text)
{
	VIEWEDOBJ *vo;
	DOUBLE cpt[2];
	
	if (PIC_VALID(pic) == NO)
	{
		strcpy(text,"pic invalid");
		return (1);
	}
	
	vo = PIC_VO(pic);
	V2_TRAFOM3_V2(mp,InvObsTrafo,cpt);
	
	sprintf(text,"(%5.3e,%5.3e)",cpt[0],cpt[1]);
	
	return (0);
}

/****************************************************************************/
/*
   EW_PreProcess_EVector2D_FR - Initialize for findrange of vector plot in 2D 

   SYNOPSIS:
   static INT EW_PreProcess_EVector2D_FR (PICTURE *thePicture, WORK *theWork);

   PARAMETERS:
.  thePicture -
.  theWork -

   DESCRIPTION:
   This function initializes for findrange of vector plot in 2D.

   RETURN VALUE:
   INT
.n    0 if ok
.n    1 if error occured.
*/ 
/****************************************************************************/

static INT EW_PreProcess_EVector2D_FR (PICTURE *thePicture, WORK *theWork)
{
	if (EW_PreProcess_EVector2D (thePicture,theWork))
		return (1);

	/* reset min and max values */
	GEN_FR_put = W_FINDRANGE_WORK(theWork)->put;
	GEN_FR_min = MAX_D;
	GEN_FR_max = -MAX_D;	
	
	return (0);
}
	
#endif /* __TWODIM__ */

/*********************** Part only for 3D Version **********************/

#ifdef __THREEDIM__

static INT SideCornerIndexPYR (ELEMENT *theElement, INT side, INT NodeOrder, INT i[4], INT icon[4][2])
{
	INT k, corner[4], b,j,n, cornercon[4][2], ncorners;

	ncorners = CORNERS_OF_SIDE(theElement,side);

	/* get corner number of this side */
	for (k=0; k<ncorners; ++k)  
	   corner[k] = CORNER_OF_SIDE(theElement,side,k);
	   
	/* init the corner connection field */
	switch (ncorners) {
		case (3):
			cornercon[0][0] = corner[1]; cornercon[0][1] = corner[2];
			cornercon[1][0] = corner[0]; cornercon[1][1] = corner[2];
			cornercon[2][0] = corner[0]; cornercon[2][1] = corner[1];
			cornercon[3][0] = -1; cornercon[3][1] = -1;
			break;
		case (4):
			cornercon[0][0] = corner[1]; cornercon[0][1] = corner[3];
			cornercon[1][0] = corner[0]; cornercon[1][1] = corner[2];
			cornercon[2][0] = corner[1]; cornercon[2][1] = corner[3];
			cornercon[3][0] = corner[0]; cornercon[3][1] = corner[2];
			break;
		default:
			RETURN(1);
	}
	   
	j = 0;
	for (k=0; k<CORNERS_OF_ELEM(theElement); ++k)
	{
		b = GETBITS(NodeOrder, 3*k+2);
		if ((ncorners==3)&&(b!=corner[0])&&(b!=corner[1])&&(b!=corner[2])) continue;
		if ((ncorners==4)&&(b!=corner[0])&&(b!=corner[1])&&(b!=corner[2])&&(b!=corner[3])) continue;
		i[j] = b;
		++j;
	}
	
	for (k=0; k<ncorners; ++k)
	{
		for (j=0; j<ncorners; ++j)
		   if (corner[k]==i[j]) break;
		   
		for (n=0; n<ncorners; ++n)
		   if (cornercon[k][0]==i[n]) break;
		icon[j][0] = n;
		for (n=0; n<ncorners; ++n)
		   if (cornercon[k][1]==i[n]) break;
		icon[j][1] = n;
	}
	return (0);	
}

static INT SideCornerIndexPRI (ELEMENT *theElement, INT side, INT NodeOrder, INT i[4], INT icon[4][2])
{
	INT k, corner[4], b,j,n, cornercon[4][2], ncorners;

	ncorners = CORNERS_OF_SIDE(theElement,side);

	/* get corner number of this side */
	for (k=0; k<ncorners; ++k)  
	   corner[k] = CORNER_OF_SIDE(theElement,side,k);
	   
	/* init the corner connection field */
	switch (ncorners) {
		case (3):
			cornercon[0][0] = corner[1]; cornercon[0][1] = corner[2];
			cornercon[1][0] = corner[0]; cornercon[1][1] = corner[2];
			cornercon[2][0] = corner[0]; cornercon[2][1] = corner[1];
			cornercon[3][0] = -1; cornercon[3][1] = -1;
			break;
		case (4):
			cornercon[0][0] = corner[1]; cornercon[0][1] = corner[3];
			cornercon[1][0] = corner[0]; cornercon[1][1] = corner[2];
			cornercon[2][0] = corner[1]; cornercon[2][1] = corner[3];
			cornercon[3][0] = corner[0]; cornercon[3][1] = corner[2];
			break;
		default:
			RETURN(1);
	}
	   
	j = 0;
	for (k=0; k<CORNERS_OF_ELEM(theElement); ++k)
	{
		b = GETBITS(NodeOrder, 3*k+2);
		if ((ncorners==3)&&(b!=corner[0])&&(b!=corner[1])&&(b!=corner[2])) continue;
		if ((ncorners==4)&&(b!=corner[0])&&(b!=corner[1])&&(b!=corner[2])&&(b!=corner[3])) continue;
		i[j] = b;
		++j;
	}
	
	for (k=0; k<ncorners; ++k)
	{
		for (j=0; j<ncorners; ++j)
		   if (corner[k]==i[j]) break;
		   
		for (n=0; n<ncorners; ++n)
		   if (cornercon[k][0]==i[n]) break;
		icon[j][0] = n;
		for (n=0; n<ncorners; ++n)
		   if (cornercon[k][1]==i[n]) break;
		icon[j][1] = n;
	}
	return (0);	
}

static INT SideCornerIndexHEX (ELEMENT *theElement, INT side, INT NodeOrder, INT i[4], INT icon[4][2])
{
	INT k, corner[4], b,j,n, cornercon[4][2];

	for (k=0; k<4; ++k)              /* 4=number of corners of hexahedron side */
	   corner[k] = CORNER_OF_SIDE0(TAG(theElement),side,k);
	   
	cornercon[0][0] = corner[1]; cornercon[0][1] = corner[3];
	cornercon[1][0] = corner[0]; cornercon[1][1] = corner[2];
	cornercon[2][0] = corner[1]; cornercon[2][1] = corner[3];
	cornercon[3][0] = corner[0]; cornercon[3][1] = corner[2];
	   
	j = 0;
	for (k=0; k<8; ++k)
	{
		b = GETBITS(NodeOrder, 3*k+2);
		if ((b!=corner[0])&&(b!=corner[1])&&(b!=corner[2])&&(b!=corner[3])) continue;
		i[j] = b;
		++j;
	}
	
	for (k=0; k<4; ++k)
	{
		for (j=0; j<4; ++j)
		   if (corner[k]==i[j]) break;
		   
		for (n=0; n<4; ++n)
		   if (cornercon[k][0]==i[n]) break;
		icon[j][0] = n;
		for (n=0; n<4; ++n)
		   if (cornercon[k][1]==i[n]) break;
		icon[j][1] = n;
	}
	return (0);	
}


/* TODO: this is not the fine way, better use general element concept */
static INT PCornerOrder[5][4] = {{1,3,4,-1},{0,2,4,-1},{1,3,4,-1},{0,2,4,-1},{0,1,2,3}};

static INT CornerIndexPYR (INT NodeOrder, INT i[8], INT icon[8][4])
{
	INT k,j,n;
	
	for (k=0; k<5; ++k)
		i[k] = GETBITS(NodeOrder, 3*k+2);
		
	for (k=0; k<5; ++k)
	{
		for (j=0; j<5; ++j)
			if (i[j] == k) break;
			
		for (n=0; n<5; ++n)
			if (PCornerOrder[k][0]==i[n]) break;
		icon[j][0] = n;
		for (n=0; n<5; ++n)
			if (PCornerOrder[k][1]==i[n]) break;
		icon[j][1] = n;
		for (n=0; n<5; ++n)
			if (PCornerOrder[k][2]==i[n]) break;
		icon[j][2] = n;
		if (PCornerOrder[k][3]==-1)
			icon[j][3] = -1;
		else {
			for (n=0; n<5; ++n)
				if (PCornerOrder[k][3]==i[n]) break;
			icon[j][3] = n;
		}
	}
	return (0);	
}

static INT PRCornerOrder[6][3] = {{1,2,3},{0,2,4},{0,1,5},{0,4,5},{1,3,5},{2,3,4}};

static INT CornerIndexPRI (INT NodeOrder, INT i[8], INT icon[8][4])
{
	INT k,j,n,c;
	
	for (k=0; k<6; ++k)
		i[k] = GETBITS(NodeOrder, 3*k+2);
		
	for (k=0; k<6; ++k)
	{
		for (j=0; j<6; ++j)
			if (i[j] == k) break;
			
		for (n=0; n<6; ++n)
			if (PRCornerOrder[k][0]==i[n]) break;
		ASSERT(n!=6);
		icon[j][0] = n;
		for (n=0; n<6; ++n)
			if (PRCornerOrder[k][1]==i[n]) break;
		ASSERT(n!=6);
		icon[j][1] = n;
		for (n=0; n<6; ++n)
			if (PRCornerOrder[k][2]==i[n]) break;
		ASSERT(n!=6);
		icon[j][2] = n;
		icon[j][3] = -1;
	}
	return (0);	
}


/* TODO: this is not the fine way, better use general element concept */
static INT ECornerOrder[8][3] = {{1,3,4},{0,2,5},{1,3,6},{0,2,7},
				 {0,5,7},{1,4,6},{2,5,7},{3,4,6}};

static INT CornerIndexHEX (INT NodeOrder, INT i[8], INT icon[8][3])
{
	INT k,j,n;
	
	for (k=0; k<8; ++k)
		i[k] = GETBITS(NodeOrder, 3*k+2);
		
	for (k=0; k<8; ++k)
	{
		for (j=0; j<8; ++j)
			if (i[j] == k) break;
			
		for (n=0; n<8; ++n)
			if (ECornerOrder[k][0]==i[n]) break;
		icon[j][0] = n;
		for (n=0; n<8; ++n)
			if (ECornerOrder[k][1]==i[n]) break;
		icon[j][1] = n;
		for (n=0; n<8; ++n)
			if (ECornerOrder[k][2]==i[n]) break;
		icon[j][2] = n;
	}
	return (0);	
}

static DefinePolygon (DOUBLE_VECTOR *Poly, INT Number)
{
	int i,j,n;
	DOUBLE Center[3], rad[6][3],  MaxCos, Cos, Direction[3], tmp;
	
	Center[0]=Center[1]=Center[2]=0.0;
	/* define center of mass of polygon */
	for (i=0; i<Number; ++i)
		V3_ADD(Poly[i],Center,Center)
	V3_SCALE(1.0/(DOUBLE)Number,Center)
	
	for (i=0; i<Number; ++i)
	{
		V3_SUBTRACT(Poly[i],Center,rad[i])
		V3_EUKLIDNORM(rad[i],Cos)
		V3_SCALE(1.0/Cos,rad[i])
	}
	
	MaxCos = -1;
	n = Number - 1;
	for (j=1; j<Number; ++j){
		V3_SCALAR_PRODUCT(rad[0],rad[j],Cos)
		MaxCos = MAX(Cos,MaxCos);
		if (MaxCos==Cos) n = j;
	}
	V3_COPY(Poly[1], Center)
	V3_COPY(Poly[n],Poly[1])
	V3_COPY(Center, Poly[n])
	
	V3_COPY(rad[1], Center)
	V3_COPY(rad[n], rad[1])
	V3_COPY(Center, rad[n])
	
	V3_VECTOR_PRODUCT(rad[0],rad[1],Direction)
			
	for (i=1; i<Number-1; ++i)
	{
		MaxCos = -1;
		for (j=i+1; j<Number; ++j){
			V3_SCALAR_PRODUCT(rad[i],rad[j],Cos)
			tmp = MAX(Cos,MaxCos);
			if (tmp==Cos) 
			{
				V3_VECTOR_PRODUCT(rad[i],rad[j],Center)
				V3_SCALAR_PRODUCT(Center,Direction,Cos)
				if (Cos>=0.0){
				   n = j;
				   MaxCos = tmp;
				}
			}
		}
		V3_COPY(Poly[i+1],Center)
		V3_COPY(Poly[n],Poly[i+1])
		V3_COPY(Center, Poly[n])
		
		V3_COPY(rad[i+1],Center)
		V3_COPY(rad[n],rad[i+1])
		V3_COPY(Center, rad[n])		 
	}
	return (0);
}

/****************************************************************************/
/*
   GetPolyElemSideISHalfSpace -  Get polygon

   SYNOPSIS:
   static INT GetPolyElemSideISHalfSpace (DOUBLE **Corners, 
   DOUBLE *CutZCoord, INT NodeOrder, INT side, DOUBLE_VECTOR *Poly, 
   INT *Number);

   PARAMETERS:
.  Corners - 
.  CutZCoord - 
.  NodeOrder - 
.  side -
.  Poly - 
.  Number - 

   DESCRIPTION:
   This function gets polygon being the intersection of an elementside of an 
   tetrahedron an the half space behind cut plane.	

   RETURN VALUE:
   INT
.n    0 if ok
.n    1 if error occured.
*/												
/****************************************************************************/

static INT GetPolyElemSideISHalfSpaceTET (ELEMENT *theElement, DOUBLE **Corners, DOUBLE *CutZCoord, INT NodeOrder, INT side, DOUBLE_VECTOR *Poly, INT *Number)
{
	INT i, j, count1, count2, count3;
	DOUBLE *x[MAX_CORNERS_OF_SIDE], Z[MAX_CORNERS_OF_SIDE];
	
	/* get corners of the element side */
	count1 = 0;
	count2 = 0;
	for (i=0; i<CORNERS_OF_SIDE(theElement,side); i++)
	{
		j = SideCornerIndex[side][NodeOrder][i];
		x[i] = Corners[j];
		Z[i] = CutZCoord[j];
		if (Z[i] > 0.0)
			count1++;
		if (Z[i] < 0.0)
			count2++;
	}
	count3 = CORNERS_OF_SIDE(theElement,side)-count1-count2;
	
	/* build the polygon */
	*Number = 0;
	if (count1+count3 >= CORNERS_OF_SIDE(theElement,side))
		return (0);
	for (i=0; i<count2+count3; i++)
		for (j=0; j<DIM; j++)
			Poly[i][j] = x[CORNERS_OF_SIDE(theElement,side)-i-1][j];
	*Number = count2+count3;
	
	if (*Number == CORNERS_OF_SIDE(theElement,side))
		return (0);
	switch (count1)
	{
		case (1):
			if (*Number != 2) return (1);
			V3_LINCOMB(Z[0]/(Z[0]-Z[1]), x[1], -Z[1]/(Z[0]-Z[1]), x[0], Poly[2])
			V3_LINCOMB(Z[0]/(Z[0]-Z[2]), x[2], -Z[2]/(Z[0]-Z[2]), x[0], Poly[3])
			*Number = 4;
			return (0);
		case (2):
			if (*Number != 1) return (1);
			V3_LINCOMB(Z[0]/(Z[0]-Z[2]), x[2], -Z[2]/(Z[0]-Z[2]), x[0], Poly[1])
			V3_LINCOMB(Z[1]/(Z[1]-Z[2]), x[2], -Z[2]/(Z[1]-Z[2]), x[1], Poly[2])
			*Number = 3;
			return (0);
	}
	
	return (1);
}

/****************************************************************************/
/*																			*/
/* Function:  GetPolyElemSideISHalfSpacePYR									*/
/*																			*/
/* Purpose:   get polygon being the intersection of an elementside of an	*/
/*			  pyramid and the half space behind cut plane					*/
/*																			*/
/* Input :	  all you need													*/
/*																			*/
/* Output:	  DOUBLE_VECTOR *Poly, INT *Number: output polygon				*/
/* Return:	  INT 0: ok 													*/
/*			  INT 1: an error occurred										*/
/*																			*/
/****************************************************************************/

static INT GetPolyElemSideISHalfSpacePYR (ELEMENT *theElement, DOUBLE **Corners, DOUBLE *CutZCoord, INT NodeOrder, INT side, DOUBLE_VECTOR *Poly, INT *Number)
{
	INT i, j, count1, count2, order[4], ordercon[4][2], ncorners;
	DOUBLE *x[4], Z[4];  /* 4=number of corners of hexahedron side */
	
	/* get corners of the element side */
	ncorners = CORNERS_OF_SIDE(theElement,side);
	count1 = 0;
	count2 = 0;
	SideCornerIndexPYR (theElement, side, NodeOrder, order, ordercon); 
	for (i=0; i<ncorners; i++) 
	{
		j = order[i];
		x[i] = Corners[j];
		Z[i] = CutZCoord[j];
		if (Z[i] > SMALL_C)
			count1++;
		if (Z[i] < -SMALL_C)
			count2++;
	}
	
	*Number = 0;
	switch (ncorners) {
		case (3):
			switch (count1)
			{
				case (0):
					switch (count2)
					{
						case (0):
							RETURN(1);
						case (1):
						case (2):
						case (3):
							V3_COPY(x[0], Poly[0])
							V3_COPY(x[1], Poly[1])
							i = 0;
							if (ordercon[1][0]==0) i = 1;
							V3_COPY(x[ordercon[1][i]], Poly[2])
							*Number = 3;
							return (0);
						default:
							RETURN(1);
						break;
					}
				case (1):
					switch (count2)
					{
						case (0):
							return (0);
						case (1):
							V3_COPY(x[1], Poly[0])
							V3_COPY(x[2], Poly[1])
							i = 0;
							if (ordercon[0][0] == 1) i = 1;	
							V3_LINCOMB(Z[0]/(Z[0]-Z[ordercon[0][i]]), x[ordercon[0][i]], -Z[ordercon[0][i]]/(Z[0]-Z[ordercon[0][i]]), x[0], Poly[2])				
							*Number = 3;
							return (0);
						case (2):
							V3_COPY(x[1], Poly[0])
							V3_COPY(x[2], Poly[1])
							i = 0;
							if (ordercon[0][0] == 1) i = 1;	
							V3_LINCOMB(Z[0]/(Z[0]-Z[ordercon[0][i]]), x[ordercon[0][i]], -Z[ordercon[0][i]]/(Z[0]-Z[ordercon[0][i]]), x[0], Poly[2])				
							i = 0;
							if (ordercon[0][0] == 2) i = 1;
							V3_LINCOMB(Z[0]/(Z[0]-Z[ordercon[0][i]]), x[ordercon[0][i]], -Z[ordercon[0][i]]/(Z[0]-Z[ordercon[0][i]]), x[0], Poly[3])				
							*Number = 4;
							return (0);
						default:
							RETURN(1);
						break;
					}
				case (2):
					switch (count2)
					{
						case (0):
							return (0);
						case (1):
							V3_COPY(x[2], Poly[0])
							i = 0;
							V3_LINCOMB(Z[ordercon[2][i]]/(Z[ordercon[2][i]]-Z[2]), x[2], -Z[2]/(Z[ordercon[2][i]]-Z[2]), x[ordercon[2][i]], Poly[1])
							i = 1;
							V3_LINCOMB(Z[ordercon[2][i]]/(Z[ordercon[2][i]]-Z[2]), x[2], -Z[2]/(Z[ordercon[2][i]]-Z[2]), x[ordercon[2][i]], Poly[2])
							*Number = 3;
							return (0);
						default:
							RETURN(1);
						break;
					}
				case (3):
					switch (count2)
					{
						case (0):
							return (0);
						default:
							RETURN(1);
						break;
					}
			}
			break;
		case (4):
			switch (count1)
			{
				case (0):
					switch (count2)
					{
						case (0):
						case (1):
							RETURN(1);
						case (2):
						case (3):
						case (4):
							V3_COPY(x[0], Poly[0])
							V3_COPY(x[1], Poly[1])
							i = 0;
							if (ordercon[1][0]==0) i = 1;
							V3_COPY(x[ordercon[1][i]], Poly[2])
							i = 0;
							if (ordercon[0][0]==1) i = 1;
							V3_COPY(x[ordercon[0][i]], Poly[3])
							*Number = 4;
							return (0);
						default:
							RETURN(1);
						break;
					}
				case (1):
					switch (count2)
					{
						case (0):
							RETURN(1);
						case (1):
							V3_COPY(x[1], Poly[0])
							V3_COPY(x[2], Poly[1])
							V3_COPY(x[3], Poly[2])
							*Number = 3;
							return (0);
						case (2):
							V3_COPY(x[1], Poly[0])
							i = 0;
							if (ordercon[0][0] == 1) i = 1;	
							V3_LINCOMB(Z[0]/(Z[0]-Z[ordercon[0][i]]), x[ordercon[0][i]], -Z[ordercon[0][i]]/(Z[0]-Z[ordercon[0][i]]), x[0], Poly[1])				
							V3_COPY(x[ordercon[0][i]], Poly[2])
							i = 0;
							if (ordercon[1][0] == 0) i = 1;
							V3_COPY(x[ordercon[1][i]], Poly[3])
							*Number = 4;
							return (0);
						case (3):
							V3_LINCOMB(Z[0]/(Z[0]-Z[ordercon[0][0]]), x[ordercon[0][0]], -Z[ordercon[0][0]]/(Z[0]-Z[ordercon[0][0]]), x[0], Poly[0])				
							V3_LINCOMB(Z[0]/(Z[0]-Z[ordercon[0][1]]), x[ordercon[0][1]], -Z[ordercon[0][1]]/(Z[0]-Z[ordercon[0][1]]), x[0], Poly[1])				
							V3_COPY(x[ordercon[0][1]], Poly[2])
							V3_COPY(x[ordercon[0][0]], Poly[4])
							i = ordercon[0][1];
							j = 0;
							if (ordercon[i][0] == 0) j = 1;
							V3_COPY(x[ordercon[i][j]], Poly[3])
							*Number = 5;
							return (0);
						default:
							RETURN(1);
						break;
					}
				case (2):
					switch (count2)
					{
						case (0):
							return (0);
						case (1):
							V3_COPY(x[2], Poly[0])
							V3_COPY(x[3], Poly[1])
							i = 0;
							if (ordercon[3][0] == 2) i = 1;
							V3_LINCOMB(Z[ordercon[3][i]]/(Z[ordercon[3][i]]-Z[3]), x[3], -Z[3]/(Z[ordercon[3][i]]-Z[3]), x[ordercon[3][i]], Poly[2])
							*Number = 3;
							return (0);
						case (2):
							i = 0;
							if (ordercon[0][0] == 1) i = 1;
							V3_LINCOMB(Z[0]/(Z[0]-Z[ordercon[0][i]]), x[ordercon[0][i]], -Z[ordercon[0][i]]/(Z[0]-Z[ordercon[0][i]]), x[0], Poly[0])				
							V3_COPY(x[ordercon[0][i]], Poly[1]);
							i = 0;
							if (ordercon[1][0] == 0) i = 1;
							V3_LINCOMB(Z[1]/(Z[1]-Z[ordercon[1][i]]), x[ordercon[1][i]], -Z[ordercon[1][i]]/(Z[1]-Z[ordercon[1][i]]), x[1], Poly[3])				
							V3_COPY(x[ordercon[1][i]],Poly[2]);
							*Number = 4;
							return (0);
						default:
							RETURN(1);
						break;
					}
				case (3):
					switch (count2)
					{
						case (0):
							return (0);
						case (1):
							for (i=0; i<2; ++i)
								V3_LINCOMB(Z[ordercon[3][i]]/(Z[ordercon[3][i]]-Z[3]), x[3], -Z[3]/(Z[ordercon[3][i]]-Z[3]), x[ordercon[3][i]], Poly[i])
							V3_COPY(x[3], Poly[2])
							*Number = 3;
							return (0);
						default:
							RETURN(1);
						break;
					}
				case (4):
					switch (count2)
					{		
						case (0):	
							return (0);
						default:
							RETURN(1);
						break;
					}
			}
			break;
		default:
			RETURN(1);
	}
	
	RETURN(1);
}

/****************************************************************************/
/*																			*/
/* Function:  GetPolyElemSideISHalfSpacePRI									*/
/*																			*/
/* Purpose:   get polygon being the intersection of an elementside of an	*/
/*			  pyrism and the half space behind cut plane					*/
/*																			*/
/* Input :	  all you need													*/
/*																			*/
/* Output:	  DOUBLE_VECTOR *Poly, INT *Number: output polygon				*/
/* Return:	  INT 0: ok 													*/
/*			  INT 1: an error occurred										*/
/*																			*/
/****************************************************************************/

static INT GetPolyElemSideISHalfSpacePRI (ELEMENT *theElement, DOUBLE **Corners, DOUBLE *CutZCoord, INT NodeOrder, INT side, DOUBLE_VECTOR *Poly, INT *Number)
{
	INT i, j, count1, count2, order[4], ordercon[4][2], ncorners;
	DOUBLE *x[4], Z[4];  /* 4=number of corners of hexahedron side */
	
	/* get corners of the element side */
	ncorners = CORNERS_OF_SIDE(theElement,side);
	count1 = 0;
	count2 = 0;
	SideCornerIndexPRI (theElement, side, NodeOrder, order, ordercon); 
	for (i=0; i<ncorners; i++) 
	{
		j = order[i];
		x[i] = Corners[j];
		Z[i] = CutZCoord[j];
		if (Z[i] > SMALL_C)
			count1++;
		if (Z[i] < -SMALL_C)
			count2++;
	}
	
	*Number = 0;
	switch (ncorners) {
		case (3):
			switch (count1)
			{
				case (0):
					switch (count2)
					{
						case (0):
							RETURN(1);
						case (1):
						case (2):
						case (3):
							V3_COPY(x[0], Poly[0])
							V3_COPY(x[1], Poly[1])
							i = 0;
							if (ordercon[1][0]==0) i = 1;
							V3_COPY(x[ordercon[1][i]], Poly[2])
							*Number = 3;
							return (0);
						default:
							RETURN(1);
						break;
					}
				case (1):
					switch (count2)
					{
						case (0):
							return (0);
						case (1):
							V3_COPY(x[1], Poly[0])
							V3_COPY(x[2], Poly[1])
							i = 0;
							if (ordercon[0][0] == 1) i = 1;	
							V3_LINCOMB(Z[0]/(Z[0]-Z[ordercon[0][i]]), x[ordercon[0][i]], -Z[ordercon[0][i]]/(Z[0]-Z[ordercon[0][i]]), x[0], Poly[2])				
							*Number = 3;
							return (0);
						case (2):
							V3_COPY(x[1], Poly[0])
							V3_COPY(x[2], Poly[1])
							i = 0;
							if (ordercon[0][0] == 1) i = 1;	
							V3_LINCOMB(Z[0]/(Z[0]-Z[ordercon[0][i]]), x[ordercon[0][i]], -Z[ordercon[0][i]]/(Z[0]-Z[ordercon[0][i]]), x[0], Poly[2])				
							i = 0;
							if (ordercon[0][0] == 2) i = 1;
							V3_LINCOMB(Z[0]/(Z[0]-Z[ordercon[0][i]]), x[ordercon[0][i]], -Z[ordercon[0][i]]/(Z[0]-Z[ordercon[0][i]]), x[0], Poly[3])				
							*Number = 4;
							return (0);
						default:
							RETURN(1);
						break;
					}
				case (2):
					switch (count2)
					{
						case (0):
							return (0);
						case (1):
							V3_COPY(x[2], Poly[0])
							i = 0;
							V3_LINCOMB(Z[ordercon[2][i]]/(Z[ordercon[2][i]]-Z[2]), x[2], -Z[2]/(Z[ordercon[2][i]]-Z[2]), x[ordercon[2][i]], Poly[1])
							i = 1;
							V3_LINCOMB(Z[ordercon[2][i]]/(Z[ordercon[2][i]]-Z[2]), x[2], -Z[2]/(Z[ordercon[2][i]]-Z[2]), x[ordercon[2][i]], Poly[2])
							*Number = 3;
							return (0);
						default:
							RETURN(1);
						break;
					}
				case (3):
					switch (count2)
					{
						case (0):
							return (0);
						default:
							RETURN(1);
						break;
					}
			}
			break;
		case (4):
			switch (count1)
			{
				case (0):
					switch (count2)
					{
						case (0):
						case (1):
							RETURN(1);
						case (2):
						case (3):
						case (4):
							V3_COPY(x[0], Poly[0])
							V3_COPY(x[1], Poly[1])
							i = 0;
							if (ordercon[1][0]==0) i = 1;
							V3_COPY(x[ordercon[1][i]], Poly[2])
							i = 0;
							if (ordercon[0][0]==1) i = 1;
							V3_COPY(x[ordercon[0][i]], Poly[3])
							*Number = 4;
							return (0);
						default:
							RETURN(1);
						break;
					}
				case (1):
					switch (count2)
					{
						case (0):
							RETURN(1);
						case (1):
							V3_COPY(x[1], Poly[0])
							V3_COPY(x[2], Poly[1])
							V3_COPY(x[3], Poly[2])
							*Number = 3;
							return (0);
						case (2):
							V3_COPY(x[1], Poly[0])
							i = 0;
							if (ordercon[0][0] == 1) i = 1;	
							V3_LINCOMB(Z[0]/(Z[0]-Z[ordercon[0][i]]), x[ordercon[0][i]], -Z[ordercon[0][i]]/(Z[0]-Z[ordercon[0][i]]), x[0], Poly[1])				
							V3_COPY(x[ordercon[0][i]], Poly[2])
							i = 0;
							if (ordercon[1][0] == 0) i = 1;
							V3_COPY(x[ordercon[1][i]], Poly[3])
							*Number = 4;
							return (0);
						case (3):
							V3_LINCOMB(Z[0]/(Z[0]-Z[ordercon[0][0]]), x[ordercon[0][0]], -Z[ordercon[0][0]]/(Z[0]-Z[ordercon[0][0]]), x[0], Poly[0])				
							V3_LINCOMB(Z[0]/(Z[0]-Z[ordercon[0][1]]), x[ordercon[0][1]], -Z[ordercon[0][1]]/(Z[0]-Z[ordercon[0][1]]), x[0], Poly[1])				
							V3_COPY(x[ordercon[0][1]], Poly[2])
							V3_COPY(x[ordercon[0][0]], Poly[4])
							i = ordercon[0][1];
							j = 0;
							if (ordercon[i][0] == 0) j = 1;
							V3_COPY(x[ordercon[i][j]], Poly[3])
							*Number = 5;
							return (0);
						default:
							RETURN(1);
						break;
					}
				case (2):
					switch (count2)
					{
						case (0):
							return (0);
						case (1):
							V3_COPY(x[2], Poly[0])
							V3_COPY(x[3], Poly[1])
							i = 0;
							if (ordercon[3][0] == 2) i = 1;
							V3_LINCOMB(Z[ordercon[3][i]]/(Z[ordercon[3][i]]-Z[3]), x[3], -Z[3]/(Z[ordercon[3][i]]-Z[3]), x[ordercon[3][i]], Poly[2])
							*Number = 3;
							return (0);
						case (2):
							i = 0;
							if (ordercon[0][0] == 1) i = 1;
							V3_LINCOMB(Z[0]/(Z[0]-Z[ordercon[0][i]]), x[ordercon[0][i]], -Z[ordercon[0][i]]/(Z[0]-Z[ordercon[0][i]]), x[0], Poly[0])				
							V3_COPY(x[ordercon[0][i]], Poly[1]);
							i = 0;
							if (ordercon[1][0] == 0) i = 1;
							V3_LINCOMB(Z[1]/(Z[1]-Z[ordercon[1][i]]), x[ordercon[1][i]], -Z[ordercon[1][i]]/(Z[1]-Z[ordercon[1][i]]), x[1], Poly[3])				
							V3_COPY(x[ordercon[1][i]],Poly[2]);
							*Number = 4;
							return (0);
						default:
							RETURN(1);
						break;
					}
				case (3):
					switch (count2)
					{
						case (0):
							return (0);
						case (1):
							for (i=0; i<2; ++i)
								V3_LINCOMB(Z[ordercon[3][i]]/(Z[ordercon[3][i]]-Z[3]), x[3], -Z[3]/(Z[ordercon[3][i]]-Z[3]), x[ordercon[3][i]], Poly[i])
							V3_COPY(x[3], Poly[2])
							*Number = 3;
							return (0);
						default:
							RETURN(1);
						break;
					}
				case (4):
					switch (count2)
					{		
						case (0):	
							return (0);
						default:
							RETURN(1);
						break;
					}
			}
			break;
		default:
			RETURN(1);
	}
	
	RETURN(1);
}

/****************************************************************************/
/*																			*/
/* Function:  GetPolyElemSideISHalfSpaceHEX									*/
/*																			*/
/* Purpose:   get polygon being the intersection of an elementside of an	*/
/*			  hexahedron an the half space behind cut plane				    */
/*																			*/
/* Input :	  all you need													*/
/*																			*/
/* Output:	  DOUBLE_VECTOR *Poly, INT *Number: output polygon				*/
/*																			*/ 
/* Return:	  INT 0: ok 													*/
/*			  INT 1: an error occurred										*/
/*																			*/
/****************************************************************************/

static INT GetPolyElemSideISHalfSpaceHEX (ELEMENT *theElement, DOUBLE **Corners, DOUBLE *CutZCoord, INT NodeOrder, INT side, DOUBLE_VECTOR *Poly, INT *Number)
{
	INT i, j, count1, count2, order[4], ordercon[4][2];
	DOUBLE *x[4], Z[4];  /* 4=number of corners of hexahedron side */
	
	/* get corners of the element side */
	count1 = 0;
	count2 = 0;
	SideCornerIndexHEX (theElement, side, NodeOrder, order, ordercon); 
	for (i=0; i<4; i++) /* 4=number of corners of hexahedron side */
	{
		j = order[i];
		x[i] = Corners[j];
		Z[i] = CutZCoord[j];
		if (Z[i] > SMALL_C)
			count1++;
		if (Z[i] < -SMALL_C)
			count2++;
	}
	
	*Number = 0;
	switch (count1)
	{
		case (0):
			switch (count2)
			{
				case (0):
				case (1):
					RETURN(1);
				case (2):
				case (3):
				case (4):
					V3_COPY(x[0], Poly[0])
					V3_COPY(x[1], Poly[1])
					i = 0;
					if (ordercon[1][0]==0) i = 1;
					V3_COPY(x[ordercon[1][i]], Poly[2])
					i = 0;
					if (ordercon[0][0]==1) i = 1;
					V3_COPY(x[ordercon[0][i]], Poly[3])
					*Number = 4;
					return (0);
				default:
					RETURN(1);
				break;
			}
		case (1):
			switch (count2)
			{
				case (0):
					RETURN(1);
				case (1):
					V3_COPY(x[1], Poly[0])
					V3_COPY(x[2], Poly[1])
					V3_COPY(x[3], Poly[2])
					*Number = 3;
					return (0);
				case (2):
					V3_COPY(x[1], Poly[0])
					i = 0;
					if (ordercon[0][0] == 1) i = 1;	
					V3_LINCOMB(Z[0]/(Z[0]-Z[ordercon[0][i]]), x[ordercon[0][i]], -Z[ordercon[0][i]]/(Z[0]-Z[ordercon[0][i]]), x[0], Poly[1])				
					V3_COPY(x[ordercon[0][i]], Poly[2])
					i = 0;
					if (ordercon[1][0] == 0) i = 1;
					V3_COPY(x[ordercon[1][i]], Poly[3])
					*Number = 4;
					return (0);
				case (3):
					V3_LINCOMB(Z[0]/(Z[0]-Z[ordercon[0][0]]), x[ordercon[0][0]], -Z[ordercon[0][0]]/(Z[0]-Z[ordercon[0][0]]), x[0], Poly[0])				
					V3_LINCOMB(Z[0]/(Z[0]-Z[ordercon[0][1]]), x[ordercon[0][1]], -Z[ordercon[0][1]]/(Z[0]-Z[ordercon[0][1]]), x[0], Poly[1])				
					V3_COPY(x[ordercon[0][1]], Poly[2])
					V3_COPY(x[ordercon[0][0]], Poly[4])
					i = ordercon[0][1];
					j = 0;
					if (ordercon[i][0] == 0) j = 1;
					V3_COPY(x[ordercon[i][j]], Poly[3])
					*Number = 5;
					return (0);
				default:
					RETURN(1);
				break;
			}
		case (2):
			switch (count2)
			{
				case (0):
					return (0);
				case (1):
					V3_COPY(x[2], Poly[0])
					V3_COPY(x[3], Poly[1])
					i = 0;
					if (ordercon[3][0] == 2) i = 1;
					V3_LINCOMB(Z[ordercon[3][i]]/(Z[ordercon[3][i]]-Z[3]), x[3], -Z[3]/(Z[ordercon[3][i]]-Z[3]), x[ordercon[3][i]], Poly[2])
					*Number = 3;
					return (0);
				case (2):
					i = 0;
					if (ordercon[0][0] == 1) i = 1;
					V3_LINCOMB(Z[0]/(Z[0]-Z[ordercon[0][i]]), x[ordercon[0][i]], -Z[ordercon[0][i]]/(Z[0]-Z[ordercon[0][i]]), x[0], Poly[0])				
					V3_COPY(x[ordercon[0][i]], Poly[1]);
					i = 0;
					if (ordercon[1][0] == 0) i = 1;
					V3_LINCOMB(Z[1]/(Z[1]-Z[ordercon[1][i]]), x[ordercon[1][i]], -Z[ordercon[1][i]]/(Z[1]-Z[ordercon[1][i]]), x[1], Poly[3])				
					V3_COPY(x[ordercon[1][i]],Poly[2]);
					*Number = 4;
					return (0);
				default:
					RETURN(1);
				break;
			}
		case (3):
			switch (count2)
			{
				case (0):
					return (0);
				case (1):
					for (i=0; i<2; ++i)
						V3_LINCOMB(Z[ordercon[3][i]]/(Z[ordercon[3][i]]-Z[3]), x[3], -Z[3]/(Z[ordercon[3][i]]-Z[3]), x[ordercon[3][i]], Poly[i])
					V3_COPY(x[3], Poly[2])
					*Number = 3;
					return (0);
				default:
					RETURN(1);
				break;
			}
		case (4):
			switch (count2)
			{		
				case (0):	
					return (0);
				default:
					RETURN(1);
				break;
			}
	}
	
	RETURN(1);
}

/****************************************************************************/
/*																			*/
/* Function:  GetLineElemSideISCutPlaneTET									*/
/*																			*/
/* Purpose:   get line being the intersection of an elementside of an		*/
/*			  tetrahedron an the cut plane									*/
/*																			*/
/* Input :	  all you need													*/
/*																			*/
/* Output:	  DOUBLE_VECTOR *Line, INT *Number: output polygon: 0 or 2		*/ 
/*																	        */
/* Return:	  INT 0: ok 													*/
/*			  INT 1: an error occurred										*/
/*																			*/
/****************************************************************************/

static INT GetLineElemSideISCutPlaneTET (ELEMENT *theElement, DOUBLE **Corners, DOUBLE *CutZCoord, INT NodeOrder, INT side, DOUBLE_VECTOR *Line, INT *Number)
{
	INT i, j, c;
	DOUBLE *x[MAX_CORNERS_OF_SIDE], Z[MAX_CORNERS_OF_SIDE];
	
	/* get corners of the element side */
	c = 0;
	for (i=0; i<CORNERS_OF_SIDE(theElement,side); i++)
	{
		j = SideCornerIndex[side][NodeOrder][i];
		x[i] = Corners[j];
		Z[i] = CutZCoord[j];
		if (Z[i] > 0.0) c++;
	}
	
	/* build the polygon */
	switch (c)
	{
		case (1):
			V3_LINCOMB(Z[0]/(Z[0]-Z[1]), x[1], -Z[1]/(Z[0]-Z[1]), x[0], Line[0])
			V3_LINCOMB(Z[0]/(Z[0]-Z[2]), x[2], -Z[2]/(Z[0]-Z[2]), x[0], Line[1])
			*Number = 2;
			return (0);
		case (2):
			V3_LINCOMB(Z[0]/(Z[0]-Z[2]), x[2], -Z[2]/(Z[0]-Z[2]), x[0], Line[0])
			V3_LINCOMB(Z[1]/(Z[1]-Z[2]), x[2], -Z[2]/(Z[1]-Z[2]), x[1], Line[1])
			*Number = 2;
			return (0);
		default:
			*Number = 0;
			return (0);
	}
}

/****************************************************************************/
/*																			*/
/* Function:  GetLineElemSideISCutPlaneHEX									*/
/*																			*/
/* Purpose:   get line being the intersection of an elementside of an		*/
/*			  hexahedron an the cut plane									*/
/*																			*/
/* Input :	  all you need													*/
/*																			*/
/* Output:	  DOUBLE_VECTOR *Line, INT *Number: output polygon: 0 or 2		*/ 
/*																			*/
/* Return:	  INT 0: ok 													*/
/*			  INT 1: an error occurred										*/
/*																			*/
/****************************************************************************/

static INT GetLineElemSideISCutPlaneHEX (ELEMENT* theElement, DOUBLE **Corners, DOUBLE *CutZCoord, INT NodeOrder, INT side, DOUBLE_VECTOR *Line, INT *Number)
{
	INT i, j, c, order[4], ordercon[4][2];
	DOUBLE *x[4], Z[4];
	
	/* get corners of the element side */
	c = 0;
	switch (TAG(theElement)) {
	    case (PRISM): 
		case (PYRAMID): SideCornerIndexPYR (theElement, side, NodeOrder, order, ordercon); break;
		case (HEXAHEDRON): SideCornerIndexHEX (theElement, side, NodeOrder, order, ordercon); break;
		default: RETURN(1);
	}
	for (i=0; i<CORNERS_OF_SIDE(theElement,side); i++)
	{
		j = order[i];
		x[i] = Corners[j];
		Z[i] = CutZCoord[j];
		if (Z[i] > SMALL_C) c++;
	}
	
	/* build the polygon */
	switch (c)
	{
		case (1):
			V3_LINCOMB(Z[0]/(Z[0]-Z[ordercon[0][0]]), x[ordercon[0][0]], -Z[ordercon[0][0]]/(Z[0]-Z[ordercon[0][0]]), x[0], Line[0])
			V3_LINCOMB(Z[0]/(Z[0]-Z[ordercon[0][1]]), x[ordercon[0][1]], -Z[ordercon[0][1]]/(Z[0]-Z[ordercon[0][1]]), x[0], Line[1])
			*Number = 2;
			return (0);
		case (2):
			i = 0;
			if (ordercon[0][0] == 1) i=1;
			V3_LINCOMB(Z[0]/(Z[0]-Z[ordercon[0][i]]), x[ordercon[0][i]], -Z[ordercon[0][i]]/(Z[0]-Z[ordercon[0][i]]), x[0], Line[0])
			i = 0;
			if (ordercon[1][0] == 0) i=1;
			V3_LINCOMB(Z[1]/(Z[1]-Z[ordercon[1][i]]), x[ordercon[1][i]], -Z[ordercon[1][i]]/(Z[1]-Z[ordercon[1][i]]), x[1], Line[1])
			*Number = 2;
			return (0);			
		case (3):
			V3_LINCOMB(Z[ordercon[3][0]]/(Z[ordercon[3][0]]-Z[3]), x[3], -Z[3]/(Z[ordercon[3][0]]-Z[3]), x[ordercon[3][0]], Line[0])
			V3_LINCOMB(Z[ordercon[3][1]]/(Z[ordercon[3][1]]-Z[3]), x[3], -Z[3]/(Z[ordercon[3][1]]-Z[3]), x[ordercon[3][1]], Line[1])
			*Number = 2;
			return (0);
		default:
			*Number = 0;
			return (0);
	}
}

/****************************************************************************/
/*
   GetPolyElemISCutPlane - Get polygon

   SYNOPSIS:
   static INT GetPolyElemISCutPlane (DOUBLE **CornerDC, 
   DOUBLE *CutZCoord, INT NodeOrder, DOUBLE_VECTOR *Poly, INT *Number);

   PARAMETERS:
.  CornerDC -
.  CutZCoord -
.  NodeOrder - 
.  Poly - 
.  Number -

   DESCRIPTION:
   This function gets polygon being the intersection of an 
   tetrahedron and the cut plane.

   RETURN VALUE:
   INT
.n    0 if ok
.n    1 if error occured.
*/
/****************************************************************************/

/****************************************************************************/
/*																			*/
/* Function:  GetPolyElemISCutPlaneTET 										*/
/*																			*/
/* Purpose:   get polygon being the intersection of an tetrahedron and the	*/
/*			  cut plane 													*/
/*																			*/
/* Input :	  all you need													*/
/*																			*/
/* Output:	  DOUBLE_VECTOR *Poly, INT *Number: output polygon				*/
/*																			*/
/* Return:	  INT 0: ok 													*/
/*			  INT 1: an error occurred										*/
/*																			*/
/****************************************************************************/

static INT GetPolyElemISCutPlaneTET (DOUBLE **CornerDC, DOUBLE *CutZCoord, INT NodeOrder, DOUBLE_VECTOR *Poly, INT *Number)
{
	INT i, j, count1, count2;
	DOUBLE *x[MAX_CORNERS_OF_ELEM], Z[MAX_CORNERS_OF_ELEM];
	
	/* get corners of the element side */
	count1 = 0;
	count2 = 0;
	for (i=0; i<4; i++) 	/* 4=Corners of tetrahedron */
	{
		j = CornerIndex[NodeOrder][i];
		x[i] = CornerDC[j]; 
		Z[i] = CutZCoord[j];
		if (Z[i] > SMALL_C)
			count1++;
		if (Z[i] < -SMALL_C)
			count2++;
	}
	
	/* build the polygon */
	*Number = 0;
	switch (count1)
	{
		case (0):
			switch (count2)
			{
				case (0):
					RETURN(1);
				case (1):
					V3_COPY(x[0], Poly[0])
					V3_COPY(x[1], Poly[1])
					V3_COPY(x[2], Poly[2])
					*Number = 3;
					return (0);
				case (2):
				case (3):
				case (4):
					return (0);
			}
		case (1):
			switch (count2)
			{
				case (0):
					V3_COPY(x[1], Poly[0])
					V3_COPY(x[2], Poly[1])
					V3_COPY(x[3], Poly[2])
					*Number = 3;
					return (0);
				case (1):
					V3_COPY(x[1], Poly[0])
					V3_COPY(x[2], Poly[1])
					V3_LINCOMB(Z[0]/(Z[0]-Z[3]), x[3], -Z[3]/(Z[0]-Z[3]), x[0], Poly[2])
					*Number = 3;
					return (0);
				case (2):
					V3_COPY(x[1], Poly[0])
					V3_LINCOMB(Z[0]/(Z[0]-Z[2]), x[2], -Z[2]/(Z[0]-Z[2]), x[0], Poly[1]);
					V3_LINCOMB(Z[0]/(Z[0]-Z[3]), x[3], -Z[3]/(Z[0]-Z[3]), x[0], Poly[2]);
					*Number = 3;
					return (0);
				case (3):
					V3_LINCOMB(Z[0]/(Z[0]-Z[1]), x[1], -Z[1]/(Z[0]-Z[1]), x[0], Poly[0])
					V3_LINCOMB(Z[0]/(Z[0]-Z[2]), x[2], -Z[2]/(Z[0]-Z[2]), x[0], Poly[1])
					V3_LINCOMB(Z[0]/(Z[0]-Z[3]), x[3], -Z[3]/(Z[0]-Z[3]), x[0], Poly[2])
					*Number = 3;
					return (0);
				default:
					RETURN(1);
				break;
			}
		case (2):
			switch (count2)
			{
				case (0):
					return (0);
				case (1):
					V3_COPY(x[2], Poly[0])
					V3_LINCOMB(Z[0]/(Z[0]-Z[3]), x[3], -Z[3]/(Z[0]-Z[3]), x[0], Poly[1])
					V3_LINCOMB(Z[1]/(Z[1]-Z[3]), x[3], -Z[3]/(Z[1]-Z[3]), x[1], Poly[2])
					*Number = 3;
					return (0);
				case (2):
					V3_LINCOMB(Z[0]/(Z[0]-Z[2]), x[2], -Z[2]/(Z[0]-Z[2]), x[0], Poly[0])
					V3_LINCOMB(Z[0]/(Z[0]-Z[3]), x[3], -Z[3]/(Z[0]-Z[3]), x[0], Poly[1])
					V3_LINCOMB(Z[1]/(Z[1]-Z[3]), x[3], -Z[3]/(Z[1]-Z[3]), x[1], Poly[2])
					V3_LINCOMB(Z[1]/(Z[1]-Z[2]), x[2], -Z[2]/(Z[1]-Z[2]), x[1], Poly[3])
					*Number = 4;
					return (0);
				default:
					RETURN(1);
				break;
			}
		case (3):
			switch (count2)
			{
				case (0):
					return (0);
				case (1):
					V3_LINCOMB(Z[0]/(Z[0]-Z[3]), x[3], -Z[3]/(Z[0]-Z[3]), x[0], Poly[0])
					V3_LINCOMB(Z[1]/(Z[1]-Z[3]), x[3], -Z[3]/(Z[1]-Z[3]), x[1], Poly[1])
					V3_LINCOMB(Z[2]/(Z[2]-Z[3]), x[3], -Z[3]/(Z[2]-Z[3]), x[2], Poly[2])
					*Number = 3;
					return (0);
				default:
					RETURN(1);
			}
		case (4):
			return (0);
	}
	
	RETURN(1);
}

/****************************************************************************/
/*																			*/
/* Function:  GetPolyElemISCutPlanePYR										*/
/*																			*/
/* Purpose:   get polygon being the intersection of a pyramid and the		*/
/*			  cut plane 													*/
/*																			*/
/* Input :	  all you need													*/
/*																			*/
/* Output:	  DOUBLE_VECTOR *Poly, INT *Number: output polygon				*/
/*																			*/
/* Return:	  INT 0: ok 													*/
/*			  INT 1: an error occurred										*/
/*																			*/
/****************************************************************************/

static INT GetPolyElemISCutPlanePYR (DOUBLE **CornerDC, DOUBLE *CutZCoord, INT NodeOrder, DOUBLE_VECTOR *Poly, INT *Number)
{
	INT i, j, k, count1, count2, order[5], ordercon[5][4];
	DOUBLE *x[MAX_CORNERS_OF_ELEM], Z[MAX_CORNERS_OF_ELEM];
	
	/* get corners of the element side */
	count1 = 0;
	count2 = 0;
	CornerIndexPYR (NodeOrder, order, ordercon);
	for (i=0; i<5; i++) 	/* 5=Corners of pyramid */
	{
		j = order[i];
		x[i] = CornerDC[j]; 
		Z[i] = CutZCoord[j];
		if (Z[i] > SMALL_C)
			count1++;
		if (Z[i] < -SMALL_C)
			count2++;
	}
	
	/* build the polygon */
	*Number = 0;
	switch (count1)
	{
		case (0):
			switch (count2)
			{
				case (0):
					RETURN(1);
				case (1):
					if (ordercon[4][3]==-1) {
						RETURN(1);
					}
					V3_COPY(x[0],Poly[0])
					V3_COPY(x[1],Poly[1])
					V3_COPY(x[2],Poly[2])
					V3_COPY(x[3],Poly[3])
					*Number = 4;
					DefinePolygon (Poly, *Number);
					return(0);
				case (2):
					if (ordercon[0][3]!=-1 || ordercon[1][3]!=-1 || ordercon[2][3]!=-1) {
						V3_COPY(x[0],Poly[0])
						V3_COPY(x[1],Poly[1])
						V3_COPY(x[2],Poly[2])
						*Number = 3;
						return(0);
					}
					else {
						RETURN(1);
					}
				case (3):
				case (4):
				case (5):
					return (0);
				default:
					RETURN(1);
			}
		case (1):
			switch (count2)
			{
				case (0):
					if (ordercon[0][3]==-1) {
						RETURN(1);
					}
					else 
						V3_COPY(x[1], Poly[0])
						V3_COPY(x[2], Poly[1])
						V3_COPY(x[3], Poly[2])
						V3_COPY(x[4], Poly[3])
						*Number = 4;
						DefinePolygon (Poly, *Number);
						return(0);
				case (1):
					if (ordercon[0][3]==-1) {
						V3_COPY(x[1], Poly[0])
						V3_COPY(x[2], Poly[1])
						V3_COPY(x[3], Poly[2])
						*Number = 3;
						return (0);					
					}
					else {
						RETURN(1);
					}
				case (2):
					V3_COPY(x[1], Poly[0])
					V3_COPY(x[2], Poly[1])
					i = 0;
					if (ordercon[0][i]==1 || ordercon[0][i]==2) i++;
					if (ordercon[0][i]==1 || ordercon[0][i]==2) i++;
					V3_LINCOMB(Z[0]/(Z[0]-Z[ordercon[0][i]]), x[ordercon[0][i]], -Z[ordercon[0][i]]/(Z[0]-Z[ordercon[0][i]]), x[0], Poly[2])
					if (ordercon[0][3]==-1) {
						*Number = 3;
					}
					else {
						i++;
						if (ordercon[0][i]==1 || ordercon[0][i]==2) i++;
						if (ordercon[0][i]==1 || ordercon[0][i]==2) i++;
						ASSERT(i<4);
						V3_LINCOMB(Z[0]/(Z[0]-Z[ordercon[0][i]]), x[ordercon[0][i]], -Z[ordercon[0][i]]/(Z[0]-Z[ordercon[0][i]]), x[0], Poly[3])
						*Number = 4;
						DefinePolygon (Poly, *Number);
					}
					return (0);					
				case (3):
					V3_COPY(x[1], Poly[0])
					i = 0;
					if (ordercon[0][i]==1) i++;
					V3_LINCOMB(Z[0]/(Z[0]-Z[ordercon[0][i]]), x[ordercon[0][i]], -Z[ordercon[0][i]]/(Z[0]-Z[ordercon[0][i]]), x[0], Poly[1])
					i++;
					if (ordercon[0][i]==1) i++;
					ASSERT(i<3);
					V3_LINCOMB(Z[0]/(Z[0]-Z[ordercon[0][i]]), x[ordercon[0][i]], -Z[ordercon[0][i]]/(Z[0]-Z[ordercon[0][i]]), x[0], Poly[2])
					if (ordercon[0][3]==-1) {
						*Number = 3;
					}
					else {
						i++;
						if (ordercon[0][i]==1) i++;
						ASSERT(i<4);
						V3_LINCOMB(Z[0]/(Z[0]-Z[ordercon[0][i]]), x[ordercon[0][i]], -Z[ordercon[0][i]]/(Z[0]-Z[ordercon[0][i]]), x[0], Poly[3])
						*Number = 4;
						DefinePolygon (Poly, *Number);
					}
					return (0);					
				case (4):
					i = 0;
					V3_LINCOMB(Z[0]/(Z[0]-Z[ordercon[0][i]]), x[ordercon[0][i]], -Z[ordercon[0][i]]/(Z[0]-Z[ordercon[0][i]]), x[0], Poly[0])
					i++;
					V3_LINCOMB(Z[0]/(Z[0]-Z[ordercon[0][i]]), x[ordercon[0][i]], -Z[ordercon[0][i]]/(Z[0]-Z[ordercon[0][i]]), x[0], Poly[1])
					i++;
					V3_LINCOMB(Z[0]/(Z[0]-Z[ordercon[0][i]]), x[ordercon[0][i]], -Z[ordercon[0][i]]/(Z[0]-Z[ordercon[0][i]]), x[0], Poly[2])
					if (ordercon[0][3]==-1) {
						*Number = 3;
					}
					else {
						i++;
						V3_LINCOMB(Z[0]/(Z[0]-Z[ordercon[0][i]]), x[ordercon[0][i]], -Z[ordercon[0][i]]/(Z[0]-Z[ordercon[0][i]]), x[0], Poly[3])
						*Number = 4;
						DefinePolygon (Poly, *Number);
					}
					return (0);					
				default:
					RETURN(1);
			}
		case (2):
			switch (count2)
			{
				case (0):
					if (ordercon[0][3]!=-1 || ordercon[1][3]!=-1) {
						RETURN(1);
					}
					else 
						V3_COPY(x[2], Poly[0])
						V3_COPY(x[3], Poly[1])
						V3_COPY(x[4], Poly[2])
						*Number = 3;
						return(0);
				case (1):
					j = 0;
					V3_COPY(x[2],Poly[j])
					j++;
					V3_COPY(x[3],Poly[j])
					j++;
					if (ordercon[0][3]==-1 && ordercon[1][3]==-1) {
						i = 0;
						if ((ordercon[0][i]==1)||(ordercon[0][i]==2)||(ordercon[0][i]==3)) i++;
						if ((ordercon[0][i]==1)||(ordercon[0][i]==2)||(ordercon[0][i]==3)) i++;
						V3_LINCOMB(Z[0]/(Z[0]-Z[ordercon[0][i]]), x[ordercon[0][i]], -Z[ordercon[0][i]]/(Z[0]-Z[ordercon[0][i]]), x[0], Poly[j])
						j++;
						i = 0;
						if ((ordercon[1][i]==0)||(ordercon[1][i]==2)||(ordercon[1][i]==3)) i++;
						if ((ordercon[1][i]==0)||(ordercon[1][i]==2)||(ordercon[1][i]==3)) i++;
						V3_LINCOMB(Z[1]/(Z[1]-Z[ordercon[1][i]]), x[ordercon[1][i]], -Z[ordercon[1][i]]/(Z[1]-Z[ordercon[1][i]]), x[1], Poly[j])
						j++;
						*Number = 4;
						DefinePolygon (Poly, *Number);
					}
					else {
						k = 0;
						if (ordercon[k][3]==-1) k++;
						i = 0;
						if ((ordercon[k][i]==1)||(ordercon[k][i]==2)||(ordercon[k][i]==3)) i++;
						if ((ordercon[k][i]==1)||(ordercon[k][i]==2)||(ordercon[k][i]==3)) i++;
						if ((ordercon[k][i]==1)||(ordercon[k][i]==2)||(ordercon[k][i]==3)) i++;
						V3_LINCOMB(Z[k]/(Z[k]-Z[ordercon[k][i]]), x[ordercon[k][i]], -Z[ordercon[k][i]]/(Z[k]-Z[ordercon[k][i]]), x[k], Poly[j])
						j++;
						*Number = 3;
					}
					return(0);
				case (2):
					j = 0;
					V3_COPY(x[2],Poly[j])
					j++;
					i = 0;
					if ((ordercon[0][i]==1)||(ordercon[0][i]==2)) i++;
					if ((ordercon[0][i]==1)||(ordercon[0][i]==2)) i++;
					V3_LINCOMB(Z[0]/(Z[0]-Z[ordercon[0][i]]), x[ordercon[0][i]], -Z[ordercon[0][i]]/(Z[0]-Z[ordercon[0][i]]), x[0], Poly[j])
					j++; i++;
					if ((ordercon[0][i]==1)||(ordercon[0][i]==2)) i++;
					if (i<3 && ((ordercon[0][i]!=1)&&(ordercon[0][i]!=2))) {
						V3_LINCOMB(Z[0]/(Z[0]-Z[ordercon[0][i]]), x[ordercon[0][i]], -Z[ordercon[0][i]]/(Z[0]-Z[ordercon[0][i]]), x[0], Poly[j])
						j++; i++;
					}
					if (ordercon[0][3]!=-1) {
						if ((ordercon[0][i]==1)||(ordercon[0][i]==2)) i++;
						V3_LINCOMB(Z[0]/(Z[0]-Z[ordercon[0][i]]), x[ordercon[0][i]], -Z[ordercon[0][i]]/(Z[0]-Z[ordercon[0][i]]), x[0], Poly[j])
						j++; i++;
					}
					i = 0;
					if ((ordercon[1][i]==0)||(ordercon[1][i]==2)) i++;
					if ((ordercon[1][i]==0)||(ordercon[1][i]==2)) i++;
					V3_LINCOMB(Z[1]/(Z[1]-Z[ordercon[1][i]]), x[ordercon[1][i]], -Z[ordercon[1][i]]/(Z[1]-Z[ordercon[1][i]]), x[1], Poly[j])
					j++; i++;
					if ((ordercon[1][i]==0)||(ordercon[1][i]==2)) i++;
					if (i<3 && ((ordercon[1][i]!=0)&&(ordercon[1][i]!=2))) {
						V3_LINCOMB(Z[1]/(Z[1]-Z[ordercon[1][i]]), x[ordercon[1][i]], -Z[ordercon[1][i]]/(Z[1]-Z[ordercon[1][i]]), x[1], Poly[j])
						j++; i++;
					}
					if (ordercon[1][3]!=-1) {
						if ((ordercon[1][i]==1)||(ordercon[1][i]==2)) i++;
						V3_LINCOMB(Z[1]/(Z[1]-Z[ordercon[1][i]]), x[ordercon[1][i]], -Z[ordercon[1][i]]/(Z[1]-Z[ordercon[1][i]]), x[1], Poly[j])
						j++;i++;
					}
					*Number = j;
					DefinePolygon (Poly, *Number);
					return(0);
				case (3):
					j = 0;
					i = 0;
					if (ordercon[0][i]==1) i++;
					V3_LINCOMB(Z[0]/(Z[0]-Z[ordercon[0][i]]), x[ordercon[0][i]], -Z[ordercon[0][i]]/(Z[0]-Z[ordercon[0][i]]), x[0], Poly[j])
					j++; i++;
					if (ordercon[0][i]==1) i++;
					V3_LINCOMB(Z[0]/(Z[0]-Z[ordercon[0][i]]), x[ordercon[0][i]], -Z[ordercon[0][i]]/(Z[0]-Z[ordercon[0][i]]), x[0], Poly[j])
					j++; i++;
					if (ordercon[0][3]!=-1) { 
						if (ordercon[0][i]==1) i++;	
						V3_LINCOMB(Z[0]/(Z[0]-Z[ordercon[0][i]]), x[ordercon[0][i]], -Z[ordercon[0][i]]/(Z[0]-Z[ordercon[0][i]]), x[0], Poly[j])
						j++;
					}
					i = 0;
					if (ordercon[1][i]==0) i++;
					V3_LINCOMB(Z[1]/(Z[1]-Z[ordercon[1][i]]), x[ordercon[1][i]], -Z[ordercon[1][i]]/(Z[1]-Z[ordercon[1][i]]), x[1], Poly[j])
					j++; i++;
					if (ordercon[1][i]==0) i++;
					V3_LINCOMB(Z[1]/(Z[1]-Z[ordercon[1][i]]), x[ordercon[1][i]], -Z[ordercon[1][i]]/(Z[1]-Z[ordercon[1][i]]), x[1], Poly[j])
					j++; i++;
					if (ordercon[1][3]!=-1) { 
						if (ordercon[1][i]==0) i++;	
						V3_LINCOMB(Z[1]/(Z[1]-Z[ordercon[1][i]]), x[ordercon[1][i]], -Z[ordercon[1][i]]/(Z[1]-Z[ordercon[1][i]]), x[1], Poly[j])
						j++;
					}
					ASSERT(j>3 && j<6);
					*Number = j;
					if (*Number>3) DefinePolygon (Poly, *Number);
					return (0);				
				default:
					RETURN(1);
				break;
			}
		case (3):
			switch (count2)
			{
				case (0):
					return(0);
				case (1):
					j = 0;
					V3_COPY(x[3],Poly[j])
					j++;
					i = 0;
					if (ordercon[4][i]==3) i++;
					V3_LINCOMB(-Z[4]/(-Z[4]+Z[ordercon[4][i]]), x[ordercon[4][i]], Z[ordercon[4][i]]/(-Z[4]+Z[ordercon[4][i]]), x[4], Poly[j])
					j++; i++;
					if (ordercon[4][i]==3) i++;
					V3_LINCOMB(-Z[4]/(-Z[4]+Z[ordercon[4][i]]), x[ordercon[4][i]], Z[ordercon[4][i]]/(-Z[4]+Z[ordercon[4][i]]), x[4], Poly[j])
					j++;
					if (ordercon[4][3]!=-1) {
						i++;
						if (ordercon[4][i]==3) i++;
						V3_LINCOMB(-Z[4]/(-Z[4]+Z[ordercon[4][i]]), x[ordercon[4][i]], Z[ordercon[4][i]]/(-Z[4]+Z[ordercon[4][i]]), x[4], Poly[j])
						j++;
					}
					*Number = j;
					if (*Number>3) DefinePolygon (Poly, *Number);
					return(0);
				case (2):
					j = 0;
					i = 0;
					if (ordercon[3][i]==4) i++;
					V3_LINCOMB(-Z[3]/(-Z[3]+Z[ordercon[3][i]]), x[ordercon[3][i]], Z[ordercon[3][i]]/(-Z[3]+Z[ordercon[3][i]]), x[3], Poly[j])
					j++; i++;
					if (ordercon[3][i]==4) i++;
					V3_LINCOMB(-Z[3]/(-Z[3]+Z[ordercon[3][i]]), x[ordercon[3][i]], Z[ordercon[3][i]]/(-Z[3]+Z[ordercon[3][i]]), x[3], Poly[j])
					j++; i++;
					if (ordercon[3][3]!=-1) { 
						if (ordercon[3][i]==4) i++;	
						V3_LINCOMB(-Z[3]/(-Z[3]+Z[ordercon[3][i]]), x[ordercon[3][i]], Z[ordercon[3][i]]/(-Z[3]+Z[ordercon[3][i]]), x[3], Poly[j])
						j++;
					}
					i = 0;
					if (ordercon[4][i]==3) i++;
					V3_LINCOMB(-Z[4]/(-Z[4]+Z[ordercon[4][i]]), x[ordercon[4][i]], Z[ordercon[4][i]]/(-Z[4]+Z[ordercon[4][i]]), x[4], Poly[j])
					j++; i++;
					if (ordercon[4][i]==3) i++;
					V3_LINCOMB(-Z[4]/(-Z[4]+Z[ordercon[4][i]]), x[ordercon[4][i]], Z[ordercon[4][i]]/(-Z[4]+Z[ordercon[4][i]]), x[4], Poly[j])
					j++; i++;
					if (ordercon[4][3]!=-1) { 
						if (ordercon[4][i]==3) i++;	
						V3_LINCOMB(-Z[4]/(-Z[4]+Z[ordercon[4][i]]), x[ordercon[4][i]], Z[ordercon[4][i]]/(-Z[4]+Z[ordercon[4][i]]), x[4], Poly[j])
						j++;
					}
					*Number = j;
					if (*Number>3) DefinePolygon (Poly, *Number);
					return (0);
				default:
					RETURN(1);
			}
		case (4):
			switch (count2)
			{
				case (0):
					return (0);
				case (1):
					j = 0;
					i = 0;
					V3_LINCOMB(-Z[4]/(-Z[4]+Z[ordercon[4][i]]), x[ordercon[4][i]], Z[ordercon[4][i]]/(-Z[4]+Z[ordercon[4][i]]), x[4], Poly[j])
					j++; i++;
					V3_LINCOMB(-Z[4]/(-Z[4]+Z[ordercon[4][i]]), x[ordercon[4][i]], Z[ordercon[4][i]]/(-Z[4]+Z[ordercon[4][i]]), x[4], Poly[j])
					j++; i++;
					V3_LINCOMB(-Z[4]/(-Z[4]+Z[ordercon[4][i]]), x[ordercon[4][i]], Z[ordercon[4][i]]/(-Z[4]+Z[ordercon[4][i]]), x[4], Poly[j])
					j++; i++;
					if (ordercon[4][i]==-1)
						*Number = 3;
					else {
						V3_LINCOMB(-Z[4]/(-Z[4]+Z[ordercon[4][i]]), x[ordercon[4][i]], Z[ordercon[4][i]]/(-Z[4]+Z[ordercon[4][i]]), x[4], Poly[j])
						*Number = 4;
						DefinePolygon (Poly, *Number);
					}
					return (0);				
				default:
					RETURN(1);
			}
		case (5):
			return(0);
		default:
			RETURN(1);
	}
	
	RETURN(1);
}

/****************************************************************************/
/*																			*/
/* Function:  GetPolyElemISCutPlanePRI										*/
/*																			*/
/* Purpose:   get polygon being the intersection of a prism and the	    	*/
/*			  cut plane 													*/
/*																			*/
/* Input :	  all you need													*/
/*																			*/
/* Output:	  DOUBLE_VECTOR *Poly, INT *Number: output polygon				*/
/*																			*/
/* Return:	  INT 0: ok 													*/
/*			  INT 1: an error occurred										*/
/*																			*/
/****************************************************************************/

static INT GetPolyElemISCutPlanePRI (DOUBLE **CornerDC, DOUBLE *CutZCoord, INT NodeOrder, DOUBLE_VECTOR *Poly, INT *Number)
{
	INT i, j, k, count1, count2, count3, order[8], ordercon[8][4];
	DOUBLE *x[MAX_CORNERS_OF_ELEM], Z[MAX_CORNERS_OF_ELEM];
	
	/* get corners of the element side */
	count1 = 0;
	count2 = 0;
	CornerIndexPRI (NodeOrder, order, ordercon);
	for (i=0; i<6; i++) 
	{
		j = order[i];
		x[i] = CornerDC[j]; 
		Z[i] = CutZCoord[j];
		if (Z[i] > SMALL_C)
			count1++;
		if (Z[i] < -SMALL_C)
			count2++;
	}
	
	/* build the polygon */
	count3 = 6 - count1 - count2;
	*Number = count3;
	for (i=0; i < *Number; i++)
		V3_COPY(x[count1+i], Poly[i]);
	if (*Number == 4)
		DefinePolygon (Poly, *Number);
	if (*Number > 2)
		return(0);
	if ((count1 == 0) || (count2 == 0))
		if (count3 < 3) {
			*Number = 0;
			return(0);
		}
	for (k=0; k<count1; k++) 
		for (i=0 ; i<4; i++) {
			if (ordercon[k][i] < count1+count3) continue;
			j = ordercon[k][i];
			V3_LINCOMB(Z[k]/(Z[k]-Z[j]),x[j],-Z[j]/(Z[k]-Z[j]),x[k],Poly[*Number]);
			(*Number)++;
		}
	if (*Number > 3) DefinePolygon (Poly, *Number);
	if (*Number < 3) *Number = 0;

	return (0);
}

/****************************************************************************/
/*																			*/
/* Function:  GetPolyElemISCutPlaneHEX										*/
/*																			*/
/* Purpose:   get polygon being the intersection of an hexahedron and the	*/
/*			  cut plane 													*/
/*																			*/
/* Input :	  all you need													*/
/*																			*/
/* Output:	  DOUBLE_VECTOR *Poly, INT *Number: output polygon				*/
/*																			*/
/* Return:	  INT 0: ok 													*/
/*			  INT 1: an error occurred										*/
/*																			*/
/****************************************************************************/

static INT GetPolyElemISCutPlaneHEX (DOUBLE **CornerDC, DOUBLE *CutZCoord, INT NodeOrder, DOUBLE_VECTOR *Poly, INT *Number)
{
	INT i, j, count1, count2, order[8], ordercon[8][3];
	DOUBLE *x[MAX_CORNERS_OF_ELEM], Z[MAX_CORNERS_OF_ELEM];
	
	/* get corners of the element side */
	count1 = 0;
	count2 = 0;
	CornerIndexHEX (NodeOrder, order, ordercon);
	for (i=0; i<8; i++) 	/* 8=Corners of hexahedron */
	{
		j = order[i];
		x[i] = CornerDC[j]; 
		Z[i] = CutZCoord[j];
		if (Z[i] > SMALL_C)
			count1++;
		if (Z[i] < -SMALL_C)
			count2++;
	}
	
	/* build the polygon */
	*Number = 0;
	switch (count1)
	{
		case (0):
			switch (count2)
			{
				case (0):
				case (1):
				case (2):
				case (3):
					RETURN(1);
				case (4):
					for (i=0; i<4; ++i)
						V3_COPY(x[i], Poly[i])
					*Number = 4;
					DefinePolygon (Poly, *Number);
					return (0);
				case (5):
					RETURN(1);					
				case (6):
				case (7):
				case (8):
					return (0);
			}
		case (1):
			switch (count2)
			{
				case (0):
				case (1):
				case (2):
				case (3):
					RETURN(1);
				case (4):
					V3_COPY(x[1], Poly[0])
					V3_COPY(x[2], Poly[1])
					V3_COPY(x[3], Poly[2])
					*Number = 3;
					return (0);					
				case (5):
					V3_COPY(x[1], Poly[0])
					V3_COPY(x[2], Poly[1])
					for (i=0; i<3; ++i){
						if ((ordercon[0][i]==1)||(ordercon[0][i]==2)) continue;
						V3_LINCOMB(Z[0]/(Z[0]-Z[ordercon[0][i]]), x[ordercon[0][i]], -Z[ordercon[0][i]]/(Z[0]-Z[ordercon[0][i]]), x[0], Poly[2])
					}
					*Number = 3;
					return (0);										
				case (6):
					V3_COPY(x[1], Poly[0])
					j = 0;
					for (i=0; i<3; ++i){
						if ((ordercon[0][i]==1)) continue;
						++j;
						V3_LINCOMB(Z[0]/(Z[0]-Z[ordercon[0][i]]), x[ordercon[0][i]], -Z[ordercon[0][i]]/(Z[0]-Z[ordercon[0][i]]), x[0], Poly[j])
					}
					*Number = 3;
					return (0);										
				case (7):
					for (i=0; i<3; ++i)
						V3_LINCOMB(Z[0]/(Z[0]-Z[ordercon[0][i]]), x[ordercon[0][i]], -Z[ordercon[0][i]]/(Z[0]-Z[ordercon[0][i]]), x[0], Poly[i])
					*Number = 3;
					return (0);
				default:
					RETURN(1);
				break;
			}
		case (2):
			switch (count2)
			{
				case (0):
				case (1):
					RETURN(1);
				case (2):
					V3_COPY(x[2],Poly[0])
					V3_COPY(x[3],Poly[1])
					V3_COPY(x[4],Poly[2])
					V3_COPY(x[5],Poly[3])
					*Number = 4;
					DefinePolygon (Poly, *Number);
					return (0);				
				case (3):
					j = -1;
					for (i=0; i<3; ++i){
						if ((ordercon[0][i]==1)||(ordercon[0][i]==2)||(ordercon[0][i]==3)||(ordercon[0][i]==4)) continue;
						++j;
						V3_LINCOMB(Z[0]/(Z[0]-Z[ordercon[0][i]]), x[ordercon[0][i]], -Z[ordercon[0][i]]/(Z[0]-Z[ordercon[0][i]]), x[0], Poly[j])
					}	
					for (i=0; i<3; ++i){
						if ((ordercon[1][i]==0)||(ordercon[1][i]==2)||(ordercon[1][i]==3)||(ordercon[1][i]==4)) continue;
						++j;
						V3_LINCOMB(Z[1]/(Z[1]-Z[ordercon[1][i]]), x[ordercon[1][i]], -Z[ordercon[1][i]]/(Z[1]-Z[ordercon[1][i]]), x[1], Poly[j])
					}
					++j; V3_COPY(x[2],Poly[j])
					++j; V3_COPY(x[3],Poly[j])
					++j; V3_COPY(x[4],Poly[j])
					*Number = j + 1;
					if (*Number>3) DefinePolygon (Poly, *Number);
					return (0);				
				case (4):
					j = -1;
					for (i=0; i<3; ++i){
						if ((ordercon[0][i]==1)||(ordercon[0][i]==2)||(ordercon[0][i]==3)) continue;
						++j;
						V3_LINCOMB(Z[0]/(Z[0]-Z[ordercon[0][i]]), x[ordercon[0][i]], -Z[ordercon[0][i]]/(Z[0]-Z[ordercon[0][i]]), x[0], Poly[j])
					}	
					for (i=0; i<3; ++i){
						if ((ordercon[1][i]==0)||(ordercon[1][i]==2)||(ordercon[1][i]==3)) continue;
						++j;
						V3_LINCOMB(Z[1]/(Z[1]-Z[ordercon[1][i]]), x[ordercon[1][i]], -Z[ordercon[1][i]]/(Z[1]-Z[ordercon[1][i]]), x[1], Poly[j])
					}
					++j; V3_COPY(x[2],Poly[j])
					++j; V3_COPY(x[3],Poly[j])
					*Number = j + 1;
					if (*Number>3) DefinePolygon (Poly, *Number);
					return (0);				
				case (5):
					j = -1;
					for (i=0; i<3; ++i){
						if ((ordercon[0][i]==1)||(ordercon[0][i]==2)) continue;
						++j;
						V3_LINCOMB(Z[0]/(Z[0]-Z[ordercon[0][i]]), x[ordercon[0][i]], -Z[ordercon[0][i]]/(Z[0]-Z[ordercon[0][i]]), x[0], Poly[j])
					}	
					for (i=0; i<3; ++i){
						if ((ordercon[1][i]==0)||(ordercon[1][i]==2)) continue;
						++j;
						V3_LINCOMB(Z[1]/(Z[1]-Z[ordercon[1][i]]), x[ordercon[1][i]], -Z[ordercon[1][i]]/(Z[1]-Z[ordercon[1][i]]), x[1], Poly[j])
					}
					++j; V3_COPY(x[2],Poly[j])
					*Number = j + 1;
					if (*Number>3) DefinePolygon (Poly, *Number);
					return (0);				
				case (6):
					j = -1;
					for (i=0; i<3; ++i){
						if (ordercon[0][i]==1) continue;
						++j;
						V3_LINCOMB(Z[0]/(Z[0]-Z[ordercon[0][i]]), x[ordercon[0][i]], -Z[ordercon[0][i]]/(Z[0]-Z[ordercon[0][i]]), x[0], Poly[j])
					}	
					for (i=0; i<3; ++i){
						if (ordercon[1][i]==0) continue;
						++j;
						V3_LINCOMB(Z[1]/(Z[1]-Z[ordercon[1][i]]), x[ordercon[1][i]], -Z[ordercon[1][i]]/(Z[1]-Z[ordercon[1][i]]), x[1], Poly[j])
					}
					ASSERT(j==3);
					*Number = 4;
					DefinePolygon (Poly, *Number);
					return (0);				
				default:
					RETURN(1);
				break;
			}
		case (3):
			switch (count2)
			{
				case (0):
				case (1):
				case (2):
					RETURN(1);
				case (3):
				case (4):
				case (5):
					j = -1;
					for (i=0; i<3; ++i){
						if ((ordercon[0][i]==1)||(ordercon[0][i]==2)) continue;
						++j;
						V3_LINCOMB(Z[0]/(Z[0]-Z[ordercon[0][i]]), x[ordercon[0][i]], -Z[ordercon[0][i]]/(Z[0]-Z[ordercon[0][i]]), x[0], Poly[j])
					}
					for (i=0; i<3; ++i){	
						if ((ordercon[1][i]==0)||(ordercon[1][i]==2)) continue;
						++j;
						V3_LINCOMB(Z[1]/(Z[1]-Z[ordercon[1][i]]), x[ordercon[1][i]], -Z[ordercon[1][i]]/(Z[1]-Z[ordercon[1][i]]), x[1], Poly[j])
					}
					for (i=0; i<3; ++i){
						if ((ordercon[2][i]==0)||(ordercon[2][i]==1)) continue;	
						++j;
						V3_LINCOMB(Z[2]/(Z[2]-Z[ordercon[2][i]]), x[ordercon[2][i]], -Z[ordercon[2][i]]/(Z[2]-Z[ordercon[2][i]]), x[2], Poly[j])
					}
					ASSERT(j==4);						
					*Number = 5;
					DefinePolygon (Poly, *Number);
					return (0);
				default:
					RETURN(1);
			}
		case (4):
			switch (count2)
			{
				case (0):
					V3_COPY(x[4], Poly[0])
					V3_COPY(x[5], Poly[1])
					V3_COPY(x[6], Poly[2])
					V3_COPY(x[7], Poly[3])
					*Number = 4;
					DefinePolygon (Poly, *Number);
					return (0);
				case (1):
					j = -1;
					for (i=0; i<3; ++i){
						if ((ordercon[0][i]==1)||(ordercon[0][i]==2)||(ordercon[0][i]==3)||(ordercon[0][i]==4)||(ordercon[0][i]==5)||(ordercon[0][i]==6)) continue;
						++j;
						V3_LINCOMB(Z[0]/(Z[0]-Z[ordercon[0][i]]), x[ordercon[0][i]], -Z[ordercon[0][i]]/(Z[0]-Z[ordercon[0][i]]), x[0], Poly[j])
					}
					for (i=0; i<3; ++i){	
						if ((ordercon[1][i]==0)||(ordercon[1][i]==2)||(ordercon[1][i]==3)||(ordercon[1][i]==4)||(ordercon[1][i]==5)||(ordercon[1][i]==6)) continue;
						++j;
						V3_LINCOMB(Z[1]/(Z[1]-Z[ordercon[1][i]]), x[ordercon[1][i]], -Z[ordercon[1][i]]/(Z[1]-Z[ordercon[1][i]]), x[1], Poly[j])
					}
					for (i=0; i<3; ++i){
						if ((ordercon[2][i]==0)||(ordercon[2][i]==1)||(ordercon[2][i]==3)||(ordercon[2][i]==4)||(ordercon[2][i]==5)||(ordercon[2][i]==6)) continue;	
						++j;
						V3_LINCOMB(Z[2]/(Z[2]-Z[ordercon[2][i]]), x[ordercon[2][i]], -Z[ordercon[2][i]]/(Z[2]-Z[ordercon[2][i]]), x[2], Poly[j])
					}
					for (i=0; i<3; ++i){
						if ((ordercon[3][i]==0)||(ordercon[3][i]==1)||(ordercon[3][i]==2)||(ordercon[3][i]==4)||(ordercon[3][i]==5)||(ordercon[3][i]==6)) continue;	
						++j;
						V3_LINCOMB(Z[3]/(Z[3]-Z[ordercon[3][i]]), x[ordercon[3][i]], -Z[ordercon[3][i]]/(Z[3]-Z[ordercon[3][i]]), x[3], Poly[j])
					}
					++j; V3_COPY(x[4],Poly[j])
					++j; V3_COPY(x[5],Poly[j])
					++j; V3_COPY(x[6],Poly[j])
					*Number = j + 1;
					if (*Number>3) DefinePolygon (Poly, *Number);
					return (0);				
				case (2):
					j = -1;
					for (i=0; i<3; ++i){
						if ((ordercon[0][i]==1)||(ordercon[0][i]==2)||(ordercon[0][i]==3)||(ordercon[0][i]==4)||(ordercon[0][i]==5)) continue;
						++j;
						V3_LINCOMB(Z[0]/(Z[0]-Z[ordercon[0][i]]), x[ordercon[0][i]], -Z[ordercon[0][i]]/(Z[0]-Z[ordercon[0][i]]), x[0], Poly[j])
					}
					for (i=0; i<3; ++i){	
						if ((ordercon[1][i]==0)||(ordercon[1][i]==2)||(ordercon[1][i]==3)||(ordercon[1][i]==4)||(ordercon[1][i]==5)) continue;
						++j;
						V3_LINCOMB(Z[1]/(Z[1]-Z[ordercon[1][i]]), x[ordercon[1][i]], -Z[ordercon[1][i]]/(Z[1]-Z[ordercon[1][i]]), x[1], Poly[j])
					}
					for (i=0; i<3; ++i){
						if ((ordercon[2][i]==0)||(ordercon[2][i]==1)||(ordercon[2][i]==3)||(ordercon[2][i]==4)||(ordercon[2][i]==5)) continue;	
						++j;
						V3_LINCOMB(Z[2]/(Z[2]-Z[ordercon[2][i]]), x[ordercon[2][i]], -Z[ordercon[2][i]]/(Z[2]-Z[ordercon[2][i]]), x[2], Poly[j])
					}
					for (i=0; i<3; ++i){
						if ((ordercon[3][i]==0)||(ordercon[3][i]==1)||(ordercon[3][i]==2)||(ordercon[3][i]==4)||(ordercon[3][i]==5)) continue;	
						++j;
						V3_LINCOMB(Z[3]/(Z[3]-Z[ordercon[3][i]]), x[ordercon[3][i]], -Z[ordercon[3][i]]/(Z[3]-Z[ordercon[3][i]]), x[3], Poly[j])
					}
					++j; V3_COPY(x[4],Poly[j])
					++j; V3_COPY(x[5],Poly[j])
					*Number = j + 1;
					if (*Number>3) DefinePolygon (Poly, *Number);
					return (0);				
				case (3):
					j = -1;
					for (i=0; i<3; ++i){
						if ((ordercon[0][i]==1)||(ordercon[0][i]==2)||(ordercon[0][i]==3)||(ordercon[0][i]==4)) continue;
						++j;
						V3_LINCOMB(Z[0]/(Z[0]-Z[ordercon[0][i]]), x[ordercon[0][i]], -Z[ordercon[0][i]]/(Z[0]-Z[ordercon[0][i]]), x[0], Poly[j])
					}
					for (i=0; i<3; ++i){	
						if ((ordercon[1][i]==0)||(ordercon[1][i]==2)||(ordercon[1][i]==3)||(ordercon[1][i]==4)) continue;
						++j;
						V3_LINCOMB(Z[1]/(Z[1]-Z[ordercon[1][i]]), x[ordercon[1][i]], -Z[ordercon[1][i]]/(Z[1]-Z[ordercon[1][i]]), x[1], Poly[j])
					}
					for (i=0; i<3; ++i){
						if ((ordercon[2][i]==0)||(ordercon[2][i]==1)||(ordercon[2][i]==3)||(ordercon[2][i]==4)) continue;	
						++j;
						V3_LINCOMB(Z[2]/(Z[2]-Z[ordercon[2][i]]), x[ordercon[2][i]], -Z[ordercon[2][i]]/(Z[2]-Z[ordercon[2][i]]), x[2], Poly[j])
					}
					for (i=0; i<3; ++i){
						if ((ordercon[3][i]==0)||(ordercon[3][i]==1)||(ordercon[3][i]==2)||(ordercon[3][i]==4)) continue;	
						++j;
						V3_LINCOMB(Z[3]/(Z[3]-Z[ordercon[3][i]]), x[ordercon[3][i]], -Z[ordercon[3][i]]/(Z[3]-Z[ordercon[3][i]]), x[3], Poly[j])
					}
					++j; V3_COPY(x[4],Poly[j])
					*Number = j + 1;
					DefinePolygon (Poly, *Number);
					return (0);				
				case (4):
					j = -1;
					for (i=0; i<3; ++i){
						if ((ordercon[0][i]==1)||(ordercon[0][i]==2)||(ordercon[0][i]==3)) continue;
						++j;
						V3_LINCOMB(Z[0]/(Z[0]-Z[ordercon[0][i]]), x[ordercon[0][i]], -Z[ordercon[0][i]]/(Z[0]-Z[ordercon[0][i]]), x[0], Poly[j])
					}
					for (i=0; i<3; ++i){	
						if ((ordercon[1][i]==0)||(ordercon[1][i]==2)||(ordercon[1][i]==3)) continue;
						++j;
						V3_LINCOMB(Z[1]/(Z[1]-Z[ordercon[1][i]]), x[ordercon[1][i]], -Z[ordercon[1][i]]/(Z[1]-Z[ordercon[1][i]]), x[1], Poly[j])
					}
					for (i=0; i<3; ++i){
						if ((ordercon[2][i]==0)||(ordercon[2][i]==1)||(ordercon[2][i]==3)) continue;	
						++j;
						V3_LINCOMB(Z[2]/(Z[2]-Z[ordercon[2][i]]), x[ordercon[2][i]], -Z[ordercon[2][i]]/(Z[2]-Z[ordercon[2][i]]), x[2], Poly[j])
					}
					for (i=0; i<3; ++i){
						if ((ordercon[3][i]==0)||(ordercon[3][i]==1)||(ordercon[3][i]==2)) continue;	
						++j;
						V3_LINCOMB(Z[3]/(Z[3]-Z[ordercon[3][i]]), x[ordercon[3][i]], -Z[ordercon[3][i]]/(Z[3]-Z[ordercon[3][i]]), x[3], Poly[j])
					}
					*Number = j + 1;
					DefinePolygon (Poly, *Number);
					return (0);
				default:
					RETURN(1);
			}
		case (5):
			switch (count2)
			{
				case (1):
					j = -1;
					for (i=0; i<3; ++i){
						if ((ordercon[7][i]==6)||(ordercon[7][i]==5)) continue;
						++j;						
						V3_LINCOMB(Z[7]/(Z[7]-Z[ordercon[7][i]]), x[ordercon[7][i]], -Z[ordercon[7][i]]/(Z[7]-Z[ordercon[7][i]]), x[7], Poly[j])
					}
					++j; V3_COPY(x[5],Poly[j]);
					++j; V3_COPY(x[6],Poly[j]);
					*Number = j+1;
					if (*Number>3) DefinePolygon (Poly, *Number);
					return (0);
				case (2):
					j = -1;
					for (i=0; i<3; ++i){
						if ((ordercon[7][i]==6)||(ordercon[7][i]==5)) continue;
						++j;						
						V3_LINCOMB(Z[7]/(Z[7]-Z[ordercon[7][i]]), x[ordercon[7][i]], -Z[ordercon[7][i]]/(Z[7]-Z[ordercon[7][i]]), x[7], Poly[j])
					}
					for (i=0; i<3; ++i){
						if ((ordercon[6][i]==7)||(ordercon[6][i]==5)) continue;
						++j;						
						V3_LINCOMB(Z[6]/(Z[6]-Z[ordercon[6][i]]), x[ordercon[6][i]], -Z[ordercon[6][i]]/(Z[6]-Z[ordercon[6][i]]), x[6], Poly[j])
					}
					++j;
					V3_COPY(x[5],Poly[j]);
					*Number = j+1;
					if (*Number>3) DefinePolygon (Poly, *Number);
					return (0);
				case (3):
					j = -1;
					for (i=0; i<3; ++i){
						if ((ordercon[7][i]==5)||(ordercon[7][i]==6)) continue;
						++j;						
						V3_LINCOMB(Z[7]/(Z[7]-Z[ordercon[7][i]]), x[ordercon[7][i]], -Z[ordercon[7][i]]/(Z[7]-Z[ordercon[7][i]]), x[7], Poly[j])
					}
					for (i=0; i<3; ++i){
						if ((ordercon[6][i]==5)||(ordercon[6][i]==7)) continue;
						++j;						
						V3_LINCOMB(Z[6]/(Z[6]-Z[ordercon[6][i]]), x[ordercon[6][i]], -Z[ordercon[6][i]]/(Z[6]-Z[ordercon[6][i]]), x[6], Poly[j])
					}
					for (i=0; i<3; ++i){
						if ((ordercon[5][i]==6)||(ordercon[5][i]==7)) continue;
						++j;						
						V3_LINCOMB(Z[5]/(Z[5]-Z[ordercon[5][i]]), x[ordercon[5][i]], -Z[ordercon[5][i]]/(Z[5]-Z[ordercon[5][i]]), x[5], Poly[j])
					}
					*Number = j + 1;
					if (*Number>3) DefinePolygon (Poly, *Number);
					return (0);
				default:
					RETURN(1);
			}
		case (6):
			switch (count2)
			{
				case (0):
					return (0);
				case (1):
					j = -1;
					for (i=0; i<3; ++i){
						if ((ordercon[7][i]==6)) continue;
						++j;						
						V3_LINCOMB(Z[7]/(Z[7]-Z[ordercon[7][i]]), x[ordercon[7][i]], -Z[ordercon[7][i]]/(Z[7]-Z[ordercon[7][i]]), x[7], Poly[j])
					}
					++j; V3_COPY(x[6],Poly[j]);
					*Number = j+1;
					if (*Number>3) DefinePolygon (Poly, *Number);
					return (0);
				case (2):
					j = -1;
					for (i=0; i<3; ++i){
						if ((ordercon[7][i]==6)) continue;
						++j;						
						V3_LINCOMB(Z[7]/(Z[7]-Z[ordercon[7][i]]), x[ordercon[7][i]], -Z[ordercon[7][i]]/(Z[7]-Z[ordercon[7][i]]), x[7], Poly[j])
					}
					for (i=0; i<3; ++i){
						if ((ordercon[6][i]==7)) continue;
						++j;						
						V3_LINCOMB(Z[6]/(Z[6]-Z[ordercon[6][i]]), x[ordercon[6][i]], -Z[ordercon[6][i]]/(Z[6]-Z[ordercon[6][i]]), x[6], Poly[j])
					}
					*Number = j + 1;
					if (*Number>3) DefinePolygon (Poly, *Number);
					return (0);										
				default:
					RETURN(1);
			}
		case (7):
			switch (count2)
			{
				case (0):
					return (0);
				case (1):
					j = -1;
					for (i=0; i<3; ++i){
						++j;						
						V3_LINCOMB(Z[7]/(Z[7]-Z[ordercon[7][i]]), x[ordercon[7][i]], -Z[ordercon[7][i]]/(Z[7]-Z[ordercon[7][i]]), x[7], Poly[j])
					}
					*Number = j + 1;
					if (*Number>3) DefinePolygon (Poly, *Number);
					return (0);														
				default:
					RETURN(1);
			}
		case (8):
			return (0);
	}
	
	RETURN(1);
}

/****************************************************************************/
/*
   EW_ElementEval3D - Evaluate geometry of Tetrahedron	

   SYNOPSIS:
   static INT EW_ElementEval3D (ELEMENT *theElement, DRAWINGOBJ *theDO);

   PARAMETERS:
.  theElement - 
.  theDO - 

   DESCRIPTION:
   This function evaluates geometry of Tetrahedron.

   RETURN VALUE:
   INT
.n    0 if ok
.n    1 if error occured.
*/
/****************************************************************************/

static INT EW_ElementEval3D (ELEMENT *theElement, DRAWINGOBJ *theDO)
{
	INT i, j, NodeOrder, n;
	DOUBLE *x[MAX_CORNERS_OF_ELEM], z[MAX_CORNERS_OF_ELEM];
	DOUBLE_VECTOR Polygon[MAX_POINTS_OF_POLY];
	DOUBLE_VECTOR sx[MAX_CORNERS_OF_ELEM], MidPoint;
	INT Viewable[MAX_SIDES_OF_ELEM];
	
	DO_2c(theDO) = DO_NO_INST;

        #ifdef ModelP
	WOP_DObjPnt = theDO;
	#endif
	
	if (!CUT_CutExisting || CUTMODE(theElement)==CM_BEHIND)
	{
		/* plot full element */
		
		/* determine viewable sides */
		for (i=0; i<SIDES_OF_ELEM(theElement); i++)
			Viewable[i] = VIEWABLE(theElement,i);
		
		if (EE3D_ShrinkFactor==1.0)
		{
			if (EE3D_Elem2Plot[PLOT_ALL])
			{
				/* plot only parts lying on the boundary */ 
				if (OBJT(theElement)==BEOBJ)
				{
					for (i=0; i<SIDES_OF_ELEM(theElement); i++)
						if (INNER_SIDE(theElement,i))
							Viewable[i] = 0;
				}
				else
					return (0);
			}
			else
				for (i=0; i<SIDES_OF_ELEM(theElement); i++)
					if (NBELEM(theElement,i) != NULL)
						if (EE3D_Elem2Plot[ECLASS(NBELEM(theElement,i))])
							Viewable[i] = 0;

			/* get coordinates of corners of the element */
			for (i=0; i<CORNERS_OF_ELEM(theElement); i++)
				x[i] = CVECT(MYVERTEX(CORNER(theElement,i)));
		}
		else
		{
			/* get coordinates of corners of the element */
			V3_CLEAR(MidPoint)
			for (i=0; i<CORNERS_OF_ELEM(theElement); i++)
				V3_ADD(CVECT(MYVERTEX(CORNER(theElement,i))),MidPoint,MidPoint)
			V3_SCALE(1.0/CORNERS_OF_ELEM(theElement),MidPoint)
			
			for (i=0; i<CORNERS_OF_ELEM(theElement); i++)
			{
				V3_LINCOMB(EE3D_ShrinkFactor,CVECT(MYVERTEX(CORNER(theElement,i))),1.0-EE3D_ShrinkFactor,MidPoint,sx[i])
				x[i] = sx[i];
			}
		}
	
		/* store viewable sides on drawing obj */
		if (LEVEL(theElement)<EE3D_MaxLevel)
			for (i=0; i<SIDES_OF_ELEM(theElement); i++)
			{
				if (!Viewable[i]) continue;
			    if (EE3D_Property)
				{
				  DO_2c(theDO) = DO_SURRPOLYGON; DO_inc(theDO); 
				  DO_2c(theDO) = CORNERS_OF_SIDE(theElement,i); DO_inc(theDO) 
				  if (LEVEL(theElement)<0 || LEVEL(theElement)>EE3D_NProperty) 
					return (1);
				  DO_2l(theDO) = EE3D_PropertyColor[(int)LEVEL(theElement)]; 
				  DO_inc(theDO);
				}			  
				else if (EE3D_NoColor[COLOR_LOWER_LEVEL])
				{
					DO_2c(theDO) = DO_ERASE_SURRPOLYGON; DO_inc(theDO) 
					DO_2c(theDO) = CORNERS_OF_SIDE(theElement,i); DO_inc(theDO) 
				}
				else
				{
					DO_2c(theDO) = DO_SURRPOLYGON; DO_inc(theDO) 
					DO_2c(theDO) = CORNERS_OF_SIDE(theElement,i); DO_inc(theDO) 
					DO_2l(theDO) = EE3D_Color[COLOR_LOWER_LEVEL]; DO_inc(theDO)
				}
				DO_2l(theDO) = EE3D_Color[COLOR_EDGE]; DO_inc(theDO);
				for (j=0; j<CORNERS_OF_SIDE(theElement,i); j++)
				{
					V3_COPY(x[CORNER_OF_SIDE(theElement,i,j)],DO_2Cp(theDO));
					DO_inc_n(theDO,3);
				}
			}
		else
			for (i=0; i<SIDES_OF_ELEM(theElement); i++)
			{
				if (!Viewable[i]) continue;
				if (EE3D_Property)
				{
				  DO_2c(theDO) = DO_SURRPOLYGON; DO_inc(theDO); 
				  DO_2c(theDO) = CORNERS_OF_SIDE(theElement,i); DO_inc(theDO) 
				  if (LEVEL(theElement)<0 || LEVEL(theElement)>EE3D_NProperty) 
					return (1);
				  DO_2l(theDO) = EE3D_PropertyColor[(int)LEVEL(theElement)]; 
				  DO_inc(theDO);
				}			  
				else if (EE3D_NoColor[ECLASS(theElement)])
				{
					DO_2c(theDO) = DO_ERASE_SURRPOLYGON; DO_inc(theDO) 
					DO_2c(theDO) = CORNERS_OF_SIDE(theElement,i); DO_inc(theDO) 
				}
				else
				{
					DO_2c(theDO) = DO_SURRPOLYGON; DO_inc(theDO) 
					DO_2c(theDO) = CORNERS_OF_SIDE(theElement,i); DO_inc(theDO) 
					DO_2l(theDO) = EE3D_Color[ECLASS(theElement)]; DO_inc(theDO);
				}
				DO_2l(theDO) = EE3D_Color[COLOR_EDGE]; DO_inc(theDO);
				for (j=0; j<CORNERS_OF_SIDE(theElement,i); j++)
				{
					V3_COPY(x[CORNER_OF_SIDE(theElement,i,j)],DO_2Cp(theDO));
					DO_inc_n(theDO,3);
				}
				
				/* inverse if selected */
				if (IsElementSelected(GElem_MG,theElement))
				{
					DO_2c(theDO) = DO_INVERSE_POLYGON; DO_inc(theDO) 
														   DO_2c(theDO) = 3; DO_inc(theDO) 
					for (j=0; j<CORNERS_OF_SIDE(theElement,i); j++)
					{
						V3_COPY(x[CORNER_OF_SIDE(theElement,i,j)],DO_2Cp(theDO));
						DO_inc_n(theDO,3);
					}
				}
			}
	}
	else if (CUTMODE(theElement)==CM_INTERSECT)
	{
		/* plot cutted element */
		
		/* determine viewable sides */
		for (i=0; i<SIDES_OF_ELEM(theElement); i++)
			Viewable[i] = VIEWABLE(theElement,i);
		
		if (EE3D_ShrinkFactor==1.0)
		{
			if (EE3D_Elem2Plot[PLOT_ALL])
			{
				/* only sides lying on the boundary are visible */
				if (OBJT(theElement)==BEOBJ)
				{
					for (i=0; i<SIDES_OF_ELEM(theElement); i++)
						if (INNER_SIDE(theElement,i))
							Viewable[i] = 0;
				}
				else
					for (i=0; i<SIDES_OF_ELEM(theElement); i++)
						Viewable[i] = 0;
			}
			else
				for (i=0; i<SIDES_OF_ELEM(theElement); i++)
					if (NBELEM(theElement,i) != NULL)
						if (EE3D_Elem2Plot[ECLASS(NBELEM(theElement,i))])
							Viewable[i] = 0;
			
			/* get coordinates of corners of the element and their z coordinates in cut system */
			for (i=0; i<CORNERS_OF_ELEM(theElement); i++)
			{
				x[i] = CVECT(MYVERTEX(CORNER(theElement,i)));
				V3_TRAFO4_SC(x[i],CutTrafo,z[i])
			}
		}
		else
		{
			/* get coordinates of corners of the element */
			V3_CLEAR(MidPoint)
			for (i=0; i<CORNERS_OF_ELEM(theElement); i++)
				V3_ADD(CVECT(MYVERTEX(CORNER(theElement,i))),MidPoint,MidPoint)
			V3_SCALE(1.0/CORNERS_OF_ELEM(theElement),MidPoint)
			
			for (i=0; i<CORNERS_OF_ELEM(theElement); i++)
			{
				V3_LINCOMB(EE3D_ShrinkFactor,CVECT(MYVERTEX(CORNER(theElement,i))),1.0-EE3D_ShrinkFactor,MidPoint,sx[i])
				x[i] = sx[i];
				V3_TRAFO4_SC(x[i],CutTrafo,z[i])
			}
		}	
		
		/* get node order */
		NodeOrder = NODE_ORDER(theElement);
        
		/* plot that parts of the viewable sides of the element lying behind cut plane */
		for (i=0; i<SIDES_OF_ELEM(theElement); i++)
		{
			if (!Viewable[i]) continue;
			
			/* determine polygon arising from intersection of triangle with half space behind cut plane */
			switch (TAG(theElement)) {
              case (TETRAHEDRON):
                if (GetPolyElemSideISHalfSpaceTET (theElement,x,z,NodeOrder,i,Polygon,&n))
				  return(1);
                break;
              case (PYRAMID):
                if (GetPolyElemSideISHalfSpacePYR (theElement,x,z,NodeOrder,i,Polygon,&n))
                  return(1);
                break;
              case (PRISM):
                if (GetPolyElemSideISHalfSpacePRI (theElement,x,z,NodeOrder,i,Polygon,&n))
                  return(1);
                break;
              case (HEXAHEDRON):
                if (GetPolyElemSideISHalfSpaceHEX (theElement,x,z,NodeOrder,i,Polygon,&n))
                  return(1);
                break;
              default:
                RETURN(1);
            }
            
			if (n<=2) continue;
			
			/* store on drawing object */
			if (LEVEL(theElement)<EE3D_MaxLevel)
			{
			    if (EE3D_Property)
				{
				  DO_2c(theDO) = DO_SURRPOLYGON; DO_inc(theDO); 
				  DO_2c(theDO) = n; DO_inc(theDO);
				  if (LEVEL(theElement)<0 || LEVEL(theElement)>EE3D_NProperty) 
					return (1);
				  DO_2l(theDO) = EE3D_PropertyColor[(int)LEVEL(theElement)]; 
				  DO_inc(theDO);
				}			  
				else if (EE3D_NoColor[COLOR_LOWER_LEVEL])
				{
					DO_2c(theDO) = DO_ERASE_SURRPOLYGON; DO_inc(theDO) 
					DO_2c(theDO) = n; DO_inc(theDO) 
				}
				else
				{
					DO_2c(theDO) = DO_SURRPOLYGON; DO_inc(theDO) 
					DO_2c(theDO) = n; DO_inc(theDO) 
					DO_2l(theDO) = EE3D_Color[COLOR_LOWER_LEVEL]; DO_inc(theDO);
				}
				DO_2l(theDO) = EE3D_Color[COLOR_EDGE]; DO_inc(theDO);
				for (j=0; j<n; j++)
				{
					V3_COPY(Polygon[j],DO_2Cp(theDO));
					DO_inc_n(theDO,3);
				}
			}
			else
			{
			    if (EE3D_Property)
				{
				  DO_2c(theDO) = DO_SURRPOLYGON; DO_inc(theDO); 
				  DO_2c(theDO) = n; DO_inc(theDO);
				  if (LEVEL(theElement)<0 || LEVEL(theElement)>EE3D_NProperty) 
					return (1);
				  DO_2l(theDO) = EE3D_PropertyColor[(int)LEVEL(theElement)]; 
				  DO_inc(theDO);
				}			  
				else if (EE3D_NoColor[ECLASS(theElement)])
				{
					DO_2c(theDO) = DO_ERASE_SURRPOLYGON; DO_inc(theDO) 
					DO_2c(theDO) = n; DO_inc(theDO) 
				}
				else
				{
					DO_2c(theDO) = DO_SURRPOLYGON; DO_inc(theDO) 
					DO_2c(theDO) = n; DO_inc(theDO) 
					DO_2l(theDO) = EE3D_Color[ECLASS(theElement)]; DO_inc(theDO);
				}
				DO_2l(theDO) = EE3D_Color[COLOR_EDGE]; DO_inc(theDO);
				for (j=0; j<n; j++)
				{
					V3_COPY(Polygon[j],DO_2Cp(theDO));
					DO_inc_n(theDO,3);
				}
				
				/* inverse if selected */
				if (IsElementSelected(GElem_MG,theElement))
				{
					DO_2c(theDO) = DO_INVERSE_POLYGON; DO_inc(theDO) 
					DO_2c(theDO) = n; DO_inc(theDO) 
					for (j=0; j<n; j++)
					{
						V3_COPY(Polygon[j],DO_2Cp(theDO));
						DO_inc_n(theDO,3);
					}
				}
			}
		}
		
		/* plot intersection of element with cut plane if */
		if (CUT_CutAtFront)
		{
			switch (TAG(theElement)) {
              case (TETRAHEDRON):
                if (GetPolyElemISCutPlaneTET(x,z,NodeOrder,Polygon,&n))
                  return(1);
                break;
              case (PYRAMID):
                if (GetPolyElemISCutPlanePYR(x,z,NodeOrder,Polygon,&n))
                  return(1);
                break;
              case (PRISM):
                if (GetPolyElemISCutPlanePRI(x,z,NodeOrder,Polygon,&n))
                  return(1);
                break;
              case (HEXAHEDRON):
                if (GetPolyElemISCutPlaneHEX(x,z,NodeOrder,Polygon,&n))
                  return(1);
                break;
              default:
                RETURN(1);
            }
            
			/* store on drawing object */
			if (n>2)
				if (LEVEL(theElement)<EE3D_MaxLevel)
				{
					switch (EE3D_NoColor[COLOR_LOWER_LEVEL] | (EE3D_NoColor[COLOR_CUT_EDGE]<<1))
					{
						case 0:
							DO_2c(theDO) = DO_SURRPOLYGON; DO_inc(theDO) 
							DO_2c(theDO) = n; DO_inc(theDO) 
							DO_2l(theDO) = EE3D_Color[COLOR_LOWER_LEVEL]; DO_inc(theDO)
							DO_2l(theDO) = EE3D_Color[COLOR_CUT_EDGE]; DO_inc(theDO)
							break;
						case 1:
							DO_2c(theDO) = DO_ERASE_SURRPOLYGON; DO_inc(theDO) 
							DO_2c(theDO) = n; DO_inc(theDO) 
							DO_2l(theDO) = EE3D_Color[COLOR_CUT_EDGE]; DO_inc(theDO)
							break;
						case 2:
							DO_2c(theDO) = DO_POLYGON; DO_inc(theDO) 
							DO_2c(theDO) = n; DO_inc(theDO) 
							DO_2l(theDO) = EE3D_Color[COLOR_LOWER_LEVEL]; DO_inc(theDO)
							break;
						case 3:
							DO_2c(theDO) = DO_ERASE_POLYGON; DO_inc(theDO) 
							DO_2c(theDO) = n; DO_inc(theDO) 
							break;
					}
					for (j=0; j<n; j++)
					{
						V3_COPY(Polygon[j],DO_2Cp(theDO));
						DO_inc_n(theDO,3);
					}
				}
				else
				{
					switch (EE3D_NoColor[ECLASS(theElement)] | (EE3D_NoColor[COLOR_CUT_EDGE]<<1))
					{
						case 0:
							DO_2c(theDO) = DO_SURRPOLYGON; DO_inc(theDO) 
							DO_2c(theDO) = n; DO_inc(theDO) 
							DO_2l(theDO) = EE3D_Color[ECLASS(theElement)]; DO_inc(theDO)
							DO_2l(theDO) = EE3D_Color[COLOR_CUT_EDGE]; DO_inc(theDO)
							break;
						case 1:
							DO_2c(theDO) = DO_ERASE_SURRPOLYGON; DO_inc(theDO) 
							DO_2c(theDO) = n; DO_inc(theDO) 
							DO_2l(theDO) = EE3D_Color[COLOR_CUT_EDGE]; DO_inc(theDO)
							break;
						case 2:
							DO_2c(theDO) = DO_POLYGON; DO_inc(theDO) 
							DO_2c(theDO) = n; DO_inc(theDO) 
							DO_2l(theDO) = EE3D_Color[COLOR_LOWER_LEVEL]; DO_inc(theDO)
							break;
						case 3:
							DO_2c(theDO) = DO_ERASE_POLYGON; DO_inc(theDO) 
							DO_2c(theDO) = n; DO_inc(theDO) 
							break;
					}
					for (j=0; j<n; j++)
					{
						V3_COPY(Polygon[j],DO_2Cp(theDO));
						DO_inc_n(theDO,3);
					}
				
				
					/* inverse if selected */
					if (IsElementSelected(GElem_MG,theElement))
					{
						DO_2c(theDO) = DO_INVERSE_POLYGON; DO_inc(theDO) 
						DO_2c(theDO) = n; DO_inc(theDO) 
						for (j=0; j<n; j++)
						{
							V3_COPY(Polygon[j],DO_2Cp(theDO));
							DO_inc_n(theDO,3);
						}
					}
				}
		}
	}
	DO_2c(theDO) = DO_NO_INST;

        #ifdef ModelP
	WOP_DObjPnt = theDO;
	#endif
	
	return (0);
}

/****************************************************************************/
/*
   EW_ECutBnd3D	-  Evaluate geometry of TetraHedron

   SYNOPSIS:
   static INT EW_ECutBnd3D (ELEMENT *theElement, DRAWINGOBJ *theDO);

   PARAMETERS:
.  theElement -
.  theDO -
 
   DESCRIPTION:
   This function evaluates geometry of TetraHedron.

   RETURN VALUE:
   INT
.n    0 if ok
.n    1 if error occured.
*/
/****************************************************************************/

/****************************************************************************/
/*																			*/
/* Function:  EW_ECutBnd3D													*/
/*																			*/
/* Purpose:   evaluate geometry of TetraHedron								*/
/*																			*/
/* Input:	  ELEMENT *theElement, char *theDrawingObject					*/
/*																			*/
/* Output:	  INT 0: ok 													*/
/*				  1: error													*/
/*																			*/
/****************************************************************************/

static INT EW_ECutBnd3D (ELEMENT *theElement, DRAWINGOBJ *theDO)
{
	INT i, NodeOrder, n;
	DOUBLE *x[MAX_CORNERS_OF_ELEM], z[MAX_CORNERS_OF_ELEM];
	DOUBLE_VECTOR Line[2];
	
	DO_2c(theDO) = DO_NO_INST;

        #ifdef ModelP
	WOP_DObjPnt = theDO;
	#endif
	
	if (CUTMODE(theElement)==CM_INTERSECT && OBJT(theElement)==BEOBJ && CUT_CutAtFront)
	{
		/* get coordinates of corners of the element and their z coordinates in cut system */
		for (i=0; i<CORNERS_OF_ELEM(theElement); i++)
		{
			x[i] = CVECT(MYVERTEX(CORNER(theElement,i)));
			V3_TRAFO4_SC(x[i],CutTrafo,z[i])
		}
		
		/* get node order */
		NodeOrder = NODE_ORDER(theElement);

        /* plot boundary side intersection with cut-plane */
		for (i=0; i<SIDES_OF_ELEM(theElement); i++)
		{
			if (INNER_SIDE(theElement,i)) continue;
			
			/* determine line arising from intersection of side with cut plane */
			switch (TAG(theElement)) {
              case (TETRAHEDRON):
                if (GetLineElemSideISCutPlaneTET(theElement,x,z,NodeOrder,i,Line,&n))
                  return (1);
                break;
              case (PYRAMID):
                if (GetLineElemSideISCutPlaneHEX(theElement,x,z,NodeOrder,i,Line,&n))
                  return (1);
                break;
              case (PRISM):
                if (GetLineElemSideISCutPlaneHEX(theElement,x,z,NodeOrder,i,Line,&n))
                  return (1);
                break;
              case (HEXAHEDRON):
                if (GetLineElemSideISCutPlaneHEX(theElement,x,z,NodeOrder,i,Line,&n))
                  return (1);
                break;
			  default:
                RETURN(1);
            }
            if (n<2) continue;
			
			/* store line on drawing object */
			DO_2c(theDO) = DO_LINE; DO_inc(theDO) 
			DO_2l(theDO) = EE3D_Color[COLOR_CUT_EDGE]; DO_inc(theDO);
			V3_COPY(Line[0],DO_2Cp(theDO));	DO_inc_n(theDO,3);
			V3_COPY(Line[1],DO_2Cp(theDO));	DO_inc_n(theDO,3);
		}
	}
	DO_2c(theDO) = DO_NO_INST;

        #ifdef ModelP
	WOP_DObjPnt = theDO;
	#endif
	
	return (0);
}

/****************************************************************************/
/*
   EW_FindElement3D - Find element in 3D drawing object	

   SYNOPSIS:
   static INT EW_SelectElement3D (DRAWINGOBJ *q);

   PARAMETERS:
.  q - the drawing object

   DESCRIPTION:
   This function finds element in 3D drawing object.

   RETURN VALUE:
   INT
.n    0 if ok
.n    1 if error occured.
*/											
/****************************************************************************/

static INT EW_SelectElement3D (DRAWINGOBJ *q)
{
	INT j, n, end, found;
	DOUBLE help[3];
	COORD_POINT point[MAX_POINTS_OF_POLY];
	
	end = found = 0;
	while (!end)
	{
		switch (DO_2c(q))
		{
			case DO_NO_INST:
				end = 1;
				break;
			case DO_RANGE:
				DO_inc_RANGE(q);
				break;
			case DO_LINE:
				DO_inc_LINE(q,3);
				break;
			case DO_STYLED_LINE:
				DO_inc_STYLED_LINE(q,3);
				break;
			case DO_ARROW:
				DO_inc_ARROW(q,3);
				break;
			case DO_DEPEND:
				DO_inc_DEPEND(q,3);
				break;
			case DO_INVERSE_LINE:
				DO_inc_INVERSE_LINE(q,3);
				break;
			case DO_POLYLINE:
				DO_inc_POLYLINE(q,3);
				break;
			case DO_TEXT:
				DO_inc_TEXT(q,3);
				break;
			case DO_POLYMARK:
				DO_inc_POLYMARK(q,3);
				break;
			case DO_POLYGON:
			case DO_ERASE_SURRPOLYGON:
				DO_inc(q)
				n = DO_2c(q); DO_inc_n(q,2)
				for (j=0; j<n; j++)
				{
					V3_TRAFOM4_V3(DO_2Cp(q),ObsTrafo,help); DO_inc_n(q,3);
					(*OBS_ProjectProc)(help,point+j);
				}
				found |= PointInPolygon(point,j,FE3D_MousePos);
				break;
			case DO_INVERSE_POLYGON:
			case DO_ERASE_POLYGON:
				DO_inc(q)
				n = DO_2c(q); DO_inc(q)
				for (j=0; j<n; j++)
				{
					V3_TRAFOM4_V3(DO_2Cp(q),ObsTrafo,help); DO_inc_n(q,3);
					(*OBS_ProjectProc)(help,point+j);
				}
				found |= PointInPolygon(point,j,FE3D_MousePos);
				break;
			case DO_SURRPOLYGON:
				DO_inc(q)
				n = DO_2c(q); DO_inc_n(q,3)
				for (j=0; j<n; j++)
				{
					V3_TRAFOM4_V3(DO_2Cp(q),ObsTrafo,help); DO_inc_n(q,3);
					(*OBS_ProjectProc)(help,point+j);
				}
				found |= PointInPolygon(point,j,FE3D_MousePos);
				break;
			default:
				return (1);
		}
		
	}
	
	/* if found ... */
	if (found)
	{
		/* put in/delete from selection list */
		if (SELECTIONMODE(WOP_MG)!=elementSelection)
			ClearSelection(WOP_MG); 	
		if (AddElementToSelection(WOP_MG,WOP_Element) == GM_ERROR)
			if (RemoveElementFromSelection(WOP_MG,WOP_Element) == GM_ERROR)
				return (1);
		
		/* plot part lying in front */
		if (EW_ElementEval3D(WOP_Element,WOP_DrawingObject)) 	return (1);
		if (Draw3D(WOP_DrawingObject)) 							return (1);
		WOP_EW_GetNextElementProc	= EW_GetNextElement_vert_fw_up;
		WOP_GEN_ExecuteProc 		= Draw3D;
	}
	
	return (0);
}

/****************************************************************************/
/*
   CalcViewableSides - Determination of viewable tetrahedra sides

   SYNOPSIS:
   static void CalcViewableSides (ELEMENT *theElement);

   PARAMETERS:
.  theElement - 

   DESCRIPTION:
   This function determines viewable tetrahedra sides.

   RETURN VALUE:
   void
   */
/****************************************************************************/

static void CalcViewableSides (ELEMENT *theElement)
{
	DOUBLE_VECTOR Vector, Vector01, Vector02, Vector03, ViewDirection;
	INT Viewablility;
	INT i,j;
	DOUBLE *x[MAX_CORNERS_OF_ELEM], xc[3], xcs[3];
	DOUBLE ScalarPrd;

	/* load corners of the element */
	for( i=0; i<CORNERS_OF_ELEM(theElement); i++)
		x[i] = CVECT(MYVERTEX(CORNER(theElement,i)));
		
	switch (TAG(theElement)) {
		case (TETRAHEDRON):
			/* set view direction */
			if (OBS_Perspective == YES)
			{
				Viewablility = 0;
				for( i=0; i<SIDES_OF_ELEM(theElement); i++ )
				{
					j = CORNER_OF_SIDE(theElement,i,0);
					V3_SUBTRACT(VO_VP(OE_ViewedObj),x[j],ViewDirection)
					V3_SUBTRACT(x[CORNER_OF_SIDE(theElement,i,1)],x[j],Vector01)
					V3_SUBTRACT(x[CORNER_OF_SIDE(theElement,i,2)],x[j],Vector02)
					V3_SUBTRACT(x[CORNER_OPP_TO_SIDE(theElement,i)],x[j],Vector03)
					V3_VECTOR_PRODUCT(Vector01,Vector02,Vector)
					V3_SCALAR_PRODUCT(Vector,Vector03,ScalarPrd)
					if (ScalarPrd>0)
						V3_SCALE(-1.0, Vector)
					if (Vector[0]*ViewDirection[0]+Vector[1]*ViewDirection[1]+Vector[2]*ViewDirection[2]>0)
						Viewablility |= (1<<i);
				}
			}
			else
			{
				V3_SUBTRACT(VO_VP(OE_ViewedObj),VO_VT(OE_ViewedObj),ViewDirection);
				Viewablility = 0;
				for( i=0; i<SIDES_OF_ELEM(theElement); i++ )
				{
					j = CORNER_OF_SIDE(theElement,i,0);
					V3_SUBTRACT(x[CORNER_OF_SIDE(theElement,i,1)], x[j], Vector01)
					V3_SUBTRACT(x[CORNER_OF_SIDE(theElement,i,2)], x[j], Vector02)
					V3_SUBTRACT(x[CORNER_OPP_TO_SIDE(theElement,i)], x[j], Vector03)
					V3_VECTOR_PRODUCT(Vector01, Vector02, Vector)
					V3_SCALAR_PRODUCT(Vector,Vector03,ScalarPrd)
					if (ScalarPrd>0)
						V3_SCALE(-1.0, Vector)
					if (Vector[0]*ViewDirection[0]+Vector[1]*ViewDirection[1]+Vector[2]*ViewDirection[2]>0)
						Viewablility |= (1<<i);
				}
			}
			break;

		case (PYRAMID):
		case (PRISM):
			/* compute center of  mass */
			V3_CLEAR(xc)
			for( i=0; i<CORNERS_OF_ELEM(theElement); i++)
				V3_ADD(x[i],xc,xc)
			V3_SCALE(1.0/CORNERS_OF_ELEM(theElement),xc)

			/* set view direction */
			if (OBS_Perspective == YES)
			{
				Viewablility = 0;
				for( i=0; i<SIDES_OF_ELEM(theElement); i++ )
				{
					/* compute senter of side */
					V3_CLEAR(xcs)
					for (j=0; j<CORNERS_OF_SIDE(theElement,i); ++j)
						V3_ADD(x[CORNER_OF_SIDE(theElement,i,j)],xcs,xcs)
					V3_SCALE(1.0/CORNERS_OF_SIDE(theElement,i),xcs)
					
					V3_SUBTRACT(VO_VP(OE_ViewedObj),xcs,ViewDirection)
					V3_SUBTRACT(x[CORNER_OF_SIDE(theElement,i,0)],xcs,Vector01)
					V3_SUBTRACT(x[CORNER_OF_SIDE(theElement,i,1)],xcs,Vector02)
					V3_SUBTRACT(xc,xcs,Vector03)
					V3_VECTOR_PRODUCT(Vector01,Vector02,Vector)
					V3_SCALAR_PRODUCT(Vector,Vector03,ScalarPrd)
					if (ScalarPrd>0)
						V3_SCALE(-1.0, Vector)
					if (Vector[0]*ViewDirection[0]+Vector[1]*ViewDirection[1]+Vector[2]*ViewDirection[2]>0)
					{
						Viewablility |= (1<<i);
					}
				}
			}
			else
			{
				V3_SUBTRACT(VO_VP(OE_ViewedObj),VO_VT(OE_ViewedObj),ViewDirection);
				Viewablility = 0;
				for( i=0; i<SIDES_OF_ELEM(theElement); i++ )
				{
					/* compute senter of side */
					V3_CLEAR(xcs)
					for (j=0; j<CORNERS_OF_SIDE(theElement,i); ++j)
						V3_ADD(x[CORNER_OF_SIDE(theElement,i,j)],xcs,xcs)
					V3_SCALE(1.0/CORNERS_OF_SIDE(theElement,i),xcs)
					
					V3_SUBTRACT(x[CORNER_OF_SIDE(theElement,i,0)],xcs,Vector01)
					V3_SUBTRACT(x[CORNER_OF_SIDE(theElement,i,1)],xcs,Vector02)
					V3_SUBTRACT(xc,xcs,Vector03)
					V3_VECTOR_PRODUCT(Vector01, Vector02, Vector)
					V3_SCALAR_PRODUCT(Vector,Vector03,ScalarPrd)
					if (ScalarPrd>0)
						V3_SCALE(-1.0, Vector)
					if (Vector[0]*ViewDirection[0]+Vector[1]*ViewDirection[1]+Vector[2]*ViewDirection[2]>0)
					{
						Viewablility |= (1<<i);
					}
				}
			}
			break;

		case (HEXAHEDRON):
			/* compute center of  mass */
			V3_CLEAR(xc)
			for( i=0; i<CORNERS_OF_ELEM(theElement); i++)
				V3_ADD(x[i],xc,xc)
			V3_SCALE(0.125,xc)
				
			/* set view direction */
			if (OBS_Perspective == YES)
			{
				Viewablility = 0;
				for( i=0; i<SIDES_OF_ELEM(theElement); i++ )
				{
					/* compute senter of side */
					V3_CLEAR(xcs)
					for (j=0; j<CORNERS_OF_SIDE(theElement,i); ++j)
						V3_ADD(x[CORNER_OF_SIDE(theElement,i,j)],xcs,xcs)
					V3_SCALE(0.25,xcs)
					
					V3_SUBTRACT(VO_VP(OE_ViewedObj),xcs,ViewDirection)
					V3_SUBTRACT(x[CORNER_OF_SIDE(theElement,i,0)],xcs,Vector01)
					V3_SUBTRACT(x[CORNER_OF_SIDE(theElement,i,1)],xcs,Vector02)
					V3_SUBTRACT(xc,xcs,Vector03)
					V3_VECTOR_PRODUCT(Vector01,Vector02,Vector)
					V3_SCALAR_PRODUCT(Vector,Vector03,ScalarPrd)
					if (ScalarPrd>0)
						V3_SCALE(-1.0, Vector)
					if (Vector[0]*ViewDirection[0]+Vector[1]*ViewDirection[1]+Vector[2]*ViewDirection[2]>0)
					{
						Viewablility |= (1<<i);
					}
				}
			}
			else
			{
				V3_SUBTRACT(VO_VP(OE_ViewedObj),VO_VT(OE_ViewedObj),ViewDirection);
				Viewablility = 0;
				for( i=0; i<SIDES_OF_ELEM(theElement); i++ )
				{
					/* compute senter of side */
					V3_CLEAR(xcs)
					for (j=0; j<CORNERS_OF_SIDE(theElement,i); ++j)
						V3_ADD(x[CORNER_OF_SIDE(theElement,i,j)],xcs,xcs)
					V3_SCALE(0.25,xcs)
					
					V3_SUBTRACT(x[CORNER_OF_SIDE(theElement,i,0)],xcs,Vector01)
					V3_SUBTRACT(x[CORNER_OF_SIDE(theElement,i,1)],xcs,Vector02)
					V3_SUBTRACT(xc,xcs,Vector03)
					V3_VECTOR_PRODUCT(Vector01, Vector02, Vector)
					V3_SCALAR_PRODUCT(Vector,Vector03,ScalarPrd)
					if (ScalarPrd>0)
						V3_SCALE(-1.0, Vector)
					if (Vector[0]*ViewDirection[0]+Vector[1]*ViewDirection[1]+Vector[2]*ViewDirection[2]>0)
					{
						Viewablility |= (1<<i);
					}
				}
			}
			break;

		default:
			UserWriteF("CalcViewableSides() not implemented for elementype=%d\n",TAG(theElement));
			return;
	}

	SETVSIDES(theElement,Viewablility);
}

/****************************************************************************/
/*
   CalcViewableSidesOnGrid - Determination of viewable tetrahedra sides 

   SYNOPSIS:
   static void CalcViewableSidesOnGrid (GRID *theGrid);

   PARAMETERS:
   theGrid - pointer to grid

   DESCRIPTION:
   This function determines viewable tetrahedra sides.

   RETURN VALUE:
   void
   */
/****************************************************************************/

static void CalcViewableSidesOnGrid (GRID *theGrid)
{
	ELEMENT *theElement, *theNeighbor;
	INT i, j;

	/* calc viewable sides for each element */
	for (theElement=FIRSTELEMENT(theGrid); theElement!= NULL; theElement=SUCCE(theElement))
		CalcViewableSides(theElement);
		
	/* make the viewable sides consistent */
	for (theElement=FIRSTELEMENT(theGrid); theElement!= NULL; theElement=SUCCE(theElement))
		for (i=0; i<SIDES_OF_ELEM(theElement); i++)
		{
			if ((theNeighbor=NBELEM(theElement,i))==NULL) continue;
			if (ID(theElement) < ID(theNeighbor))
			{
				for(j=0; j<SIDES_OF_ELEM(theElement); j++)
					if (NBELEM(theNeighbor,j)==theElement)
						break;
				if (VIEWABLE(theElement,i) == VIEWABLE(theNeighbor,j))
					if (VIEWABLE(theElement,i))
						SETVSIDES(theElement,VSIDES(theElement)&(~(1<<i)));
					else
						SETVSIDES(theElement,VSIDES(theElement)|(1<<i));
			}
		}
	
}

/****************************************************************************/
/*
   OrderSons - Order elements with respect to view orientation on all levels

   SYNOPSIS:
   static INT OrderSons (ELEMENT **table,ELEMENT *theElement);

   PARAMETERS:
.  table -
.  theElement -
  
   DESCRIPTION:
   This function orders elements with respect to view orientation on all levels.

   RETURN VALUE:
   INT
.n    0 if ok
.n    1 if error occured.
*/
/****************************************************************************/

static INT OrderSons (ELEMENT **table,ELEMENT *theElement)
{
	INT i, j, Count, nsons;
	INT LastShellBegin, NewShellBegin, ActualPosition;
	ELEMENT *NbElement, *SonElement, *SonList[MAX_SONS];
	
	/* get son list (not stored in element) */
	if (GetSons(theElement,SonList)!=0) return(1);

	/* init list and numbers */
	LastShellBegin = 0;
	ActualPosition = 0;
	nsons = NSONS(theElement);
	for  (i=0; i<nsons; i++)
	{
		SonElement = SonList[i];
		
		/* count how many neighbor-sons are overlapped by SonElement */
		Count = 0;
		for (j=0; j<SIDES_OF_ELEM(SonElement); j++)
		  {
			NbElement = NBELEM(SonElement,j);
			if (NbElement != NULL)
			  if (EFATHER(NbElement)==theElement && (!VIEWABLE(SonElement,j))) Count++;
		  }
		if (Count)
		{
			SETCOUNT(SonElement,Count);
			SETUSED(SonElement,0);
		}
		else
		{
			table[ActualPosition++] = SonElement;
			SETUSED(SonElement,1);
		}
	}
	NewShellBegin = ActualPosition;
	
	/* create list */
	while (ActualPosition<nsons)
	{
		/* create a new shell */
		for (i=LastShellBegin; i<NewShellBegin; i++)
		{
			for (j=0; j<SIDES_OF_ELEM(table[i]); j++)
			{
				if ((NbElement=NBELEM(table[i],j))==NULL) continue;
				if (EFATHER(NbElement)!=theElement || USED(NbElement)) continue;
				if ((Count = COUNT(NbElement)-1)==0)
				{
					table[ActualPosition++] = NbElement;
					SETUSED(NbElement,1);
				}
				else
					SETCOUNT(NbElement,Count);
			}
		}
		
		/* set shell pointers */
		LastShellBegin = NewShellBegin;
		NewShellBegin = ActualPosition;
	}
	return (0);
}

/****************************************************************************/
/*
   CompareTriangles - Test, whether an elements side hides another elements side

   SYNOPSIS:
   static INT CompareTriangles (DOUBLE_VECTOR Triangle[2][3], 
   COORD_POINT ScreenPoint[2][3]);

   PARAMETERS:
.  Triangle[2][3] -
.  ScreenPoint[2][3] - 

   DESCRIPTION:
   This function tests, whether an elements side hides another elements side.

   RETURN VALUE:
   INT
.n     0 if triangles do not hide each other 
.n     1 if triangle 0 hides triangle 1
.n     -1 if triangle 1 hides triangle 0.	
*/
/****************************************************************************/

/* static INT CompareTriangles (DOUBLE_VECTOR Triangle[2][3], COORD_POINT ScreenPoint[2][3]) */
static INT CompareTriangles (DOUBLE_VECTOR Triangle[2][4], COORD_POINT ScreenPoint[2][4])
{
	DOUBLE alpha, beta, z[2];
	DOUBLE lambda[3], rhs[3], M[9], Inverse[9];
	int i, i1, j, j1;

	/* decide if sides of triangle are crossing */
	for (i=0; i<3; i++)
	{
		i1 = (i+1)%3;
		for( j=0; j<3; j++ )
		{
			j1 = (j+1)%3;
			if (!CalcCrossingPoint(ScreenPoint[0][i],ScreenPoint[0][i1],ScreenPoint[1][j], ScreenPoint[1][j1],&alpha,&beta)) continue;
				
			/* crossing found, now decide which hides the other */
			z[0] = (1.0-alpha)*Triangle[0][i][2] + alpha*Triangle[0][i1][2];
			z[1] = (1.0-beta)*Triangle[1][j][2] + beta*Triangle[1][j1][2];
			if (ABS(z[0]-z[1])<SMALL_C) continue;
			if (z[0]>z[1])
				return (1);
			else
				return (-1);
		}
	}
	
	/* now check, if one side contains the other. In that case, the */
	/* covering side also covers the other one's center of mass.    */
	for (i=0; i<2; i++) 
	{
		/* now invert a 3x3 system */
		M[0]=(DOUBLE)ScreenPoint[i][0].x, M[3]=(DOUBLE)ScreenPoint[i][1].x, M[6] =(DOUBLE)ScreenPoint[i][2].x;
		M[1]=(DOUBLE)ScreenPoint[i][0].y, M[4]=(DOUBLE)ScreenPoint[i][1].y, M[7] =(DOUBLE)ScreenPoint[i][2].y;
		M[2]=1.0,						 M[5]=1.0,						  M[8] =1.0;

		if (M3_Invert(Inverse, M)) continue;
		j=1-i;
		rhs[0] =(DOUBLE) ((ScreenPoint[j][0].x + ScreenPoint[j][1].x + ScreenPoint[j][2].x)/3.0);
		rhs[1] =(DOUBLE) ((ScreenPoint[j][0].y + ScreenPoint[j][1].y + ScreenPoint[j][2].y)/3.0);
		rhs[2] = 1.0;
		M3_TIMES_V3(Inverse,rhs,lambda)
		
		/* decide if MidPoint of Triangle[j] lies in the interior of Triangle[i] */
		if (lambda[0]>=0.0 && lambda[1]>=0.0 && lambda[2]>=0.0)
		{
			z[i] = lambda[0]*Triangle[i][0][2] + lambda[1]*Triangle[i][1][2] + lambda[2]*Triangle[i][2][2];
			z[j] = (Triangle[j][0][2] + Triangle[j][1][2] + Triangle[j][2][2])/3.0;
			if (z[i]>z[j])
				return ((i>j)?(-1):(1));
			else
				return ((i>j)?(1):(-1));
		}
	}
	return (0);
}

static INT CompareQuadrilaterals (DOUBLE_VECTOR Triangle[2][4], COORD_POINT ScreenPoint[2][4], INT Corners[2])
{
	DOUBLE alpha, beta, z[2], Xmax[2], Xmin[2], Ymax[2], Ymin[2];
	DOUBLE lambda[3], rhs[3], M[9], Inverse[9];
	int i, i1, j, j1;

	/* decide if sides of triangle are crossing */
	for (i=0; i<Corners[0]; i++)
	{
		i1 = (i+1)%Corners[0];
		for( j=0; j<Corners[1]; j++ )
		{
			j1 = (j+1)%Corners[1];
			if (!CalcCrossingPoint(ScreenPoint[0][i],ScreenPoint[0][i1],ScreenPoint[1][j], ScreenPoint[1][j1],&alpha,&beta)) continue;
				
			/* crossing found, now decide which hides the other */
			z[0] = (1.0-alpha)*Triangle[0][i][2] + alpha*Triangle[0][i1][2];
			z[1] = (1.0-beta)*Triangle[1][j][2] + beta*Triangle[1][j1][2];
			if (ABS(z[0]-z[1])<SMALL_C) continue;
			if (z[0]>z[1])
				return (1);
			else
				return (-1);
		}
	}
	
	/* now check, if one side contains the other. In that case, the */
	/* covering side also covers the other one's center of mass.    */
	
	for (i=0; i<2; i++) 
	{
		Xmax[i]=Xmin[i]=(DOUBLE)ScreenPoint[i][0].x;
		Ymax[i]=Ymin[i]=(DOUBLE)ScreenPoint[i][0].y;
		for (j=0; j<Corners[i]; ++j)
		{
			Xmax[i] = MAX(Xmax[i],(DOUBLE)ScreenPoint[i][j].x);
			Xmin[i] = MIN(Xmin[i],(DOUBLE)ScreenPoint[i][j].x);
			Ymax[i] = MAX(Ymax[i],(DOUBLE)ScreenPoint[i][j].y);
			Ymin[i] = MIN(Ymin[i],(DOUBLE)ScreenPoint[i][j].y);
		}
	}	
	for (i=0; i<2; i++) 
	{
		j=1-i;
		if (((Xmin[i]<=Xmin[j])&&(Xmax[i]>=Xmax[j])) &&
		    ((Ymin[i]<=Ymin[j])&&(Ymax[i]>=Ymax[j]))) 
		{
		/* Now, quadrangle 'i' on the screen contains quadrangle 'j' */
		/* So, we must find the point in quadrangle 'i' where to     */
		/* compare Z-components.                                     */
		/* Do this as in the case of comparing trisngles.            */
		
			M[0]=(DOUBLE)ScreenPoint[i][0].x, M[3]=(DOUBLE)ScreenPoint[i][1].x, M[6] =(DOUBLE)ScreenPoint[i][2].x;
			M[1]=(DOUBLE)ScreenPoint[i][0].y, M[4]=(DOUBLE)ScreenPoint[i][1].y, M[7] =(DOUBLE)ScreenPoint[i][2].y;
			M[2]=1.0,			 M[5]=1.0,			  M[8] =1.0;
		
			if (M3_Invert(Inverse, M)) continue;
			rhs[0] =(DOUBLE) ScreenPoint[j][0].x;
			rhs[1] =(DOUBLE) ScreenPoint[j][0].y;
			rhs[2] = 1.0;
			M3_TIMES_V3(Inverse,rhs,lambda)
			
			z[i] = lambda[0]*Triangle[i][0][2] + lambda[1]*Triangle[i][1][2] + lambda[2]*Triangle[i][2][2];
			z[j] = Triangle[j][0][2];
			if (z[i]>z[j])
				return ((i>j)?(-1):(1));
			else
				return ((i>j)?(1):(-1));
		}
	
	}
	return (0);
}

/****************************************************************************/
/*
   CompareElements - Test, whether a element hides another 

   SYNOPSIS:
   static int CompareElements (const void *ElementHandle0, const void *ElementHandle1);

   PARAMETERS:
.  ElementHandle0 - 
.  ElementHandle1 - 

   DESCRIPTION:
   This function tests, whether a element hides another.

   RETURN VALUE:
   INT
.n     1 whenelement0 hides element1
.n      -1 when element1 hides element0
.n     0 when elements do not hide each other.
*/
/****************************************************************************/

static INT CompareElements (const void *ElementHandle0, 
							const void *ElementHandle1)
{
	ELEMENT *theElement[2];
	INT i, j, k, i1, k1, b0, b1, found, view0, view1, num0, num1, NCorners[2];
	DOUBLE radius[2], norm, alpha, beta[2], scale;
	DOUBLE *Corners[2][8];
	DOUBLE_VECTOR VectorMid[2], ViewDir[2], Vector0, Vector1, Triangle[2][4];
	COORD_POINT ScreenPoints[2][4];
	
	theElement[0] = *((ELEMENT **)ElementHandle0);
	theElement[1] = *((ELEMENT **)ElementHandle1);

	/* test, if elements have a common side */
	for (i=0; i<SIDES_OF_ELEM(theElement[0]); i++)
		if( NBELEM(theElement[0],i) == theElement[1] )
		{
			if (VIEWABLE(theElement[0],i))
				return(-1);
			for (j=0; j<SIDES_OF_ELEM(theElement[1]); j++)
				if( NBELEM(theElement[1],j) == theElement[0] )
					if (VIEWABLE(theElement[1],j))
						return(1);
			return(0);
		}

	/* do some initializing */
	for (j=0; j<2; ++j){
		V3_CLEAR(VectorMid[j])
		for (i=0; i<CORNERS_OF_ELEM(theElement[j]); i++)
		{
			Corners[j][i] = CVECT(MYVERTEX(CORNER(theElement[j],i)));
			V3_ADD(Corners[j][i],VectorMid[j],VectorMid[j])
		}
		scale = (DOUBLE)(1.0/(DOUBLE)CORNERS_OF_ELEM(theElement[j])); 
		V3_SCALE(scale,VectorMid[j])
	}
	
	radius[0] = radius[1] = 0.0;
	for (j=0; j<2; j++)
		for (i=0; i<EDGES_OF_ELEM(theElement[j]); i++)		
		{
			V3_SUBTRACT(Corners[j][CORNER_OF_EDGE(theElement[j],i,0)],Corners[j][CORNER_OF_EDGE(theElement[j],i,1)],Vector0)
			V3_EUKLIDNORM(Vector0,norm)
			radius[j] = MAX(radius[j],norm);
		}
	
	/* check if tetrahedrons are contained in spheres which do not overlap */
	switch(OBS_Perspective)
	{
		case (YES):
			for (i=0; i<2; i++)
			{
				V3_SUBTRACT(VectorMid[i],VO_VP(OE_ViewedObj),ViewDir[i])
				V3_EUKLIDNORM(ViewDir[i],norm);
				beta[i] = SQRT(1.0-radius[i]*radius[i]/norm/norm);
				V3_Normalize(ViewDir[i]);
			}
			V3_SCALAR_PRODUCT(ViewDir[0],ViewDir[1],alpha)
			if (beta[0]*beta[1]-SQRT(1.0-beta[0]*beta[0])*SQRT(1.0-beta[1]*beta[1]) > alpha)
				return(0);
			break;
		case (NO):
			V3_SUBTRACT(VectorMid[0],VectorMid[1],Vector0)
			V3_Orthogonalize(Vector0,OBS_ViewDirection,Vector1);
			V3_EUKLIDNORM(Vector1,norm)
			if (radius[0]+radius[1]<norm)
				return (0);
			break;
		default:
				return (1);
	}
				
	/* determine the viewable sides and its numbers */
	view0  = VSIDES(theElement[0]);
	view1  = VSIDES(theElement[1]);
	num0   = NoOfViewableSides[view0];
	num1   = NoOfViewableSides[view1];
	
	/* use visible or unvisible sides, depending on which are less */
	b0 = (num0>2);
	b1 = (num1>2);
	if (b0) num0 = SIDES_OF_ELEM(theElement[0]) - num0;
	if (b1) num1 = SIDES_OF_ELEM(theElement[1]) - num1;
	
	NCorners[0] = CORNERS_OF_SIDE(theElement[0],0);
	NCorners[1] = CORNERS_OF_SIDE(theElement[1],0);
	
	/* test the tetrahedrons by testing triangles */
	i1=0;
	for (i=0; i<num0; i++)
	{
		/* determine triangle of theElement0 */
		while( ((view0>>i1)&1) == b0) i1++;
		for (j=0; j<CORNERS_OF_SIDE(theElement[0],i1); j++)
		{
			V3_TRAFOM4_V3(Corners[0][CORNER_OF_SIDE(theElement[0],i1,j)],ObsTrafo,Triangle[0][j])
			(*OBS_ProjectProc)(Triangle[0][j],&(ScreenPoints[0][j]));
		}
	
		/* determine triangle of theElement1 and compare triangles */
		k1=0;
		for (k=0; k<num1; k++)
		{
			while ( ((view1>>k1)&1) == b1 ) k1++;
			for (j=0; j<CORNERS_OF_SIDE(theElement[1],k1); j++)
			{
				V3_TRAFOM4_V3(Corners[1][CORNER_OF_SIDE(theElement[1],k1,j)],ObsTrafo,Triangle[1][j])
				(*OBS_ProjectProc)(Triangle[1][j],&(ScreenPoints[1][j]));
			}
			if ((NCorners[0]==3)&&(NCorners[1]==3))
			   	found = CompareTriangles(Triangle,ScreenPoints);
			else
				found = CompareQuadrilaterals (Triangle,ScreenPoints,NCorners);
			if (found)
				return (found);
			k1++;
		}
		i1++;
	}

	return(0);	
}

/****************************************************************************/
/*
   OrderElements - order elements w.r.t. theViewedObject 
  
   SYNOPSIS:
   static INT OrderElements_3D (MULTIGRID *theMG, VIEWEDOBJ *theViewedObj);

   PARAMETERS:
.  theMG - pointer to multigrid 
.  theViewObject -

   DESCRIPTION:
   This function orders elements w.r.t. theViewedObject.

   RETURN VALUE:
   INT
.n     0 if ok
.n     1 if error occured.
*/
/****************************************************************************/

static INT OrderElements_3D (MULTIGRID *theMG, VIEWEDOBJ *theViewedObj)
{
	HEAP *theHeap;
	ELEMENT **table, *theElement;
	GRID *theGrid;
	INT i;

	/* check if multigrid is allready ordered */
	/*.....*/
	
	/* inits */
	OE_ViewedObj = theViewedObj;	
		
	/* calculate the viewable sides of all elements on all levels */
	for (i=0; i<=theMG->topLevel; i++)	
		CalcViewableSidesOnGrid(theMG->grids[i]);
	
	/* allocate memory for the element list on level 0 (but at least for maximal number of sons of an element) */
	theGrid = theMG->grids[0];
	if (theGrid->nElem<1)
	  return(0);
	theHeap = theMG->theHeap;
	Mark(theHeap,FROM_TOP);
	if ( (table=(ELEMENT **)GetMem(theHeap,MAX(theGrid->nElem,MAX_SONS)*sizeof(ELEMENT *),FROM_TOP)) == NULL )
	{
		Release(theHeap,FROM_TOP);
		UserWrite("ERROR: could not allocate memory from the MGHeap\n");
		return (1);
	}
	
	/* order elements on level zero */
	i=0;
	for (theElement=FIRSTELEMENT(theGrid); theElement!= NULL; theElement=SUCCE(theElement))
		table[i++] = theElement;
#	ifndef ModelP
	if (i!=NT(theGrid)) return (1);
#	endif

	SelectionSort((void *)table,i,sizeof(*table),CompareElements);
	if (PutAtEndOfList(theGrid,i,table)!=GM_OK) return (1);	

	/* now order level 1 to toplevel hirarchically */
	for (i=0; i<theMG->topLevel; i++)
	{
		theGrid = theMG->grids[i];
		for (theElement=FIRSTELEMENT(theGrid); theElement!= NULL; theElement=SUCCE(theElement))
		{
			if (NSONS(theElement)<=0) continue;
			OrderSons(table,theElement);
			if (PutAtEndOfList(UPGRID(theGrid),NSONS(theElement),table)!=GM_OK) return (1);
		}
	}
	
	/* release heap */
	Release(theHeap,FROM_TOP);
	
	/* store the view to which elements are ordered */
	/*...*/
	
	return (0);
}

/****************************************************************************/
/*
   OrderNodes - order nodes in elements
  
   SYNOPSIS:
   static INT OrderNodes (MULTIGRID *theMG);

   PARAMETERS:
.  theMG - pointer to multigrid	

   DESCRIPTION:
   This function orders nodes in elements with respect to cutting orientation 
   on all levels.												

   RETURN VALUE:
   INT
.n    0 if ok
.n    1 if error occured.
*/
/****************************************************************************/

static INT OrderNodes (MULTIGRID *theMG, DOUBLE ShrinkFactor)
{
	ELEMENT *theElement;
	DOUBLE z[MAX_CORNERS_OF_ELEM], zm;
	INT i, j, order[MAX_CORNERS_OF_ELEM], imax, jm,jj;
	DOUBLE_VECTOR MidPoint, help;

	/* check if nodes are allready ordered w.r.t current cut */
	/*...*/
	
	/* calculate the node order of all elements on all levels */
	for (j=0; j<=theMG->topLevel; j++)
	{	
		for (theElement=FIRSTELEMENT(theMG->grids[j]); theElement!= NULL; theElement=SUCCE(theElement))
		{
			V3_CLEAR(MidPoint)
			for (i=0; i<CORNERS_OF_ELEM(theElement); i++)
				V3_ADD(CVECT(MYVERTEX(CORNER(theElement,i))),MidPoint,MidPoint)
			V3_SCALE(1.0/CORNERS_OF_ELEM(theElement),MidPoint)
			
			/* calculate Z-coordinates in the cutting VRS of corners */
			if (ShrinkFactor==1.0)
				for (i=0; i<CORNERS_OF_ELEM(theElement); i++)
					V3_TRAFO4_SC(CVECT(MYVERTEX(CORNER(theElement,i))),CutTrafo,z[i])
			else	
				for (i=0; i<CORNERS_OF_ELEM(theElement); i++)
				{
					V3_LINCOMB(ShrinkFactor,CVECT(MYVERTEX(CORNER(theElement,i))),1.0-ShrinkFactor,MidPoint,help)
					V3_TRAFO4_SC(help,CutTrafo,z[i])
				}
			
			if (TAG(theElement)==TETRAHEDRON)
			{
				i = ((z[0]>=z[1])<<5) + ((z[0]>=z[2])<<4) + ((z[0]>=z[3])<<3)
					+ ((z[1]>=z[2])<<2) + ((z[1]>=z[3])<<1) +  (z[2]>=z[3]);
	
				if (OrderIndex[i] == -1)
					return(1);

				/* set flags in element */
				SETNORDER(theElement,OrderIndex[i]);
				if (z[CornerIndex[OrderIndex[i]][0]] < -SMALL_C)
					SETCUTMODE(theElement,CM_BEHIND);
				else if (z[CornerIndex[OrderIndex[i]][3]] > SMALL_C)
					SETCUTMODE(theElement,CM_INFRONT);
				else
					SETCUTMODE(theElement,CM_INTERSECT);
			}
			
			if (TAG(theElement)==HEXAHEDRON || 
				TAG(theElement)==PRISM || 
				TAG(theElement)==PYRAMID)
			{
				/* Sort nodes: z[order[0]] >= z[order[1]] >= ... z[order[7]] */
				for (i=0; i<CORNERS_OF_ELEM(theElement); ++i)
	   				order[i] = i;
	
				for (i=0; i<CORNERS_OF_ELEM(theElement)-1; ++i)
				{
	  				zm = z[order[i]];
	   				for (jj=i; jj<CORNERS_OF_ELEM(theElement); ++jj)
	   				{
	      					zm = MAX(z[order[jj]],zm);
	      					if (zm==z[order[jj]])
	      				 	{
	         					imax = order[jj];
	         					jm = jj;
	     			        	}
	   				}
	   				order[jm] = order[i];
	   				order[i] = imax;
				}
				
				/* set node order */
				if (TAG(theElement) != TETRAHEDRON)
				{
					SET_NODE_ORDER(theElement,0);
					for (i=0; i<CORNERS_OF_ELEM(theElement); ++i)
						SET_NODE_ORDER(theElement,
									   SETBITS(NODE_ORDER(theElement),
											   3*i+2,order[i])); 
				}

				if (z[order[0]] < -SMALL_C)	
					SETCUTMODE(theElement,CM_BEHIND);
				else if (z[order[CORNERS_OF_ELEM(theElement)-1]] > SMALL_C)
					SETCUTMODE(theElement,CM_INFRONT);
				else
					SETCUTMODE(theElement,CM_INTERSECT);	
					
			}
		}
	}

	return(0);
}

/****************************************************************************/
/*
   EW_PreProcess_PlotGrid3D - Initialize input variables of EW_ElementEval3D for GridPlot3D

   SYNOPSIS:
   static INT EW_PreProcess_PlotGrid3D (PICTURE *thePicture, WORK *theWork);

   PARAMETERS:
.  thePicture -
.  theWork -

   DESCRIPTION:
   This function initialize input variables of EW_ElementEval3D for GridPlot3D.

   RETURN VALUE:
   INT
.n    0 if ok
.n    1 if error occured.
*/										
/****************************************************************************/

static INT EW_PreProcess_PlotGrid3D (PICTURE *thePicture, WORK *theWork)
{
	struct GridPlotObj3D *theGpo;
	OUTPUTDEVICE *theOD;
	MULTIGRID *theMG;
	INT i;
	
	theGpo = &(PIC_PO(thePicture)->theGpo);
	theOD  = PIC_OUTPUTDEV(thePicture);
	theMG  = PO_MG(PIC_PO(thePicture));
	
	EE3D_NoColor[COLOR_LOWER_LEVEL] 	= 1;
	EE3D_NoColor[COLOR_CUT_EDGE] 		= 0;	
	if (theGpo->ElemColored == YES)
	{
		EE3D_NoColor[COLOR_COPY]		= 0;	
		EE3D_NoColor[COLOR_IRR] 		= 0;	
		EE3D_NoColor[COLOR_REG] 		= 0;	
		EE3D_NoColor[COLOR_REG] 		= 0;	
	}
	else
	{
		EE3D_NoColor[COLOR_COPY]		= 1;	
		EE3D_NoColor[COLOR_IRR] 		= 1;	
		EE3D_NoColor[COLOR_REG] 		= 1;	
	}
	
	EE3D_Color[COLOR_COPY]			= theOD->yellow;
	EE3D_Color[COLOR_IRR]			= theOD->green;
	EE3D_Color[COLOR_REG]			= theOD->red;
	EE3D_Color[COLOR_LOWER_LEVEL]	= theOD->white;
	EE3D_Color[COLOR_EDGE]			= theOD->black;
	EE3D_Color[COLOR_CUT_EDGE]		= theOD->orange;
		
	EE3D_Elem2Plot[PLOT_ALL]		= 0;
	EE3D_Elem2Plot[PLOT_COPY]		= 0;
	EE3D_Elem2Plot[PLOT_IRR]		= 0;
	EE3D_Elem2Plot[PLOT_REG]		= 0;
	
	EE3D_ShrinkFactor				= theGpo->ShrinkFactor;
	#ifdef ModelP
	{
		GRID *theGrid = GRID_ON_LEVEL(theMG,0);
		NODE *theNode;
		INT  nodes = 0;

		theGrid = GRID_ON_LEVEL(theMG,0);
		EE3D_PartShrinkFactor			= theGpo->PartShrinkFactor;
		if (EE3D_PartShrinkFactor < 1.0)
		{
			nodes = 0;
			V3_CLEAR(EE3D_PartMidPoint)
			for (theNode=PFIRSTNODE(theGrid); theNode!=NULL; theNode=SUCCN(theNode)) {
				V3_ADD(EE3D_PartMidPoint,CVECT(MYVERTEX(theNode)),EE3D_PartMidPoint)
				nodes++;
			}
			if (nodes > 0)
				V3_SCALE(1.0/(DOUBLE)nodes,EE3D_PartMidPoint)
		}
	}
	#endif
	
		
	switch (theGpo->WhichElem)
	{
		case PO_NO:
			break;
		case PO_ALL:
			EE3D_Elem2Plot[PLOT_ALL] = 1;
		case PO_COPY:
			EE3D_Elem2Plot[PLOT_COPY] = 1;
		case PO_IRR:
			EE3D_Elem2Plot[PLOT_IRR] = 1;
		case PO_REG:
			EE3D_Elem2Plot[PLOT_REG] = 1;
	}
	
	/* build cut trafo */
	if (BuildCutTrafo(&(theGpo->theCut),OBS_ViewDirection)) return (1);

	/* order nodes if */
	if (theGpo->theCut.status==ACTIVE)
		if (OrderNodes(theMG,EE3D_ShrinkFactor)) return (1);
	
	/* mark surface elements */
	EE3D_MaxLevel = CURRENTLEVEL(theMG);
	if (MarkElements3D(theMG,0,CURRENTLEVEL(theMG))) return (1);
	
	EE3D_Property = 0;
	if (theGpo->ElemColored==2)
	{
		EE3D_NProperty = MAX(1,TOPLEVEL(theMG));
		if (EE3D_NProperty>0 && EE3D_NProperty<EE_MAX_PROP)
		{
		    EE3D_Property = 1;
			for (i=0; i<=EE3D_NProperty; i++)
			EE3D_PropertyColor[i] = theOD->spectrumStart 
			  + i*(DOUBLE)(theOD->spectrumEnd - theOD->spectrumStart)
				/ (DOUBLE)EE3D_NProperty;
		}
		else
		{
			theGpo->ElemColored = 1;
			UserWrite("wrong NProperty, switch back to standard mode\n");
		}
	}

	return (0);
}

/****************************************************************************/
/*
   ClickAct_Grid3D - toll dependend act on click for 3D Grid

   SYNOPSIS:
   INT ClickAct_Grid3D (PICTURE *pic, INT tool, INT fct, const INT mp[2])

   PARAMETERS:
.  pic  - print info for this picture
.  tool - current tool
.  fct  - currenr tool function
.  mp   - mouse position in window

   DESCRIPTION:
.  heart - select element

   RETURN VALUE:
   INT
.n   0 if ok
.n   __LINE__ if an error occured
*/
/****************************************************************************/

static INT ClickAct_Grid3D (PICTURE *pic, INT tool, INT fct, const INT mp[2])
{
	WORK theWork;
	
	switch (tool)
	{
		case heartTool:
			W_ID(&theWork) = SELECTELEMENT_WORK;
			W_SELECTELEMENT_WORK(&theWork)->PixelX = mp[0];
			W_SELECTELEMENT_WORK(&theWork)->PixelY = mp[1];
			break;
		default:
			return (1);
	}
	if (WorkOnPicture(pic,&theWork))
		return (__LINE__);
	
	return (0);
}
	
/****************************************************************************/
/*
   EW_PreProcess_SelectElement3D - Initialize input variables of EW_ElementEval3D for GridPlot3D

   SYNOPSIS:
   static INT EW_PreProcess_SelectElement3D (PICTURE *thePicture, WORK *theWork);

   PARAMETERS:
.  thePicture -
.  theWork -

   DESCRIPTION:
   This function initializes input variables of EW_ElementEval3D for GridPlot3D.

   RETURN VALUE:
   INT
.n     0 if ok
.n     1 if an error occured.
*/								
/****************************************************************************/

static INT EW_PreProcess_SelectElement3D (PICTURE *thePicture, WORK *theWork)
{
	FE3D_MousePos.x = W_SELECTELEMENT_WORK(theWork)->PixelX;
	FE3D_MousePos.y = W_SELECTELEMENT_WORK(theWork)->PixelY;
	
	if (EW_PreProcess_PlotGrid3D (thePicture,theWork)) return (1);
	
	return (0);
}
	
/****************************************************************************/
/*
   EW_PreProcess_EScalar3D_BackGrid - Initialize for plot of backgrid for 
   C(olor)C(ontour) plot 

   SYNOPSIS:
   static INT EW_PreProcess_EScalar3D_BackGrid (PICTURE *thePicture, 
   WORK *theWork);

   PARAMETERS:
.  thePicture -
.  theWork -

   DESCRIPTION:
   This function initializes for plot of backgrid for C(olor)C(ontour) plot.

   RETURN VALUE:
   INT
.n    0 if ok
.n    1 if an error occured
*/
/****************************************************************************/

static INT EW_PreProcess_EScalar3D_BackGrid (PICTURE *thePicture, WORK *theWork)
{
	struct ElemScalarPlotObj3D *theEspo;
	OUTPUTDEVICE *theOD;
	MULTIGRID *theMG;
	
	theEspo = &(PIC_PO(thePicture)->theEspo);
	theOD  = PIC_OUTPUTDEV(thePicture);
	theMG  = PO_MG(PIC_PO(thePicture));
	
	EE3D_NoColor[COLOR_CUT_EDGE]		= 1;
	EE3D_NoColor[COLOR_LOWER_LEVEL] 	= 1;
	EE3D_NoColor[COLOR_COPY]			= 1;	
	EE3D_NoColor[COLOR_IRR] 			= 1;	
	EE3D_NoColor[COLOR_REG] 			= 1;	
	
	EE3D_Color[COLOR_COPY]			= theOD->yellow;
	EE3D_Color[COLOR_IRR]			= theOD->green;
	EE3D_Color[COLOR_REG]			= theOD->red;
	EE3D_Color[COLOR_LOWER_LEVEL]	= theOD->white;
	EE3D_Color[COLOR_EDGE]			= theOD->black;
	EE3D_Color[COLOR_CUT_EDGE]		= theOD->orange;
		
	EE3D_Elem2Plot[PLOT_ALL]		= 1;
	EE3D_Elem2Plot[PLOT_COPY]		= 1;
	EE3D_Elem2Plot[PLOT_IRR]		= 1;
	EE3D_Elem2Plot[PLOT_REG]		= 1;
	
	EE3D_ShrinkFactor				= 1.0;
	#ifdef ModelP
	EE3D_PartShrinkFactor			= 1.0;
	#endif
	
	/* build cut trafo */
	if (BuildCutTrafo(&(theEspo->theCut),OBS_ViewDirection)) return (1);

	/* order nodes if */
	if (theEspo->theCut.status==ACTIVE)
		if (OrderNodes(theMG,1.0)) return (1);
	
	/* mark suface elements on boundary and cut if */
	if (theEspo->mode == PO_COLOR)
	{
		if (MarkElements_MGS_Bnd(theMG,0,CURRENTLEVEL(theMG))) return (1);
	}
	else
		if (MarkElements_MGS_Bnd_Cut(theMG,0,CURRENTLEVEL(theMG))) return (1);
	
	return (0);
}
	
/****************************************************************************/
/*
   EW_PreProcess_CutBnd3D - Initialize for plot of backgrid for C(olor)C(ontour) plot	

   SYNOPSIS:
   static INT EW_PreProcess_CutBnd3D (PICTURE *thePicture, 
   WORK *theWork);

   PARAMETERS:
.  thePicture -
.  theWork -

   DESCRIPTION:
   This function initializes for plot of backgrid for C(olor)C(ontour) plot.

   RETURN VALUE:
   INT
.n    0 if ok
.n    1 if an error occured
*/
/****************************************************************************/

static INT EW_PreProcess_CutBnd3D (PICTURE *thePicture, WORK *theWork)
{
	struct ElemScalarPlotObj3D *theEspo;
	OUTPUTDEVICE *theOD;
	MULTIGRID *theMG;
	
	theEspo = &(PIC_PO(thePicture)->theEspo);
	theOD  = PIC_OUTPUTDEV(thePicture);
	theMG  = PO_MG(PIC_PO(thePicture));
	
	if (theEspo->theCut.status!=ACTIVE)
		return (1);
	
	EE3D_Color[COLOR_CUT_EDGE]		= theOD->black;
		
	/* build cut trafo */
	if (BuildCutTrafo(&(theEspo->theCut),OBS_ViewDirection)) return (1);

	/* order nodes */
	if (OrderNodes(theMG,1.0)) return (1);
	
	/* mark suface elements on boundary which are cut */
	if (MarkElements_MGS_Bnd_and_Cut(theMG,0,CURRENTLEVEL(theMG))) return (1);
	
	return (0);
}
	
/****************************************************************************/
/*
   EW_PreProcess_EVector3D_BackGrid - Initialize for plot of backgrid for 
   C(olor)C(ontour) plot	

   SYNOPSIS:
   static INT EW_PreProcess_EVector3D_BackGrid (PICTURE *thePicture, 
   WORK *theWork);

   PARAMETERS:
.  thePicture -
.  theWork -

   DESCRIPTION:
   This function initializes for plot of backgrid for C(olor)C(ontour) plot.

   RETURN VALUE: 
   INT
.n    0 if ok
.n    1 if an error occured
*/							
/****************************************************************************/

static INT EW_PreProcess_EVector3D_BackGrid (PICTURE *thePicture, WORK *theWork)
{
	struct ElemVectorPlotObj3D *theEvpo;
	OUTPUTDEVICE *theOD;
	MULTIGRID *theMG;
	
	theEvpo = &(PIC_PO(thePicture)->theEvpo);
	theOD  = PIC_OUTPUTDEV(thePicture);
	theMG  = PO_MG(PIC_PO(thePicture));
	
	EE3D_NoColor[COLOR_CUT_EDGE]		= 1;
	EE3D_NoColor[COLOR_LOWER_LEVEL] 	= 1;
	EE3D_NoColor[COLOR_COPY]			= 1;	
	EE3D_NoColor[COLOR_IRR] 			= 1;	
	EE3D_NoColor[COLOR_REG] 			= 1;	
	
	EE3D_Color[COLOR_COPY]			= theOD->yellow;
	EE3D_Color[COLOR_IRR]			= theOD->green;
	EE3D_Color[COLOR_REG]			= theOD->red;
	EE3D_Color[COLOR_LOWER_LEVEL]	= theOD->white;
	EE3D_Color[COLOR_EDGE]			= theOD->black;
	EE3D_Color[COLOR_CUT_EDGE]		= theOD->orange;
		
	EE3D_Elem2Plot[PLOT_ALL]		= 1;
	EE3D_Elem2Plot[PLOT_COPY]		= 1;
	EE3D_Elem2Plot[PLOT_IRR]		= 1;
	EE3D_Elem2Plot[PLOT_REG]		= 1;
	
	EE3D_ShrinkFactor				= 1.0;
	#ifdef ModelP
	EE3D_PartShrinkFactor			= 1.0;
	#endif

	/* build cut trafo */
	if (BuildCutTrafo(&(theEvpo->theCut),OBS_ViewDirection)) return (1);

	/* order nodes if */
	if (theEvpo->theCut.status==ACTIVE)
		if (OrderNodes(theMG,1.0)) return (1);
	
	/* mark suface elements on boundary and cut */
	if (MarkElements_MGS_Bnd_Cut(theMG,0,CURRENTLEVEL(theMG))) return (1);
	
	return (0);
}
	
/****************************************************************************/
/*
   EW_PreProcess_EScalar3D - Initialize for C(olor)C(ontour) plot

   SYNOPSIS:
   static INT EW_PreProcess_EScalar3D (PICTURE *thePicture, WORK *theWork);

   PARAMETERS:
.  thePicture -
.  theWork -

   DESCRIPTION:
   This function initializes for C(olor)C(ontour) plot.	

   RETURN VALUE:
   INT
.n     0 if ok
.n     1 if en arror occured.
*/
/****************************************************************************/

static INT EW_PreProcess_EScalar3D (PICTURE *thePicture, WORK *theWork)
{
	struct ElemScalarPlotObj3D *theEspo;
	OUTPUTDEVICE *theOD;
	MULTIGRID *theMG;
	INT i;
	
	theEspo = &(PIC_PO(thePicture)->theEspo);
	theOD  = PIC_OUTPUTDEV(thePicture);
	theMG  = PO_MG(PIC_PO(thePicture));

	/* set value->color fct, eval fct */
	if (theEspo->max - theEspo->min < SMALL_D)
		if (W_ID(theWork) != FINDRANGE_WORK)
		{
			UserWrite("maxValue has to be larger than minValue\n");
			return (1);
		}
	
	/* do not plot if cut plane is on the back */
	if (!CUT_CutAtFront) return (1);
	
	EScalar3D_EvalFct	 = theEspo->EvalFct->EvalProc;
	EScalar3D_V2C_factor = (theOD->spectrumEnd - theOD->spectrumStart)/(theEspo->max - theEspo->min);
	EScalar3D_V2C_offset = theOD->spectrumStart - EScalar3D_V2C_factor*theEspo->min;	
	EScalar3D_mode		 = theEspo->mode;
	if (EScalar3D_mode == PO_CONTOURS_EQ)
	{
		EScalar3D_numOfContours = theEspo->numOfContours;
		for (i=0; i<theEspo->numOfContours; i++)
			EScalar3D_ContValues[i] = theEspo->contValues[i];
	}
	EScalar3D_depth 		= theEspo->depth;
	
	/* mark suface elements on boundary */
	if (MarkElements_MGS_Cut(theMG,0,CURRENTLEVEL(theMG))) return (1);
	
	/* prepare evaluation routine */
	if (theEspo->EvalFct->PreprocessProc != NULL)
		if ((*theEspo->EvalFct->PreprocessProc)(PO_NAME(theEspo),theMG)) return (1);

	return (0);
}
	
/****************************************************************************/
/*
   EW_PreProcess_EScalar3D_FR - Initialize for findrange of scalar plot	

   SYNOPSIS:
   static INT EW_PreProcess_EScalar3D_FR (PICTURE *thePicture, WORK *theWork);

   PARAMETERS:
.  thePicture -
.  theWork -

   DESCRIPTION:
   This function initializes for findrange of scalar plot.

   RETURN VALUE:
   INT
.n     0 if ok
.n     1 if en arror occured.
*/
/****************************************************************************/

static INT EW_PreProcess_EScalar3D_FR (PICTURE *thePicture, WORK *theWork)
{
	struct ElemScalarPlotObj3D *theEspo;
	OUTPUTDEVICE *theOD;
	MULTIGRID *theMG;
	
	theEspo = &(PIC_PO(thePicture)->theEspo);
	theOD  = PIC_OUTPUTDEV(thePicture);
	theMG  = PO_MG(PIC_PO(thePicture));

	if (theEspo->theCut.status!=ACTIVE)	return (1);
	
	EScalar3D_EvalFct	 = theEspo->EvalFct->EvalProc;
	EScalar3D_V2C_factor = (theOD->spectrumEnd - theOD->spectrumStart);
	EScalar3D_V2C_offset = theOD->spectrumStart;	
	EScalar3D_mode		 = PO_COLOR;
	EScalar3D_depth 		= theEspo->depth;
	
	/* build cut trafo */
	if (BuildCutTrafo(&(theEspo->theCut),OBS_ViewDirection)) return (1);

	/* order nodes */
	if (OrderNodes(theMG,1.0)) return (1);
	
	/* mark suface elements on boundary */
	if (MarkElements_MGS_Cut(theMG,0,CURRENTLEVEL(theMG))) return (1);
	
	/* prepare evaluation routine */
	if (theEspo->EvalFct->PreprocessProc != NULL)
		if ((*theEspo->EvalFct->PreprocessProc)(PO_NAME(theEspo),theMG)) return (1);

	/* reset min and max values */
	GEN_FR_put = W_FINDRANGE_WORK(theWork)->put;
	GEN_FR_min = MAX_D;
	GEN_FR_max = -MAX_D;	
	
	return (0);
}
	
/****************************************************************************/
/*																			*/
/* Function:  EW_PreProcess_EVector3D										*/
/*																			*/
/* Purpose:   initialize for vector plot 3D 								*/
/*																			*/
/* Input:	  PICTURE *thePicture, WORK *theWork							*/
/*																			*/
/* Return:	  INT 0: ok 													*/
/*			  INT 1: an error occurred										*/
/*																			*/
/****************************************************************************/

static INT EW_PreProcess_EVector3D (PICTURE *thePicture, WORK *theWork)
{
	struct ElemVectorPlotObj3D *theEvpo;
	OUTPUTDEVICE *theOD;
	MULTIGRID *theMG;
	
	theEvpo = &(PIC_PO(thePicture)->theEvpo);
	theOD  = PIC_OUTPUTDEV(thePicture);
	theMG  = PO_MG(PIC_PO(thePicture));

	/* set value->length fct, eval fct */
	if (theEvpo->max < SMALL_D)
		if (W_ID(theWork) != FINDRANGE_WORK)
		{
			UserWrite("maxValue has to be larger than zero\n");
			return (1);
		}
	
	/* do not plot if cut plane is on the back */
	if (!CUT_CutAtFront) return (1);
	
	EVector_rastersize		= theEvpo->RasterSize;
	EVector_cutvector		= theEvpo->CutVector;
	EVector_CutLenFactor    = theEvpo->CutLenFactor;
	EVector3D_projectvector = theEvpo->ProjectVector;
	EVector_EvalFct 		= theEvpo->EvalFct->EvalProc;
	EVector_V2L_factor		= EVector_rastersize/theEvpo->max;				/* scale length of vectors			*/
	EVector3D_V2C_factor	= 0.5*(theOD->spectrumEnd - theOD->spectrumStart); /* transformation from (-1,1) to 	*/
	EVector3D_V2C_offset	= theOD->spectrumStart + EVector3D_V2C_factor;		/* color spectrum					*/
	EVector_ColorCut		= theOD->black;
	
	/* mark suface elements on boundary */
	if (MarkElements_MGS_Cut(theMG,0,CURRENTLEVEL(theMG))) return (1);
	
	/* prepare evaluation routine */
	if (theEvpo->EvalFct->PreprocessProc!=NULL)
		if ((*theEvpo->EvalFct->PreprocessProc)(PO_NAME(theEvpo),theMG)) return (1);;

	return (0);
}
	
/****************************************************************************/
/*																			*/
/* Function:  EW_PreProcess_EVector3D_FR									*/
/*																			*/
/* Purpose:   initialize for findrange of vector plot						*/
/*																			*/
/* Input:	  PICTURE *thePicture, WORK *theWork							*/
/*																			*/
/* Return:	  INT 0: ok 													*/
/*			  INT 1: an error occurred										*/
/*																			*/
/****************************************************************************/

static INT EW_PreProcess_EVector3D_FR (PICTURE *thePicture, WORK *theWork)
{
	if (EW_PreProcess_EVector3D(thePicture,theWork))
		return (1);
	
	/* reset min and max values */
	GEN_FR_put = W_FINDRANGE_WORK(theWork)->put;
	GEN_FR_min = MAX_D;
	GEN_FR_max = -MAX_D;	
	
	return (0);
}

/****************************************************************************/
/*
   PlotColorTriangle3D - Plot on triangle color(3D coord) with depth

   SYNOPSIS:
   static INT PlotColorTriangle3D (ELEMENT *theElement, 
   DOUBLE **CornersOfElem, DOUBLE *TP0, DOUBLE *TP1, DOUBLE *TP2, 
   INT depth, DRAWINGOBJ **theDO);

   PARAMETERS:
.  theElement -
.  CornersOfElem -
.  TP0 -
.  TP1 -
.  TP2 -
.  depth -
.  theDO - the drawing object to draw on

   DESCRIPTION:
   This function plot on triangle color(3D coord) with depth.

   RETURN VALUE:
   INT
.n     0 if ok
.n     1 if en arror occured.
*/
/****************************************************************************/

static INT PlotColorTriangle3D (ELEMENT *theElement, DOUBLE **CornersOfElem, DOUBLE *TP0, DOUBLE *TP1, DOUBLE *TP2, 
DOUBLE *LP0, DOUBLE *LP1, DOUBLE *LP2, INT depth, DRAWINGOBJ **theDO)
{
	DOUBLE_VECTOR EvalPoint, LocalCoord, MP0, MP1, MP2, LMP0, LMP1, LMP2;
	INT i;
	long Color;
	DOUBLE value;

	if (depth<=0)
	{
		/* get values */		
		for (i=0; i<DIM; i++){
			EvalPoint[i] = (TP0[i]+TP1[i]+TP2[i])/3.0;
			LocalCoord[i]= (LP0[i]+LP1[i]+LP2[i])/3.0;
		}
		value = (*EScalar3D_EvalFct)(theElement,
									 (const DOUBLE **)CornersOfElem,LocalCoord);
		Color = (long)(EScalar3D_V2C_factor*value+EScalar3D_V2C_offset);
		Color = MIN(Color,WOP_OutputDevice->spectrumEnd);
		Color = MAX(Color,WOP_OutputDevice->spectrumStart);

		/* draw */
		DO_2c(*theDO) = DO_POLYGON; DO_inc(*theDO) 
		DO_2c(*theDO) = 3; DO_inc(*theDO) 
		DO_2l(*theDO) = Color; DO_inc(*theDO)
		V3_COPY(TP0,DO_2Cp(*theDO)); DO_inc_n(*theDO,3)
		V3_COPY(TP1,DO_2Cp(*theDO)); DO_inc_n(*theDO,3)
		V3_COPY(TP2,DO_2Cp(*theDO)); DO_inc_n(*theDO,3)
		
		/* store range */
		EScalar3D_minValue = MIN(EScalar3D_minValue,value);
		EScalar3D_maxValue = MAX(EScalar3D_maxValue,value);
	}
	else
	{
		/* find corners of subdivided triangles in global and local coord. */
		for (i=0; i<DIM; i++)
		{       /* global */                   /* local */
			MP0[i] = 0.5*(TP0[i]+TP1[i]);  LMP0[i] = 0.5*(LP0[i]+LP1[i]);
			MP1[i] = 0.5*(TP1[i]+TP2[i]);  LMP1[i] = 0.5*(LP1[i]+LP2[i]);
			MP2[i] = 0.5*(TP2[i]+TP0[i]);  LMP2[i] = 0.5*(LP2[i]+LP0[i]);
		}
		if (PlotColorTriangle3D(theElement,CornersOfElem,TP0,MP0,MP2,LP0,LMP0,LMP2,depth-1,theDO)) return (1);	
		if (PlotColorTriangle3D(theElement,CornersOfElem,MP0,TP1,MP1,LMP0,LP1,LMP1,depth-1,theDO)) return (1);	
		if (PlotColorTriangle3D(theElement,CornersOfElem,TP2,MP2,MP1,LP2,LMP2,LMP1,depth-1,theDO)) return (1);	
		if (PlotColorTriangle3D(theElement,CornersOfElem,MP0,MP1,MP2,LMP0,LMP1,LMP2,depth-1,theDO)) return (1);	
	}
	return (0);
}

/****************************************************************************/
/*
   PlotColorQuadrilateral3D - Plot on quadrilateral color(3D coord) with depth	

   SYNOPSIS:
   static INT PlotColorQuadrilateral3D (ELEMENT *theElement,
   DOUBLE **CornersOfElem, DOUBLE *QP0, DOUBLE *QP1, DOUBLE *QP2, 
   DOUBLE *QP3, INT depth, DRAWINGOBJ **theDO);

   PARAMETERS:
.  theElement -
.  CornersOfElem -
.  QP0 -
.  QP1 -
.  QP2 -
.  QP3 -
.  depth -
.  theDO - the drawing object to draw on
 
   DESCRIPTION:
   This function plots on quadrilateral color(3D coord) with depth.	

   RETURN VALUE:
   INT
.n     0 if ok
.n     1 if en arror occured.
*/
/****************************************************************************/

static INT PlotColorQuadrilateral3D (ELEMENT *theElement, DOUBLE **CornersOfElem, DOUBLE *QP0, DOUBLE *QP1, DOUBLE *QP2, DOUBLE *QP3, 
DOUBLE *LQP0, DOUBLE *LQP1, DOUBLE *LQP2, DOUBLE *LQP3,INT depth, DRAWINGOBJ **theDO)
{
	DOUBLE_VECTOR EVP, LEVP, MP0, MP1, MP2, MP3, LMP0, LMP1, LMP2, LMP3;
	INT i;
	long Color;
	DOUBLE value;

	for (i=0; i<DIM; i++){
		EVP[i] = (QP0[i]+QP1[i]+QP2[i]+QP3[i])*0.25;
		LEVP[i] = (LQP0[i]+LQP1[i]+LQP2[i]+LQP3[i])*0.25;
	}
	if (depth<=0)
	{
		/* get values */		
		value = (*EScalar3D_EvalFct)(theElement,
									 (const DOUBLE **)CornersOfElem,LEVP);
		Color = (long)(EScalar3D_V2C_factor*value+EScalar3D_V2C_offset);
		Color = MIN(Color,WOP_OutputDevice->spectrumEnd);
		Color = MAX(Color,WOP_OutputDevice->spectrumStart);

		/* draw */
		DO_2c(*theDO) = DO_POLYGON; DO_inc(*theDO) 
		DO_2c(*theDO) = 4; DO_inc(*theDO) 
		DO_2l(*theDO) = Color; DO_inc(*theDO)
		V3_COPY(QP0,DO_2Cp(*theDO)); DO_inc_n(*theDO,3)
		V3_COPY(QP1,DO_2Cp(*theDO)); DO_inc_n(*theDO,3)
		V3_COPY(QP2,DO_2Cp(*theDO)); DO_inc_n(*theDO,3)
		V3_COPY(QP3,DO_2Cp(*theDO)); DO_inc_n(*theDO,3)

		/* store range */
		EScalar3D_minValue = MIN(EScalar3D_minValue,value);
		EScalar3D_maxValue = MAX(EScalar3D_maxValue,value);
	}
	else
	{
		/* find corners of subdivided quadrilaterals in global and local coord. */
		for (i=0; i<DIM; i++)
		{
			MP0[i] = 0.5*(QP0[i]+QP1[i]); LMP0[i] = 0.5*(LQP0[i]+LQP1[i]);
			MP1[i] = 0.5*(QP1[i]+QP2[i]); LMP1[i] = 0.5*(LQP1[i]+LQP2[i]);
			MP2[i] = 0.5*(QP2[i]+QP3[i]); LMP2[i] = 0.5*(LQP2[i]+LQP3[i]);
			MP3[i] = 0.5*(QP3[i]+QP0[i]); LMP3[i] = 0.5*(LQP3[i]+LQP0[i]);
		}
		if (PlotColorQuadrilateral3D(theElement,CornersOfElem,QP0,MP0,EVP,MP3,LQP0,LMP0,LEVP,LMP3,depth-1,theDO)) return (1);	
		if (PlotColorQuadrilateral3D(theElement,CornersOfElem,MP0,QP1,MP1,EVP,LMP0,LQP1,LMP1,LEVP,depth-1,theDO)) return (1);	
		if (PlotColorQuadrilateral3D(theElement,CornersOfElem,EVP,MP1,QP2,MP2,LEVP,LMP1,LQP2,LMP2,depth-1,theDO)) return (1);	
		if (PlotColorQuadrilateral3D(theElement,CornersOfElem,EVP,MP2,QP3,MP3,LEVP,LMP2,LQP3,LMP3,depth-1,theDO)) return (1);	
	}
	return (0);
}

/****************************************************************************/
/*                                                                          */
/*  PlotColorContourPolygon3D:                                              */
/*  plots color (key=0) or contour (key=1) polygon (n>3)                    */
/*                      by plotting corresponding triangles,                */
/*                      which are obtained  by connecting center of mass of */
/*                      the polygon with its corners.                       */
/*                                                                          */
/****************************************************************************/

static INT PlotColorContourPolygon3D(INT key, INT n, ELEMENT *theElement, DOUBLE **CornersOfElem, DOUBLE_VECTOR Poly[MAX_POINTS_OF_POLY], DOUBLE_VECTOR PolyLoc[MAX_POINTS_OF_POLY],INT depth, DRAWINGOBJ **theDO)
{
	int i;
	DOUBLE CM[3], LCM[3], Tr[3][3], LTr[3][3];
	
	/* center of mass of polygon */
	CM[0]=CM[1]=CM[2]=LCM[0]=LCM[1]=LCM[2]=0.0;
	for (i=0; i<n; ++i){
		V3_ADD(Poly[i], CM, CM)
		V3_ADD(PolyLoc[i],LCM,LCM)
	}
	V3_SCALE(1.0/(DOUBLE)n,CM)
	V3_SCALE(1.0/(DOUBLE)n,LCM)
	
	/* plot each of the triangles */
	for (i=0; i<n; ++i)
	{
		/* define triangle */
		V3_COPY(CM, Tr[0])
		V3_COPY(LCM, LTr[0])
		
		V3_COPY(Poly[i%n], Tr[1])
		V3_COPY(Poly[(i+1)%n], Tr[2])
		
		V3_COPY(PolyLoc[i%n], LTr[1])
		V3_COPY(PolyLoc[(i+1)%n], LTr[2])
		
		/* plot triangle */
		if (key==0){
			if (PlotColorTriangle3D(theElement,CornersOfElem,Tr[0],Tr[1],Tr[2],LTr[0],LTr[1],LTr[2],depth,theDO)) return (1);		
		}
		if (key==1){
			if (PlotContourTriangle3D(theElement,CornersOfElem,Tr[0],Tr[1],Tr[2],LTr[0],LTr[1],LTr[2],depth,theDO)) return (1);
		}
	}
	return (0);
}

/****************************************************************************/
/*
   PointOnLine3D - Cals point between two points with contourValue

   SYNOPSIS:
   static INT PointOnLine3D (DOUBLE contourValue, DOUBLE value0, 
   DOUBLE value1, DOUBLE_VECTOR vec0, DOUBLE_VECTOR vec1, DOUBLE_VECTOR p);

   PARAMETERS:
.  contourValue -
.  value0 -
.  value1 -
.  vec0 -
.  vec1 -
.  p -

   DESCRIPTION:
   This function cals point between two points with contourValue.

   RETURN VALUE:
   INT
.n     0 if ok
.n     1 if en arror occured.
*/
/****************************************************************************/

static INT PointOnLine3D (DOUBLE contourValue, DOUBLE value0, DOUBLE value1, DOUBLE_VECTOR vec0, DOUBLE_VECTOR vec1, DOUBLE_VECTOR p)
{
	DOUBLE alpha;

	if (ABS(value0-value1)<SMALL_D)
	{
		if (value0==contourValue)
		{
			V3_LINCOMB(0.5,vec0,0.5,vec1,p);
			return (1);
		}
		return (0);
	}
	else
	{
		alpha = (contourValue-value0)/(value1-value0);
		if (alpha<0.0 || alpha>=1.0) return (0);
		V3_LINCOMB(1.0-alpha,vec0,alpha,vec1,p);
		return (1);
	}
}

/****************************************************************************/
/*
   PlotContourTriangle3D - Plot on triangle contourlines (3D coord) with depth

   SYNOPSIS:
   static INT PlotContourTriangle3D (ELEMENT *theElement, 
   DOUBLE **CornersOfElem, 
   DOUBLE *TP0, DOUBLE *TP1, DOUBLE *TP2, 
   DOUBLE *LTP0, DOUBLE *LTP1, DOUBLE *LTP2, 
   INT depth, DRAWINGOBJ **theDO);

   PARAMETERS:
.  theElement -
.  CornerOfElem -
.  TP) -
.  TP1 -
.  TP2 -
.  depth -
.  theDO - the drawing object to draw on

   DESCRIPTION:
   This function plots on triangle contourlines (3D coord) with depth.	

   RETURN VALUE:
   INT
.n     0 if ok
.n     1 if en arror occured.
*/
/****************************************************************************/

static INT PlotContourTriangle3D (ELEMENT *theElement, DOUBLE **CornersOfElem, 
								  DOUBLE *TP0, DOUBLE *TP1, DOUBLE *TP2, 
								  DOUBLE *LTP0, DOUBLE *LTP1, DOUBLE *LTP2, 
								  INT depth, DRAWINGOBJ **theDO)
{
	DOUBLE_VECTOR MP0, MP1, MP2, LMP0, LMP1, LMP2,PointMid, Point[3];
	INT i, j, n, min, max;
	long Color;
	DOUBLE v0, v1, v2, vmin, vmax;

	if (depth<=0)
	{
		/* get values at the corners */ 	
		v0	= (*EScalar3D_EvalFct)(theElement,
								   (const DOUBLE **)CornersOfElem,LTP0);
		v1	= (*EScalar3D_EvalFct)(theElement,
								   (const DOUBLE **)CornersOfElem,LTP1);
		v2	= (*EScalar3D_EvalFct)(theElement,
								   (const DOUBLE **)CornersOfElem,LTP2);
		vmin = MIN(v0,v1); vmin = MIN(vmin,v2);
		vmax = MAX(v0,v1); vmax = MAX(vmax,v2);

		/* store range */
		EScalar3D_minValue = MIN(EScalar3D_minValue,vmin);
		EScalar3D_maxValue = MAX(EScalar3D_maxValue,vmax);

		/* find contours to be plotted */
		for (min=0; min<EScalar3D_numOfContours; min++)
			if (EScalar3D_ContValues[min]>=vmin)
				break;
		for (max=EScalar3D_numOfContours-1; max>=0; max--)
			if (EScalar3D_ContValues[max]<=vmax)
				break;

		/* draw contours */
		for (i=min; i<=max; i++)
		{
			/* set color */
			Color = (long)(EScalar3D_V2C_factor*EScalar3D_ContValues[i]+EScalar3D_V2C_offset);
			Color = MIN(Color,WOP_OutputDevice->spectrumEnd);
			Color = MAX(Color,WOP_OutputDevice->spectrumStart);

			/* calculate points on each side of triangle having the right value */
			n=0;
			if (PointOnLine3D(EScalar3D_ContValues[i],v0,v1,TP0,TP1,Point[n])) n++; 
			if (PointOnLine3D(EScalar3D_ContValues[i],v1,v2,TP1,TP2,Point[n])) n++; 
			if (PointOnLine3D(EScalar3D_ContValues[i],v2,v0,TP2,TP0,Point[n])) n++;
			
			/* draw */
			switch (n)
			{
				case 2:
					DO_2c(*theDO) = DO_LINE; DO_inc(*theDO) 
					DO_2l(*theDO) = Color; DO_inc(*theDO)
					V3_COPY(Point[0],DO_2Cp(*theDO)); DO_inc_n(*theDO,3);
					V3_COPY(Point[1],DO_2Cp(*theDO)); DO_inc_n(*theDO,3);
					break;
				case 3:
					DO_2c(*theDO) = DO_POLYLINE; DO_inc(*theDO) 
					DO_2c(*theDO) = 5; DO_inc(*theDO) 
					DO_2l(*theDO) = Color; DO_inc(*theDO);
					for (j=0; j<3; j++)
						PointMid[j] = (Point[0][j]+Point[1][j]+Point[2][j])/3.0;
					V3_COPY(Point[0],DO_2Cp(*theDO)); DO_inc_n(*theDO,3);
					V3_COPY(PointMid,DO_2Cp(*theDO)); DO_inc_n(*theDO,3);
					V3_COPY(Point[1],DO_2Cp(*theDO)); DO_inc_n(*theDO,3);
					V3_COPY(PointMid,DO_2Cp(*theDO)); DO_inc_n(*theDO,3);
					V3_COPY(Point[2],DO_2Cp(*theDO)); DO_inc_n(*theDO,3);
					break;
			}
		}
	}
	else
	{
		/* find corners of subdivided triangles */
		for (i=0; i<DIM; i++)
		{
			MP0[i] = 0.5*(TP0[i]+TP1[i]); LMP0[i] = 0.5*(LTP0[i]+LTP1[i]);
			MP1[i] = 0.5*(TP1[i]+TP2[i]); LMP1[i] = 0.5*(LTP1[i]+LTP2[i]);
			MP2[i] = 0.5*(TP2[i]+TP0[i]); LMP2[i] = 0.5*(LTP2[i]+LTP0[i]);
		}
		if (PlotContourTriangle3D(theElement,CornersOfElem,TP0,MP0,MP2,LTP0,LMP0,LMP2,depth-1,theDO)) return (1);	
		if (PlotContourTriangle3D(theElement,CornersOfElem,MP0,TP1,MP1,LMP0,LTP1,LMP1,depth-1,theDO)) return (1);	
		if (PlotContourTriangle3D(theElement,CornersOfElem,TP2,MP2,MP1,LTP2,LMP2,LMP1,depth-1,theDO)) return (1);	
		if (PlotContourTriangle3D(theElement,CornersOfElem,MP0,MP1,MP2,LMP0,LMP1,LMP2,depth-1,theDO)) return (1);	
	}
	return (0);
}

/****************************************************************************/
/*
   PlotContourQuadrilateral3D - Plot on quadrilateral contourlines (3D coord) with depth

   SYNOPSIS:
   static INT PlotContourQuadrilateral3D (ELEMENT *theElement, 
   DOUBLE **CornersOfElem, DOUBLE *QP0, DOUBLE *QP1, DOUBLE *QP2, 
   DOUBLE *QP3, INT depth, DRAWINGOBJ **theDO);

   PARAMETERS:
.  theElement -
.  CornersOfElem -
.  QP1 -
.  QP2 -
.  QP3 -
.  depth -
.  theDO - the drawing object to draw on
 
   DESCRIPTION:
   This function plots on quadrilateral contourlines (3D coord) with depth. 

   RETURN VALUE:
   INT
.n     0 if ok
.n     1 if en arror occured.
*/
/****************************************************************************/

static INT PlotContourQuadrilateral3D (ELEMENT *theElement, DOUBLE **CornersOfElem, DOUBLE *QP0, DOUBLE *QP1, DOUBLE *QP2, DOUBLE *QP3, INT depth, DRAWINGOBJ **theDO)
{
	DOUBLE_VECTOR EVP, LocalCoord, MP0, MP1, MP2, MP3, PointMid, Point[4];
	INT i, j, n, min, max, coe;
	long Color;
	DOUBLE v0, v1, v2, v3, vmin, vmax;

	coe = CORNERS_OF_ELEM(theElement);
	for (i=0; i<DIM; i++)
		EVP[i] = (QP0[i]+QP1[i]+QP2[i]+QP3[i])*0.25;
	if (depth<=0)
	{
		/* get values at the corners */ 	
		if (UG_GlobalToLocal(coe,(const DOUBLE **)CornersOfElem,QP0,LocalCoord))
		  return (1);
		v0	= (*EScalar3D_EvalFct)(theElement,
								   (const DOUBLE **)CornersOfElem,LocalCoord);
		if (UG_GlobalToLocal(coe,(const DOUBLE **)CornersOfElem,QP1,LocalCoord))
		  return (1);
		v1	= (*EScalar3D_EvalFct)(theElement,
								   (const DOUBLE **)CornersOfElem,LocalCoord);
		if (UG_GlobalToLocal(coe,(const DOUBLE **)CornersOfElem,QP2,LocalCoord))
		  return (1);
		v2	= (*EScalar3D_EvalFct)(theElement,
								   (const DOUBLE **)CornersOfElem,LocalCoord);
		if (UG_GlobalToLocal(coe,(const DOUBLE **)CornersOfElem,QP3,LocalCoord))
		  return (1);
		v3	= (*EScalar3D_EvalFct)(theElement,
								   (const DOUBLE **)CornersOfElem,LocalCoord);
		vmin = MIN(v0,v1); vmin = MIN(vmin,v2); vmin = MIN(vmin,v3);
		vmax = MAX(v0,v1); vmax = MAX(vmax,v2); vmax = MAX(vmin,v3);

		/* store range */
		EScalar3D_minValue = MIN(EScalar3D_minValue,vmin);
		EScalar3D_maxValue = MAX(EScalar3D_maxValue,vmax);

		/* find contours to be plotted */
		for (min=0; min<EScalar3D_numOfContours; min++)
			if (EScalar3D_ContValues[min]>=vmin)
				break;
		for (max=EScalar3D_numOfContours-1; max>=0; max--)
			if (EScalar3D_ContValues[max]<=vmax)
				break;

		/* draw contours */
		for (i=min; i<=max; i++)
		{
			/* set color */
			Color = (long)(EScalar3D_V2C_factor*EScalar3D_ContValues[i]+EScalar3D_V2C_offset);
			Color = MIN(Color,WOP_OutputDevice->spectrumEnd);
			Color = MAX(Color,WOP_OutputDevice->spectrumStart);

			/* calculate points on each side of triangle having the right value */
			n=0;
			if (PointOnLine3D(EScalar3D_ContValues[i],v0,v1,QP0,QP1,Point[n])) n++; 
			if (PointOnLine3D(EScalar3D_ContValues[i],v1,v2,QP1,QP2,Point[n])) n++; 
			if (PointOnLine3D(EScalar3D_ContValues[i],v2,v3,QP2,QP3,Point[n])) n++;
			if (PointOnLine3D(EScalar3D_ContValues[i],v3,v0,QP3,QP0,Point[n])) n++;
			
			/* draw */
			switch (n)
			{
				case 2:
					DO_2c(*theDO) = DO_LINE; DO_inc(*theDO) 
					DO_2l(*theDO) = Color; DO_inc(*theDO);
					V3_COPY(Point[0],DO_2Cp(*theDO)); DO_inc_n(*theDO,3);
					V3_COPY(Point[1],DO_2Cp(*theDO)); DO_inc_n(*theDO,3);
					break;
				case 3:
					DO_2c(*theDO) = DO_POLYLINE; DO_inc(*theDO) 
					DO_2c(*theDO) = 5; DO_inc(*theDO) 
					DO_2l(*theDO) = Color; DO_inc(*theDO);
					for (j=0; j<3; j++)
						PointMid[j] = (Point[0][j]+Point[1][j]+Point[2][j])/3.0;
					V3_COPY(Point[0],DO_2Cp(*theDO)); DO_inc_n(*theDO,3);
					V3_COPY(PointMid,DO_2Cp(*theDO)); DO_inc_n(*theDO,3);
					V3_COPY(Point[1],DO_2Cp(*theDO)); DO_inc_n(*theDO,3);
					V3_COPY(PointMid,DO_2Cp(*theDO)); DO_inc_n(*theDO,3);
					V3_COPY(Point[2],DO_2Cp(*theDO)); DO_inc_n(*theDO,3);
					break;
				case 4:
					DO_2c(*theDO) = DO_LINE; DO_inc(*theDO) 
					DO_2l(*theDO) = Color; DO_inc(*theDO);
					V3_COPY(Point[0],DO_2Cp(*theDO)); DO_inc_n(*theDO,3);
					V3_COPY(Point[2],DO_2Cp(*theDO)); DO_inc_n(*theDO,3);
					
					DO_2c(*theDO) = DO_LINE; DO_inc(*theDO) 
					DO_2l(*theDO) = Color; DO_inc(*theDO);
					V3_COPY(Point[1],DO_2Cp(*theDO)); DO_inc_n(*theDO,3);
					V3_COPY(Point[3],DO_2Cp(*theDO)); DO_inc_n(*theDO,3);
			}
		}
	}
	else
	{
		/* find corners of subdivided quadrilaterals */
		for (i=0; i<DIM; i++)
		{
			MP0[i] = 0.5*(QP0[i]+QP1[i]);
			MP1[i] = 0.5*(QP1[i]+QP2[i]);
			MP2[i] = 0.5*(QP2[i]+QP3[i]);
			MP3[i] = 0.5*(QP3[i]+QP0[i]);
		}
		if (PlotContourQuadrilateral3D(theElement,CornersOfElem,QP0,MP0,EVP,MP3,depth-1,theDO)) return (1); 
		if (PlotContourQuadrilateral3D(theElement,CornersOfElem,MP0,QP1,MP1,EVP,depth-1,theDO)) return (1); 
		if (PlotContourQuadrilateral3D(theElement,CornersOfElem,EVP,MP1,QP2,MP2,depth-1,theDO)) return (1); 
		if (PlotContourQuadrilateral3D(theElement,CornersOfElem,EVP,MP2,QP3,MP3,depth-1,theDO)) return (1); 
	}
	return (0);
}

/****************************************************************************/
/*
   EW_EScalar3D	- Initialize for C(olor)C(ontour) plot

   SYNOPSIS:
   static INT EW_EScalar3D (ELEMENT *theElement, DRAWINGOBJ *theDO);

   PARAMETERS:
.  theElement -
.  theDO -

   DESCRIPTION:
   This function initializes for C(olor)C(ontour) plot.	

   RETURN VALUE:
   INT
.n     0 if ok
.n     1 if en arror occured.
*/
/****************************************************************************/

static INT EW_EScalar3D (ELEMENT *theElement, DRAWINGOBJ *theDO)
{
	INT i, n, NodeOrder;
	DOUBLE_VECTOR Poly[MAX_POINTS_OF_POLY], PolyLoc[MAX_POINTS_OF_POLY];
	DOUBLE *x[MAX_CORNERS_OF_ELEM], z[MAX_CORNERS_OF_ELEM];
	DRAWINGOBJ *range;
	
	DO_2c(theDO) = DO_NO_INST;

        #ifdef ModelP
	WOP_DObjPnt = theDO;
	#endif
	
	/* get node order */
	NodeOrder = NODE_ORDER(theElement);

	/* get coordinates of corners of the element and their z coordinates in cut system */
	for (i=0; i<CORNERS_OF_ELEM(theElement); i++)
	{
		x[i] = CVECT(MYVERTEX(CORNER(theElement,i)));
		V3_TRAFO4_SC(x[i],CutTrafo,z[i])
	}
		
	/* determine polygon being intersection of element wth cut plane */
   	switch (TAG(theElement)) {
      case (TETRAHEDRON):
        if (GetPolyElemISCutPlaneTET(x,z,NodeOrder,Poly,&n))
          return(1);
        break;
      case (PYRAMID):
        if (GetPolyElemISCutPlanePYR(x,z,NodeOrder,Poly,&n))
          return(1);
        break;
      case (PRISM):
        if (GetPolyElemISCutPlanePRI(x,z,NodeOrder,Poly,&n))
          return(1);
        break;
      case (HEXAHEDRON):
        if (GetPolyElemISCutPlaneHEX(x,z,NodeOrder,Poly,&n))
          return(1);
        break;
      default:
        RETURN(1);
    }
    
	if (n<=2) return (0);
	
	/* draw polygon with depth */
	EScalar3D_minValue = MAX_D; EScalar3D_maxValue = -MAX_D; 
    DO_2c(theDO) = DO_RANGE; DO_inc(theDO); range = theDO; DO_inc_n(theDO,2);

	/* Find local coordinates of polygon verticies */
	for (i=0; i<n; ++i)
	  if (UG_GlobalToLocal(CORNERS_OF_ELEM(theElement),
						  (const DOUBLE **)x,Poly[i],PolyLoc[i])) 
		PrintErrorMessage('W',"EW_EScalar3D",
                          "could not compute global coordinates");
	
	switch (EScalar3D_mode)
	{
		case PO_COLOR:
			if (n==TRIANGLE)
			{
              if (PlotColorTriangle3D(theElement,x,Poly[0],Poly[1],Poly[2],PolyLoc[0],PolyLoc[1],PolyLoc[2],EScalar3D_depth,&theDO)) return (1);
			}
			else if (n==4)
			{
              if (PlotColorQuadrilateral3D(theElement,x,Poly[0],Poly[1],Poly[2],Poly[3],PolyLoc[0],PolyLoc[1],PolyLoc[2],PolyLoc[3],EScalar3D_depth,&theDO)) return (1);
			}
            else if ((n==5)||(n==6))
			{
				if (PlotColorContourPolygon3D(0, n, theElement, x, Poly, PolyLoc,EScalar3D_depth,&theDO))   return (1);
			}
			break;
		case PO_CONTOURS_EQ:
			if (n==TRIANGLE)
			{
              if (PlotContourTriangle3D(theElement,x,Poly[0],Poly[1],Poly[2],PolyLoc[0],PolyLoc[1],PolyLoc[2],EScalar3D_depth,&theDO)) return (1);
			}
            else if ((n==4)||(n==5)||(n==6))
			{
				if (PlotColorContourPolygon3D(1, n, theElement, x, Poly, PolyLoc,EScalar3D_depth,&theDO))   return (1);
			}
			break;
		default:
			return (1);
	}
	
	DO_2c(theDO) = DO_NO_INST;

        #ifdef ModelP
	WOP_DObjPnt = theDO;
	#endif

	DO_2C(range) = EScalar3D_minValue; DO_inc(range);
	DO_2C(range) = EScalar3D_maxValue;
		
	return (0);
}

/****************************************************************************/
/*
   FindRasterPoints3D - Find rasterpoints in 3D	

   SYNOPSIS:
   static INT FindRasterPoints3D (DOUBLE RasterSize, DOUBLE_VECTOR *Polygon, 
   INT Number, DOUBLE_VECTOR *RasterPoints, INT *RPNumber);

   PARAMETERS:
.  Rastersize -
.  Polygon -
.  Number -
.  RasterPoints -
.  RPNumber -

   DESCRIPTION:
   This function finds rasterpoints in 3D.

   RETURN VALUE:
   INT
.n     0 if ok
.n     1 if en arror occured.
*/
/****************************************************************************/

static INT FindRasterPoints3D (DOUBLE RasterSize, DOUBLE_VECTOR *Polygon, INT Number, DOUBLE_VECTOR *RasterPoints, INT *RPNumber)
{
	INT i, j, k, i0, i1, j0, j1, c0, c1;
	DOUBLE xmin, xmax, ymin, ymax;
	DOUBLE diff[MAX_POINTS_OF_POLY][2], test[2];
	
	*RPNumber = 0;
	if (Number<2) return (0);
	
	xmin = ymin = MAX_C;
	xmax = ymax = -MAX_C;
	for (i=0; i<Number; i++)
	{
		xmin = MIN(xmin,Polygon[i][0]);
		xmax = MAX(xmax,Polygon[i][0]);
		ymin = MIN(ymin,Polygon[i][1]);
		ymax = MAX(ymax,Polygon[i][1]);
		diff[i][0] = Polygon[(i+1)%Number][0] - Polygon[i][0];
		diff[i][1] = Polygon[(i+1)%Number][1] - Polygon[i][1];
	}
	i0 = (INT)ceil(xmin/RasterSize);
	i1 = (INT)floor(xmax/RasterSize);
	j0 = (INT)ceil(ymin/RasterSize);
	j1 = (INT)floor(ymax/RasterSize);
	
	for (i=i0; i<=i1; i++)
		for (j=j0; j<=j1; j++)
		{
			c0 = c1 = 0;
			for (k=0; k<Number; k++)
			{
				test[0] = RasterSize*(DOUBLE)(i) - Polygon[k][0];
				test[1] = RasterSize*(DOUBLE)(j) - Polygon[k][1];
				if (diff[k][0]*test[1]>=diff[k][1]*test[0]) c0++;
				if (diff[k][0]*test[1]<=diff[k][1]*test[0]) c1++;
			}
			if (c0==Number || c1==Number)
			{
				RasterPoints[*RPNumber][0] = RasterSize*(DOUBLE)(i);
				RasterPoints[*RPNumber][1] = RasterSize*(DOUBLE)(j);
				RasterPoints[(*RPNumber)++][2] = 0.0;
			}
			if (*RPNumber==RASTERPOINTS_MAX)
				return (0);
		}
	
	return (0);
}

/****************************************************************************/
/*																			*/
/* Function:  EW_EVector3D													*/
/*																			*/
/* Purpose:   evaluate elements for vector drawing							*/
/*																			*/
/* Input:	  PICTURE *thePicture, WORK *theWork							*/
/*																			*/
/* Return:	  INT 0: ok 													*/
/*			  INT 1: an error occurred										*/
/*																			*/
/****************************************************************************/

static INT EW_EVector3D (ELEMENT *theElement, DRAWINGOBJ *theDO)
{
	INT i, n, NodeOrder, nr;
	DOUBLE_VECTOR LocalCoord, Poly[MAX_POINTS_OF_POLY], Poly2[MAX_POINTS_OF_POLY], RasterPoint[RASTERPOINTS_MAX];
	DOUBLE *x[MAX_CORNERS_OF_ELEM], z[MAX_CORNERS_OF_ELEM];
	DOUBLE scprd, norm, value;
	long Color;
	DOUBLE min, max;
	DOUBLE_VECTOR Arrow;
	
	/* get node order */
	NodeOrder = NODE_ORDER(theElement);
     		
	/* get coordinates of corners of the element and their z coordinates in cut system */
	for (i=0; i<CORNERS_OF_ELEM(theElement); i++)
	{
		x[i] = CVECT(MYVERTEX(CORNER(theElement,i)));
		V3_TRAFO4_SC(x[i],CutTrafo,z[i])
	}
		
  	/* determine polygon being intersection of element wth cut plane */
   	switch (TAG(theElement)) {
      case (TETRAHEDRON):
        if (GetPolyElemISCutPlaneTET(x,z,NodeOrder,Poly,&n))
          return(1);
        break;
      case (PYRAMID):
        if (GetPolyElemISCutPlanePYR(x,z,NodeOrder,Poly,&n))
          return(1);
        break;
      case (PRISM):
        if (GetPolyElemISCutPlanePRI(x,z,NodeOrder,Poly,&n))
          return(1);
        break;
      case (HEXAHEDRON):
        if (GetPolyElemISCutPlaneHEX(x,z,NodeOrder,Poly,&n))
          return(1);
        break;
      default:
        RETURN(1);
    }

	if (n<=2)
	{
		DO_2c(theDO) = DO_NO_INST;

                #ifdef ModelP
	        WOP_DObjPnt = theDO;
	        #endif

		return (0);
	}
	
	/* get arrows with rastersize (transform to cutsystem and back) */
	for (i=0; i<n; i++)
		V3_TRAFOM4_V3(Poly[i],CutTrafo,Poly2[i])
	if (FindRasterPoints3D(EVector_rastersize,Poly2,n,RasterPoint,&nr)) return (1);
	for (i=0; i<nr; i++)
	{
		V3_TRAFOM4_V3(RasterPoint[i],InvCutTrafo,Arrow)
		V3_COPY(Arrow,RasterPoint[i])
	}
			
	/* handle arrows */
	min = MAX_D; max = -MAX_D;
	for (i=0; i<nr; i++)
	{
		if (UG_GlobalToLocal(CORNERS_OF_ELEM(theElement),(const DOUBLE **)x,
							RasterPoint[i],LocalCoord)) return (1);
		(*EVector_EvalFct)(theElement,(const DOUBLE **)x,LocalCoord,Arrow);
		V3_SCALE(EVector_V2L_factor,Arrow)
				
		/* find color and size of arrow, define its endpoint on the cutplane */
		V3_SCALAR_PRODUCT(Arrow,CUT_CutNormal,scprd)
		V3_EUKLIDNORM(Arrow,norm)
			
		if (norm!=0.0) 	value = scprd/norm;
		else			value = 0.0;
		if (EVector3D_projectvector==YES && norm>SMALL_C)
		{
			V3_LINCOMB(1.0,Arrow,-scprd,CUT_CutNormal,Arrow)
			V3_EUKLIDNORM(Arrow,norm)
		}
		max = MAX(max,norm); min = MIN(min,norm);
		if ((norm>EVector_rastersize*EVector_CutLenFactor) && EVector_cutvector)
		{
			Color = EVector_ColorCut;
			V3_SCALE(EVector_rastersize*EVector_CutLenFactor/norm,Arrow)
		}
		else
		{
			Color = EVector3D_V2C_factor*value + EVector3D_V2C_offset;
			Color = MIN(Color,WOP_OutputDevice->spectrumEnd);
			Color = MAX(Color,WOP_OutputDevice->spectrumStart);
		}
		V3_ADD(RasterPoint[i],Arrow,Arrow);
		
		/* draw arrow */
		DO_2c(theDO) = DO_ARROW; DO_inc(theDO) 
									 DO_2l(theDO) = Color; DO_inc(theDO);
		V3_COPY(RasterPoint[i],DO_2Cp(theDO)); DO_inc_n(theDO,3);
		V3_COPY(Arrow,DO_2Cp(theDO)); DO_inc_n(theDO,3); 	
	}

	/* store range */
	DO_2c(theDO) = DO_RANGE; DO_inc(theDO) 
	DO_2C(theDO) = min/EVector_V2L_factor; DO_inc(theDO);
	DO_2C(theDO) = max/EVector_V2L_factor; DO_inc(theDO);
			
	DO_2c(theDO) = DO_NO_INST;

        #ifdef ModelP
	WOP_DObjPnt = theDO;
	#endif
	
	return (0);
}

#endif /* __THREEDIM__ */

/****************************************************************************/
/*D
   DrawWindowText - Draw text in a window

   SYNOPSIS:
   INT DrawWindowText (UGWINDOW *theWin, COORD_POINT pos,
   					const char *text, INT size, INT center, INT inverse)

   PARAMETERS:
.  theWin  - draw a frame of the picture
.  pos     - draw text here (the lower left corner is the origin)
.  text    - text to draw
.  size    - texz size (if 0 take default)
.  center  - center text at position if TRUE
.  inverse - draw text inverse (rather than black)
  
   DESCRIPTION:
   This function draws text into a ug window at a certain position given in
   pixel coordinates. The text can be centered and inverse.

   RETURN VALUE:
   INT
.n     0 if ok
.n     1 if en arror occured.
D*/
/****************************************************************************/

INT DrawWindowText (UGWINDOW *theWin, COORD_POINT pos, const char *text, INT size, INT center, INT mode)
{
	if (PrepareGraphWindow(theWin)) return (1);
	
	/* transform from standard system to device coordinates */
	if (UGW_LLL(theWin)[_X_]<UGW_LUR(theWin)[_X_])
		pos.x = UGW_LLL(theWin)[_X_] + pos.x;
	else
		pos.x = UGW_LLL(theWin)[_X_] - pos.x;
	
	if (UGW_LLL(theWin)[_Y_]<UGW_LUR(theWin)[_Y_])
		pos.y = UGW_LLL(theWin)[_Y_] + pos.y;
	else
		pos.y = UGW_LLL(theWin)[_Y_] - pos.y;
	
	UgSetColor(UGW_OUTPUTDEV(theWin)->black);
	if (size!=0)
		UgSetTextSize(size);
	else
		UgSetTextSize(WINDOW_TEXT_SIZE);
	
	if (center)
		UgCenteredText(pos,text,mode);
	else
	{
		UgMove(pos);
		UgText(text,mode);
	}
	
	return (0);
}

/****************************************************************************/
/*D
   SetTextFactor - set a factor to multiply textsizes with

   SYNOPSIS:
   INT SetTextFactor (DOUBLE textfactor)

   PARAMETERS:
.  mode - YES: do frame, NO: do not frame
  
   DESCRIPTION:
   This function sets a factor to multiply textsizes with.

   RETURN VALUE:
   INT
.n     0 if ok
D*/
/****************************************************************************/

INT SetTextFactor (DOUBLE textfactor)
{
	TextFactor = textfactor;
	return (0);
}

/****************************************************************************/
/*D
   SetDoFramePicture - toggle picture framing

   SYNOPSIS:
   INT SetDoFramePicture (INT mode)

   PARAMETERS:
.  mode - YES: do frame, NO: do not frame
  
   DESCRIPTION:
   This function toggles the framing of 'PICTURE's.

   RETURN VALUE:
   INT
.n     0 if ok
D*/
/****************************************************************************/

INT SetDoFramePicture (INT mode)
{
	DoFramePicture = mode;
	return (0);
}

/****************************************************************************/
/*D
   DrawPictureFrame - Draw a frame around picture

   SYNOPSIS:
   INT DrawPictureFrame (PICTURE *thePicture, INT mode);

   PARAMETERS:
.  thePicture - draw a frame of the picture
.  mode - mode of the frame
  
   DESCRIPTION:
   This function draws a frame around 'thePicture'. The mode determines the color of the
   frame. 'mode'==WOP_ACTIVE results in a orange frame, 'mode'==WOP_NOT_ACTIVE in a black
   and 'mode'==WOP_WORKING in a red one.

   RETURN VALUE:
   INT
.n     0 if ok
.n     1 if en arror occured.
D*/
/****************************************************************************/

INT DrawPictureFrame (PICTURE *thePicture, INT mode)
{
	OUTPUTDEVICE *theOD;
	long color;
	COORD_POINT p[5];
	
	#ifdef ModelP
	if (me != master)
		return(0);
	#endif

	if (!DoFramePicture)
		return (0);
	
	/* prepare graph for plot */
	if (PrepareGraph (thePicture)) return (1);
	
	/* set color */
	theOD  = PIC_OUTPUTDEV(thePicture);
	switch (mode)
	{
		case WOP_ACTIVE:
			color = theOD->orange;
			break;
		case WOP_NOT_ACTIVE:
			color = theOD->black;
			break;
		case WOP_WORKING:
			color = theOD->red;
			break;
	}
	
	/* plot invalidMode */
	UgSetLineWidth(1);
	UgSetColor(color);
	p[0].x = PIC_GLL(thePicture)[0]; p[0].y = PIC_GLL(thePicture)[1];
	p[1].x = PIC_GUR(thePicture)[0]; p[1].y = PIC_GLL(thePicture)[1];
	p[2].x = PIC_GUR(thePicture)[0]; p[2].y = PIC_GUR(thePicture)[1];
	p[3].x = PIC_GLL(thePicture)[0]; p[3].y = PIC_GUR(thePicture)[1];
	p[4].x = PIC_GLL(thePicture)[0]; p[4].y = PIC_GLL(thePicture)[1];
	UgPolyLine(p,5);
	
	return (0);
}

/****************************************************************************/
/*D
   ErasePicture	- Erase area of the Picture 

   SYNOPSIS:
   INT ErasePicture (PICTURE *thePicture);

   PARAMETERS:
.  thePicture - to be erased

   DESCRIPTION:
   This function erases area of the Picture.

   RETURN VALUE:
   INT
.n     0 if ok
.n     1 if en arror occured.
D*/
/****************************************************************************/

INT ErasePicture (PICTURE *thePicture)
{
	COORD_POINT p[4];	
	
	if (PrepareGraph (thePicture)) return (1);
	p[0].x = PIC_GLL(thePicture)[0]; p[0].y = PIC_GLL(thePicture)[1];
	p[1].x = PIC_GUR(thePicture)[0]; p[1].y = PIC_GLL(thePicture)[1];
	p[2].x = PIC_GUR(thePicture)[0]; p[2].y = PIC_GUR(thePicture)[1];
	p[3].x = PIC_GLL(thePicture)[0]; p[3].y = PIC_GUR(thePicture)[1];
	UgErasePolygon(p,4);

	return (0);
}


/****************************************************************************

            Parallel Extensions for WorkOnPicture

****************************************************************************/


#ifdef ModelP

/****************************************************************************/
/*
   ConnectWopTree - connects the tree for communicating DOs

   SYNOPSIS:
   void ConnectWopTree() 

   PARAMETERS:
   none

   DESCRIPTION:
   This function connects the tree for communicating DrawingObjects.
   (parallel version only)

   RETURN VALUE:
   void
*/
/****************************************************************************/

void ConnectWopTree()  
{
	INT i, k;
	
	/* setup connections ... */

	WOP_UpChannel = NULL;
	for (i=0; i<WOP_DOWN_CHANNELS; i++) WOP_DownChannel[i] = NULL;

	if (me != master) 
		WOP_UpChannel = ConnASync((me-1)/WOP_DOWN_CHANNELS, 4711);
	for (i=0; i<WOP_DOWN_CHANNELS; i++) {
		k = me * WOP_DOWN_CHANNELS + i + 1;
		if (k < procs)
			WOP_DownChannel[i] = ConnASync(k, 4711);
		else
			break;
	}

	/* ... and wait for completion */

	if (me != master)
		while (InfoAConn(WOP_UpChannel) != 1);
	for (i=0; i<WOP_DOWN_CHANNELS; i++) {
		k = me * WOP_DOWN_CHANNELS + i + 1;
		if (k < procs) 
			while (InfoAConn(WOP_DownChannel[i]) != 1);
		else
			break;
	}
}


/****************************************************************************/
/*
   NumberOfDesc - computes the number of descandants of every node

   SYNOPSIS:
   void NumberOfDesc(INT *nbDesc)

   PARAMETERS:
   nbDesc - array to store computed number of descandants 

   DESCRIPTION:
   This function computes the number of descandants of every node in the
   tree for communicationg DOs.

   RETURN VALUE:
   void
*/
/****************************************************************************/

void NumberOfDesc(INT *nbDesc)
{
	msgid umid, dmid[WOP_DOWN_CHANNELS];
	INT   uerr, derr[WOP_DOWN_CHANNELS];
	INT   sum;
	INT   i, noDesc;

	noDesc = 1;
	for (i=0; i<WOP_DOWN_CHANNELS; i++) {
		nbDesc[i] = 0;
		noDesc    = (noDesc && WOP_DownChannel[i] == NULL);
	}
	if (noDesc) {
		sum  = 1;
		umid = SendASync(WOP_UpChannel, &sum, sizeof(sum), &uerr);
		while (InfoASend(WOP_UpChannel, umid) != 1);
	}
	else {
		for (i=0; i<WOP_DOWN_CHANNELS; i++) {
			if (WOP_DownChannel[i] != NULL) {
				dmid[i] = RecvASync(WOP_DownChannel[i], &nbDesc[i], 
									sizeof(INT), &derr[i]);
				while (InfoARecv(WOP_DownChannel[i], dmid[i]) != 1);
			}
			else 
				break;
		}
		if (WOP_UpChannel != NULL) {
			sum = 0;
			for (i=0; i<WOP_DOWN_CHANNELS; i++)
				sum += nbDesc[i];
			sum++;
			umid = SendASync(WOP_UpChannel, &sum, sizeof(sum), &uerr);
			while (InfoASend(WOP_UpChannel, umid) != 1);
		}
	}
}


/****************************************************************************/
/*
   PWorkGEN_Init - Initialisation for PWorkXX_Evaluate and PWorkXX_Execute

   SYNOPSIS:
   void PWorkGEN_Init()

   PARAMETERS:
   none

   RETURN VALUE:
   void
*/
/****************************************************************************/

void PWorkGEN_Init()
{
	int i;

	for (i=0; i<WOP_DOWN_CHANNELS; i++) {
		WOP_Sending[i]   = 0;
		WOP_Receiving[i] = 0; 
		WOP_NbTokens[i]  = 0;
		WOP_Count[i]     = 0;
		WOP_Front[i]     = 0;
		WOP_Rear[i]      = 0;
		WOP_More[i]      = (WOP_DownChannel[i] != NULL);
	}
	WOP_Sending[WOP_DOWN_CHANNELS] = 0;
	WOP_Count  [WOP_DOWN_CHANNELS] = 0;
	WOP_Front  [WOP_DOWN_CHANNELS] = 0;
	WOP_Rear   [WOP_DOWN_CHANNELS] = 0;
	WOP_More   [WOP_DOWN_CHANNELS] = 1;

	WOP_CurrDoLen = 0;
}   


/****************************************************************************/
/*
   PWorkGEN_Quit - test for end of PWorkXX_Evaluate / PWorkXX_Execute loop

   SYNOPSIS:
   INT PWorkGEN_Quit()

   PARAMETERS:
   none

   RETURN VALUE:
   INT
.n     0 if loop ends
.n     1 if loop continues
*/
/****************************************************************************/

INT PWorkGEN_Quit()
{
	INT i, quit;

	quit = 1;
	for (i = 0; i <= WOP_DOWN_CHANNELS; i++) 
		quit = (quit && !WOP_More[i] && WOP_Count[i] == 0);
	return (quit);
}


/****************************************************************************/
/*
   PWorkGEN_Execute - executes and communicates DOs

   SYNOPSIS:
   void PWorkGEN_Execute()

   PARAMETERS:
   none

   RETURN VALUE:
   void
*/
/****************************************************************************/

void PWorkGEN_Execute()
{
	INT i;
	DRAWINGOBJ *p;

	/* receive DOs */

	for (i = 0; i < WOP_DOWN_CHANNELS; i++) {
		if (WOP_Receiving[i])
			if (InfoARecv(WOP_DownChannel[i], WOP_Inmsg[i]) == 1) {
				WOP_Count[i]++;
				WOP_Receiving[i] = 0;
				if (DO_2c(WOP_DO_Buffer[i][WOP_Front[i]]) == DO_END_TOKEN)
					WOP_More[i] = (++WOP_NbTokens[i] < WOP_NbDesc[i]);
				WOP_Front[i] = (WOP_Front[i] + 1) % DO_BUFFER_SLOTS;
			}
		if (WOP_More[i] && !WOP_Receiving[i])
			if (WOP_Count[i] < DO_BUFFER_SLOTS) {
				WOP_Receiving[i] = 1;
				WOP_Inmsg[i] = RecvASync(WOP_DownChannel[i],
										 WOP_DO_Buffer[i][WOP_Front[i]], 
										 DO_SLOT_SIZE*sizeof(DRAWINGOBJ), &WOP_RError[i]);
			}
	}

      /* handle communication */
	/* execute or send own and received DOs */

	if (me == master) { 
		for (i = 0; i <= WOP_DOWN_CHANNELS; i++)
			if (WOP_Count[i] > 0) {
				p = WOP_DO_Buffer[i][WOP_Rear[i]];
				DO_inc(p);
				(*WOP_GEN_ExecuteProc)(p);
				WOP_Count[i]--;
				WOP_Rear[i] = (WOP_Rear[i] + 1) % DO_BUFFER_SLOTS;
			}
	}
	else {
		for (i = 0; i <= WOP_DOWN_CHANNELS; i++) {
			if (WOP_Sending[i])
				if (InfoASend(WOP_UpChannel, WOP_Outmsg[i]) == 1) {
					WOP_Count[i]--;
					WOP_Sending[i] = 0;
					WOP_Rear[i] = (WOP_Rear[i] + 1) % DO_BUFFER_SLOTS;
				}
			if (!WOP_Sending[i])
				if (WOP_Count[i] > 0) {
					WOP_Sending[i] = 1;
					WOP_Outmsg[i] = SendASync(WOP_UpChannel, WOP_DO_Buffer[i][WOP_Rear[i]], 
											  DO_SLOT_SIZE*sizeof(DRAWINGOBJ), &WOP_SError[i]);
				}
		}
	}
}

/****************************************************************************/
/*
   PWorkEW_Evaluate - evaluates elementwise

   SYNOPSIS:
   void PWorkEW_Evaluate()

   PARAMETERS:
   none

   RETURN VALUE:
   void
*/
/****************************************************************************/

void PWorkEW_Evaluate()
{
	INT i;
	DRAWINGOBJ *p, *p1;

	i = WOP_DOWN_CHANNELS;
	if (WOP_More[i] && WOP_Count[i] < DO_BUFFER_SLOTS) {
		p = p1 = WOP_DO_Buffer[i][WOP_Front[i]];
		DO_inc(p);
		DO_2c(p1) = DO_NO_INST;
		do {
			if (WOP_CurrDoLen > 0) {
				memcpy(p, WOP_DrawingObject, (size_t)(WOP_CurrDoLen));
				p = (DRAWINGOBJ *) (DO_2cp(p) + WOP_CurrDoLen);
			}
			if (WOP_Element == NULL) {
				DO_2c(p1) = DO_END_TOKEN;
				WOP_More[i] = 0;
				break;
			}
			else {
				(*WOP_EW_EvaluateProc)(WOP_Element, WOP_DrawingObject);
				WOP_CurrDoLen = (INT)(WOP_DObjPnt) - (INT)(WOP_DrawingObject);
				WOP_Element = (*WOP_EW_GetNextElementProc)(WOP_Element);
			}
		} while (DO_SLOT_SIZE*sizeof(DRAWINGOBJ) - ((INT)(p) - (INT)(p1)) 
				 > WOP_CurrDoLen);
		DO_2c(p) = DO_NO_INST;
		WOP_Count[i]++;
		WOP_Front[i] = (WOP_Front[i] + 1) % DO_BUFFER_SLOTS;
	}
}


/****************************************************************************/
/*
   PWorkNW_Evaluate - evaluates nodewise

   SYNOPSIS:
   void PWorkNW_Evaluate()

   PARAMETERS:
   none

   RETURN VALUE:
   void
*/
/****************************************************************************/

void PWorkNW_Evaluate()
{
	INT i;
	DRAWINGOBJ *p, *p1;

	i = WOP_DOWN_CHANNELS;
	if (WOP_More[i] && WOP_Count[i] < DO_BUFFER_SLOTS) {
		p = p1 = WOP_DO_Buffer[i][WOP_Front[i]];
		DO_inc(p);
		DO_2c(p1) = DO_NO_INST;
		do {
			if (WOP_CurrDoLen > 0) {
				memcpy(p, WOP_DrawingObject, (size_t)(WOP_CurrDoLen));
				p = (DRAWINGOBJ *) (DO_2cp(p) + WOP_CurrDoLen);
			}
			if (WOP_Node == NULL) {
				DO_2c(p1) = DO_END_TOKEN;
				WOP_More[i] = 0;
				break;
			}
			else {
				(*WOP_NW_EvaluateProc)(WOP_Node, WOP_DrawingObject);
				WOP_CurrDoLen = (INT)(WOP_DObjPnt) - (INT)(WOP_DrawingObject);
				WOP_Node = (*WOP_NW_GetNextNodeProc)(WOP_Node);
			}
		} while (DO_SLOT_SIZE*sizeof(DRAWINGOBJ) - ((INT)(p) - (INT)(p1)) 
				 > WOP_CurrDoLen);
		DO_2c(p) = DO_NO_INST;
		WOP_Count[i]++;
		WOP_Front[i] = (WOP_Front[i] + 1) % DO_BUFFER_SLOTS;
	}
}


/****************************************************************************/
/*
   PWorkVW_Evaluate - evaluates vectorwise

   SYNOPSIS:
   void PWorkVW_Evaluate()

   PARAMETERS:
   none

   RETURN VALUE:
   void
*/
/****************************************************************************/

void PWorkVW_Evaluate()
{
	INT i;
	DRAWINGOBJ *p, *p1;

	i = WOP_DOWN_CHANNELS;
	if (WOP_More[i] && WOP_Count[i] < DO_BUFFER_SLOTS) {
		p = p1 = WOP_DO_Buffer[i][WOP_Front[i]];
		DO_inc(p);
		DO_2c(p1) = DO_NO_INST;
		do {
			if (WOP_CurrDoLen > 0) {
				memcpy(p, WOP_DrawingObject, (size_t)(WOP_CurrDoLen));
				p = (DRAWINGOBJ *) (DO_2cp(p) + WOP_CurrDoLen);
			}
			if (WOP_Vector == NULL) {
				DO_2c(p1) = DO_END_TOKEN;
				WOP_More[i] = 0;
				break;
			}
			else {
				(*WOP_VW_EvaluateProc)(WOP_Vector, WOP_DrawingObject);
				WOP_CurrDoLen = (INT)(WOP_DObjPnt) - (INT)(WOP_DrawingObject);
				WOP_Vector = (*WOP_VW_GetNextVectorProc)(WOP_Vector);
			}
		} while (DO_SLOT_SIZE*sizeof(DRAWINGOBJ) - ((INT)(p) - (INT)(p1)) 
				 > WOP_CurrDoLen);
		DO_2c(p) = DO_NO_INST;
		WOP_Count[i]++;
		WOP_Front[i] = (WOP_Front[i] + 1) % DO_BUFFER_SLOTS;
	}
}

#endif


/****************************************************************************/
/*D
   WOP_Init - Initialize next WOP Cycle

   SYNOPSIS:
   INT WOP_Init(INT WOP_WorkMode)

   PARAMETERS:
.  WOP_WorkMode - the work mode which needs to be initialized

   DESCRIPTION:
   This function initializes the next cycle for WOP. This includes e.g. ordering
   routines and the setting of functin pointers.

   RETURN VALUE:
   INT
.n     0 if ok
.n     1 if en arror occured.
D*/
/****************************************************************************/

INT WOP_Init(INT WOP_WorkMode)
{
	switch (WOP_WorkMode)
	{
		case ELEMENTWISE:
		
			/* order elements if */
			if (WOP_ViewDim == TYPE_3D)
			{
				#ifdef __TWODIM__
					if (OrderElements_2D(WOP_MG,WOP_ViewedObj))
				#endif
				#ifdef __THREEDIM__
					if (OrderElements_3D(WOP_MG,WOP_ViewedObj))
				#endif
				{
					UserWrite("ording of elements failed\n");
					return (1);
				}
			}
		
			/* set execution functions */
			WOP_GEN_PreProcessProc		=	WP_ELEMWISE(WOP_WorkProcs)->EW_PreProcessProc;
			WOP_EW_GetFirstElementProc	= (*WP_ELEMWISE(WOP_WorkProcs)->EW_GetFirstElementProcProc)(WOP_ViewedObj);
			WOP_EW_GetNextElementProc	= (*WP_ELEMWISE(WOP_WorkProcs)->EW_GetNextElementProcProc)(WOP_ViewedObj);
			WOP_EW_EvaluateProc 		=	WP_ELEMWISE(WOP_WorkProcs)->EW_EvaluateProc;
			WOP_GEN_ExecuteProc 		=	WP_ELEMWISE(WOP_WorkProcs)->EW_ExecuteProc;
			WOP_GEN_PostProcessProc 	=	WP_ELEMWISE(WOP_WorkProcs)->EW_PostProcessProc;
			if (WOP_EW_EvaluateProc==NULL || WOP_GEN_ExecuteProc==NULL) return (1);
		
			break;
		
		case NODEWISE:
		
			/* order elements if */
			if (WOP_ViewDim == TYPE_3D)
			{
				/* still missing */
			}
		
			/* set execution functions */
			WOP_GEN_PreProcessProc		=	WP_NODEWISE(WOP_WorkProcs)->NW_PreProcessProc;
			WOP_NW_GetFirstNodeProc 	= (*WP_NODEWISE(WOP_WorkProcs)->NW_GetFirstNodeProcProc)(WOP_ViewedObj);
			WOP_NW_GetNextNodeProc		= (*WP_NODEWISE(WOP_WorkProcs)->NW_GetNextNodeProcProc)(WOP_ViewedObj);
			WOP_NW_EvaluateProc 		=	WP_NODEWISE(WOP_WorkProcs)->NW_EvaluateProc;
			WOP_GEN_ExecuteProc 		=	WP_NODEWISE(WOP_WorkProcs)->NW_ExecuteProc;
			WOP_GEN_PostProcessProc 	=	WP_NODEWISE(WOP_WorkProcs)->NW_PostProcessProc;
			if (WOP_NW_EvaluateProc==NULL || WOP_GEN_ExecuteProc==NULL) return (1);
		
			break;
		
		case VECTORWISE:
		
			/* set execution functions */
			WOP_GEN_PreProcessProc		=	WP_VECTORWISE(WOP_WorkProcs)->VW_PreProcessProc;
			WOP_VW_GetFirstVectorProc	= (*WP_VECTORWISE(WOP_WorkProcs)->VW_GetFirstVectorProcProc)(WOP_ViewedObj);
			WOP_VW_GetNextVectorProc	= (*WP_VECTORWISE(WOP_WorkProcs)->VW_GetNextVectorProcProc)(WOP_ViewedObj);
			WOP_VW_EvaluateProc 		=	WP_VECTORWISE(WOP_WorkProcs)->VW_EvaluateProc;
			WOP_GEN_ExecuteProc 		=	WP_VECTORWISE(WOP_WorkProcs)->VW_ExecuteProc;
			WOP_GEN_PostProcessProc 	=	WP_VECTORWISE(WOP_WorkProcs)->VW_PostProcessProc;
			if (WOP_VW_EvaluateProc==NULL || WOP_GEN_ExecuteProc==NULL) return (1);
		
			break;
		
		case EXTERN:
		
			/* set execution functions */
			WOP_GEN_PreProcessProc			= WP_EXTERNWISE(WOP_WorkProcs)->EXT_PreProcessProc;
			WOP_EXT_EvaluateProc			= WP_EXTERNWISE(WOP_WorkProcs)->EXT_EvaluateProc;
			WOP_GEN_ExecuteProc 			= WP_EXTERNWISE(WOP_WorkProcs)->EXT_ExecuteProc;
			WOP_GEN_PostProcessProc 		= WP_EXTERNWISE(WOP_WorkProcs)->EXT_PostProcessProc;
			if (WOP_EXT_EvaluateProc==NULL || WOP_GEN_ExecuteProc==NULL)
			{
				UserWrite("evaluation or execution procedure is missing\n");
				return (1);
			}
		
			break;
		
		case RECURSIVE:
		
			/* set execution functions */
			WOP_GEN_PreProcessProc			= WP_RECURSIVEWISE(WOP_WorkProcs)->RECURSIVE_PreProcessProc;
			WOP_RECURSIVE_EvaluateProc		= WP_RECURSIVEWISE(WOP_WorkProcs)->RECURSIVE_EvaluateProc;
			WOP_GEN_ExecuteProc 			= WP_RECURSIVEWISE(WOP_WorkProcs)->RECURSIVE_ExecuteProc;
			WOP_GEN_PostProcessProc 		= WP_RECURSIVEWISE(WOP_WorkProcs)->RECURSIVE_PostProcessProc;
			if (WOP_RECURSIVE_EvaluateProc==NULL || WOP_GEN_ExecuteProc==NULL)
			{
				UserWrite("evaluation or execution procedure is missing\n");
				return (1);
			}
		
			break;
		
		default:
			return (1);
	}
	return (0);
}

/****************************************************************************/
/*
   WorkEW - does work elementwise

   SYNOPSIS:
   INT WorkEW()

   PARAMETERS:
   none

   DESCRIPTION:
   
   This function evaluates all elements and executes the produced DOs.
   The parallel version sends them to the master to get them executed there.

   RETURN VALUE:
   INT
.n     0 if ok
.n     1 if en arror occured.
*/
/****************************************************************************/

INT WorkEW()
{
#ifndef ModelP
  
	/*** Sequential Version ***/

	for (WOP_Element=(*WOP_EW_GetFirstElementProc)(WOP_MG,0,
												   WOP_MG->currentLevel);
		 WOP_Element!=NULL;
		 WOP_Element=(*WOP_EW_GetNextElementProc)(WOP_Element))
	{
		if ((*WOP_EW_EvaluateProc)(WOP_Element,WOP_DrawingObject))  return (1);
		if ((*WOP_GEN_ExecuteProc)(WOP_DrawingObject))	        return (1);
	}
	return (0);

#else

	/*** Parallel Version ***/

	PWorkGEN_Init();

	WOP_Element = (CONTEXT(me) ?
      (*WOP_EW_GetFirstElementProc)(WOP_MG, 0, WOP_MG->currentLevel) : NULL);

	for (;;) {
		if (PWorkGEN_Quit()) break;
		PWorkGEN_Execute();
		PWorkEW_Evaluate();
	}
  
	return (0);

#endif
}


/****************************************************************************/
/*
   WorkNW - does work nodewise

   SYNOPSIS:
   INT WorkNW()

   PARAMETERS:
   none

   DESCRIPTION:
   This function evaluates all nodes and executes the produced DOs.
   The parallel version sends them to the master to get them executed there.

   RETURN VALUE:
   INT
.n     0 if ok
.n     1 if en arror occured.
*/
/****************************************************************************/

INT WorkNW()
{
#ifndef ModelP

	/*** Sequential Version ***/

	for (WOP_Node=(*WOP_NW_GetFirstNodeProc)(WOP_MG,0,
											 WOP_MG->currentLevel); 
		 WOP_Node!=NULL; 
		 WOP_Node=(*WOP_NW_GetNextNodeProc)(WOP_Node))
	{
		if ((*WOP_NW_EvaluateProc)(WOP_Node,WOP_DrawingObject)) return (1);
		if ((*WOP_GEN_ExecuteProc)(WOP_DrawingObject))	    return (1);
	}
	return (0);

#else

	/*** Parallel Version ***/

	PWorkGEN_Init();

	WOP_Node=(CONTEXT(me) ?
      (*WOP_NW_GetFirstNodeProc)(WOP_MG, 0, WOP_MG->currentLevel) : NULL);

	for (;;) {
		if (PWorkGEN_Quit()) break;
		PWorkGEN_Execute();
		PWorkNW_Evaluate();
	}
  
	return (0);

#endif
}


/****************************************************************************/
/*
   WorkVW - does work vectorwise

   SYNOPSIS:
   INT WorkVW()

   PARAMETERS:
   none

   DESCRIPTION:
   This function evaluates all vectors and executes the produced DOs.
   The parallel version sends them to the master to get them executed there.

   RETURN VALUE:
   INT
.n     0 if ok
.n     1 if en arror occured.
*/
/****************************************************************************/

INT WorkVW()
{
#ifndef ModelP

	/*** Sequential Version ***/

	for (WOP_Vector=(*WOP_VW_GetFirstVectorProc)(WOP_MG,0,
												 WOP_MG->currentLevel); 
		 WOP_Vector!=NULL; 
		 WOP_Vector=(*WOP_VW_GetNextVectorProc)(WOP_Vector))
	{
		if ((*WOP_VW_EvaluateProc)(WOP_Vector,WOP_DrawingObject)) return (1);
		if ((*WOP_GEN_ExecuteProc)(WOP_DrawingObject))	      return (1);
	}
	return (0);

#else

	/*** Parallel Version ***/

	PWorkGEN_Init();

	WOP_Vector=(CONTEXT(me) ?
       (*WOP_VW_GetFirstVectorProc)(WOP_MG, 0, WOP_MG->currentLevel) : NULL);

	for (;;) {
		if (PWorkGEN_Quit()) break;
		PWorkGEN_Execute();
		PWorkVW_Evaluate();
	}
  
	return (0);

#endif
}


INT WorkET()
{
	INT end;

	end = 0;
	while (!end)
	{
		if ((*WOP_EXT_EvaluateProc)(WOP_DrawingObject,&end))                    return (1);
		if ((*WOP_GEN_ExecuteProc)(WOP_DrawingObject))                          return (1);
	}
}

INT WorkRC()
{
	if ((*WOP_RECURSIVE_EvaluateProc)(WOP_DrawingObject,WOP_GEN_ExecuteProc))                   return (1);
}

/****************************************************************************/
/*D
   WorkOnPicture - Work on picture

   SYNOPSIS:
   INT WorkOnPicture (PICTURE *thePicture, WORK *theWork);

   PARAMETERS:
.  thePicture - the picture to work on
.  theWork - the work to be performed

   DESCRIPTION:
   This function executes the specified 'WORK' on the specified 'PICTURE'. The 'PICTURE'
   has to have the 'status' 'ACTIVE' (completely initialized). An attempt to perform a
   'WORK' which is not executable results in an output on ug shell "action not executable"
   without errormessage.

   RETURN VALUE:
   INT
.n     0 if ok
.n     1 if en arror occured.
D*/
/****************************************************************************/

INT WorkOnPicture (PICTURE *thePicture, WORK *theWork)
{
	INT i, end, error;

#ifdef __DO_HEAP_USED__
	char buffer[128];

	/* inits */
	Heap_Used_Min = MAX_I;	
	Heap_Used_Max = -MAX_I;	
#endif
	
	if (thePicture==NULL || theWork==NULL)	return (1);
	WOP_Picture = thePicture;
	WOP_ViewedObj = PIC_VO(WOP_Picture);
	if (VO_STATUS(WOP_ViewedObj) != ACTIVE)
	{
		UserWrite("PlotObject and View have to be initialized\n");
		return (0);
	}
	WOP_Work					= theWork;
	WOP_OutputDevice			= UGW_OUTPUTDEV(PIC_UGW(WOP_Picture));
	WOP_PlotObjHandling 		= (PLOTOBJHANDLING*)PO_POT(PIC_PO(WOP_Picture));
	WOP_MG						= PO_MG(PIC_PO(WOP_Picture));
	if (WOP_MG == NULL) return (1);
	WOP_ViewDim 				= PO_DIM(PIC_PO(WOP_Picture));
	if (WOP_ViewDim == NOT_DEFINED) return (1);
	
	/* if FINDWORK: is plot valid? */
	if (W_ISSELECTWORK(WOP_Work))
		if (PIC_VALID(WOP_Picture) == NO)
		{
			UserWrite("cannot execute find-work: picture is not valid\n");
			return (0);
		}
	
	/* build transformation */
	if (BuildObsTrafo(WOP_Picture))
	{
		UserWrite("cannot build transformation\n");
		return (1);
	}
	
	/* activate low level grahic */
	#ifdef ModelP
	if (me == master)
	#endif
	error=PrepareGraph(WOP_Picture);
        #ifdef ModelP
        Broadcast(&error, sizeof(error));
        #endif
        if (error)
	{
		UserWrite("cannot activate low level graphic\n");
		return (1);
	}
	
	if (POH_NBCYCLES(WOP_PlotObjHandling,W_ID(WOP_Work)) <= 0)
	{
		UserWrite("action not executable on this plot object\n");
		return (0);
	}
	
	/* clear if if DRAW_WORK */

	if (W_ID(theWork) == DRAW_WORK)
	{
	        if (PO_CBD(PIC_PO(WOP_Picture)) == YES) {
                        #ifdef ModelP
                        if (me == master)
			#endif
			error = (ErasePicture(WOP_Picture));
                        #ifdef ModelP
                        Broadcast(&error, sizeof(error));
                        #endif 
			if (error) return (1);
                }
                #ifdef ModelP
                if (me == master)
	        #endif
		error = DrawPictureFrame(WOP_Picture,WOP_WORKING);
                #ifdef ModelP
                Broadcast(&error, sizeof(error));
                #endif 
                if (error) return(1);
	}
			
	for (i=0; i<POH_NBCYCLES(WOP_PlotObjHandling,W_ID(WOP_Work)); i++)
	{
		WOP_WorkProcs = POH_WORKPROGS(WOP_PlotObjHandling,W_ID(WOP_Work),i);
		WOP_WorkMode = WP_WORKMODE(WOP_WorkProcs);

		/* initialize */
		if (WOP_Init(WOP_WorkMode)!=0) RETURN(1);

		/* work */
		if (WOP_GEN_PreProcessProc!=NULL)
			if ((*WOP_GEN_PreProcessProc)(WOP_Picture,WOP_Work))
				break;

		switch (WOP_WorkMode)
		{
			case ELEMENTWISE:

				if (WorkEW()) return (1);
				break;

			case NODEWISE:

				if (WorkNW()) return (1);
				break;

			case VECTORWISE:
				if (WorkVW()) return (1);
				break;

			case EXTERN:

				if (WorkET()) return (1);

				break;

			case RECURSIVE:
				
				if (WorkRC()) return (1);
				break;

			default:
				RETURN(1);
		}

		if (WOP_GEN_PostProcessProc!=NULL)
			if ((*WOP_GEN_PostProcessProc)(WOP_Picture,WOP_Work))			return (1);
	}
	
	/* may be picture is valid now */
	if (W_ID(theWork) == DRAW_WORK)
		PIC_VALID(WOP_Picture) = YES;
			
	/* flush cash */
	UgFlush();
	
	/* print heap used */
#ifdef __DO_HEAP_USED__
	sprintf(buffer,"Heap_min = %d\nHeap_max = %d\n",(int)Heap_Used_Min,(int)Heap_Used_Max);	
	UserWrite(buffer);
#endif	
		
	return (0);
}


/****************************************************************************/
/*D
   DragPicture - drag a picture with the mouse

   SYNOPSIS:
   INT DragPicture (PICTURE *thePicture, INT *MousePos)

   PARAMETERS:
.  thePicture - the picture to work on
.  MousePos - current mouse position

   DESCRIPTION:
   This function follows the mouse as long as the button is down and after
   released it adjusts the view of the picture to the new position.

   RETURN VALUE:
   INT
.n     0 if ok
.n     1 if en arror occured.
D*/
/****************************************************************************/

INT DragPicture (PICTURE *thePicture, INT *OldMousePos)
{
	VIEWEDOBJ *theViewedObj;
	COORD_POINT FrameLL,FrameLR,FrameUR,FrameUL;
	DOUBLE oldpos[2],pos[2],shift[2];
	DOUBLE xmin,xmax,ymin,ymax;
	INT LastMousePos[2],MousePos[2];
	INT theViewDim,MouseMoved,rejected;

	if (thePicture==NULL)	return (1);
	theViewedObj = PIC_VO(thePicture);
	
	if (VO_STATUS(theViewedObj) != ACTIVE)
	{
		PrintErrorMessage('E',"DragPicture","PlotObject and View have to be initialized");
		return (0);
	}
	theViewDim 				= PO_DIM(PIC_PO(thePicture));
	if (theViewDim != TYPE_2D) return (0);
	
	/* build transformation */
	if (BuildObsTrafo(thePicture))
	{
		PrintErrorMessage('E',"DragPicture","cannot build transformation");
		return (1);
	}
	
	/* activate low level grahic */
	if (PrepareGraph(thePicture))
	{
		PrintErrorMessage('E',"DragPicture","cannot activate low level graphics");
		return (1);
	}
	
	xmin	= MIN(PIC_GLL(thePicture)[_X_],PIC_GUR(thePicture)[_X_]);
	xmax	= MAX(PIC_GLL(thePicture)[_X_],PIC_GUR(thePicture)[_X_]);
	ymin	= MIN(PIC_GLL(thePicture)[_Y_],PIC_GUR(thePicture)[_Y_]);
	ymax	= MAX(PIC_GLL(thePicture)[_Y_],PIC_GUR(thePicture)[_Y_]);
	
	/* old mouse position in the physical system */
	V2_TRAFOM3_V2(OldMousePos,InvObsTrafo,oldpos);
	
	rejected = MouseMoved = FALSE;
	V2_COPY(OldMousePos,LastMousePos);
	while (MouseStillDown())
	{
		MousePosition(MousePos);
		
		if (V2_ISEQUAL(MousePos,LastMousePos)) continue;
		
		/* inside picture? */
		if ((MousePos[0]<xmin) || (MousePos[0]>xmax) || (MousePos[1]<ymin) || (MousePos[1]>ymax))
		{
			rejected = TRUE;
			break;
		}
		
		V2_COPY(MousePos,LastMousePos);
		
		if (MouseMoved)
		{
			/* invert last frame */
			UgInverseLine(FrameLL,FrameLR);
			UgInverseLine(FrameLR,FrameUR);
			UgInverseLine(FrameUR,FrameUL);
			UgInverseLine(FrameUL,FrameLL);
		}
		
		MouseMoved = TRUE;
		
		/* calculate shifted picture frame in screen coords */
		V2_SUBTRACT(MousePos,OldMousePos,shift);
		FrameUL.x = FrameLL.x = PIC_GLL(thePicture)[_X_]+shift[_X_];
		FrameLR.y = FrameLL.y = PIC_GLL(thePicture)[_Y_]+shift[_Y_];
		FrameLR.x = FrameUR.x = PIC_GUR(thePicture)[_X_]+shift[_X_];
		FrameUL.y = FrameUR.y = PIC_GUR(thePicture)[_Y_]+shift[_Y_];
		
		/* invert new frame */
		UgInverseLine(FrameLL,FrameLR);
		UgInverseLine(FrameLR,FrameUR);
		UgInverseLine(FrameUR,FrameUL);
		UgInverseLine(FrameUL,FrameLL);
		UgFlush();
	}
	
	if (MouseMoved)
	{
		/* invert last frame */
		UgInverseLine(FrameLL,FrameLR);
		UgInverseLine(FrameLR,FrameUR);
		UgInverseLine(FrameUR,FrameUL);
		UgInverseLine(FrameUL,FrameLL);
		UgFlush();
	}
	
	if (rejected) return (0);
	
	/* mouse position in the physical system */
	V2_TRAFOM3_V2(MousePos,InvObsTrafo,pos);
	
	/* adjust view */
	if (theViewDim==TYPE_2D)
	{
		V2_SUBTRACT(oldpos,pos,shift);
		V2_ADD(VO_VT(theViewedObj),shift,VO_VT(theViewedObj));
		V2_ADD(VO_PMP(theViewedObj),shift,VO_PMP(theViewedObj));
	}
	/* else if (theViewDim==TYPE_2D) <-- maybe extended later */
	
	PIC_VALID(thePicture) = NO;
	
	return (0);
}

/****************************************************************************/
/*D
   ZoomPicture - zoom a picture with the mouse

   SYNOPSIS:
   INT ZoomPicture (PICTURE *thePicture, INT *MousePos)

   PARAMETERS:
.  thePicture - the picture to work on
.  MousePos - current mouse position

   DESCRIPTION:
   This function follows the mouse as long as the button is down and pulls
   up a rectangle. This rectangle indicates the new visible window. After the
   button is released the view of the picture is adjusted.

   RETURN VALUE:
   INT
.n     0 if ok
.n     1 if en arror occured.
D*/
/****************************************************************************/

INT ZoomPicture (PICTURE *thePicture, INT *OldMousePos)
{
	VIEWEDOBJ *theViewedObj;
	DOUBLE MidPoint[2],pos[2];
	DOUBLE xmin,xmax,ymin,ymax;
	DOUBLE CanvasRatio,FrameRatio,factor;
	INT status,theViewDim;

	if (thePicture==NULL)	return (1);
	theViewedObj = PIC_VO(thePicture);
	
	if (VO_STATUS(theViewedObj) != ACTIVE)
	{
		PrintErrorMessage('E',"ZoomPicture","PlotObject and View have to be initialized");
		return (0);
	}
	theViewDim 				= PO_DIM(PIC_PO(thePicture));
	if (theViewDim != TYPE_2D) return (0);
	
	/* build transformation */
	if (BuildObsTrafo(thePicture))
	{
		PrintErrorMessage('E',"ZoomPicture","cannot build transformation");
		return (1);
	}
	
	/* activate low level grahic */
	if (PrepareGraph(thePicture))
	{
		PrintErrorMessage('E',"ZoomPicture","cannot activate low level graphics");
		return (1);
	}
	
	status = MousePullFrame(thePicture,OldMousePos,&xmin,&xmax,&ymin,&ymax);
	
	if (status!=MOUSE_MOVED)
		return (0);
	
	/* adjust view */
	if (theViewDim==TYPE_2D)
	{
		/* new midpoint */
		MidPoint[_X_] = 0.5*(xmin+xmax);
		MidPoint[_Y_] = 0.5*(ymin+ymax);
		V2_TRAFOM3_V2(MidPoint,InvObsTrafo,pos);
		V2_COPY(pos,VO_VT(theViewedObj));
		V2_COPY(pos,VO_PMP(theViewedObj));
		
		/* zoom factor */
		CanvasRatio = fabs(((DOUBLE)(PIC_GLL(thePicture)[1]-PIC_GUR(thePicture)[1]))/((DOUBLE)(PIC_GLL(thePicture)[0]-PIC_GUR(thePicture)[0])));
		FrameRatio  = (ymax-ymin) / (xmax-xmin);
		if (FrameRatio>CanvasRatio)
			factor = fabs((ymax-ymin)/((DOUBLE)(PIC_GLL(thePicture)[1]-PIC_GUR(thePicture)[1])));
		else
			factor = fabs((xmax-xmin)/((DOUBLE)(PIC_GLL(thePicture)[0]-PIC_GUR(thePicture)[0])));
		V2_SCALE(factor,VO_PXD(theViewedObj))
		V2_SCALE(factor,VO_PYD(theViewedObj))
	}
	/* else if (theViewDim==TYPE_3D) <-- maybe extended later */
	
	PIC_VALID(thePicture) = NO;
	
	return (0);
}

/****************************************************************************/
/*D
   DrawUgPicture - Draw the picture

   SYNOPSIS:
   INT DrawUgPicture (PICTURE *thePicture);

   PARAMETERS:
.  thePicture - the picture to draw

   DESCRIPTION:
   This function draws the picture. It initializes a structure 'WORK' to be a
   'DRAW_WORK', and calls the function 'WOorkOnPicture'.

   RETURN VALUE:
   INT
.n     0 if ok
.n     1 if en arror occured.
D*/
/****************************************************************************/

INT DrawUgPicture (PICTURE *thePicture)
{
	WORK theWork;
	
	theWork.WorkID = DRAW_WORK;
    if (WorkOnPicture(thePicture,&theWork)) return (1);
	
	return (0);
}

/****************************************************************************/
/*D
   InitWOP - Initialization

   SYNOPSIS:
   INT InitWOP (void);

   PARAMETERS:
.  void

   DESCRIPTION:
   This function enrols all 'PLOTOBJHANDLING's. First all ptr. to functions which
   perform the work are set and then the substructures 'PLOTOBJTYPE' of the
   'PLOTOBJHANDLING' are initialized by the call of 'InitPlotObjTypes'.
   All functions used for working on the 'PICTURE's are defined in this file.
   See 'PLOTOBJHANDLING', 'PLOTOBJTYPE'.

   RETURN VALUE:
   INT
.n     0 if ok
.n     1 if en arror occured.
D*/
/****************************************************************************/

INT InitWOP (void)
{
	PLOTOBJHANDLING *thePOH;
	WORKPROCS *theWP;
	ELEMWISEWORK *theEWW;
	VECTORWISEWORK *theVWW;
    EXTERNWORK *theEXW;
    RECURSIVEWORK *theREW;
	#ifdef __TWODIM__
		NODEWISEWORK *theNWW;
	#endif
	
	/* create WorkHandling for 'Matrix' */
	if ((thePOH=CreatePlotObjHandling ("Matrix"))	   == NULL) return (__LINE__);
		
	POH_DYNAMIC_INFO(thePOH) = DynInfo_Matrix;
	
	/* draw work */
	POH_NBCYCLES(thePOH,DRAW_WORK) = 2;

	theWP = POH_WORKPROGS(thePOH,DRAW_WORK,0);
	WP_WORKMODE(theWP) = VECTORWISE;
	theVWW = WP_VECTORWISE(theWP);
	theVWW->VW_PreProcessProc				= VW_MatrixPreProcess;
	theVWW->VW_GetFirstVectorProcProc		= VW_GetFirstVector_Proc;
	theVWW->VW_GetNextVectorProcProc		= VW_GetNextVector_Proc;
	theVWW->VW_EvaluateProc 				= VW_MatrixEval;
	theVWW->VW_ExecuteProc					= Draw2D;
	theVWW->VW_PostProcessProc				= NULL;

	theWP = POH_WORKPROGS(thePOH,DRAW_WORK,1);
	WP_WORKMODE(theWP) = RECURSIVE;
	theREW = WP_RECURSIVEWISE(theWP);
	theREW->RECURSIVE_PreProcessProc				= RECURSIVE_BVPreProcess;
	theREW->RECURSIVE_EvaluateProc					= RECURSIVE_BVEval;
	theREW->RECURSIVE_ExecuteProc					= Draw2D;
	theREW->RECURSIVE_PostProcessProc				= NULL;

	/* findrange work */
	POH_NBCYCLES(thePOH,FINDRANGE_WORK) = 1;
	
	theWP = POH_WORKPROGS(thePOH,FINDRANGE_WORK,0);
	WP_WORKMODE(theWP) = VECTORWISE;
	theVWW = WP_VECTORWISE(theWP);
	theVWW->VW_PreProcessProc				= VW_PreProcess_Matrix_FR;
	theVWW->VW_GetFirstVectorProcProc		= VW_GetFirstVector_Proc;
	theVWW->VW_GetNextVectorProcProc		= VW_GetNextVector_Proc;
	theVWW->VW_EvaluateProc 				= VW_MatrixEval;
	theVWW->VW_ExecuteProc					= FindRange2D;
	theVWW->VW_PostProcessProc				= GEN_PostProcess_Matrix_FR;
	
	#ifdef __TWODIM__
	
		/* create WorkHandling for 'Grid' */
		if ((thePOH=CreatePlotObjHandling ("Grid")) 	== NULL) return (__LINE__);
		
		POH_DYNAMIC_INFO(thePOH) = DynInfo_Grid2D;
		POH_CLICKACTION(thePOH)  = ClickAct_Grid2D;
		
		/* draw work */
		POH_NBCYCLES(thePOH,DRAW_WORK) = 4;
		
		theWP = POH_WORKPROGS(thePOH,DRAW_WORK,0);
		WP_WORKMODE(theWP) = ELEMENTWISE;
		theEWW = WP_ELEMWISE(theWP);
		theEWW->EW_PreProcessProc				= EW_PreProcess_PlotElements2D;
		theEWW->EW_GetFirstElementProcProc		= EW_GetFirstElement_vert_fw_up_Proc;
		theEWW->EW_GetNextElementProcProc		= EW_GetNextElement_vert_fw_up_Proc;
		theEWW->EW_EvaluateProc 				= EW_ElementEval2D;
		theEWW->EW_ExecuteProc					= Draw2D;
		theEWW->EW_PostProcessProc				= InvertElementSelection2D;

		theWP = POH_WORKPROGS(thePOH,DRAW_WORK,1);
		WP_WORKMODE(theWP) = ELEMENTWISE;
		theEWW = WP_ELEMWISE(theWP);
		theEWW->EW_PreProcessProc				= EW_PreProcess_PlotBndOfElem2D;
		theEWW->EW_GetFirstElementProcProc		= EW_GetFirstElement_vert_fw_up_Proc;
		theEWW->EW_GetNextElementProcProc		= EW_GetNextElement_vert_fw_up_Proc;
		theEWW->EW_EvaluateProc 				= EW_BndOfElemEval2D;
		theEWW->EW_ExecuteProc					= Draw2D;
		theEWW->EW_PostProcessProc				= NULL;
		
		theWP = POH_WORKPROGS(thePOH,DRAW_WORK,2);
		WP_WORKMODE(theWP) = ELEMENTWISE;
		theEWW = WP_ELEMWISE(theWP);
		theEWW->EW_PreProcessProc				= EW_PreProcess_Bnd2D;
		theEWW->EW_GetFirstElementProcProc		= EW_GetFirstElement_vert_fw_up_Proc;
		theEWW->EW_GetNextElementProcProc		= EW_GetNextElement_vert_fw_up_Proc;
	    theEWW->EW_EvaluateProc			     	= EW_BndEval2D;
		theEWW->EW_ExecuteProc					= Draw2D;
		theEWW->EW_PostProcessProc				= EW_PostProcess_Bnd2D;

		theWP = POH_WORKPROGS(thePOH,DRAW_WORK,3);
		WP_WORKMODE(theWP) = NODEWISE;
		theNWW = WP_NODEWISE(theWP);
		theNWW->NW_PreProcessProc				= NW_PreProcess_PlotNodes2D;
		theNWW->NW_GetFirstNodeProcProc 		= NW_GetFirstNode_hor_fw_up_Proc;
		theNWW->NW_GetNextNodeProcProc			= NW_GetNextNode_hor_fw_up_Proc;
		theNWW->NW_EvaluateProc 				= NW_NodesEval2D;
		theNWW->NW_ExecuteProc					= Draw2D;
		theNWW->NW_PostProcessProc				= InvertNodeSelection2D;

		/* selectnode work */
		POH_NTOOLFUNC(thePOH,handTool) = 1;
		strcpy(POH_TOOLNAME(thePOH,handTool,0),"select node");
		
		POH_NBCYCLES(thePOH,SELECTNODE_WORK) = 1;

		theWP = POH_WORKPROGS(thePOH,SELECTNODE_WORK,0);
		WP_WORKMODE(theWP) = NODEWISE;
		theNWW = WP_NODEWISE(theWP);
		theNWW->NW_PreProcessProc				= NW_PreProcess_SelectNode2D;
		theNWW->NW_GetFirstNodeProcProc 		= NW_GetFirstNode_hor_fw_up_Proc;
		theNWW->NW_GetNextNodeProcProc			= NW_GetNextNode_hor_fw_up_Proc;
		theNWW->NW_EvaluateProc 				= NW_SelectNodeEval2D;
		theNWW->NW_ExecuteProc					= NW_SelectNode2D;
		theNWW->NW_PostProcessProc				= NULL;

		/* selectelement work */
		POH_NTOOLFUNC(thePOH,heartTool) = 1;
		strcpy(POH_TOOLNAME(thePOH,heartTool,0),"select element");
		
		POH_NBCYCLES(thePOH,SELECTELEMENT_WORK) = 1;
		
		theWP = POH_WORKPROGS(thePOH,SELECTELEMENT_WORK,0);
		WP_WORKMODE(theWP) = ELEMENTWISE;
		theEWW = WP_ELEMWISE(theWP);
		theEWW->EW_PreProcessProc				= EW_PreProcess_SelectElement2D;
		theEWW->EW_GetFirstElementProcProc		= EW_GetFirstElement_vert_fw_up_Proc;
		theEWW->EW_GetNextElementProcProc		= EW_GetNextElement_vert_fw_up_Proc;
		theEWW->EW_EvaluateProc 				= EW_ElementEval2D;
		theEWW->EW_ExecuteProc					= EW_SelectElement2D;
		theEWW->EW_PostProcessProc				= NULL;
		
		/* markelement work */
		POH_NTOOLFUNC(thePOH,gnoedelTool) = 5;
		strcpy(POH_TOOLNAME(thePOH,gnoedelTool,0),"unmark");
		strcpy(POH_TOOLNAME(thePOH,gnoedelTool,1),"mark copy");
		strcpy(POH_TOOLNAME(thePOH,gnoedelTool,2),"mark red");
		strcpy(POH_TOOLNAME(thePOH,gnoedelTool,3),"mark blue");
		strcpy(POH_TOOLNAME(thePOH,gnoedelTool,4),"mark coarse");
		
		POH_NBCYCLES(thePOH,MARKELEMENT_WORK) = 1;
		
		theWP = POH_WORKPROGS(thePOH,MARKELEMENT_WORK,0);
		WP_WORKMODE(theWP) = ELEMENTWISE;
		theEWW = WP_ELEMWISE(theWP);
		theEWW->EW_PreProcessProc				= EW_PreProcess_MarkElement2D;
		theEWW->EW_GetFirstElementProcProc		= EW_GetFirstElement_vert_fw_up_Proc;
		theEWW->EW_GetNextElementProcProc		= EW_GetNextElement_vert_fw_up_Proc;
		theEWW->EW_EvaluateProc 				= EW_MarkElementEval2D;
		theEWW->EW_ExecuteProc					= EW_MarkElement2D;
		theEWW->EW_PostProcessProc				= NULL;
		
		/* insertnode work */
		POH_NTOOLFUNC(thePOH,circleTool) = 1;
		strcpy(POH_TOOLNAME(thePOH,circleTool,0),"insert node");
		
		POH_NBCYCLES(thePOH,INSERTNODE_WORK) = 1;
		
		theWP = POH_WORKPROGS(thePOH,INSERTNODE_WORK,0);
		WP_WORKMODE(theWP) = EXTERN;
		theEXW = WP_EXTERNWISE(theWP);
		theEXW->EXT_PreProcessProc				= EXT_PreProcess_InsertNode2D;
		theEXW->EXT_EvaluateProc				= EXT_NodesEval2D;
		theEXW->EXT_ExecuteProc					= Draw2D;
		theEXW->EXT_PostProcessProc				= NULL;

		/* movenode work */
		POH_NTOOLFUNC(thePOH,choiceTool) = 1;
		strcpy(POH_TOOLNAME(thePOH,choiceTool,0),"move node");
		
		POH_NBCYCLES(thePOH,MOVENODE_WORK) = 1;
		
		theWP = POH_WORKPROGS(thePOH,MOVENODE_WORK,0);
		WP_WORKMODE(theWP) = EXTERN;
		theEXW = WP_EXTERNWISE(theWP);
		theEXW->EXT_PreProcessProc				= EXT_PreProcess_MoveNode2D;
		theEXW->EXT_EvaluateProc				= EXT_MoveNodeEval2D;
		theEXW->EXT_ExecuteProc					= Draw2D;
		theEXW->EXT_PostProcessProc				= EXT_PostProcess_MoveNode2D;

		/* insertbndnode work */
		POH_NTOOLFUNC(thePOH,crossTool) = 1;
		strcpy(POH_TOOLNAME(thePOH,crossTool,0),"insert bnd node");
		
		POH_NBCYCLES(thePOH,INSERTBNDNODE_WORK) = 1;
		
		theWP = POH_WORKPROGS(thePOH,INSERTBNDNODE_WORK,0);
		WP_WORKMODE(theWP) = EXTERN;
		theEXW = WP_EXTERNWISE(theWP);
		theEXW->EXT_PreProcessProc				= EXT_PreProcess_InsertNode2D;
		theEXW->EXT_EvaluateProc				= EXT_NodesEval2D;
		theEXW->EXT_ExecuteProc					= Draw2D;
		theEXW->EXT_PostProcessProc				= NULL;


		/* create WorkHandling for 'EScalar' */
		if ((thePOH=CreatePlotObjHandling ("EScalar"))	   == NULL) return (__LINE__);
		
		POH_DYNAMIC_INFO(thePOH) = DynInfo_EScalar2D;
		
		/* draw work */
		POH_NBCYCLES(thePOH,DRAW_WORK) = 4;
		
		theWP = POH_WORKPROGS(thePOH,DRAW_WORK,0);
		WP_WORKMODE(theWP) = ELEMENTWISE;
		theEWW = WP_ELEMWISE(theWP);
		theEWW->EW_PreProcessProc				= EW_PreProcess_PlotGridBefore2D;
		theEWW->EW_GetFirstElementProcProc		= EW_GetFirstElement_vert_fw_up_Proc;
		theEWW->EW_GetNextElementProcProc		= EW_GetNextElement_vert_fw_up_Proc;
		theEWW->EW_EvaluateProc 				= EW_ElementBdryEval2D;
		theEWW->EW_ExecuteProc					= Draw2D;
		theEWW->EW_PostProcessProc				= NULL;
		
		theWP = POH_WORKPROGS(thePOH,DRAW_WORK,1);
		WP_WORKMODE(theWP) = ELEMENTWISE;
		theEWW = WP_ELEMWISE(theWP);
		theEWW->EW_PreProcessProc				= EW_PreProcess_EScalar2D;
		theEWW->EW_GetFirstElementProcProc		= EW_GetFirstElement_vert_fw_up_Proc;
		theEWW->EW_GetNextElementProcProc		= EW_GetNextElement_vert_fw_up_Proc;
		theEWW->EW_EvaluateProc 				= EW_EScalar2D;
		theEWW->EW_ExecuteProc					= Draw2D;
		theEWW->EW_PostProcessProc				= NULL;

		theWP = POH_WORKPROGS(thePOH,DRAW_WORK,2);
		WP_WORKMODE(theWP) = ELEMENTWISE;
		theEWW = WP_ELEMWISE(theWP);
		theEWW->EW_PreProcessProc				= EW_PreProcess_PlotBlackBnd2D;
		theEWW->EW_GetFirstElementProcProc		= EW_GetFirstElement_vert_fw_up_Proc;
		theEWW->EW_GetNextElementProcProc		= EW_GetNextElement_vert_fw_up_Proc;
		theEWW->EW_EvaluateProc 				= EW_BndOfElemEval2D;
		theEWW->EW_ExecuteProc					= Draw2D;
		theEWW->EW_PostProcessProc				= NULL;
		
		theWP = POH_WORKPROGS(thePOH,DRAW_WORK,3);
		WP_WORKMODE(theWP) = ELEMENTWISE;
		theEWW = WP_ELEMWISE(theWP);
		theEWW->EW_PreProcessProc				= EW_PreProcess_PlotGridAfter2D;
		theEWW->EW_GetFirstElementProcProc		= EW_GetFirstElement_vert_fw_up_Proc;
		theEWW->EW_GetNextElementProcProc		= EW_GetNextElement_vert_fw_up_Proc;
		theEWW->EW_EvaluateProc 				= EW_ElementBdryEval2D;
		theEWW->EW_ExecuteProc					= Draw2D;
		theEWW->EW_PostProcessProc				= NULL;

		/* findrange work */
		POH_NBCYCLES(thePOH,FINDRANGE_WORK) = 1;
		
		theWP = POH_WORKPROGS(thePOH,FINDRANGE_WORK,0);
		WP_WORKMODE(theWP) = ELEMENTWISE;
		theEWW = WP_ELEMWISE(theWP);
		theEWW->EW_PreProcessProc				= EW_PreProcess_EScalar2D_FR;
		theEWW->EW_GetFirstElementProcProc		= EW_GetFirstElement_vert_fw_up_Proc;
		theEWW->EW_GetNextElementProcProc		= EW_GetNextElement_vert_fw_up_Proc;
		theEWW->EW_EvaluateProc 				= EW_EScalar2D;
		theEWW->EW_ExecuteProc					= FindRange2D;
		theEWW->EW_PostProcessProc				= GEN_PostProcess_Scalar_FR;


		/* create WorkHandling for 'EVector' */
		if ((thePOH=CreatePlotObjHandling ("EVector"))	== NULL) return (__LINE__);
		
		POH_DYNAMIC_INFO(thePOH) = DynInfo_EVector2D;
		
		/* draw work */
		POH_NBCYCLES(thePOH,DRAW_WORK) = 3;
		
		theWP = POH_WORKPROGS(thePOH,DRAW_WORK,0);
		WP_WORKMODE(theWP) = ELEMENTWISE;
		theEWW = WP_ELEMWISE(theWP);
		theEWW->EW_PreProcessProc				= EW_EVectorPreProcess_PlotGridBefore2D;
		theEWW->EW_GetFirstElementProcProc		= EW_GetFirstElement_vert_fw_up_Proc;
		theEWW->EW_GetNextElementProcProc		= EW_GetNextElement_vert_fw_up_Proc;
		theEWW->EW_EvaluateProc 				= EW_ElementBdryEval2D;
		theEWW->EW_ExecuteProc					= Draw2D;
		theEWW->EW_PostProcessProc				= NULL;
		
		theWP = POH_WORKPROGS(thePOH,DRAW_WORK,1);
		WP_WORKMODE(theWP) = ELEMENTWISE;
		theEWW = WP_ELEMWISE(theWP);
		theEWW->EW_PreProcessProc				= EW_PreProcess_PlotBlackBnd2D;
		theEWW->EW_GetFirstElementProcProc		= EW_GetFirstElement_vert_fw_up_Proc;
		theEWW->EW_GetNextElementProcProc		= EW_GetNextElement_vert_fw_up_Proc;
		theEWW->EW_EvaluateProc 				= EW_BndOfElemEval2D;
		theEWW->EW_ExecuteProc					= Draw2D;
		theEWW->EW_PostProcessProc				= NULL;

		theWP = POH_WORKPROGS(thePOH,DRAW_WORK,2);
		WP_WORKMODE(theWP) = ELEMENTWISE;
		theEWW = WP_ELEMWISE(theWP);
		theEWW->EW_PreProcessProc				= EW_PreProcess_EVector2D;
		theEWW->EW_GetFirstElementProcProc		= EW_GetFirstElement_vert_fw_up_Proc;
		theEWW->EW_GetNextElementProcProc		= EW_GetNextElement_vert_fw_up_Proc;
		theEWW->EW_EvaluateProc 				= EW_EVector2D;
		theEWW->EW_ExecuteProc					= Draw2D;
		theEWW->EW_PostProcessProc				= NULL;

		/* findrange work */
		POH_NBCYCLES(thePOH,FINDRANGE_WORK) = 1;
		
		theWP = POH_WORKPROGS(thePOH,FINDRANGE_WORK,0);
		WP_WORKMODE(theWP) = ELEMENTWISE;
		theEWW = WP_ELEMWISE(theWP);
		theEWW->EW_PreProcessProc				= EW_PreProcess_EVector2D_FR;
		theEWW->EW_GetFirstElementProcProc		= EW_GetFirstElement_vert_fw_up_Proc;
		theEWW->EW_GetNextElementProcProc		= EW_GetNextElement_vert_fw_up_Proc;
		theEWW->EW_EvaluateProc 				= EW_EVector2D;
		theEWW->EW_ExecuteProc					= FindRange2D;
		theEWW->EW_PostProcessProc				= GEN_PostProcess_Vector_FR;
		
		
		/* create WorkHandling for 'VecMat' */
		if ((thePOH=CreatePlotObjHandling ("VecMat"))	   == NULL) return (__LINE__);
		
		POH_DYNAMIC_INFO(thePOH) = DynInfo_VecMat2D;
		POH_CLICKACTION(thePOH)  = ClickAct_VecMat2D;
		
		/* draw work */
		POH_NBCYCLES(thePOH,DRAW_WORK) = 3;
		
		theWP = POH_WORKPROGS(thePOH,DRAW_WORK,0);
		WP_WORKMODE(theWP) = ELEMENTWISE;
		theEWW = WP_ELEMWISE(theWP);
		theEWW->EW_PreProcessProc				= EW_PreProcess_VecMatBnd2D;
		theEWW->EW_GetFirstElementProcProc		= EW_GetFirstElement_vert_fw_up_Proc;
		theEWW->EW_GetNextElementProcProc		= EW_GetNextElement_vert_fw_up_Proc;
	    theEWW->EW_EvaluateProc				    = EW_BndEval2D;
		theEWW->EW_ExecuteProc					= Draw2D;
		theEWW->EW_PostProcessProc				= EW_PostProcess_Bnd2D;
		
		theWP = POH_WORKPROGS(thePOH,DRAW_WORK,1);
		WP_WORKMODE(theWP) = VECTORWISE;
		theVWW = WP_VECTORWISE(theWP);
		theVWW->VW_PreProcessProc				= VW_VecMatPreProcess;
		theVWW->VW_GetFirstVectorProcProc		= VW_GetFirstVector_Proc;
		theVWW->VW_GetNextVectorProcProc		= VW_GetNextVector_Proc;
		theVWW->VW_EvaluateProc 				= VW_MatEval;
		theVWW->VW_ExecuteProc					= Draw2D;
		theVWW->VW_PostProcessProc				= NULL;

		theWP = POH_WORKPROGS(thePOH,DRAW_WORK,2);
		WP_WORKMODE(theWP) = VECTORWISE;
		theVWW = WP_VECTORWISE(theWP);
		theVWW->VW_PreProcessProc				= NULL;
		theVWW->VW_GetFirstVectorProcProc		= VW_GetFirstVector_Proc;
		theVWW->VW_GetNextVectorProcProc		= VW_GetNextVector_Proc;
		theVWW->VW_EvaluateProc 				= VW_VecEval;
		theVWW->VW_ExecuteProc					= Draw2D;
		theVWW->VW_PostProcessProc				= InvertVectorSelectionOrPlotVMData2D;
		
		/* selectvector work */
		POH_NTOOLFUNC(thePOH,handTool) = 1;
		strcpy(POH_TOOLNAME(thePOH,handTool,0),"select vector");
		
		POH_NBCYCLES(thePOH,SELECTVECTOR_WORK) = 1;

		theWP = POH_WORKPROGS(thePOH,SELECTVECTOR_WORK,0);
		WP_WORKMODE(theWP) = VECTORWISE;
		theVWW = WP_VECTORWISE(theWP);
		theVWW->VW_PreProcessProc				= VW_PreProcess_SelectVector2D;
		theVWW->VW_GetFirstVectorProcProc		= VW_GetFirstVector_Proc;
		theVWW->VW_GetNextVectorProcProc		= VW_GetNextVector_Proc;
		theVWW->VW_EvaluateProc 				= VW_SelectVectorEval2D;
		theVWW->VW_ExecuteProc					= VW_SelectVector2D;
		theVWW->VW_PostProcessProc				= NULL;
				

		/* create WorkHandling for 'Line' */
		if ((thePOH=CreatePlotObjHandling ("Line"))	   == NULL) return (__LINE__);
		
		/* draw work */
		POH_NBCYCLES(thePOH,DRAW_WORK) = 1;
		
		theWP = POH_WORKPROGS(thePOH,DRAW_WORK,0);
		WP_WORKMODE(theWP) = ELEMENTWISE;
		theEWW = WP_ELEMWISE(theWP);
		theEWW->EW_PreProcessProc				= EW_PreProcess_Line2D;
		theEWW->EW_GetFirstElementProcProc		= EW_GetFirstElement_vert_fw_up_Proc;
		theEWW->EW_GetNextElementProcProc		= EW_GetNextElement_vert_fw_up_Proc;
		theEWW->EW_EvaluateProc 				= EW_LineElement2D;
		theEWW->EW_ExecuteProc					= Draw2D;
		theEWW->EW_PostProcessProc				= EW_PostProcess_Line2D;
		
		/* findrange work */
		POH_NBCYCLES(thePOH,FINDRANGE_WORK) = 1;
		
		theWP = POH_WORKPROGS(thePOH,FINDRANGE_WORK,0);
		WP_WORKMODE(theWP) = ELEMENTWISE;
		theEWW = WP_ELEMWISE(theWP);
		theEWW->EW_PreProcessProc				= EW_PreProcess_Line2D_FR;
		theEWW->EW_GetFirstElementProcProc		= EW_GetFirstElement_vert_fw_up_Proc;
		theEWW->EW_GetNextElementProcProc		= EW_GetNextElement_vert_fw_up_Proc;
		theEWW->EW_EvaluateProc 				= EW_LineElement2D;
		theEWW->EW_ExecuteProc					= FindRange2D;
		theEWW->EW_PostProcessProc				= GEN_PostProcess_Line_FR;
	
	#endif
		
		
		
	#ifdef __THREEDIM__
	
		/* allocate VSIDES, COUNT, NORDER and CUTMODE in element ctrl or element flag */
		if (AllocateControlEntry(ELEMENT_CW,VSIDES_LEN,&ce_VSIDES) != GM_OK)
			if (AllocateControlEntry(FLAG_CW,VSIDES_LEN,&ce_VSIDES) != GM_OK) 
				return (__LINE__);
		if (AllocateControlEntry(ELEMENT_CW,NORDER_LEN,&ce_NORDER) != GM_OK)
			if (AllocateControlEntry(FLAG_CW,NORDER_LEN,&ce_NORDER) != GM_OK)
				return (__LINE__);
		if (AllocateControlEntry(ELEMENT_CW,COUNT_LEN,&ce_COUNT) != GM_OK)
			if (AllocateControlEntry(FLAG_CW,COUNT_LEN,&ce_COUNT) != GM_OK) 
				return (__LINE__);
		if (AllocateControlEntry(ELEMENT_CW,CUTMODE_LEN,&ce_CUTMODE) != GM_OK)
			if (AllocateControlEntry(FLAG_CW,CUTMODE_LEN,&ce_CUTMODE) != GM_OK)
				return (__LINE__);
	
		/* create WorkHandling for 'Grid' */
		if ((thePOH=CreatePlotObjHandling ("Grid")) 	== NULL) return (__LINE__);
		
		POH_CLICKACTION(thePOH)  = ClickAct_Grid3D;
		
		/* draw work */
		POH_NBCYCLES(thePOH,DRAW_WORK) = 1;
		
		theWP = POH_WORKPROGS(thePOH,DRAW_WORK,0);
		WP_WORKMODE(theWP) = ELEMENTWISE;
		theEWW = WP_ELEMWISE(theWP);
		theEWW->EW_PreProcessProc				= EW_PreProcess_PlotGrid3D;
		theEWW->EW_GetFirstElementProcProc		= EW_GetFirstElement_vert_fw_up_Proc;
		theEWW->EW_GetNextElementProcProc		= EW_GetNextElement_vert_fw_up_Proc;
		theEWW->EW_EvaluateProc 				= EW_ElementEval3D;
		theEWW->EW_ExecuteProc					= Draw3D;
		theEWW->EW_PostProcessProc				= NULL;

		/* selectelement work */
		POH_NTOOLFUNC(thePOH,heartTool) = 1;
		strcpy(POH_TOOLNAME(thePOH,heartTool,0),"select element");
		
		POH_NBCYCLES(thePOH,SELECTELEMENT_WORK) = 1;
		
		theWP = POH_WORKPROGS(thePOH,SELECTELEMENT_WORK,0);
		WP_WORKMODE(theWP) = ELEMENTWISE;
		theEWW = WP_ELEMWISE(theWP);
		theEWW->EW_PreProcessProc				= EW_PreProcess_SelectElement3D;
		theEWW->EW_GetFirstElementProcProc		= EW_GetFirstElement_vert_bw_up_Proc;
		theEWW->EW_GetNextElementProcProc		= EW_GetNextElement_vert_bw_up_Proc;
		theEWW->EW_EvaluateProc 				= EW_ElementEval3D;
		theEWW->EW_ExecuteProc					= EW_SelectElement3D;
		theEWW->EW_PostProcessProc				= NULL;

		
		/* create WorkHandling for 'EScalar' */
		if ((thePOH=CreatePlotObjHandling ("EScalar"))	== NULL) return (__LINE__);
		
		/* draw work */
		POH_NBCYCLES(thePOH,DRAW_WORK) = 3;
		
		theWP = POH_WORKPROGS(thePOH,DRAW_WORK,0);
		WP_WORKMODE(theWP) = ELEMENTWISE;
		theEWW = WP_ELEMWISE(theWP);
		theEWW->EW_PreProcessProc				= EW_PreProcess_EScalar3D_BackGrid;
		theEWW->EW_GetFirstElementProcProc		= EW_GetFirstElement_vert_fw_up_Proc;
		theEWW->EW_GetNextElementProcProc		= EW_GetNextElement_vert_fw_up_Proc;
		theEWW->EW_EvaluateProc 				= EW_ElementEval3D;
		theEWW->EW_ExecuteProc					= Draw3D;
		theEWW->EW_PostProcessProc				= NULL;

		theWP = POH_WORKPROGS(thePOH,DRAW_WORK,1);
		WP_WORKMODE(theWP) = ELEMENTWISE;
		theEWW = WP_ELEMWISE(theWP);
		theEWW->EW_PreProcessProc				= EW_PreProcess_EScalar3D;
		theEWW->EW_GetFirstElementProcProc		= EW_GetFirstElement_vert_fw_up_Proc;
		theEWW->EW_GetNextElementProcProc		= EW_GetNextElement_vert_fw_up_Proc;
		theEWW->EW_EvaluateProc 				= EW_EScalar3D;
		theEWW->EW_ExecuteProc					= Draw3D;
		theEWW->EW_PostProcessProc				= NULL;

		theWP = POH_WORKPROGS(thePOH,DRAW_WORK,2);
		WP_WORKMODE(theWP) = ELEMENTWISE;
		theEWW = WP_ELEMWISE(theWP);
		theEWW->EW_PreProcessProc				= EW_PreProcess_CutBnd3D;
		theEWW->EW_GetFirstElementProcProc		= EW_GetFirstElement_vert_fw_up_Proc;
		theEWW->EW_GetNextElementProcProc		= EW_GetNextElement_vert_fw_up_Proc;
		theEWW->EW_EvaluateProc 				= EW_ECutBnd3D;
		theEWW->EW_ExecuteProc					= Draw3D;
		theEWW->EW_PostProcessProc				= NULL;

		/* findrange work */
		POH_NBCYCLES(thePOH,FINDRANGE_WORK) = 1;
		
		theWP = POH_WORKPROGS(thePOH,FINDRANGE_WORK,0);
		WP_WORKMODE(theWP) = ELEMENTWISE;
		theEWW = WP_ELEMWISE(theWP);
		theEWW->EW_PreProcessProc				= EW_PreProcess_EScalar3D_FR;
		theEWW->EW_GetFirstElementProcProc		= EW_GetFirstElement_vert_fw_up_Proc;
		theEWW->EW_GetNextElementProcProc		= EW_GetNextElement_vert_fw_up_Proc;
		theEWW->EW_EvaluateProc 				= EW_EScalar3D;
		theEWW->EW_ExecuteProc					= FindRange3D;
		theEWW->EW_PostProcessProc				= GEN_PostProcess_Scalar_FR;
		
		
		/* create WorkHandling for 'EVector' */
		if ((thePOH=CreatePlotObjHandling ("EVector"))	== NULL) return (__LINE__);
		
		/* draw work */
		POH_NBCYCLES(thePOH,DRAW_WORK) = 3;
		
		theWP = POH_WORKPROGS(thePOH,DRAW_WORK,0);
		WP_WORKMODE(theWP) = ELEMENTWISE;
		theEWW = WP_ELEMWISE(theWP);
		theEWW->EW_PreProcessProc				= EW_PreProcess_EVector3D_BackGrid;
		theEWW->EW_GetFirstElementProcProc		= EW_GetFirstElement_vert_fw_up_Proc;
		theEWW->EW_GetNextElementProcProc		= EW_GetNextElement_vert_fw_up_Proc;
		theEWW->EW_EvaluateProc 				= EW_ElementEval3D;
		theEWW->EW_ExecuteProc					= Draw3D;
		theEWW->EW_PostProcessProc				= NULL;

		theWP = POH_WORKPROGS(thePOH,DRAW_WORK,1);
		WP_WORKMODE(theWP) = ELEMENTWISE;
		theEWW = WP_ELEMWISE(theWP);
		theEWW->EW_PreProcessProc				= EW_PreProcess_CutBnd3D;
		theEWW->EW_GetFirstElementProcProc		= EW_GetFirstElement_vert_fw_up_Proc;
		theEWW->EW_GetNextElementProcProc		= EW_GetNextElement_vert_fw_up_Proc;
		theEWW->EW_EvaluateProc 				= EW_ECutBnd3D;
		theEWW->EW_ExecuteProc					= Draw3D;
		theEWW->EW_PostProcessProc				= NULL;

		theWP = POH_WORKPROGS(thePOH,DRAW_WORK,2);
		WP_WORKMODE(theWP) = ELEMENTWISE;
		theEWW = WP_ELEMWISE(theWP);
		theEWW->EW_PreProcessProc				= EW_PreProcess_EVector3D;
		theEWW->EW_GetFirstElementProcProc		= EW_GetFirstElement_vert_fw_up_Proc;
		theEWW->EW_GetNextElementProcProc		= EW_GetNextElement_vert_fw_up_Proc;
		theEWW->EW_EvaluateProc 				= EW_EVector3D;
		theEWW->EW_ExecuteProc					= Draw3D;
		theEWW->EW_PostProcessProc				= NULL;

		/* findrange work */
		POH_NBCYCLES(thePOH,FINDRANGE_WORK) = 1;
		
		theWP = POH_WORKPROGS(thePOH,FINDRANGE_WORK,0);
		WP_WORKMODE(theWP) = ELEMENTWISE;
		theEWW = WP_ELEMWISE(theWP);
		theEWW->EW_PreProcessProc				= EW_PreProcess_EVector3D_FR;
		theEWW->EW_GetFirstElementProcProc		= EW_GetFirstElement_vert_fw_up_Proc;
		theEWW->EW_GetNextElementProcProc		= EW_GetNextElement_vert_fw_up_Proc;
		theEWW->EW_EvaluateProc 				= EW_EVector3D;
		theEWW->EW_ExecuteProc					= FindRange3D;
		theEWW->EW_PostProcessProc				= GEN_PostProcess_Vector_FR;
		
	#endif
		
	/* set PlotObjTypes of PlotObjHandlings */
	if (InitPlotObjTypes()) return (__LINE__);

        #ifdef ModelP
        ConnectWopTree();
        NumberOfDesc(WOP_NbDesc);
        #endif

	return(0);
}
