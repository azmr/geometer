#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#include <stdlib.h>
#include <windows.h>
#include <mmsystem.h>
#include "geometer.h"
#define USING_INPUT
#include <fonts.c>
#include <win32.h>
#include <live_edit.h>
/* #include <stb_sprintf.h> */
/* #include <loop_edit.h> */

global_variable b32 GlobalRunning;
global_variable b32 GlobalPause;
global_variable WINDOWPLACEMENT GlobalWindowPosition = {sizeof(GlobalWindowPosition)};
#include <win32_gfx.h>
#include <win32_input.h>


typedef UPDATE_AND_RENDER(update_and_render);

int CALLBACK
WinMain(HINSTANCE Instance,
		HINSTANCE PrevInstance,
		LPSTR CommandLine,
		int ShowCode)
{
	// UNUSED:
	ShowCode; CommandLine; PrevInstance;

	win32_window Window;
	GlobalRunning = 1;
	if(!Win32BasicWindow(Instance, &Window, 960, 540, "Geometer"))
	{
		GlobalRunning = 0;
	}

#define MemSize (Megabytes(64))
	memory Memory = {0};
	// TODO: WORK HERE
	Memory.PermanentStorageSize = MemSize;
	Memory.TransientStorageSize = MemSize/2;
	Memory.PermanentStorage = VirtualAlloc(0, Memory.PermanentStorageSize + Memory.TransientStorageSize,
			MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
	Memory.TransientStorage = (u8 *)Memory.PermanentStorage + Memory.PermanentStorageSize;
	
	if(!Memory.PermanentStorage || !Memory.TransientStorage)
	{
		OutputDebugStringA("Memory not allocated properly");
		GlobalRunning = 0;
	}


	Win32LoadXInput();
	// TODO: Pool with bitmap VirtualAlloc and font?
	char *LibFnNames[] = {"UpdateAndRender"};
	win32_library Lib = Win32Library(LibFnNames, 0, ArrayCount(LibFnNames),
									 0, "geometer.dll", "geometer_temp.dll", "lock.tmp");

	win32_image_buffer Win32Buffer = {0};
	Win32ResizeDIBSection(&Win32Buffer, 960, 540);

	// ASSETS
	state *State = (state *)Memory.PermanentStorage;
#if 1
	void *FontBuffer = VirtualAlloc(0, 1<<25, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
	INIT_FONT(Bitstream, "E:/Downloads/fonts/bitstream-vera-sans-mono/Bitstream Vera Sans Mono Roman.ttf", FontBuffer, 1<<25);
	State->DefaultFont = Bitstream;
#endif

	//////////

	win32_frame_timing FrameTimer = Win32InitFrameTimer(Window.TargetSecondsPerFrame);

	input Input = {0};
	input_state Inputs[2] = {0};
	Input.New = &Inputs[0];
	Input.Old = &Inputs[1];

	LARGE_INTEGER PerfCountFreq;
	QueryPerformanceFrequency(&PerfCountFreq);

	b32 Fullscreen = 0;
	while(GlobalRunning)
	{
		// TODO: failed on pressing escape
		FrameTimer.Start = Win32GetWallClock();
		/* FrameTimer = Win32StartFrameTimer(FrameTimer); */
		old_new_controller Keyboard = UpdateController(Input, 0);
		/* controller_input Keyboards[2]; */
		/* Keyboard.New = &Keyboards[0]; */
		/* Keyboard.Old = &Keyboards[1]; */

		Win32ProcessPendingMessages(Keyboard.New);
		// TODO: zero controllers
		Win32ProcessXInputControllers(&Input);
		Win32GetWindowDimensionAndOffset(&Window, Win32Buffer.Width, Win32Buffer.Height);
		Win32UpdateMouse(Window.Handle, Input.New, Window.OffsetX, Window.OffsetY, Win32Buffer.Height);
		if(Fullscreen)
		{
			// TODO: Finish fixing
			Input.New->Mouse.P.X /= 2;
			Input.New->Mouse.P.Y /= 2;
			// TODO: this is a hack, fix it properly!
			Input.New->Mouse.P.X += 240;
			Input.New->Mouse.P.Y += 134;
		}

		Win32ReloadLibOnRecompile(&Lib); 

		image_buffer GameImageBuffer = *(image_buffer *) &Win32Buffer;
		update_and_render *UpdateAndRender = ((update_and_render *)Lib.Functions[0].Function);
		if(!UpdateAndRender) { break; }
		Fullscreen = Win32DisplayBufferInWindow(&Win32Buffer, Window);

		UpdateAndRender(&GameImageBuffer, &Memory, Input);
		if(State->CloseApp) {GlobalRunning = 0; }
		State->dt = FrameTimer.SecondsElapsedForFrame;

		FrameTimer = Win32WaitForFrameEnd(FrameTimer);
		FrameTimer = Win32EndFrameTimer(FrameTimer);
		++State->FrameCount;
	}
	// TODO:? free timer resolution
}

DECLARE_DEBUG_RECORDS;
