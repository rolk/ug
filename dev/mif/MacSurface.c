// -*- tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
// vi: set et ts=4 sw=2 sts=2:
/****************************************************************************/
/*																			*/
/* File:	  MacSurface.c													*/
/*																			*/
/* Purpose:                                                                                                                             */
/*																			*/
/* Author:	  Peter Bastian/Henrik Rentz-Reichert							*/
/*			  Institut fuer Computeranwendungen III                                                 */
/*			  Universitaet Stuttgart										*/
/*			  Pfaffenwaldring 27											*/
/*			  70569 Stuttgart												*/
/*			  email: ug@ica3.uni-stuttgart.de						        */
/*																			*/
/* History:   29.01.92 begin, ug version 2.0								*/
/*			  13.02.95 begin, ug version 3.0								*/
/*																			*/
/* Remarks:   former MacGuSurfacei.c/.h                                                                         */
/*																			*/
/****************************************************************************/


/****************************************************************************/
/*																			*/
/* include files															*/
/*			  system include files											*/
/*			  application include files                                                                     */
/*																			*/
/****************************************************************************/

#include "config.h"

/* Macintosh toolbox includes */
#include <Types.h>
#include <Events.h>
#include <Menus.h>
#include <Windows.h>
#include <ToolUtils.h>
#include <Devices.h>
#include <MacWindows.h>

/* standard C includes */
#include <string.h>
#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

/* dev interface */
#include "ugdevices.h"

/* interface includes */
#include "ugtypes.h"
#include "general.h"


/* mif includes */
#include "MacMain.m"
#include "MacSurface.h"

/****************************************************************************/
/*																			*/
/* defines in the following order											*/
/*																			*/
/*		  compile time constants defining static data size (i.e. arrays)	*/
/*		  other constants													*/
/*		  macros															*/
/*																			*/
/****************************************************************************/

#define AboutMargin     0       /* margin in pixels seperating pict and window	*/

/****************************************************************************/
/*																			*/
/* data structures used in this source file (exported data structures are	*/
/*		  in the corresponding include file!)								*/
/*																			*/
/****************************************************************************/

/****************************************************************************/
/*																			*/
/* definition of exported global variables									*/
/*																			*/
/****************************************************************************/

/****************************************************************************/
/*																			*/
/* definition of variables global to this source file only (static!)		*/
/*																			*/
/****************************************************************************/

/* menu handles */
static MenuHandle myMenus[menuCount];   /* menuCount defined in MacGui.m	*/

static short currentCurs = 128;                 /* resource id of current cursor	*/

static Rect dragRect;                                   /* rect where windows can be moved	*/

int screenWidth;                                                /* We are not prepared for changing the */
int screenHeight;                                       /* screen size while program is running */

/* RCS string */
static char RCS_ID("$Header$",UG_RCS_STRING);

/****************************************************************************/
/*																			*/
/* forward declarations of functions used before they are defined			*/
/*																			*/
/****************************************************************************/

Rect *DragRect ()
{
  return(&dragRect);
}

void SetMyCursor (short id)
{
  CursHandle cursor;

  cursor = GetCursor(id);
  if (cursor==NULL) return;
  SetCursor(*cursor);
  currentCurs = id;
}

void MousePosition (INT *ScreenPoint)
{
  Point pt;

  GetMouse(&pt);
  ScreenPoint[0] = (INT) pt.h;
  ScreenPoint[1] = (INT) pt.v;
  return;
}

INT MouseStillDown(void)
{
  /*if (StillDown())
          return (1);
     else
     {
          char buffer[8];

          UserIn(buffer);
          return (0);
     }*/
  return (StillDown());
}

/****************************************************************************/
/*																			*/
/* Function:  About                                                                                                             */
/*																			*/
/* Purpose:   display about window											*/
/*																			*/
/* Input:	  none															*/
/*																			*/
/* Output:	  none															*/
/*																			*/
/****************************************************************************/

void About ()
{
  short h,v;
  int theEnd;
  EventRecord theEvent;
  WindowRef theWindow,whichWindow;
  GrafPtr myPort;
  PicHandle myPicture;
  Rect dstRect;



  h = (SCREEN_WIDTH+1-ABOUTPICT_X-2*AboutMargin)/2;
  v = (SCREEN_HEIGHT+1-ABOUTPICT_Y-2*AboutMargin)/2;

  theWindow = GetNewCWindow(ABOUT_RSRC_ID, nil, (WindowPtr)-1L);
  if (theWindow==NULL)
    return;

  myPort = (GrafPtr) theWindow;
  MoveWindow(theWindow,h,v,false);
  SizeWindow(theWindow,ABOUTPICT_X+2*AboutMargin,ABOUTPICT_Y+2*AboutMargin,false);
  ShowWindow(theWindow);
  SelectWindow(theWindow);
  SetPort(myPort);

  myPicture = GetPicture(ABOUTPICT_RSRC_ID);
  if (myPicture!=NULL)
  {
    SetRect(&dstRect,AboutMargin,AboutMargin,AboutMargin+ABOUTPICT_X,AboutMargin+ABOUTPICT_Y);
    DrawPicture(myPicture,&dstRect);
  }

  theEnd = 0;
  while (!theEnd)
  {
    if (GetNextEvent(everyEvent,&theEvent))
      switch (theEvent.what)
      {
      case mouseDown :
        if (FindWindow(theEvent.where,&whichWindow)==inContent)
          if (whichWindow==theWindow)
            theEnd = 1;
        break;

      case keyDown :
      case autoKey :
        theEnd = 1;
        break;
      }
  }

  DisposeWindow(theWindow);
}

/****************************************************************************/
/*																			*/
/* Function:  ScheduleCommand												*/
/*																			*/
/* Purpose:   pass control to the command selected from a menu				*/
/*																			*/
/* Input:	  short theMenu: menu number									*/
/*			  short theItem: item number									*/
/*																			*/
/* Output:	  none															*/
/*																			*/
/****************************************************************************/

void ScheduleCommand (short theMenu,short theItem)
{
  switch (theMenu)
  {
  case appleID :
    switch (theItem)
    {
    case aboutCommand :      About(); break;
    }
    break;

  default :
    break;
  }
  return;
}

/****************************************************************************/
/*																			*/
/* Function:  DoCommand                                                                                                         */
/*																			*/
/* Purpose:   process command selection                                                                         */
/*																			*/
/* Input:	  long: menu and item information								*/
/*																			*/
/* Output:	  none															*/
/*																			*/
/****************************************************************************/

void DoCommand (long mResult)
{
  short theItem,theMenu;
        #ifndef __USE_CARBON_FOR_UG__
  short temp;
  Str255 name;
        #endif

  theItem = LoWrd(mResult);
  theMenu = HiWrd(mResult);

  switch (theMenu)
  {

  case appleID :
    if (theItem==1)
      About();
    else
    {
                                #ifndef __USE_CARBON_FOR_UG__
      GetMenuItemText(myMenus[appleM],theItem,name);
      temp = OpenDeskAcc(name);
                                #endif
    }
    break;
  default :
    break;
  }
  HiliteMenu(0);
}

/****************************************************************************/
/*																			*/
/* Function:  SetUpMenus													*/
/*																			*/
/* Purpose:   read menus from resource file                                                             */
/*																			*/
/* Input:	  none															*/
/*																			*/
/* Output:	  none															*/
/*																			*/
/****************************************************************************/

void SetUpMenus ()
{
  int i;

  for (i=0; i<menuCount; i++)
  {
    myMenus[i] = GetMenu(i+appleID);
    assert(myMenus[i]!=NULL);
    AppendResMenu(myMenus[i],'DRVR');
  }
  for (i=0; i<menuCount; i++)
    InsertMenu(myMenus[i],0);                                           /* Add menus to menu bar */

  DrawMenuBar();
}

/****************************************************************************/
/*
   GetScreenSize - Return the available size of the screen

   SYNOPSIS:
   INT GetScreenSize (INT size[2]);

   PARAMETERS:
   .  size[2] - pointer to size vector

   DESCRIPTION:
   This function returns the available size of the screen.

   RETURN VALUE:
   INT
   .n     1 if screen available
   .n     0 if not.
 */

/****************************************************************************/

INT GetScreenSize (INT size[2])
{
  size[0] = SCREEN_WIDTH-SCROLL_BAR;
  size[1] = SCREEN_HEIGHT-MENU_BAR-TITLE_BAR;

  return (1);
}

/****************************************************************************/
/*
   InitMacGuiCommands - Load mac specific commands into environment

   SYNOPSIS:
   int InitMacSurface (void)

   PARAMETERS:
   .  void

   DESCRIPTION:
   This function loads mac specific commands into environment,
   makes Macintosh menus.

   RETURN VALUE:
   int
   0, if all is o.k.

   1, if an error occurred
 */
/****************************************************************************/


int InitMacSurface (void)
{
  /* set global variables screenWidth and screenHeight, used by
     SCREEN_WIDTH and SCREEN_HEIGHT */
  BitMap *bmap = nil;

  bmap = GetQDGlobalsScreenBits(bmap);
        #warning genauer anschauen
  screenWidth  = bmap->bounds.bottom;
  screenHeight = bmap->bounds.right;

  /* reactangle where windows can be moved */
  SetRect(&dragRect,      -1,
          MENU_BAR-SCROLL_BAR-1,
          SCREEN_WIDTH  + SCROLL_BAR,
          SCREEN_HEIGHT + SCROLL_BAR);

  return(0);
}

/****************************************************************************/
/*
   InitMacGuiCommands - Load mac specific commands into environment

   SYNOPSIS:
   int InitMacSurface (void)

   PARAMETERS:
   .  void

   DESCRIPTION:
   This function loads mac specific commands into environment,
   makes Macintosh menus.

   RETURN VALUE:
   int
   0, if all is o.k.

   1, if an error occurred
 */
/****************************************************************************/
int MacScreenWidth ( void )
{
  return screenWidth;
}


int MacScreenHeight( void )
{
  return screenHeight;
}
