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
#include "svg.h"

global_variable b32 GlobalRunning;
global_variable b32 GlobalPause;
global_variable WINDOWPLACEMENT GlobalWindowPosition = {sizeof(GlobalWindowPosition)};
#include <win32_gfx.h>
#include <win32_input.h>

typedef UPDATE_AND_RENDER(update_and_render);

typedef enum cursor_type
{
	CURSOR_Normal = 0,
	CURSOR_Basis,
	CURSOR_Pan,
	CURSOR_Arc,
	CURSOR_Seg,
	CURSOR_Count
} cursor_type;

typedef enum header_section
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
	ssnprintf(Buf, 1024, "%s(%u): Unable to open file at %s", FuncName, Line, FilePath);
	if(!File) MessageBox(WindowHandle, Buf, "File Opening Error", MB_ICONERROR);
}

internal inline void
ChangeFilePath(state *State, char *FilePath, uint cchFilePath)
{
	free(State->FilePath);
	State->FilePath = FilePath;
	State->cchFilePath = cchFilePath;
}

internal inline b32
FileHasName(state *State)
{
	b32 Result = State->FilePath[0];
	return Result;
}

internal inline b32
IsModified(state *State)
{
	b32 Result = State->iCurrentDraw != State->iSaveDraw;
	return Result;
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

		// all in from now?
		ChangeFilePath(State, FilePath, cchFilePath);
		State->iCurrentDraw = 0;
		State->cDraws = 0;
		State->iLastDraw = 0;

		uint iCurrentDraw = State->iCurrentDraw;
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
#define DRAW_LVL iCurrentDraw
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

		// TODO IMPORTANT: CRC32
		// fclose?
		UpdateDrawPointers(State, DRAW_LVL);
	}

open_end:
	// TODO (rm): State->OpenFile = 0;
	return Result;

open_error:
	MessageBox(WindowHandle, "Wrong filetype or corrupted file. Try again.", "Filetype Error", MB_ICONERROR);
	Result = 0;
	goto open_end;
}

// TODO: error checking
internal void
SaveToFile(state *State, HWND WindowHandle, char *FilePath)
{
	BEGIN_TIMED_BLOCK;
	FILE *SaveFile = fopen(FilePath, "wb");
	FileErrorOnFail(WindowHandle, SaveFile, FilePath); 
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
	//           elementType          cElements           arraybase

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

	Assert(State->maIntersects.Used <= State->maIntersects.Size);
	// NOTE: Can only create 1 shape per frame
	Assert(DRAW_STATE.maShapes.Used <= DRAW_STATE.maShapes.Size);
	if(DRAW_STATE.maShapes.Used >= NextDraw->maShapes.Size)
	{ LOG("Adding to shapes arena");
		MemErrorOnFail(WindowHandle, ArenaRealloc(&NextDraw->maShapes, NextDraw->maShapes.Size * 2));
		MemErrorOnFail(WindowHandle, ArenaRealloc(&State->maIntersects, State->maIntersects.Size * 2));
	}
	Assert(State->maActions.Used <= State->maActions.Size);
	// TODO: this will change once action = user action
	if(State->maActions.Used >= State->maActions.Size/2 - sizeof(action))
	{ LOG("Adding to actions arena");
		MemErrorOnFail(WindowHandle, ArenaRealloc(&State->maActions, State->maActions.Size * 2));
	}
	if(State->maIntersects.Used >= State->maIntersects.Size/2 - sizeof(action))
	{ LOG("Adding to actions arena");
	}
}

/// Open a new geometer window.
/// If FilePath is an empty string (""), open a blank file,
/// otherwise try to open the FilePath given.
/// Returns 0 on failure.
internal b32
Win32OpenGeometerWindow(char *FilePath)
{
	size_t PathLen;
	char *EXEPath = Win32GetEXEPath(&PathLen);
	size_t FileLen = strlen(FilePath);
	// TODO (opt): move to string pool / better string system
	size_t SysLen = sizeof("start") + PathLen + FileLen + 2; // extra space and \0
	char *SysCall = malloc(SysLen);
	ssnprintf(SysCall, (int)SysLen, "start %s %s", EXEPath, FilePath);

	// start call returns 0 on success
	b32 Result = ! system(SysCall);

	free(EXEPath);
	free(SysCall);
	return Result;
}

internal void
Save(state *State, HWND WindowHandle, b32 SaveAs)
{
	BEGIN_TIMED_BLOCK;
	char *SavePath = State->FilePath;
	b32 Unnamed = ! FileHasName(State);
	b32 ContinueSave = 1;
	if(SaveAs || Unnamed) // explicitly requested or no current name
	{
		OPENFILENAME File = OpenFilenameDefault(WindowHandle, State->cchFilePath);
		char *DialogTitle = SaveAs ? "Save as and open in new window" : 0;
		SavePath = Win32GetSaveFilename(WindowHandle, &File, DialogTitle);
		if(SavePath)
		{ // for SaveAs, the new window will have the new file path
			if(Unnamed && ! SaveAs)
			{ ChangeFilePath(State, SavePath, File.nMaxFile); }
		}
		else
		{
			LOG("FILEPATH NOT FOUND");
			ContinueSave = 0;
		}
	}

	if(ContinueSave)
	{
		SaveToFile(State, WindowHandle, SavePath);

		if(SaveAs)
		{ // open the file just saved in a new window
			Win32OpenGeometerWindow(SavePath);
			free(SavePath); // only needed temporarily,
			// ...as that file is no longer relevant after save
		}
		else
		{
			State->iSaveDraw = State->iCurrentDraw;
		}
	}
	END_TIMED_BLOCK;
}

/// Confirms with user that they want to close if the file has been modified.
/// Saves if user wants to.
/// Returns 1 to continue with close or 0 to not close.
internal b32
Win32ConfirmFileClose(state *State, HWND WindowHandle)
{
	b32 Result = 1;
	while(IsModified(State))
	{
		uint ButtonResponse = MessageBox(WindowHandle, "Your file has been modified since you last saved.\n"
				"Would you like to save before closing?", "Save Changes?", MB_YESNOCANCEL | MB_ICONWARNING);
		if(ButtonResponse == IDYES)
		{ // Save and confirm close
			Save(State, WindowHandle, 0);
			// loop back to check Modified has been set to 0
		}
		else if(ButtonResponse == IDNO)
		{ // don't save and confirm close
			break;
		}
		else // cancelled/unknown response
		{ // don't close, return to program
			Result = 0;
			break;
		}
	}
	return Result;
}

internal aabb
AABBFromShape(v2 *Points, shape Shape)
{
	aabb Result = {0};
	switch(Shape.Kind)
	{
		case SHAPE_Segment:
		{
			v2 po1 = Points[Shape.Line.P1];
			v2 po2 = Points[Shape.Line.P2];
			minmaxf32 x = MinMaxF32(po1.X, po2.X);
			minmaxf32 y = MinMaxF32(po1.Y, po2.Y);
			Result.MinX = x.Min;
			Result.MaxX = x.Max;
			Result.MinY = y.Min;
			Result.MaxY = y.Max;
		} break;

		// TODO (optimize): arc AABB may be smaller than circle
		case SHAPE_Arc:
		case SHAPE_Circle:
		{
			v2 Focus = Points[Shape.Circle.ipoFocus];
			f32 Radius = Dist(Focus, Points[Shape.Circle.ipoRadius]);
			Result.MinX = Focus.X - Radius;
			Result.MaxX = Focus.X + Radius;
			Result.MinY = Focus.Y - Radius;
			Result.MaxY = Focus.Y + Radius;
		} break;

		default:
		{
			Assert(0);
		}
	}
	return Result;
}

internal aabb
AABBExpand(aabb Expandee, aabb Expander)
{
	aabb Result = Expandee;
	if(Expander.MinX < Result.MinX) { Result.MinX = Expander.MinX; }
	if(Expander.MaxX > Result.MaxX) { Result.MaxX = Expander.MaxX; }
	if(Expander.MinY < Result.MinY) { Result.MinY = Expander.MinY; }
	if(Expander.MaxY > Result.MaxY) { Result.MaxY = Expander.MaxY; }
	return Result;
}

internal b32
ArcMoreThanSemiCircle(v2 poFocus, v2 poStart, v2 poEnd)
{
	v2 DirStart = V2Sub(poStart, poFocus);
	v2 DirEnd   = V2Sub(poEnd, poFocus);
	b32 Result = IsCCW(DirStart, DirEnd) ? 0 : 1;
	return Result;
}

internal v2
CanvasToSVG(v2 P, aabb AABB)
{
	v2 Result;
	f32 Border = 10.f;
	AABB.MinX -= Border;
	AABB.MinY -= Border;
	AABB.MaxX += Border;
	AABB.MaxY += Border;
	/* f32 Height = AABB.MaxY - AABB.MinY; */
	// TODO (feature): scale to apparent zoom level? start at INITIAL_ZOOM of 1?
	Result.X = P.X - AABB.MinX; // * invINITIAL_ZOOM;
	Result.Y = AABB.MaxY - P.Y; // * invINITIAL_ZOOM;
	return Result;
}

internal uint
FirstValidShape(state *State)
{
	uint iFirstValidShape = 0;
	for(uint iShape = 1; iShape <= State->iLastShape; ++iShape)
	{
		shape Shape = State->Shapes[iShape];
		if(Shape.Kind != SHAPE_Free)
		{
			iFirstValidShape = iShape;
			break;
		}
	}
	return iFirstValidShape;
}

internal aabb
AABBOfAllShapes(v2 *Points, shape *Shapes, uint iFirstValidShape, uint iLastShape)
{
	Assert(iFirstValidShape);
	shape Shape = Shapes[iFirstValidShape];
	aabb Result = AABBFromShape(Points, Shape);
	for(uint iShape = iFirstValidShape + 1; iShape <= iLastShape; ++iShape)
	{
		Shape = Shapes[iShape];
		aabb AABB = AABBFromShape(Points, Shape);
		Result = AABBExpand(Result, AABB);
	}
	return Result;
}

internal void
ExportSVGToFile(state *State, char *FilePath)
{
	v2 *Points = State->Points;
	shape *Shapes = State->Shapes;
	uint iLastShape = State->iLastShape;
	uint iFirstValidShape = FirstValidShape(State);

	if(iFirstValidShape)
	{
		aabb TotalAABB = AABBOfAllShapes(Points, Shapes, iFirstValidShape, iLastShape);
		FILE *SVGFile = NewSVG(FilePath, "fill='none' stroke-width='2' stroke='black' stroke-linecap='round'");
		for(uint iShape = iFirstValidShape; iShape <= iLastShape; ++iShape)
		{
			shape Shape = Shapes[iShape];
			if(Shape.Kind != SHAPE_Free)
			{
				/* SVGRect(SVGFile, StrokeWidth, AABB.MinX, AABB.MinY, AABB.MaxX-AABB.MinX, AABB.MaxY-AABB.MinY); */
				switch(Shape.Kind)
				{
					case SHAPE_Circle:
					{
						circle Circle = Shape.Circle;
						v2 poFocus  = CanvasToSVG(Points[Circle.ipoFocus],  TotalAABB);
						v2 poRadius = CanvasToSVG(Points[Circle.ipoRadius], TotalAABB);
						f32 Radius = Dist(poFocus, poRadius);
						SVGCircle(SVGFile, poFocus.X, poFocus.Y, Radius);
					} break;

					case SHAPE_Arc:
					{
						arc Arc = Shape.Arc;
						v2 poFocus = Points[Arc.ipoFocus];
						v2 poStart = Points[Arc.ipoStart];
						v2 poEnd   = Points[Arc.ipoEnd];
						f32 Radius = Dist(poFocus, poStart);
						b32 LargeArc = ArcMoreThanSemiCircle(poFocus, poStart, poEnd);
						poFocus = CanvasToSVG(poFocus, TotalAABB);
						poStart = CanvasToSVG(poStart, TotalAABB);
						poEnd   = CanvasToSVG(poEnd,   TotalAABB);
						SVGArc(SVGFile, Radius, poStart.X, poStart.Y, poEnd.X, poEnd.Y, LargeArc);
					} break;

					case SHAPE_Segment:
					{
						line Line = Shape.Line;
						v2 po1 = CanvasToSVG(Points[Line.P1], TotalAABB);
						v2 po2 = CanvasToSVG(Points[Line.P2], TotalAABB);
						SVGLine(SVGFile, po1.X, po1.Y, po2.X, po2.Y);
					} break;

					default:
					{
						Assert(0 && "not sure what shape is at index " && iShape);
					}
				}
			}
		}
	/* SVGRect( AABB.xMin, AABB.yMin, AABB.xMax-AABB.xMin, AABB.yMax-AABB.yMin ); */
	int CloseStatus = EndSVG(SVGFile);
	Assert(CloseStatus == 0);
	}
}

internal void
ExportSVG(state *State, HWND WindowHandle)
{
	BEGIN_TIMED_BLOCK;
	// TODO (fix): seems to set the title of the window
	OPENFILENAME File = OpenFilenameDefault(WindowHandle, State->cchFilePath);
	File.lpstrFilter = "Vector graphics (*.svg)\0*.svg\0" "All Files\0*.*\0";
	char *DialogTitle = "Export SVG";
	// malloc'd:
	char *ExportPath = Win32GetSaveFilename(WindowHandle, &File, DialogTitle);
	if(ExportPath)
	{
		ExportSVGToFile(State, ExportPath);
		free(ExportPath);
	}
	else
	{
		MessageBox(WindowHandle, "Unable to export SVG, could not find file name.", "Export failed", MB_ICONERROR);
	}
	END_TIMED_BLOCK;
}

internal void
FreeStateArenas(state *State)
{
#define cSTART_POINTS 32
	draw_state *Draw = State->Draw;
	for(uint i = 0; i < NUM_UNDO_STATES; ++i)
	{
		Free(Draw[i].maPoints.Base);
		Free(Draw[i].maPointStatus.Base);
		Free(Draw[i].maShapes.Base);
	}
	Free(State->maActions.Base);
	Free(State->maIntersects.Base);
#undef cSTART_POINTS
}

internal void
AllocStateArenas(state *State)
{
#define cSTART_POINTS 32
	draw_state *Draw = State->Draw;
	for(uint i = 0; i < NUM_UNDO_STATES; ++i)
	{
		Draw[i].maPoints	  = ArenaCalloc(sizeof(v2)	   * cSTART_POINTS);
		Draw[i].maPointStatus = ArenaCalloc(sizeof(u8)	   * cSTART_POINTS);
		Draw[i].maShapes	  = ArenaCalloc(sizeof(shape)  * cSTART_POINTS);
	}
	State->maActions		  = ArenaCalloc(sizeof(action) * cSTART_POINTS);
	State->maIntersects		  = ArenaCalloc(sizeof(v2)     * cSTART_POINTS);
#undef cSTART_POINTS
}

internal void
HardReset(state *State, FILE *OpenFile)
{
	if(OpenFile) { fclose(OpenFile); }
	FreeStateArenas(State);
	Free(State->FilePath);
	state NewState = {0};
	AllocStateArenas(&NewState);
	ChangeFilePath(&NewState, calloc(1, 1), 1); // 1 byte set to 0 (empty string)
	
	Reset(&NewState);
	*State = NewState;
	// NOTE: need initial save state to undo to
	SaveUndoState(State);
}

int CALLBACK
WinMain(HINSTANCE Instance, HINSTANCE PrevInstance, LPSTR CommandLine, int ShowCode)
{
	OPEN_LOG("win32_geometer_log", ".txt");
	// UNUSED:
	ShowCode; CommandLine; PrevInstance;

	win32_window Window;
	GlobalRunning = 1;
	if(!Win32BasicWindow(Instance, &Window, 960, 540, "Geometer", "Icon", "IconSmall"))
	{ GlobalRunning = 0; }

	/* Win32SetIcon(Window.Handle, GIcon32, cGIcon32, GIcon16, cGIcon16); */

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
	AllocStateArenas(State);
	Reset(State);

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
	{ MessageBox(Window.Handle, "Unable to load font.", "Font error", MB_ICONERROR); }
#else
	void *FontBuffer = VirtualAlloc(0, 1<<25, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
	INIT_FONT(Bitstream, "Bitstream.ttf", FontBuffer, 1<<25);
	State->DefaultFont = Bitstream;
#endif

	HCURSOR Cursors[CURSOR_Count];
	Cursors[CURSOR_Normal] = LoadCursor(0, IDC_ARROW);
	Cursors[CURSOR_Basis]  = LoadCursor(0, IDC_UPARROW);
	Cursors[CURSOR_Pan]    = LoadCursor(0, IDC_SIZEALL);
	Cursors[CURSOR_Arc]    = LoadCursor(0, IDC_CROSS);
	Cursors[CURSOR_Seg]    = LoadCursor(0, IDC_CROSS);
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

		{ // if the window is moved to a higher resolution monitor, reallocate
			int NewScreenWidth, NewScreenHeight;
			Win32ScreenResolution(Window.Handle, &NewScreenWidth, &NewScreenHeight);
			if(NewScreenWidth > ScreenWidth || NewScreenHeight > ScreenHeight)
			{ // width is larger than allocation allows: reallocate
				ScreenWidth = NewScreenWidth;
				ScreenHeight = NewScreenHeight;
				Win32ResizeDIBSection(&Win32Buffer, ScreenWidth, ScreenHeight);
			}
		}

		// TODO: move to open/save?
		ssnprintf(TitleText, sizeof(TitleText), "%s - %s %s", "Geometer",
				FileHasName(State) ? State->FilePath : "[New File]",
				IsModified(State) ? "[Modified]" : "");
		SetWindowText(Window.Handle, TitleText);

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

		UpdateKeyboard(Input);

		// TODO: keep position of canvas static during resizing
		// TODO: continue updating client while resizing
		Input.New->Mouse.ScrollV = 0;
		Input.New->Mouse.ScrollH = 0;
		Win32ProcessKeyboardAndScrollMessages(&Input.New->Keyboard, &Input.New->Mouse);
		// TODO: things break when this is removed... extract the important bits
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

		platform_request PlatRequest = UpdateAndRender(&GameImageBuffer, &Memory, Input);
		// TODO (fix): frame timing
		State->dt = FrameTimer.SecondsElapsedForFrame;

		switch(PlatRequest.Action)
		{
			case FILE_Close:
			{
				GlobalRunning = 0;
			} break;

			case FILE_Save:
			{ // save file, possibly to new name & window
				Save(State, Window.Handle, PlatRequest.NewWindow);
			} break;

			case FILE_ExportSVG:
			{
				ExportSVG(State, Window.Handle);
			} break;

			case FILE_Open:
			{ // open file in same or new window
				OPENFILENAME File = OpenFilenameDefault(Window.Handle, State->cchFilePath);
				char *DialogTitle = PlatRequest.NewWindow ? "Open in new window" : 0;
				char *OpenPath = Win32GetOpenFilename(Window.Handle, &File, DialogTitle);
				if(OpenPath)
				{
					if(PlatRequest.NewWindow)
					{
						LOG("OPEN NEW GEOMETER WINDOW");
						Assert(Win32OpenGeometerWindow(OpenPath));
						free(OpenPath); // allocc'd above
					}
					else if(Win32ConfirmFileClose(State, Window.Handle))
					{
						HardReset(State, OpenedFile);
						OpenFileInCurrentWindow(State, OpenPath, File.nMaxFile, Window.Handle);
					}
					// else cancelled
				}
			} break;

			case FILE_New:
			{ // new file in same or new window
				if(PlatRequest.NewWindow)
				{
					LOG("OPEN NEW GEOMETER WINDOW");
					Assert(Win32OpenGeometerWindow(""));
				}
				else if(Win32ConfirmFileClose(State, Window.Handle))
				{
					HardReset(State, OpenedFile);
				}
				// else cancelled
			} break;

			default:
			{ // do nothing
			}
		}


#if 1
		// TODO (ui fix): cursor is set to normal if moving on the help screen
		f32 MX = Input.New->Mouse.P.X;
		f32 MY = Input.New->Mouse.P.Y;
		if(0.f <= MX && MX <= ClientWidth && // mouse is inside client area
		   0.f <= MY && MY <= ClientHeight)  // otherwise resize arrows act up
		{ // change the cursor based on the input mode
			if(PlatRequest.Pan)
			{
				SetCursor(Cursors[CURSOR_Pan]);
			}
			else
			{
				switch(State->InputMode)
				{
					case MODE_Normal:
					{ SetCursor(Cursors[CURSOR_Normal]); } break;
					case MODE_SetBasis:
					{ SetCursor(Cursors[CURSOR_Basis]); } break;
					case MODE_SetLength:
					case MODE_DrawArc:
					case MODE_ExtendArc:
					{ SetCursor(Cursors[CURSOR_Arc]); } break;
					case MODE_QuickSeg:
					case MODE_DrawSeg:
					case MODE_SetPerp:
					case MODE_ExtendSeg:
					case MODE_ExtendLinePt:
					{ SetCursor(Cursors[CURSOR_Seg]); } break;
					default:
					{ Assert(0); }
				}
			}
		}
#else
			if(PlatRequest.Pan)
			{
				gCursorHandle = (Cursors[CURSOR_Pan]);
			}
			else
			{
				switch(State->InputMode)
				{
					case MODE_Normal:
					{ gCursorHandle = (Cursors[CURSOR_Normal]); } break;
					case MODE_SetBasis:
					{ gCursorHandle = (Cursors[CURSOR_Basis]); } break;
					case MODE_SetLength:
					case MODE_DrawArc:
					case MODE_ExtendArc:
					{ gCursorHandle = (Cursors[CURSOR_Arc]); } break;
					case MODE_QuickSeg:
					case MODE_DrawSeg:
					case MODE_SetPerp:
					case MODE_ExtendSeg:
					case MODE_ExtendLinePt:
					{ gCursorHandle = (Cursors[CURSOR_Seg]); } break;
					default:
					{ Assert(0); }
				}
			}
#endif
		
		
		ReallocateArenas(State, Window.Handle);

		FrameTimer = Win32WaitForFrameEnd(FrameTimer);
		FrameTimer = Win32EndFrameTimer(FrameTimer);
		++State->FrameCount;

		// NOTE: while should allow for cancels part way through the save
		if(!GlobalRunning)
		{ GlobalRunning = ! Win32ConfirmFileClose(State, Window.Handle); }
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
