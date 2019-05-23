/*
 * Copyright (C) 1997-2001 Id Software, Inc.
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * =======================================================================
 *
 * This file is the starting point of the program and implements
 * the main loop
 *
 * =======================================================================
 */

#include <fcntl.h>
#include <locale.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../../common/header/common.h"
#include "header/unix.h"

#if defined(__APPLE__) && !defined(DEDICATED_ONLY)
#include <SDL/SDL.h>
#endif

int main_time, main_oldtime, main_newtime;
void main_frame()
{
	do
	{
		main_newtime = Sys_Milliseconds();
		main_time = main_newtime - main_oldtime;
	}
	while (main_time < 1);
	Qcommon_Frame(main_time);
	main_oldtime = main_newtime;
}
extern void (*AndroidEraseSwp_p)();
#include <stdbool.h>
extern bool (*AndroidGetSwp_p)();
void Q3E_DrawFrame()
{
	(*AndroidEraseSwp_p)();
	do//magic
	{
	main_frame();
	}
	while (!(*AndroidGetSwp_p)());
}

void Q3E_OGLRestart()
{
VID_Restart_f();
}

float analogx=0.0f;
float analogy=0.0f;
int analogenabled=0;
void Q3E_Analog(int enable,float x,float y)
{
analogenabled=enable;
analogx=x;
analogy=y;
}

int
main(int argc, char **argv)
{
	freopen("stdout.txt","w",stdout);
	setvbuf(stdout, NULL, _IONBF, 0);
	freopen("stderr.txt","w",stderr);
	setvbuf(stderr, NULL, _IONBF, 0);

	/* register signal handler */
	registerHandler();

	/* enforce C locale */
	setenv("LC_ALL", "C", 1);

	printf("\nYamagi Quake II v%s\n", VERSION);
	printf("=====================\n\n");

#ifndef DEDICATED_ONLY
	printf("Client build options:\n");
#ifdef CDA
	printf(" + CD audio\n");
#else
	printf(" - CD audio\n");
#endif
#ifdef OGG
	printf(" + OGG/Vorbis\n");
#else
	printf(" - OGG/Vorbis\n");
#endif
#ifdef USE_OPENAL
	printf(" + OpenAL audio\n");
#else
	printf(" - OpenAL audio\n");
#endif
#ifdef ZIP
	printf(" + Zip file support\n");
#else
	printf(" - Zip file support\n");
#endif
#endif

	printf("Platform: %s\n", BUILDSTRING);
	printf("Architecture: %s\n", CPUSTRING);

	/* Seed PRNG */
	randk_seed();

	/* Initialze the game */
	Qcommon_Init(argc, argv);

	main_oldtime = Sys_Milliseconds();

	return 0;
}

