#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#include <stdlib.h>
#include <windows.h>
#include <mmsystem.h>
#if SINGLE_EXECUTABLE
#include "geometer.c"
#else // SINGLE_EXECUTABLE
#include "geometer.h"
#endif // SINGLE_EXECUTABLE
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

#include "GIcons.h"

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

	Win32SetIcon(Window.Handle, GIcon32, cGIcon32, GIcon16, cGIcon16);

#define MemSize (Megabytes(64))
	memory Memory = {0};
	// TODO: Track memory use and realloc when necessary
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
#if !SINGLE_EXECUTABLE
	char *LibFnNames[] = {"UpdateAndRender"};
	win32_library Lib = Win32Library(LibFnNames, 0, ArrayCount(LibFnNames),
									 0, "geometer.dll", "geometer_temp.dll", "lock.tmp");
#endif // !SINGLE_EXECUTABLE

	int ScreenWidth, ScreenHeight;
	Win32ScreenResolution(Window.Handle, &ScreenWidth, &ScreenHeight);
	win32_image_buffer Win32Buffer = {0};
	Win32ResizeDIBSection(&Win32Buffer, ScreenWidth, ScreenHeight);

	// ASSETS
	state *State = (state *)Memory.PermanentStorage;
#if 1
	void *FontBuffer = VirtualAlloc(0, 1<<25, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
	INIT_FONT(Bitstream, "Bitstream.ttf", FontBuffer, 1<<25);
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
		FrameTimer.Start = Win32GetWallClock();
		/* FrameTimer = Win32StartFrameTimer(FrameTimer); */
		/* old_new_controller Keyboard = UpdateController(Input, 0); */

#if 1
		// TODO: only fill buffer inside client
		RECT ClientRect;
		GetClientRect(Window.Handle, &ClientRect);
		int ClientWidth = ClientRect.right - ClientRect.left;
		int ClientHeight = ClientRect.bottom - ClientRect.top;
		Win32Buffer.Width = ClientWidth;
		Win32Buffer.Height = ClientHeight;
		Win32Buffer.Info.bmiHeader.biWidth = ClientWidth;
		Win32Buffer.Info.bmiHeader.biHeight = ClientHeight;
		Win32Buffer.Pitch = ClientWidth * BytesPerPixel;

	// TODO: Probably clear to black
#endif

		UpdateKeyboard(Input);

		/* Win32ProcessPendingMessages(Keyboard.New); */
		Input.New->Mouse.ScrollV = 0;
		Input.New->Mouse.ScrollH = 0;
		Win32ProcessKeyboardAndScrollMessages(&Input.New->Keyboard, &Input.New->Mouse);
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

		image_buffer GameImageBuffer = *(image_buffer *) &Win32Buffer;
		Fullscreen = Win32DisplayBufferInWindow(&Win32Buffer, Window);

#if !SINGLE_EXECUTABLE
		Win32ReloadLibOnRecompile(&Lib); 
		update_and_render *UpdateAndRender = ((update_and_render *)Lib.Functions[0].Function);
		if(!UpdateAndRender) { break; }
#endif // !SINGLE_EXECUTABLE

		UpdateAndRender(&GameImageBuffer, &Memory, Input);
		if(State->CloseApp) {GlobalRunning = 0; }
		State->dt = FrameTimer.SecondsElapsedForFrame;

		FrameTimer = Win32WaitForFrameEnd(FrameTimer);
		FrameTimer = Win32EndFrameTimer(FrameTimer);
		++State->FrameCount;
	}
	// TODO:? free timer resolution
}

#if SINGLE_EXECUTABLE
#undef DEBUG_PREFIX
#define DEBUG_PREFIX Win32
#endif // SINGLE_EXECUTABLE
DECLARE_DEBUG_RECORDS;
