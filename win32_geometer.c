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

typedef enum
{
	HEAD_Points_v1,
	HEAD_PointStatus_v1,
	HEAD_Shapes_v1,
	HEAD_Actions_v1,
	HEAD_Basis_v1,
} header_section;

typedef struct file_header
{
	char ID[8];        // unique(ish) text id e.g. "Geometer"/"GeoMeTeR"
	u16 FormatVersion; // file format version num
	u16 cArrays;       // for data section
	u32 CRC32;      // checksum of data
	u64 cBytes;        // bytes in data section (everything after this point)
	// Data following this point:
	//   cArrays * [
	//     - u32 ElementType; // array type tag (from which element size is known)
	//     - u32 cElements;   // size of array (could be 1 for individual data element)
	//     - Elements...      // array elements
	//   ]
} file_header;

internal inline void
MemErrorOnFail(HWND WindowHandle, b32 Success)
{
	if(!Success) MessageBox(WindowHandle, "Unable to allocate more memory. Save and quit.", "Memory Allocation Error", MB_ICONERROR);
}

#define FileErrorOnFail(hwnd, file, path) FileErrorOnFail_(hwnd, file, path, __func__, __LINE__)
internal inline void
FileErrorOnFail_(HWND WindowHandle, FILE *File, char *FilePath, char *FuncName, uint Line)
{
	char Buf[1024];
	stbsp_snprintf(Buf, 1024, "%s(%u): Unable to open file at %s", FuncName, Line, FilePath);
	if(!File) MessageBox(WindowHandle, Buf, "File Opening Error", MB_ICONERROR);
}

internal inline void
ChangeFilePath(state *State, char *FilePath, uint cchFilePath)
{
	free(State->FilePath);
	State->FilePath = FilePath;
	State->cchFilePath = cchFilePath;
}

internal OPENFILENAME
OpenFilenameDefault(HWND OwnerWindow, uint cchFilePath)
{
	cchFilePath = CeilPow2(cchFilePath);
	char *FilePath = calloc(cchFilePath, 1);
	Assert(FilePath);
	OPENFILENAME File = {0};
	File.lStructSize = sizeof(OPENFILENAME);
	File.hwndOwner = OwnerWindow;
	// NOTE: Dialog box template?:
	// File.hInstance;
	File.lpstrFilter = "Geometer files (*.geo)\0*.geo\0" "All Files\0*.*\0";
	/* lpstrCustomFilter; */
	/* nMaxCustFilter; */
	/* File.nFilterIndex; */
	File.lpstrFile = FilePath;
	File.nMaxFile = cchFilePath;
	// NOTE: without path info
	/* File.lpstrFileTitle; */
	/* File.nMaxFileTitle; */
	/* File.lpstrInitialDir; */
	// NOTE: dialog box title
	/* File.lpstrTitle; */
	File.Flags = OFN_OVERWRITEPROMPT;// | OFN_EXPLORER;
	/* File.nFileOffset; */
	/* File.nFileExtension; */
	File.lpstrDefExt = "geo";
	/* File.lCustData; */
	/* File.lpfnHook; */
	/* File.lpTemplateName; */
	return File;
}

internal inline u64
ReadFileArrayToArena(FILE *File, memory_arena *maPoints, u32 cElements, u32 ElementSize, u32 Factor, u32 Add, HWND Window)
{
	u64 cBytesEl = cElements * ElementSize + ElementSize; // Account for index 0
	MemErrorOnFail(Window, ArenaRealloc(maPoints, CeilPow2U64(Factor * cBytesEl + Add * ElementSize)));
	maPoints->Used = cBytesEl;
	// TODO: check individual array size is right
	// NOTE: add ElementSize to avoid writing valid elements to index 0
	Assert(maPoints->Base);
	u64 cElCheck = fread(maPoints->Base + ElementSize, ElementSize, cElements, File);
	Assert(cElements == cElCheck);
	return cBytesEl;
}

internal FILE *
OpenFileInCurrentWindow(state *State, char *FilePath, uint cchFilePath, HWND WindowHandle)
{
	FILE *Result = 0;
	if(FilePath)
	{
		Result = fopen(FilePath, "r+b");
		FileErrorOnFail(WindowHandle, Result, FilePath); 
		file_header FH;
		LOG("\tCHECK ID");
		Assert(fread(&FH, sizeof(FH), 1, Result));
		if(!(FH.ID[0]=='G'&&FH.ID[1]=='e'&&FH.ID[2]=='o'&&FH.ID[3]=='m'&&
			 FH.ID[4]=='e'&&FH.ID[5]=='t'&&FH.ID[6]=='e'&&FH.ID[7]=='r'))
		{
			goto open_end;
		}

		ChangeFilePath(State, FilePath, cchFilePath);

		switch(FH.FormatVersion)
		{
			case 1:
			{
				u64 cBytesCheck = 0;
				u32 ElType;
				u32 cElements;
				for(uint iArray = 0; iArray < FH.cArrays; ++iArray)
				{
					Assert(cBytesCheck += fread(&ElType,    sizeof(ElType),    1, Result) * sizeof(ElType));
					Assert(cBytesCheck += fread(&cElements, sizeof(cElements), 1, Result) * sizeof(cElements));
					switch(ElType)
					{
#define DRAW_LVL 0
						case HEAD_Points_v1:
						{
							cBytesCheck += ReadFileArrayToArena(Result, &State->Draw[DRAW_LVL].maPoints,
									cElements, sizeof(v2), 2, 0, WindowHandle);
						} break;

						case HEAD_PointStatus_v1:
						{
							cBytesCheck += ReadFileArrayToArena(Result, &State->Draw[DRAW_LVL].maPointStatus,
									cElements, sizeof(u8), 2, 0, WindowHandle);
						} break;

						case HEAD_Shapes_v1:
						{
							cBytesCheck += ReadFileArrayToArena(Result, &State->Draw[DRAW_LVL].maShapes,
									cElements, sizeof(shape), 1, 2, WindowHandle);
						} break;

						case HEAD_Actions_v1:
						{
							cBytesCheck += ReadFileArrayToArena(Result, &State->maActions,
									cElements, sizeof(action), 2, 2, WindowHandle);
						} break;

						case HEAD_Basis_v1:
						{
							u64 cElCheck = fread(&State->Draw[DRAW_LVL].Basis, sizeof(basis), cElements, Result);
							cBytesCheck += cElCheck * sizeof(basis);
							Assert(cElCheck == 1);
						} break;

						default:
						{
							// NOTE: unknown tag
							MessageBox(WindowHandle, "Unexpected data or corrupted file. Try again.", "Filetype Error", MB_ICONERROR);
							Assert(0);
							ElType;
							Result = 0;
							goto open_end;
						} break;
					}
				}
			} break;

			default:
			{
				goto open_error;
			} break;
		}

		// TODO: CRC32
		// TODO: where to put fclose?
		UpdateDrawPointers(State, DRAW_LVL);
	}

open_end:
	State->OpenFile = 0;
	return Result;

open_error:
	MessageBox(WindowHandle, "Wrong filetype or corrupted file. Try again.", "Filetype Error", MB_ICONERROR);
	Result = 0;
	goto open_end;
}

// TODO: error checking
internal void
SaveToFile(state *State, HWND WindowHandle)
{
	BEGIN_TIMED_BLOCK;
	if(State->SaveAs || !State->FilePath[0])
	{
		OPENFILENAME File = OpenFilenameDefault(WindowHandle, State->cchFilePath);
		char *FilePath = Win32GetSaveFilename(WindowHandle, &File);
		if(FilePath)
		{
			ChangeFilePath(State, FilePath, File.nMaxFile);
		}
		else
		{
			goto save_end;
		}
		State->SaveAs = 0;
	}

	FILE *SaveFile = fopen(State->FilePath, "wb");
	FileErrorOnFail(WindowHandle, SaveFile, State->FilePath); 
	file_header Header = {0};
	Header.ID[0] = 'G';
	Header.ID[1] = 'e';
	Header.ID[2] = 'o';
	Header.ID[3] = 'm';
	Header.ID[4] = 'e';
	Header.ID[5] = 't';
	Header.ID[6] = 'e';
	Header.ID[7] = 'r';
	Header.FormatVersion = 1;
	Header.cArrays = 5; // IMPORTANT: Keep updated when adding new arrays!
	Header.CRC32 = 0;  // edited in following macros
	Header.cBytes = 0; // edited in following macros

#define PROCESS_DATA_ARRAY() \
	DATA_PROCESS(HEAD_Points_v1,      State->iLastPoint,  State->Points + 1);\
	DATA_PROCESS(HEAD_PointStatus_v1, State->iLastPoint,  State->PointStatus + 1);\
	DATA_PROCESS(HEAD_Shapes_v1,      State->iLastShape,  State->Shapes + 1);\
	DATA_PROCESS(HEAD_Actions_v1,     State->iLastAction, (action *)State->maActions.Base + 1); \
	DATA_PROCESS(HEAD_Basis_v1,       One,                State->Basis)
	//                       elementType          cElements           arraybase

	// NOTE: needed to fix size of enum
	u32 Tag; 
	u32 One = 1; // To make it addressable
	// CRC processing and byte count
#define DATA_PROCESS(tag, count, arraybase) \
	Tag = tag; \
	Header.CRC32 = CRC32(&Tag, sizeof(Tag), Header.CRC32); \
	Header.CRC32 = CRC32(&count, sizeof(count), Header.CRC32); \
	Header.CRC32 = CRC32(arraybase, count, Header.CRC32); \
	Header.cBytes += sizeof(Tag) + sizeof(count) + (count)*sizeof((arraybase)[0])

	PROCESS_DATA_ARRAY();
#undef DATA_PROCESS

	// Write header to file
	fwrite(&Header, sizeof(Header), 1, SaveFile);

	// Write data to file
#define DATA_PROCESS(tag, count, arraybase) \
	Tag = tag; \
	fwrite(&Tag, sizeof(Tag), 1, SaveFile); \
	fwrite(&count, sizeof(count), 1, SaveFile); \
	fwrite((arraybase), sizeof((arraybase)[0]), count, SaveFile)

	PROCESS_DATA_ARRAY();
#undef DATA_PROCESS

	// TODO: use checksum to ensure written properly?
	fclose(SaveFile);

	State->Modified = 0;
save_end:
	State->SaveFile = 0;
	END_TIMED_BLOCK;
}

internal void
ReallocateArenas(state *State, HWND WindowHandle)
{ LOG("REALLOCATION")
	draw_state *NextDraw = &State->Draw[iDrawOffset(State, 1)];
	// TODO: what to do if reallocation fails? Ensure no more shapes/points etc; Error message box: https://msdn.microsoft.com/en-us/library/windows/desktop/ms645505(v=vs.85).aspx
	// NOTE: Can add multiple points per frame (intersections), but can't double
	// NOTE: Realloc the next undo state if needed
	Assert(DRAW_STATE.maPoints.Used <= DRAW_STATE.maPoints.Size);
	Assert(DRAW_STATE.maPointStatus.Used <= DRAW_STATE.maPointStatus.Size);
	if(DRAW_STATE.maPoints.Used >= NextDraw->maPoints.Size / 2)
	{ LOG("Adding to points arena");
		// NOTE: points and pointstatus should have exactly the same number of members
		Assert(DRAW_STATE.maPointStatus.Used/sizeof(u8) == DRAW_STATE.maPoints.Used/sizeof(v2));
		MemErrorOnFail(WindowHandle, ArenaRealloc(&NextDraw->maPoints, NextDraw->maPoints.Size * 2));
		MemErrorOnFail(WindowHandle, ArenaRealloc(&NextDraw->maPointStatus, NextDraw->maPointStatus.Size * 2));
	}

	// NOTE: Can only create 1 shape per frame
	Assert(DRAW_STATE.maShapes.Used <= DRAW_STATE.maShapes.Size);
	if(DRAW_STATE.maShapes.Used >= NextDraw->maShapes.Size)
	{ LOG("Adding to shapes arena");
		MemErrorOnFail(WindowHandle, ArenaRealloc(&NextDraw->maShapes, NextDraw->maShapes.Size * 2));
	}
	Assert(State->maActions.Used <= State->maActions.Size);
	// TODO: this will change once action = user action
	if(State->maActions.Used >= State->maActions.Size/2 - sizeof(action))
	{ LOG("Adding to actions arena");
		MemErrorOnFail(WindowHandle, ArenaRealloc(&State->maActions, State->maActions.Size * 2));
	}
}

int CALLBACK
WinMain(HINSTANCE Instance,
		HINSTANCE PrevInstance,
		LPSTR CommandLine,
		int ShowCode)
{
	OPEN_LOG("win32_geometer_log", ".txt");
	// UNUSED:
	ShowCode; CommandLine; PrevInstance;

	win32_window Window;
	GlobalRunning = 1;
	if(!Win32BasicWindow(Instance, &Window, 960, 540, "Geometer"))
	{
		GlobalRunning = 0;
	}

	Win32SetIcon(Window.Handle, GIcon32, cGIcon32, GIcon16, cGIcon16);

	LOG("ALLOC MEMORY");
#define MemSize (Kilobytes(64))

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

	state *State = (state *)Memory.PermanentStorage;

	LOG("OPEN BLANK FILE");
#define cSTART_POINTS 32
	draw_state *Draw = State->Draw;
	for(uint i = 0; i < NUM_UNDO_STATES; ++i)
	{
		Draw[i].maPoints      = ArenaCalloc(sizeof(v2)     * cSTART_POINTS);
		Draw[i].maPointStatus = ArenaCalloc(sizeof(u8)     * cSTART_POINTS);
		Draw[i].maShapes      = ArenaCalloc(sizeof(shape)  * cSTART_POINTS);
	}
	State->maActions          = ArenaCalloc(sizeof(action) * cSTART_POINTS);
#undef cSTART_POINTS

	State->cchFilePath = 1024;
	State->FilePath = calloc(State->cchFilePath, sizeof(char));
	FILE *OpenedFile = 0;

	char **argv = __argv;
	uint argc = __argc;
	if(argc > 1)
	{
		LOG("OPEN FILENAME");
		uint ArgLen = (uint)strlen(argv[1]) + 1;
		uint cchFilePath = State->cchFilePath >= ArgLen ? State->cchFilePath : CeilPow2(ArgLen);
		char *FilePath = calloc(cchFilePath, 1);
		strcpy(FilePath, argv[1]);
		OpenedFile = OpenFileInCurrentWindow(State, FilePath, cchFilePath, Window.Handle);
		State->OpenFile = 1;

		if(argc > 2)
		{ // Custom location/size
			if(argc == 6)
			{
				/* u32 x = atoi(argv[2]); */
				/* u32 y = atoi(argv[3]); */
				/* u32 w = atoi(argv[4]); */
				/* u32 h = atoi(argv[5]); */
			}
			else
			{
				MessageBox(Window.Handle, "Missing argument for window location/size", "Missing Argument", MB_ICONERROR);
			}
		}
	}

	ReallocateArenas(State, Window.Handle);

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
#if 1
	if(!InitLoadedFont(&State->DefaultFont, BitstreamBinary))
	{
		MessageBox(Window.Handle, "Unable to load font.", "Font error", MB_ICONERROR);
	}
#else
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
	char TitleText[2048] = {0};
	while(GlobalRunning)
	{
		FrameTimer.Start = Win32GetWallClock();
		/* FrameTimer = Win32StartFrameTimer(FrameTimer); */
		/* old_new_controller Keyboard = UpdateController(Input, 0); */

		{
			// NOTE: if the window is moved to a higher resolution monitor, reallocate
			int NewScreenWidth, NewScreenHeight;
			Win32ScreenResolution(Window.Handle, &NewScreenWidth, &NewScreenHeight);
			if(NewScreenWidth > ScreenWidth || NewScreenHeight > ScreenHeight)
			{
				ScreenWidth = NewScreenWidth;
				ScreenHeight = NewScreenHeight;
				/* char ScreenStr[512]; */
				/* stbsp_sprintf(ScreenStr, "Changed to larger monitor. New size: %u x %u.", ScreenWidth, ScreenHeight); */
				/* MessageBox(Window.Handle, ScreenStr, "Monitor Change", 0); */
				Win32ResizeDIBSection(&Win32Buffer, ScreenWidth, ScreenHeight);
			}
		}

		// TODO: move to open/save?
		stbsp_snprintf(TitleText, sizeof(TitleText), "%s - %s %s - Length: %f", "Geometer",
				State->FilePath[0] ? State->FilePath : "[New File]", State->Modified ? "[Modified]" : "", State->Length);
		SetWindowText(Window.Handle, TitleText);

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

		// TODO: keep position of canvas static during resizing
		// TODO: continue updating client while resizing
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
		if(State->CloseApp)  { GlobalRunning = 0; }
		State->dt = FrameTimer.SecondsElapsedForFrame;

		// SAVE/OPEN
		if(State->SaveFile)
		{
			SaveToFile(State, Window.Handle);
		}

		else if(State->OpenFile)
		{
			// TODO: either open in new window or confirm with if(State->Modified) {confirm...}
			OPENFILENAME File = OpenFilenameDefault(Window.Handle, State->cchFilePath);
			char *FilePath = Win32GetOpenFilename(Window.Handle, &File, State->SaveAs ? "Open in new window" : 0);
			if(FilePath)
			{
				LOG("OPEN NEW GEOMETER WINDOW");
				size_t PathLen;
				char *EXEPath = Win32GetEXEPath(&PathLen);
				size_t FileLen = strlen(FilePath);
				// TODO: move to string pool
				size_t SysLen = sizeof("start") + PathLen + FileLen + 2; // extra space and \0
				char *SysCall = malloc(SysLen);
				stbsp_snprintf(SysCall, (int)SysLen, "start %s %s", EXEPath, FilePath);

				system(SysCall);

				free(EXEPath);
				free(SysCall);
				free(FilePath);

				if(!State->SaveAs /* || IsBlankFile */)
				{
					// NOTE: instead of loading into current file, just open a new one and close this
					GlobalRunning = 0;
					// TODO: open 'properly':
					/* OpenFileInCurrentWindow(State, FilePath, File.nMaxFile, Window.Handle); */
				}
				State->SaveAs = 0;
			}

			State->OpenFile = 0;
		}
		
		ReallocateArenas(State, Window.Handle);

		FrameTimer = Win32WaitForFrameEnd(FrameTimer);
		FrameTimer = Win32EndFrameTimer(FrameTimer);
		++State->FrameCount;

		// NOTE: while should allow for cancels part way through the save
		while(!GlobalRunning && State->Modified)
		{
			uint ButtonResponse = MessageBox(Window.Handle, "Your file has been modified since you last saved.\n"
					"Would you like to save before closing?", "Save Changes?", MB_YESNOCANCEL | MB_ICONWARNING);
			if(ButtonResponse == IDYES)     { SaveToFile(State, Window.Handle); }
			else if(ButtonResponse == IDNO) { break; } // leave loop and close
			else                            { GlobalRunning = 1; } // return to program if cancelled/unknown response occurs
		}
	}
	// TODO:? free timer resolution
	if(OpenedFile)  { fclose(OpenedFile); }
	CLOSE_LOG();
}

#if SINGLE_EXECUTABLE
#undef DEBUG_PREFIX
#define DEBUG_PREFIX Win32
#endif // SINGLE_EXECUTABLE
DECLARE_DEBUG_RECORDS;
