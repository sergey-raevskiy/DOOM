/*
** mus2midi.cpp
** Simple converter from MUS to MIDI format
**
**---------------------------------------------------------------------------
** Copyright 1998-2006 Randy Heit
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
** MUS files are essentially format 0 MIDI files with some
** space-saving modifications. Conversion is quite straight-forward.
** If you were to hook a main() into this that calls ProduceMIDI,
** you could create a self-contained MUS->MIDI converter.
*/


#include <malloc.h>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <mmsystem.h>

#include "i_sound_mus.h"
#include "doomdef.h"
#include "doomtype.h"

#define MIDI_SYSEX		((byte)0xF0)		 // SysEx begin
#define MIDI_SYSEXEND	((byte)0xF7)		 // SysEx end
#define MIDI_META		((byte)0xFF)		 // Meta event begin
#define MIDI_META_TEMPO ((byte)0x51)
#define MIDI_META_EOT	((byte)0x2F)		 // End-of-track
#define MIDI_META_SSPEC	((byte)0x7F)		 // System-specific event

#define MIDI_NOTEOFF	((byte)0x80)		 // + note + velocity
#define MIDI_NOTEON 	((byte)0x90)		 // + note + velocity
#define MIDI_POLYPRESS	((byte)0xA0)		 // + pressure (2 bytes)
#define MIDI_CTRLCHANGE ((byte)0xB0)		 // + ctrlr + value
#define MIDI_PRGMCHANGE ((byte)0xC0)		 // + new patch
#define MIDI_CHANPRESS	((byte)0xD0)		 // + pressure (1 byte)
#define MIDI_PITCHBEND	((byte)0xE0)		 // + pitch bend (2 bytes)

#define MUS_NOTEOFF 	((byte)0x00)
#define MUS_NOTEON		((byte)0x10)
#define MUS_PITCHBEND	((byte)0x20)
#define MUS_SYSEVENT	((byte)0x30)
#define MUS_CTRLCHANGE	((byte)0x40)
#define MUS_SCOREEND	((byte)0x60)

#pragma pack(push, 1)
typedef struct
{
	byte Magic[4];
	unsigned short SongLen;
	unsigned short SongStart;
	unsigned short NumChans;
	unsigned short NumSecondaryChans;
	unsigned short NumInstruments;
	unsigned short Pad;
} MUSHeader;
#pragma pack(pop)

static const byte StaticMIDIhead[] =
{ 'M','T','h','d', 0, 0, 0, 6,
0, 0, // format 0: only one track
0, 1, // yes, there is really only one track
0, 70, // 70 divisions
'M','T','r','k', 0, 0, 0, 0,
// The first event sets the tempo to 500,000 microsec/quarter note
0, 255, 81, 3, 0x07, 0xa1, 0x20
};

static const byte MUSMagic[4] = { 'M', 'U', 'S', 0x1a };

static const byte CtrlTranslate[15] =
{
	0,	// program change
	0,	// bank select
	1,	// modulation pot
	7,	// volume
	10, // pan pot
	11, // expression pot
	91, // reverb depth
	93, // chorus depth
	64, // sustain pedal
	67, // soft pedal
	120, // all sounds off
	123, // all notes off
	126, // mono
	127, // poly
	121, // reset all controllers
};

typedef struct {
	byte *data;
	int len;
	int nalloc;
} Membuf;

static void MembufInit(Membuf *buf, int prep)
{
	buf->data = malloc(prep);
	buf->len = 0;
	buf->nalloc = prep;
}

static void MembufPush(Membuf *buf, const byte *data, int len)
{
	int req = buf->len + len;
	if (req > buf->nalloc)
	{
		buf->data = realloc(buf->data, buf->nalloc * 2);
		buf->nalloc *= 2;
	}

	memcpy(&buf->data[buf->len], data, len);
	buf->len += len;
}

static void MembufFree(Membuf *buf)
{
	free(buf->data);
	buf->data = NULL;
}

static size_t ReadVarLen(const byte *buf, int *time_out)
{
	int time = 0;
	size_t ofs = 0;
	byte t;

	do
	{
		t = buf[ofs++];
		time = (time << 7) | (t & 127);
	} while (t & 128);
	*time_out = time;
	return ofs;
}

static size_t WriteVarLen(Membuf *buf, int time)
{
	long buffer;
	size_t ofs;

	buffer = time & 0x7f;
	while ((time >>= 7) > 0)
	{
		buffer = (buffer << 8) | 0x80 | (time & 0x7f);
	}
	for (ofs = 0;;)
	{
		byte b = (byte)(buffer & 0xff);
		MembufPush(buf, &b, 1);
		if (buffer & 0x80)
			buffer >>= 8;
		else
			break;
	}
	return ofs;
}

// We are on intel anyways
#define LittleShort(a) (a)

static boolean DoMus2Midi(const byte *musBuf, Membuf *out)
{
	byte midStatus, midArgs, mid1, mid2;
	size_t mus_p, maxmus_p;
	byte event;
	int deltaTime;
	const MUSHeader *musHead = (const MUSHeader *) musBuf;
	byte status;
	byte chanUsed[16];
	byte lastVel[16];
	long trackLen;
	boolean no_op;
	MIDIEVENT ev;

	// Do some validation of the MUS file
	if (memcmp(musHead, MUSMagic, sizeof(MUSMagic)))
		return false;

	if (LittleShort(musHead->NumChans) > 15)
		return false;

	// Prep for conversion
	//MembufPush(out, StaticMIDIhead, sizeof(StaticMIDIhead));
	memset(&ev, 0, sizeof(ev));
	ev.dwDeltaTime = 0;
	ev.dwStreamID = 0;
	ev.dwEvent = 0x0107a120;
	MembufPush(out, &ev, 12);
	

	musBuf += LittleShort(musHead->SongStart);
	maxmus_p = LittleShort(musHead->SongLen);
	mus_p = 0;

	memset(lastVel, 100, 16);
	memset(chanUsed, 0, 16);
	event = 0;
	deltaTime = 0;
	status = 0;

	while (mus_p < maxmus_p && (event & 0x70) != MUS_SCOREEND)
	{
		int channel;
		byte t = 0;

		event = musBuf[mus_p++];

		if ((event & 0x70) != MUS_SCOREEND)
		{
			t = musBuf[mus_p++];
		}
		channel = event & 15;
		if (channel == 15)
		{
			channel = 9;
		}
		else if (channel >= 9)
		{
			channel++;
		}

		if (!chanUsed[channel])
		{
			// This is the first time this channel has been used,
			// so sets its volume to 127.
			chanUsed[channel] = 1;

			memset(&ev, 0, sizeof(ev));
			ev.dwDeltaTime = 0;
			ev.dwStreamID = 0;
			ev.dwEvent = 0x007f07b0 | channel;
			//byte data[4];
			//data[0] = 0;
			//data[1] = (0xB0 | channel);
			//data[2] = 7;
			//data[3] = 127;
			//MembufPush(out, data, sizeof(data));
			MembufPush(out, &ev, 12);
		}

		midStatus = channel;
		midArgs = 0;		// Most events have two args (0 means 2, 1 means 1)
		no_op = false;

		switch (event & 0x70)
		{
		case MUS_NOTEOFF:
			midStatus |= MIDI_NOTEOFF;
			mid1 = t & 127;
			mid2 = 64;
			break;

		case MUS_NOTEON:
			midStatus |= MIDI_NOTEON;
			mid1 = t & 127;
			if (t & 128)
			{
				lastVel[channel] = musBuf[mus_p++] & 127;
			}
			mid2 = lastVel[channel];
			break;

		case MUS_PITCHBEND:
			midStatus |= MIDI_PITCHBEND;
			mid1 = (t & 1) << 6;
			mid2 = (t >> 1) & 127;
			break;

		case MUS_SYSEVENT:
			if (t < 10 || t > 14)
			{
				no_op = true;
			}
			else
			{
				midStatus |= MIDI_CTRLCHANGE;
				mid1 = CtrlTranslate[t];
				mid2 = t == 12 /* Mono */ ? LittleShort(musHead->NumChans) : 0;
			}
			break;

		case MUS_CTRLCHANGE:
			if (t == 0)
			{ // program change
				midArgs = 1;
				midStatus |= MIDI_PRGMCHANGE;
				mid1 = musBuf[mus_p++] & 127;
				mid2 = 0;	// Assign mid2 just to make GCC happy
			}
			else if (t > 0 && t < 10)
			{
				midStatus |= MIDI_CTRLCHANGE;
				mid1 = CtrlTranslate[t];
				mid2 = musBuf[mus_p++];
			}
			else
			{
				no_op = true;
			}
			break;

		case MUS_SCOREEND:
			midStatus = MIDI_META;
			mid1 = MIDI_META_EOT;
			mid2 = 0;
			break;

		default:
			return false;
		}

		if (no_op)
		{
			// A system-specific event with no data is a no-op.
			midStatus = MIDI_META;
			mid1 = MIDI_META_SSPEC;
			mid2 = 0;
		}

		memset(&ev, 0, sizeof(ev));
		ev.dwDeltaTime = deltaTime;
		ev.dwStreamID = 0;

		//WriteVarLen(out, deltaTime);

		if (midStatus != status)
		{
			status = midStatus;
			//MembufPush(out, &status, 1);
			ev.dwEvent |= status;
		}
		//MembufPush(out, &mid1, 1);
		ev.dwEvent |= (mid1 << 8);
		if (midArgs == 0)
		{
			//MembufPush(out, &mid2, 1);
			ev.dwEvent |= (mid2 << 16);
		}

		MembufPush(out, &ev, sizeof(ev) - 4);

		if (event & 128)
		{
			mus_p += ReadVarLen(&musBuf[mus_p], &deltaTime);
		}
		else
		{
			deltaTime = 0;
		}
	}

	// fill in track length
	trackLen = out->len - 22;
	//out->data[18] = (byte)((trackLen >> 24) & 255);
	//out->data[19] = (byte)((trackLen >> 16) & 255);
	//out->data[20] = (byte)((trackLen >> 8) & 255);
	//out->data[21] = (byte)(trackLen & 255);
	return true;
}

boolean I_Mus2Midi(const byte *musBuf, byte **midi, int *midiLen)
{
	Membuf buf;
	MembufInit(&buf, 32);
	if (DoMus2Midi(musBuf, &buf))
	{
		*midi = buf.data;
		*midiLen = buf.len;
		return true;
	}
	else
	{
		MembufFree(&buf);
		return false;
	}
}
