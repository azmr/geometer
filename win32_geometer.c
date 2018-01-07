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

#if INTERNAL
#include <stdio.h>
#endif // INTERNAL

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
	HEAD_Lengths_v1,
} header_section;

typedef struct file_header
{
	char ID[8];        // unique(ish) text id: "Geometer"
	u16 FormatVersion; // file format version num
	u16 cArrays;       // for data section
	u32 CRC32;         // checksum of data
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
	b32 Result = State->iCurrentAction != State->iSaveAction;
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

internal inline size_t
ArenaAllocSize(u32 cElements, size_t ElementSize, u32 Factor, u32 Add)
{
	size_t cBytesEl = cElements * ElementSize + ElementSize; // Account for index 0
	size_t Result = CeilPow2U64(Factor * cBytesEl + Add * ElementSize);
	if(Result < ElementSize * cSTART_POINTS)
	{  Result = ElementSize * cSTART_POINTS; }
	return Result;
}

internal inline u64
ReadFileArrayToArena(FILE *File, memory_arena *Arena, u32 cElements, u32 ElementSize, u32 Factor, u32 Add, HWND Window)
{
	size_t cBytesEl = cElements * ElementSize;
	MemErrorOnFail(Window, ArenaRealloc(Arena, ArenaAllocSize(cElements, ElementSize, Factor, Add)));
	Arena->Used = cBytesEl + ElementSize; // Account for index 0
	// TODO: check individual array size is right
	Assert(Arena->Base);
	size_t cElCheck = fread(Arena->Bytes + ElementSize, ElementSize, cElements, File);
	Assert(cElements == cElCheck);
	return cBytesEl;
}

internal inline u32
CRC32FileArray(u32 Continuation, u32 Tag, u32 Count, void *Array, size_t ElSize)
{
	u32 Result = Continuation;
	Result = CRC32(&Tag, sizeof(Tag), Result);
	Result = CRC32(&Count, sizeof(Count), Result);
	Result = CRC32(Array, Count * ElSize, Result);
	return Result;
}

internal FILE *
OpenFileInCurrentWindow(state *State, char *FilePath, uint cchFilePath, HWND WindowHandle)
{
	FILE *Result = 0;
	if(FilePath)
	{
		Result = fopen(FilePath, "r+b");
		FileErrorOnFail(WindowHandle, Result, FilePath); 
		u32 OpenCRC32 = 0;
		file_header FH;
		LOG("\tCHECK ID");
		DoAssert(fread(&FH, sizeof(FH), 1, Result));
		if(!(FH.ID[0]=='G'&&FH.ID[1]=='e'&&FH.ID[2]=='o'&&FH.ID[3]=='m'&&
			 FH.ID[4]=='e'&&FH.ID[5]=='t'&&FH.ID[6]=='e'&&FH.ID[7]=='r'))
		{ goto open_end; }

		// TODO (opt): read in entire file in one go based on cBytes
		// all in from now?
		ChangeFilePath(State, FilePath, cchFilePath);
		State->iCurrentAction = 0;
		State->iLastAction    = 0;

#define FileDataWarning(desc, title) MessageBox(WindowHandle, desc \
				"\nThe file might be corrupted, or Geometer might have made an error.\n\n" \
				"If your file looks correct you can continue, " \
				"but I advise you to back up the existing file before saving over it " \
				"(e.g. copy and paste the file in 'My Computer').", title, MB_ICONWARNING)

		u64 cBytesCheck = 0;
		switch(FH.FormatVersion)
		{
			case 1:
			{
				u64 cBytesCheckT = 0;
				u32 ElType;
				u32 cElements;
				for(uint iArray = 0; iArray < FH.cArrays; ++iArray)
				{
					DoAssert(cBytesCheckT = fread(&ElType,    sizeof(ElType),    1, Result) * sizeof(ElType));
					cBytesCheck += cBytesCheckT;
					DoAssert(cBytesCheckT = fread(&cElements, sizeof(cElements), 1, Result) * sizeof(cElements));
					cBytesCheck += cBytesCheckT;
					switch(ElType)
					{
#define FILE_HEADER_ARENA(ID, vNum, type, mult, add) \
						case HEAD_## ID ##_## vNum: { \
							cBytesCheckT = ReadFileArrayToArena(Result, &State->ma## ID.Arena, cElements, sizeof(type), mult, add, WindowHandle); \
							cBytesCheck += cBytesCheckT; \
							if(cBytesCheckT != cElements * sizeof(type))  \
							{ FileDataWarning("Unexpected quantity of data or corrupted file. ("#ID")", "Unexpected quantity of data"); } \
							ArenaRealloc(&State->maPointsOnScreen.Arena, ArenaAllocSize(cElements, sizeof(type), mult, add)); \
							OpenCRC32 = CRC32FileArray(OpenCRC32, HEAD_## ID ##_## vNum, cElements, State->ma## ID.Bytes + sizeof(type), sizeof(type)); \
						} break

						FILE_HEADER_ARENA(Points,      v1, v2,     2, 0);
						FILE_HEADER_ARENA(PointStatus, v1, u8,     2, 0);
						FILE_HEADER_ARENA(Shapes,      v1, shape,  1, 2);
						FILE_HEADER_ARENA(Actions,     v1, action, 2, 0);
#undef FILE_HEADER_ARENA

#define FILE_HEADER_ARRAY(ID, vNum, to, num) \
						case HEAD_## ID ##_## vNum: { \
							u64 cElCheck = fread(to, sizeof(*(to)), cElements, Result); \
							if(cElCheck != cElements) \
							{ FileDataWarning("Unexpected quantity of data or corrupted file. ("#ID")", "Unexpected quantity of data"); } \
							cBytesCheck += cElCheck * sizeof(*(to)); \
							Assert(cElCheck == num); \
							OpenCRC32 = CRC32FileArray(OpenCRC32, HEAD_## ID ##_## vNum, cElements, to, sizeof(*(to))); \
						} break

						FILE_HEADER_ARRAY(Lengths, v1, State->LengthStores, 26);
						FILE_HEADER_ARRAY(Basis, v1, &State->Basis, 1);
#undef FILE_HEADER_ARRAY

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

			default: { goto open_error; }
		}
 
		if(cBytesCheck != FH.cBytes)
		{
			if(cBytesCheck < FH.cBytes)
			{ FileDataWarning("There was less data in the file than expected.", "Unexpected quantity of data"); }
			else
			{ FileDataWarning("There was more data in the file than expected.", "Unexpected quantity of data"); }
		}
		if(OpenCRC32 != FH.CRC32) { FileDataWarning( "The validity check (CRC32) for opening this file failed." , "CRC32 check failed"); }
		// fclose?
		State->iLastPoint  = (uint)Len(State->maPoints)  - 1;
		State->iLastShape  = (uint)Len(State->maShapes)  - 1;
		State->iLastAction = (uint)Len(State->maActions) - 1;
		State->iCurrentAction = State->iSaveAction = State->iLastAction;
		uint cIntersects = CountShapeIntersects(State->maPoints.Items, State->maShapes.Items + 1, State->iLastShape);
		MemErrorOnFail(0, ArenaRealloc(&State->maIntersects.Arena, ArenaAllocSize(cIntersects, sizeof(v2), 2, 0)));
		RecalcAllIntersects(State);
#undef FileDataWarning
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
internal u32
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
	Header.cArrays = 6; // IMPORTANT: Keep updated when adding new arrays!
	Header.CRC32 = 0;  // edited in following macros
	Header.cBytes = 0; // edited in following macros

#define PROCESS_DATA_ARRAY() \
	DATA_PROCESS(HEAD_Points_v1,      State->iLastPoint,  State->maPoints.Items + 1); \
	DATA_PROCESS(HEAD_PointStatus_v1, State->iLastPoint,  State->maPointStatus.Items + 1); \
	DATA_PROCESS(HEAD_Shapes_v1,      State->iLastShape,  State->maShapes.Items + 1); \
	DATA_PROCESS(HEAD_Actions_v1,     State->iLastAction, State->maActions.Items + 1); \
	DATA_PROCESS(HEAD_Lengths_v1,     cLengthStores,      State->LengthStores); \
	DATA_PROCESS(HEAD_Basis_v1,       One,                &State->Basis)
	//           elementType          cElements           arraybase

	u32 Tag; 
	u32 One = 1; // To make it addressable
	u32 cLengthStores = ArrayCount(State->LengthStores);
	// CRC processing and byte count
#define DATA_PROCESS(tag, count, arraybase) \
	Header.CRC32 = CRC32FileArray(Header.CRC32, tag, count, arraybase, sizeof(*(arraybase))); \
	Header.cBytes += 2*sizeof(u32) + (count)*sizeof(*(arraybase))

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
	return Header.CRC32;
}

internal void
ReallocateArenas(state *State, HWND WindowHandle)
{ LOG("REALLOCATION")
#define ArenaAssert(arena) Assert(arena->Used <= arena->Size)
	// TODO: what to do if reallocation fails? Ensure no more shapes/points etc; Error message box: https://msdn.microsoft.com/en-us/library/windows/desktop/ms645505(v=vs.85).aspx
	v2_arena *maPoints              = &State->maPoints;
	u8_arena *maPointStatus         = &State->maPointStatus;
	shape_arena *maShapes           = &State->maShapes;
	v2_arena *maPointsOnScreen      = &State->maPointsOnScreen;
	shape_arena *maShapesNearScreen = &State->maShapesNearScreen;
	v2_arena *maIntersects          = &State->maIntersects;
	action_arena *maActions         = &State->maActions;

	// NOTE: Can add multiple points per frame (intersections), but can't double
	// NOTE: Realloc the next undo state if needed
	ArenaAssert(maPoints);
	ArenaAssert(maPointStatus);
	ArenaAssert(maPointsOnScreen);
	if(maPoints->Used >= maPoints->Size / 2)
	{ LOG("Adding to points arena");
		// NOTE: points and pointstatus should have exactly the same number of members
		Assert(maPointStatus->Used/sizeof(u8) == maPoints->Used/sizeof(v2));
		MemErrorOnFail(WindowHandle, ArenaRealloc(&maPoints->Arena,         maPoints->Size * 2));
		MemErrorOnFail(WindowHandle, ArenaRealloc(&maPointStatus->Arena,    maPointStatus->Size * 2));
		MemErrorOnFail(WindowHandle, ArenaRealloc(&maPointsOnScreen->Arena, maPointsOnScreen->Size * 2));
	}

	// NOTE: Can only create 1 shape per frame
	ArenaAssert(maShapes);
	ArenaAssert(maShapesNearScreen);
	ArenaAssert(maIntersects);
	if(maIntersects->Used >= maIntersects->Size / 2)
	{ MemErrorOnFail(WindowHandle, ArenaRealloc(&maIntersects->Arena,         maIntersects->Size * 2)); }
	if(maShapes->Used >= maShapes->Size)
	{ LOG("Adding to shapes arena");
		MemErrorOnFail(WindowHandle, ArenaRealloc(&maShapes->Arena,           maShapes->Size * 2));
		MemErrorOnFail(WindowHandle, ArenaRealloc(&maShapesNearScreen->Arena, maShapesNearScreen->Size * 2));
	}
	ArenaAssert(maActions);
	// TODO: this will change once action = user action
	if(maActions->Used >= maActions->Size/2 - sizeof(action))
	{ LOG("Adding to actions arena");
		MemErrorOnFail(WindowHandle, ArenaRealloc(&maActions->Arena, maActions->Size * 2));
	}
#undef ArenaAssert
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
			State->iSaveAction = State->iCurrentAction;
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
		shape Shape = Pull(State->maShapes, iShape);
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
	v2 *Points = State->maPoints.Items;
	shape *Shapes = State->maShapes.Items;
	uint iLastShape = State->iLastShape;
	uint iFirstValidShape = FirstValidShape(State);

	if(iFirstValidShape)
	{
		aabb TotalAABB = AABBOfAllShapes(Points, Shapes, iFirstValidShape, iLastShape);
		// TODO: have better automatic sizing for export
#if 0
		// NOTE: svg is top down. I'm assuming bottom-up.
		v2 Min = CanvasToSVG(V2(TotalAABB.MinX, TotalAABB.MaxY), TotalAABB);
		v2 Max = CanvasToSVG(V2(TotalAABB.MaxX, TotalAABB.MinY), TotalAABB);
		aabb BorderAABB = { Min.X, Min.Y, Max.X, Max.Y };
#endif
		FILE *SVGFile = NewSVG(FilePath, "fill='none' stroke-width='1' stroke='black' stroke-linecap='round'");
				/* AABBWidth(TotalAABB), AABBHeight(TotalAABB)); */
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
	// else has been cancelled (or an error message has already been presented)
	END_TIMED_BLOCK;
}

internal void
FreeStateArenas(state *State)
{
	Free(State->maPoints.Base);
	Free(State->maPointStatus.Base);
	Free(State->maShapes.Base);
	Free(State->maActions.Base);
	Free(State->maIntersects.Base);
	Free(State->maPointsOnScreen.Base);
	Free(State->maShapesNearScreen.Base);
}

internal void
AllocStateArenas(state *State)
{
	State->maPointStatus.Arena      = ArenaCalloc(sizeof(u8    ) * cSTART_POINTS);
	State->maPoints.Arena           = ArenaCalloc(sizeof(v2    ) * cSTART_POINTS);
	State->maShapes.Arena           = ArenaCalloc(sizeof(shape ) * cSTART_POINTS);
	State->maActions.Arena          = ArenaCalloc(sizeof(action) * cSTART_POINTS);
	State->maIntersects.Arena       = ArenaCalloc(sizeof(v2    ) * cSTART_POINTS);
	State->maPointsOnScreen.Arena   = ArenaCalloc(sizeof(v2    ) * cSTART_POINTS);
	State->maShapesNearScreen.Arena = ArenaCalloc(sizeof(shape ) * cSTART_POINTS);
}

internal void
HardReset(state *State, FILE *OpenFile)
{
	// TODO: set length and pLength (and bases?)
	if(OpenFile) { fclose(OpenFile); }
	FreeStateArenas(State);
	Free(State->FilePath);
	state NewState = {0};
	AllocStateArenas(&NewState);
	ChangeFilePath(&NewState, calloc(1, 1), 1); // 1 byte set to 0 (empty string)
	NewState.DefaultFont = State->DefaultFont;
	NewState.maActions.Used = sizeof(action);
	
	ResetNoAction(&NewState, 0);
	*State = NewState;
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
	// Should already be cleared to 0, but just to be doubly sure...
	state EmptyState = {0};
	*State = EmptyState;

	LOG("OPEN BLANK FILE");
	HardReset(State, 0);

	State->cchFilePath = 1024;
	State->FilePath = calloc(State->cchFilePath, sizeof(char));
	FILE *OpenedFile = 0;

	char **argv = __argv;
	uint   argc = __argc;
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
		// TODO IMPORTANT: fix random overwrites of data
		// may be OS freeing data used in Lib..?
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

#if INTERNAL && DEBUG_LOG_ACTIONS
		// Just in case the location-specific logs missed any changes
		// TODO: How best to assert for anything missed?
		LogActionsToFile(State, "ActionLog.txt");
#endif

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
