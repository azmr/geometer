#ifndef GEOMETER_H
#include <types.h>

typedef struct debug_text
{
#define DEBUG_TEXT_SIZE 8192
	uint Length;
	char Text[DEBUG_TEXT_SIZE];
} debug_text;
static debug_text DebugText;
#define DebugAdd(txt, ...) DebugText.Length += stbsp_snprintf(DebugText.Text, DEBUG_TEXT_SIZE, "%s"txt, DebugText.Text, __VA_ARGS__)
#define DebugClear() if(DebugText.Length > DEBUG_TEXT_SIZE) DebugText.Length = DEBUG_TEXT_SIZE;\
					 for(unsigned int i = 0; i < DebugText.Length; ++i)  DebugText.Text[i] = 0
#define DebugReplace(txt, ...) DebugClear(); DebugAdd(txt, __VA_ARGS__)

#include <stb_sprintf.h>
#include <maths.h>
#include <intrinsics.h>
#define LOGGING 1
#include <debug.h>
#include <geometry.h>
#include <platform.h>
#include <memory.h>
#include "gfx.h"
#include <fonts.h>
#include <input.h>

#define POINT_EPSILON 0.02f

typedef struct line_points
{
	// NOTE: corresponds with point in index
	uint P1;
	uint P2;
} line_points;
line_points ZeroLineP = {0};

typedef struct circle
{
	uint ipoFocus;
	uint ipoRadius;
} circle;

typedef struct arc
{
	uint ipoFocus;
	uint ipoStart;
	uint ipoEnd;
} arc;

typedef void drawstring(image_buffer *ImgBuffer, font *Font, char *Str, f32 SizeInEms, f32 XOffset, f32 YOffset, b32 InvDirection, colour Colour);
typedef struct debug
{
	image_buffer *Buffer;
	drawstring *Print;
	font Font;
	v2 P;
	f32 FontSize;
} debug;
global_variable debug Debug;
#define DebugPrint() Debug.Print(Debug.Buffer, &Debug.Font, DebugText.Text, Debug.FontSize, Debug.P.X, Debug.P.Y, 0, BLACK)

// TODO: make these as orthogonal as possible?
typedef enum
{
	POINT_Free         = 0,
	POINT_Extant       = (1 << 0),
	POINT_Line         = (1 << 1),
	POINT_Intersection = (1 << 2),
	POINT_Focus        = (1 << 3),
	POINT_Text         = (1 << 4),
	POINT_Radius       = (1 << 5), // maybe POINT_Dist | POINT_Arc ?
	// POINT_Dir
	POINT_Arc          = (1 << 6),
	POINT_Dist         = (1 << 7), 
} point_flags;

typedef struct basis
{
	v2 XAxis; // NOTE: describes X-axis. Y-axis is the perp.
	v2 Offset;
	// TODO: include in XAxis
	f32 Zoom;
} basis;

typedef struct draw_state
{
	basis Basis;

	uint iLastPoint;
	uint cPoints;
	uint iLastLinePoint;
	uint cLinePoints;
	uint iLastCircle;
	uint cCircles;
	// TODO: should circles and arcs be consolidated?
	uint iLastArc;
	uint cArcs;
	// TODO: allocate dynamically
#define NUM_POINTS 256

	uint LinePoints[NUM_POINTS*2];
	circle Circles[NUM_POINTS];
	arc Arcs[NUM_POINTS];
	v2 Points[NUM_POINTS];
	u8 PointStatus[NUM_POINTS];
} draw_state;

typedef struct state
{
#define NUM_UNDO_STATES 16
	uint CurrentDrawState;
	uint cDrawStates;
	// TODO: uint StateWhenSaved;
	draw_state Draw[NUM_UNDO_STATES];
		
	u64 FrameCount;
	f32 dt;
	font DefaultFont;
	// TODO: turn bools into flags?
	b32 ShowDebugInfo;
	b32 CloseApp;

	// v2 Basis;
	uint ipoDrag; // TODO: consolidate into ipoSelect
	uint ipoSelect;
	uint ipoArcStart;
	v2 poSaved;
	b32 PointSnap;

	u8 SavedStatus[2];
	// NOTE: woefully underspecced:
	u64 OverflowTest;
} state;

#define UPDATE_AND_RENDER(name) void name(image_buffer *ScreenBuffer, memory *Memory, input Input)

DECLARE_DEBUG_FUNCTION;

#define TEST_INPUT() DrawRectangleFilled(ScreenBuffer, Origin, ScreenSize, RED)
#define TEST_INPUT2() DrawRectangleFilled(ScreenBuffer, Origin, ScreenSize, GREEN)
#define TEST_INPUT3() DrawRectangleFilled(ScreenBuffer, Origin, ScreenSize, BLUE)

#define GEOMETER_H
#endif
