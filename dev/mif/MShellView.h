// -*- tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
// vi: set et ts=4 sw=2 sts=2:
/****************************************************************************/
/*																			*/
/* File:	  MShellView.h													*/
/*																			*/
/* Purpose:   Interface for the MShellView class.							*/
/*																			*/
/* Author:	  Volker Reichenberger											*/
/*			  Interdisziplin"ares Zentrum f"ur Wissenschaftliches Rechnen	*/
/*			  Universit"at Heidelberg										*/
/*			  Im Neuenheimer Feld 368										*/
/*			  69210 Heidelberg												*/
/*			  email: Volker.Reichenberger@IWR.Uni-Heidelberg.DE		        */
/*																			*/
/*	History:  June 4, 1999 begin (based on OPENSTEP code)					*/
/*																			*/
/****************************************************************************/

#import <AppKit/AppKit.h>

#import "MShellTextView.h"

/* UG includes */
#include "ugtypes.h"
#include "misc.h"
#include "defaults.h"
#include "general.h"
#include "devices.h"
#include "cmdint.h"

@interface MShellView : NSView
{
  NSScrollView        *theScrollView;
  MShellTextView      *theTextView;
}

- (float)fontSize;
- (id)initWithFrame:(NSRect)frameRect;
- (id)initWithCoder:(NSCoder *)coder;
- (id) _init;

- (MShellTextView *)textView;
- (NSScrollView *)scrollView;

@end
