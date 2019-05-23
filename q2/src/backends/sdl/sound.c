/*
 * Copyright (C) 1997-2001 Id Software, Inc.
 * Copyright (C) 2010 Yamagi Burmeister
 * Copyright (C) 2005 Ryan C. Gordon
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307,
 * USA.
 *
 * =======================================================================
 *
 * The lower layer of the sound system. It utilizes SDL for writing the
 * sound data to the device. This file was rewritten by Yamagi to solve
 * a lot problems arrising from the old Icculus Quake 2 SDL backend like
 * stuttering and cracking. This implementation is based on ioQuake3s
 * snd_sdl.c.
 *
 * =======================================================================
 */

#include "../../client/header/client.h"
#include "../../client/sound/header/local.h"

void (*setState)(int shown);
void (*initAudio)(void *buffer, int size);
void (*writeAudio)(int offset, int length);

static int buf_size=0;
static int bytes_per_sample=0;
static int chunkSizeBytes=0;
static int dmapos=0;
static int snd_inited=0;

qboolean SNDDMA_Init(void)
{
    if (snd_inited)
    {
	return 1;
    }

    Com_Printf("Initializing Android Sound subsystem\n");

    dma.channels = 2;
    dma.samples = 32768;
    dma.samplebits = 16;

    dma.submission_chunk = 4096;
    dma.speed = 44100;

    bytes_per_sample = dma.samplebits/8;
    buf_size = dma.samples * bytes_per_sample;
    dma.buffer = calloc(1, buf_size);

    chunkSizeBytes = dma.submission_chunk * bytes_per_sample;

    s_numchannels = MAX_CHANNELS;
    S_InitScaletable();

    initAudio(dma.buffer, buf_size);
    snd_inited=1;

    return 1;
}


int SNDDMA_GetDMAPos(void)
{
    return dmapos;
}

void SNDDMA_Shutdown(void)
{
    Com_Printf("SNDDMA_ShutDown\n");
}
#if 0
qboolean
SNDDMA_Init(void)
{
	char drivername[128];
	char reqdriver[128];
	SDL_AudioSpec desired;
	SDL_AudioSpec obtained;
	int tmp, val;

	/* This should never happen,
	   but this is Quake 2 ... */
	if (snd_inited)
	{
		return 1;
	}

	int sndbits = (Cvar_Get("sndbits", "16", CVAR_ARCHIVE))->value;
	int sndfreq = (Cvar_Get("s_khz", "44", CVAR_ARCHIVE))->value;
	int sndchans = (Cvar_Get("sndchannels", "2", CVAR_ARCHIVE))->value;

#ifdef _WIN32
	s_sdldriver = (Cvar_Get("s_sdldriver", "dsound", CVAR_ARCHIVE));
#elif __linux__
	s_sdldriver = (Cvar_Get("s_sdldriver", "alsa", CVAR_ARCHIVE));
#elif __APPLE__
	s_sdldriver = (Cvar_Get("s_sdldriver", "CoreAudio", CVAR_ARCHIVE));
#else
	s_sdldriver = (Cvar_Get("s_sdldriver", "dsp", CVAR_ARCHIVE));
#endif

	snprintf(reqdriver, sizeof(drivername), "%s=%s", "SDL_AUDIODRIVER", s_sdldriver->string);
	putenv(reqdriver);

	Com_Printf("Starting SDL audio callback.\n");

	if (!SDL_WasInit(SDL_INIT_AUDIO))
	{
		if (SDL_Init(SDL_INIT_AUDIO) == -1)
		{
			Com_Printf ("Couldn't init SDL audio: %s.\n", SDL_GetError ());
			return 0;
		}
	}

	if (SDL_AudioDriverName(drivername, sizeof(drivername)) == NULL)
	{
		strcpy(drivername, "(UNKNOW)");
	}

	Com_Printf("SDL audio driver is \"%s\".\n", drivername);

	memset(&desired, '\0', sizeof(desired));
	memset(&obtained, '\0', sizeof(obtained));

	/* Users are stupid */
	if ((sndbits != 16) && (sndbits != 8))
	{
		sndbits = 16;
	}

	if (sndfreq == 48)
	{
		desired.freq = 48000;
	}
	else if (sndfreq == 44)
	{
		desired.freq = 44100;
	}
	else if (sndfreq == 22)
	{
		desired.freq = 22050;
	}
	else if (sndfreq == 11)
	{
		desired.freq = 11025;
	}

	desired.format = ((sndbits == 16) ? AUDIO_S16SYS : AUDIO_U8);

	if (desired.freq <= 11025)
	{
		desired.samples = 256;
	}
	else if (desired.freq <= 22050)
	{
		desired.samples = 512;
	}
	else if (desired.freq <= 44100)
	{
		desired.samples = 1024;
	}
	else
	{
		desired.samples = 2048;
	}

	desired.channels = sndchans;
	desired.callback = sdl_audio_callback;

	/* Okay, let's try our luck */
	if (SDL_OpenAudio(&desired, &obtained) == -1)
	{
		Com_Printf("SDL_OpenAudio() failed: %s\n", SDL_GetError());
		SDL_QuitSubSystem(SDL_INIT_AUDIO);
		return 0;
	}

	/* This points to the frontend */
	dmabackend = &dma;

	dmapos = 0;
	dmabackend->samplebits = obtained.format & 0xFF;
	dmabackend->channels = obtained.channels;

	tmp = (obtained.samples * obtained.channels) * 10;
	if (tmp & (tmp - 1))
	{	/* make it a power of two */
		val = 1;
		while (val < tmp)
			val <<= 1;

		tmp = val;
	}
	dmabackend->samples = tmp;

	dmabackend->submission_chunk = 1;
	dmabackend->speed = obtained.freq;
	dmasize = (dmabackend->samples * (dmabackend->samplebits / 8));
	dmabackend->buffer = calloc(1, dmasize);

	s_numchannels = MAX_CHANNELS;
	S_InitScaletable();

	SDL_PauseAudio(0);

	Com_Printf("SDL audio initialized.\n");
	snd_inited = 1;
	return 1;
}
#endif

void Q3E_SetCallbacks(void *init_audio, void *write_audio, void *set_state)
{
    setState=set_state;
    writeAudio = write_audio;
    initAudio = init_audio;
}

void Q3E_GetAudio(void)
{
    int offset = (dmapos * bytes_per_sample) & (buf_size - 1);
    writeAudio(offset, chunkSizeBytes);
    dmapos+=dma.submission_chunk;
}

void
SNDDMA_Submit(void)
{
}

void
SNDDMA_BeginPainting(void)
{
}

