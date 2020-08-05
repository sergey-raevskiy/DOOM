// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id:$
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//
// $Log:$
//
// DESCRIPTION:
//
//-----------------------------------------------------------------------------

static const char
rcsid[] = "$Id: m_bbox.c,v 1.1 1997/02/03 22:45:10 b1 Exp $";


#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <stdarg.h>

#include "doomdef.h"
#include "m_misc.h"
#include "i_video.h"
#include "i_sound.h"

#include "d_net.h"
#include "g_game.h"

#ifdef __GNUG__
#pragma implementation "i_system.h"
#endif
#include "i_system.h"




int	mb_used = 6;


void
I_Tactile
( int	on,
  int	off,
  int	total )
{
  // UNUSED.
  on = off = total = 0;
}

ticcmd_t	emptycmd;
ticcmd_t*	I_BaseTiccmd(void)
{
    return &emptycmd;
}


int  I_GetHeapSize (void)
{
    return mb_used*1024*1024;
}

byte* I_ZoneBase (int*	size)
{
    *size = mb_used*1024*1024;
    return (byte *) malloc (*size);
}

__declspec(dllimport) unsigned __int64 __stdcall GetTickCount64();

//
// I_GetTime
// returns time in 1/70th second tics
//
int  I_GetTime (void)
{
    unsigned __int64 tp;
    int			newtics;
    static unsigned __int64		basetime=0;
  
    tp = GetTickCount64();
    if (!basetime)
	basetime = tp;
    newtics = (tp-basetime)*TICRATE/1000;
    return newtics;
}



//
// I_Init
//
void I_Init (void)
{
    I_InitSound();
    //  I_InitGraphics();
	I_InitMusic();
}

//
// I_Quit
//
void I_Quit (void)
{
    D_QuitNetGame ();
    I_ShutdownSound();
    I_ShutdownMusic();
    M_SaveDefaults ();
    I_ShutdownGraphics();

int endoom = W_GetNumForName("ENDOOM");
	int len = W_LumpLength(endoom);
	byte *endoomData = malloc(len);
	W_ReadLump(endoom, endoomData);
	for (int i = 0;i < 25;i++)
	{
		for (int j = 0; j < 80; j++)
		{
			I_TextSetAt(j, i, endoomData[(i * 80 + j)*2+0], endoomData[(i * 80 + j) * 2 + 1]);
		}
	}

	I_TextFlush();

	for (;;);
}

void I_WaitVBL(int count)
{
#ifdef SGI
    sginap(1);                                           
#else
#ifdef SUN
    sleep(0);
#else
    //usleep (count * (1000000/70) );                                
#endif
#endif
}

void I_BeginRead(void)
{
}

void I_EndRead(void)
{
}

byte*	I_AllocLow(int length)
{
    byte*	mem;
        
    mem = (byte *)malloc (length);
    memset (mem,0,length);
    return mem;
}


//
// I_Error
//
extern boolean demorecording;

void I_Error (char *error, ...)
{
    va_list	argptr;

    // Message first.
    va_start (argptr,error);
    fprintf (stderr, "Error: ");
    vfprintf (stderr,error,argptr);
    fprintf (stderr, "\n");
    va_end (argptr);

    fflush( stderr );

    // Shutdown. Here might be other errors.
    if (demorecording)
	G_CheckDemoStatus();

    D_QuitNetGame ();
    I_ShutdownGraphics();
    
    exit(-1);
}

void dprintf(const char *fmt, ...)
{
	va_list	argptr;
	char buf[1024];

	va_start(argptr, fmt);
	vsprintf(buf, fmt, argptr);
	va_end(argptr);

	static int x = 0;
	static int y = 0;
	static byte attr = 7;

	char *p = buf;
	while (*p)
	{
		if (*p == '\n')
			x = 0, y++;
		else if (*p == 8)
		{
			if (x > 0)
				x--;
		}
		else {
			I_TextSetAt(x, y, *p, attr);
			x++;
			if (x >= 80)
				y++, x = 0;
		}
		if (y >= 25)
			y = 0;//kek
		p++;
	}

	I_TextFlush();
}
