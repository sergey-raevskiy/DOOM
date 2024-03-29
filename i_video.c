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
//	DOOM graphics stuff for X11, UNIX.
//
//-----------------------------------------------------------------------------

static const char
rcsid[] = "$Id: i_x.c,v 1.6 1997/02/03 22:45:10 b1 Exp $";

#include <stdlib.h>

#include <stdarg.h>
#include <sys/types.h>

#include <signal.h>

#include "doomstat.h"
#include "i_system.h"
#include "v_video.h"
#include "m_argv.h"
#include "d_main.h"

#include "doomdef.h"

#include "vga_fonts.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define POINTER_WARP_COUNTDOWN	1

// 80x25 text screen (640x400 pix)
#define TEXTSCREENHEIGHT 25
#define TEXTSCREENWIDTH 80
#define TEXTFONTHEIGHT 16
#define TEXTFONTWIDTH 8
#define TEXTSCREENPXHEIGHT (TEXTSCREENHEIGHT * TEXTFONTHEIGHT)
#define TEXTSCREENPXWIDTH (TEXTSCREENWIDTH * TEXTFONTWIDTH)

static BYTE g_TextScreen[TEXTSCREENHEIGHT*TEXTSCREENWIDTH][2];

static HWND g_MainWindow;
static HWND g_MainWindowDC;
static HBITMAP g_Bitmap;
static HBITMAP g_TextModeBitmap;
static HDC g_BitmapDC;
int		X_width;
int		X_height;

// Fake mouse handling.
// This cannot work properly w/o DGA.
// Needs an invisible mouse cursor at least.
boolean		grabMouse;
int		doPointerWarp = POINTER_WARP_COUNTDOWN;

// Blocky mode,
// replace each 320x200 pixel with multiply*multiply pixels.
// According to Dave Taylor, it still is a bonehead thing
// to use ....
static int	multiply=1;

// Current palette
static COLORREF	colors[256];

//
//  Translates the key currently in X_event
//

int xlatekey(int rc)
{
    switch(rc)
    {
      case VK_LEFT:	rc = KEY_LEFTARROW;	break;
      case VK_RIGHT:	rc = KEY_RIGHTARROW;	break;
      case VK_DOWN:	rc = KEY_DOWNARROW;	break;
      case VK_UP:	rc = KEY_UPARROW;	break;
      case VK_ESCAPE:	rc = KEY_ESCAPE;	break;
      case VK_RETURN:	rc = KEY_ENTER;		break;
      case VK_TAB:	rc = KEY_TAB;		break;
      case VK_F1:	rc = KEY_F1;		break;
      case VK_F2:	rc = KEY_F2;		break;
      case VK_F3:	rc = KEY_F3;		break;
      case VK_F4:	rc = KEY_F4;		break;
      case VK_F5:	rc = KEY_F5;		break;
      case VK_F6:	rc = KEY_F6;		break;
      case VK_F7:	rc = KEY_F7;		break;
      case VK_F8:	rc = KEY_F8;		break;
      case VK_F9:	rc = KEY_F9;		break;
      case VK_F10:	rc = KEY_F10;		break;
      case VK_F11:	rc = KEY_F11;		break;
      case VK_F12:	rc = KEY_F12;		break;
	
      case VK_BACK:
      case VK_DELETE:	rc = KEY_BACKSPACE;	break;

      case VK_PAUSE:	rc = KEY_PAUSE;		break;

      case VK_OEM_PLUS:	rc = KEY_EQUALS;	break;

      case VK_OEM_MINUS:	rc = KEY_MINUS;		break;

      case VK_SHIFT:
	rc = KEY_RSHIFT;
	break;
	
      case VK_CONTROL:
	rc = KEY_RCTRL;
	break;
	
      case VK_MENU:
      case VK_LWIN:
      case VK_RWIN:
	rc = KEY_RALT;
	break;
	
      default:
	if (rc >= 'A' && rc <= 'Z')
	    rc = rc - 'A' + 'a';
	break;
    }

    return rc;

}

void I_ShutdownGraphics(void)
{
#if 0
  // Detach from X server
  if (!XShmDetach(X_display, &X_shminfo))
	    I_Error("XShmDetach() failed in I_ShutdownGraphics()");

  // Release shared memory.
  shmdt(X_shminfo.shmaddr);
  shmctl(X_shminfo.shmid, IPC_RMID, 0);

  // Paranoia.
  image->data = NULL;
#endif
}



//
// I_StartFrame
//
void I_StartFrame (void)
{
    // er?

}

static int	lastmousex = 0;
static int	lastmousey = 0;
boolean		mousemoved = false;
boolean		shmFinished;

void I_GetEvent(void)
{
#if 0
    event_t event;

    // put event-grabbing stuff in here
    switch (X_Event.EventType)
    {
      case KEY_EVENT:
	event.type = X_Event.Event.KeyEvent.bKeyDown ? ev_keydown : ev_keyup;
	//event.data1 = xlatekey();
	D_PostEvent(&event);
	break;
      case ButtonPress:
	event.type = ev_mouse;
	event.data1 =
	    (X_event.xbutton.state & Button1Mask)
	    | (X_event.xbutton.state & Button2Mask ? 2 : 0)
	    | (X_event.xbutton.state & Button3Mask ? 4 : 0)
	    | (X_event.xbutton.button == Button1)
	    | (X_event.xbutton.button == Button2 ? 2 : 0)
	    | (X_event.xbutton.button == Button3 ? 4 : 0);
	event.data2 = event.data3 = 0;
	D_PostEvent(&event);
	// dprintf("b");
	break;
      case ButtonRelease:
	event.type = ev_mouse;
	event.data1 =
	    (X_event.xbutton.state & Button1Mask)
	    | (X_event.xbutton.state & Button2Mask ? 2 : 0)
	    | (X_event.xbutton.state & Button3Mask ? 4 : 0);
	// suggest parentheses around arithmetic in operand of |
	event.data1 =
	    event.data1
	    ^ (X_event.xbutton.button == Button1 ? 1 : 0)
	    ^ (X_event.xbutton.button == Button2 ? 2 : 0)
	    ^ (X_event.xbutton.button == Button3 ? 4 : 0);
	event.data2 = event.data3 = 0;
	D_PostEvent(&event);
	// dprintf("bu");
	break;
      case MotionNotify:
	event.type = ev_mouse;
	event.data1 =
	    (X_event.xmotion.state & Button1Mask)
	    | (X_event.xmotion.state & Button2Mask ? 2 : 0)
	    | (X_event.xmotion.state & Button3Mask ? 4 : 0);
	event.data2 = (X_event.xmotion.x - lastmousex) << 2;
	event.data3 = (lastmousey - X_event.xmotion.y) << 2;

	if (event.data2 || event.data3)
	{
	    lastmousex = X_event.xmotion.x;
	    lastmousey = X_event.xmotion.y;
	    if (X_event.xmotion.x != X_width/2 &&
		X_event.xmotion.y != X_height/2)
	    {
		D_PostEvent(&event);
		// dprintf("m");
		mousemoved = false;
	    } else
	    {
		mousemoved = true;
	    }
	}
	break;
	
      case Expose:
      case ConfigureNotify:
	break;
	
      default:
	//XXXif (doShm && X_event.type == X_shmeventtype) shmFinished = true;
	break;
    }
#endif
}

//
// I_StartTic
//
void I_StartTic (void)
{
	MSG msg;

	while (PeekMessage(&msg, g_MainWindow, 0, 0, PM_REMOVE))
	{
        TranslateMessage(&msg);
        DispatchMessage(&msg);
	}

    // Warp the pointer back to the middle of the window
    //  or it will wander off - that is, the game will
    //  loose input focus within X11.
    if (grabMouse)
    {
	if (!--doPointerWarp)
	{
	    //XXXXWarpPointer( X_display,
		//XXX	  None,
		//XXX	  X_mainWindow,
		//XXX	  0, 0,
		//XXX	  0, 0,
		//XXX	  X_width/2, X_height/2);
		//XXX
	    //XXXdoPointerWarp = POINTER_WARP_COUNTDOWN;
	}
    }

    mousemoved = false;
}


//
// I_UpdateNoBlit
//
void I_UpdateNoBlit (void)
{
    // what is this?
}

//
// I_FinishUpdate
//
void I_FinishUpdate (void)
{

    static int	lasttic;
    int		tics;
    int		i;
    // UNUSED static unsigned char *bigscreen=0;

    // draws little dots on the bottom of the screen
    if (devparm)
    {

	i = I_GetTime();
	tics = i - lasttic;
	lasttic = i;
	if (tics > 20) tics = 20;

	for (i=0 ; i<tics*2 ; i+=2)
	    screens[0][ (SCREENHEIGHT-1)*SCREENWIDTH + i] = 0xff;
	for ( ; i<20*2 ; i+=2)
	    screens[0][ (SCREENHEIGHT-1)*SCREENWIDTH + i] = 0x0;
    
    }

	static BYTE buf[SCREENHEIGHT * SCREENWIDTH * 3];

	for (int y = 0; y < SCREENHEIGHT; y++)
	{
		byte *srcline = &screens[0][y * SCREENWIDTH];
		BYTE *dstline = &buf[y * SCREENWIDTH * 3];

		for (int x = 0; x < SCREENWIDTH; x++)
		{
			byte pix = srcline[x];
			COLORREF c = colors[pix];
			dstline[x * 3 + 0] = (c >> 16) & 0xff;
			dstline[x * 3 + 1] = (c >> 8) & 0xff;
			dstline[x * 3 + 2] = (c >> 0) & 0xff;
		}
	}

	// create bitmap
	{
		BITMAPINFO BmpInfo;

		BmpInfo.bmiHeader.biSize = sizeof(BmpInfo.bmiHeader);
		BmpInfo.bmiHeader.biWidth = SCREENWIDTH;
		BmpInfo.bmiHeader.biHeight = 0 - SCREENHEIGHT;
		BmpInfo.bmiHeader.biPlanes = 1;
		BmpInfo.bmiHeader.biBitCount = 24;
		BmpInfo.bmiHeader.biCompression = BI_RGB;
		BmpInfo.bmiHeader.biSizeImage = 0;
		BmpInfo.bmiHeader.biXPelsPerMeter = 0;
		BmpInfo.bmiHeader.biYPelsPerMeter = 0;
		BmpInfo.bmiHeader.biClrUsed = 0;
		BmpInfo.bmiHeader.biClrImportant = 0;

		SetDIBits(g_BitmapDC, g_Bitmap, 0, SCREENHEIGHT, buf, &BmpInfo, DIB_RGB_COLORS);
	}

    HBITMAP old = SelectObject(g_BitmapDC, g_Bitmap);
    StretchBlt(g_MainWindowDC,
               0, 0, X_width, X_height,
               g_BitmapDC, 0, 0,
               SCREENWIDTH, SCREENHEIGHT, SRCCOPY);
	SelectObject(g_BitmapDC, old);
}


//
// I_ReadScreen
//
void I_ReadScreen (byte* scr)
{
    memcpy (scr, screens[0], SCREENWIDTH*SCREENHEIGHT);
}


//
// Palette stuff.
//

void UploadNewPalette(byte *palette)
{

    register int	i;
	byte r, g, b;

	    // set the X colormap entries
	    for (i=0 ; i<256 ; i++)
	    {
		r = gammatable[usegamma][*palette++];
		g = gammatable[usegamma][*palette++];
		b = gammatable[usegamma][*palette++];
		colors[i] = RGB(r, g, b);
	    }
}

//
// I_SetPalette
//
void I_SetPalette (byte* palette)
{
    UploadNewPalette(palette);
}

static LRESULT CALLBACK MainWinProc(HWND hw, UINT msg, WPARAM wp, LPARAM lp)
{
	event_t event;

	switch (msg)
	{
	case WM_KEYDOWN:
		event.type = ev_keydown;
        event.data1 = xlatekey(wp);
        D_PostEvent(&event);
		break;
    case WM_KEYUP:
        event.type = ev_keyup;
        event.data1 = xlatekey(wp);
        D_PostEvent(&event);
        break;
    case WM_CLOSE:
        event.type = ev_quit;
        D_PostEvent(&event);
        break;
	}

	return DefWindowProc(hw, msg, wp, lp);
}

void I_InitVideo()
{
	WNDCLASS wc = { 0 };
    wc.style = 0;
    wc.lpfnWndProc = MainWinProc;
    wc.cbClsExtra = wc.cbWndExtra = 0;
    wc.hInstance = NULL;
    wc.hIcon = NULL;
    wc.hCursor = NULL;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszMenuName = NULL;
    wc.lpszClassName = L"Doom Main Windows Class";
	if (!RegisterClass(&wc))
	{
		I_Error("RegisterClass() failed");
	}

    if (M_CheckParm("-1"))
        multiply = 1;

    if (M_CheckParm("-2"))
        multiply = 2;

    if (M_CheckParm("-3"))
        multiply = 3;

    if (M_CheckParm("-4"))
        multiply = 4;

    X_width = SCREENWIDTH * multiply;
    X_height = SCREENHEIGHT * multiply;

	RECT sz;
	sz.top = 0;
	sz.left = 0;
	sz.bottom = X_height;
	sz.right = X_width;
	AdjustWindowRect(&sz, WS_OVERLAPPEDWINDOW, FALSE);

    g_MainWindow = CreateWindow(L"Doom Main Windows Class",
                                L"DOOM",
                                WS_OVERLAPPEDWINDOW,
                                CW_USEDEFAULT, CW_USEDEFAULT,
                                sz.right-sz.left, sz.bottom-sz.top,
                                NULL, NULL, NULL, NULL);

	ShowWindow(g_MainWindow, SW_SHOWNA);

	g_MainWindowDC = GetDC(g_MainWindow);
    g_BitmapDC = CreateCompatibleDC(g_MainWindowDC);
    g_Bitmap = CreateCompatibleBitmap(g_MainWindowDC, SCREENWIDTH, SCREENHEIGHT);
	g_TextModeBitmap = CreateCompatibleBitmap(g_MainWindowDC, TEXTSCREENPXWIDTH, TEXTSCREENPXHEIGHT);
}

void I_TextSetAt(int x, int y, byte ch, byte attr)
{
	g_TextScreen[y*TEXTSCREENWIDTH + x][0] = ch;
	g_TextScreen[y*TEXTSCREENWIDTH + x][1] = attr;
}

static COLORREF g_TextModeColors[16] =
{
	RGB(0x00, 0x00, 0x00),
	RGB(0x00, 0x00, 0x80),
	RGB(0x00, 0x80, 0x00),
	RGB(0x00, 0x80, 0x80),
	RGB(0x80, 0x00, 0x00),
	RGB(0x80, 0x00, 0x80),
	RGB(0x80, 0x80, 0x00),
	RGB(0xc0, 0xc0, 0xc0),
	RGB(0x80, 0x80, 0x80),
	RGB(0x00, 0x00, 0xff),
	RGB(0x00, 0xff, 0x00),
	RGB(0x00, 0xff, 0xff),
	RGB(0xff, 0x00, 0x00),
	RGB(0xff, 0x00, 0xff),
	RGB(0xff, 0xff, 0x00),
	RGB(0xff, 0xff, 0xff),
};

void I_TextFlush()
{
	static BYTE buf[TEXTSCREENPXHEIGHT * TEXTSCREENPXWIDTH * 3];

	for (int y = 0; y < TEXTSCREENPXHEIGHT; y++)
	{
		BYTE *dstline = &buf[y * TEXTSCREENPXWIDTH * 3];

		for (int x = 0; x < TEXTSCREENPXWIDTH; x++)
		{
			int tx = x / TEXTFONTWIDTH;
			int ty = y / TEXTFONTHEIGHT;
			int gx = x % TEXTFONTWIDTH;
			int gy = y % TEXTFONTHEIGHT;
			BYTE ch = g_TextScreen[ty*TEXTSCREENWIDTH + tx][0];
			BYTE attr = g_TextScreen[ty*TEXTSCREENWIDTH + tx][1];
			COLORREF bgcolor = g_TextModeColors[attr >> 4];
			COLORREF fgcolor = g_TextModeColors[attr & 0xf];

			COLORREF c = ((font8x16[ch][gy] << gx) & 0x80) ? fgcolor : bgcolor;
			dstline[x * 3 + 0] = (c >> 16) & 0xff;
			dstline[x * 3 + 1] = (c >> 8) & 0xff;
			dstline[x * 3 + 2] = (c >> 0) & 0xff;
		}
	}

	// create bitmap
	{
		BITMAPINFO BmpInfo;

		BmpInfo.bmiHeader.biSize = sizeof(BmpInfo.bmiHeader);
		BmpInfo.bmiHeader.biWidth = TEXTSCREENPXWIDTH;
		BmpInfo.bmiHeader.biHeight = 0 - TEXTSCREENPXHEIGHT;
		BmpInfo.bmiHeader.biPlanes = 1;
		BmpInfo.bmiHeader.biBitCount = 24;
		BmpInfo.bmiHeader.biCompression = BI_RGB;
		BmpInfo.bmiHeader.biSizeImage = 0;
		BmpInfo.bmiHeader.biXPelsPerMeter = 0;
		BmpInfo.bmiHeader.biYPelsPerMeter = 0;
		BmpInfo.bmiHeader.biClrUsed = 0;
		BmpInfo.bmiHeader.biClrImportant = 0;

		SetDIBits(g_BitmapDC, g_TextModeBitmap, 0, TEXTSCREENPXHEIGHT, buf, &BmpInfo, DIB_RGB_COLORS);
	}

    HBITMAP old = SelectObject(g_BitmapDC, g_TextModeBitmap);
    StretchBlt(g_MainWindowDC,
               0, 0, X_width, X_height,
               g_BitmapDC, 0, 0,
               TEXTSCREENPXWIDTH, TEXTSCREENPXHEIGHT, SRCCOPY);
	SelectObject(g_BitmapDC, old);
}

void I_InitGraphics(void)
{
    char*		displayname;
    char*		d;
    int			n;
    int			pnum;
    int			x=0;
    int			y=0;
    
    // warning: char format, different type arg
    char		xsign=' ';
    char		ysign=' ';
    
    int			oktodraw;
    unsigned long	attribmask;
    int			valuemask;
    static int		firsttime=1;

    if (!firsttime)
	return;
    firsttime = 0;

    //XXXsignal(SIGINT, (void (*)(int)) I_Quit);

    if (M_CheckParm("-2"))
	multiply = 2;

    if (M_CheckParm("-3"))
	multiply = 3;

    if (M_CheckParm("-4"))
	multiply = 4;

    X_width = SCREENWIDTH * multiply;
    X_height = SCREENHEIGHT * multiply;

    // check for command-line display name
    if ( (pnum=M_CheckParm("-disp")) ) // suggest parentheses around assignment
	displayname = myargv[pnum+1];
    else
	displayname = 0;

    // check if the user wants to grab the mouse (quite unnice)
    grabMouse = !!M_CheckParm("-grabmouse");

    // check for command-line geometry
    if ( (pnum=M_CheckParm("-geom")) ) // suggest parentheses around assignment
    {
	// warning: char format, different type arg 3,5
	n = sscanf(myargv[pnum+1], "%c%d%c%d", &xsign, &x, &ysign, &y);
	
	if (n==2)
	    x = y = 0;
	else if (n==6)
	{
	    if (xsign == '-')
		x = -x;
	    if (ysign == '-')
		y = -y;
	}
	else
	    I_Error("bad -geom parameter");
    }
}
