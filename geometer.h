#ifndef GEOMETER_H
#include <types.h>

typedef struct debug_text
{
#define DEBUG_TEXT_SIZE 16384
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

typedef struct line
{
	// NOTE: corresponds with point in index
	uint P1;
	uint P2;
} line;
line ZeroLine = {0};

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

typedef enum shape_types
{
	SHAPE_Free = 0,
	SHAPE_Line,
	SHAPE_Ray,
	SHAPE_Segment,
	SHAPE_Circle,
	SHAPE_Arc,
} shape_types;

typedef enum action_types
{
	ACTION_Reset   = -2,
	ACTION_Remove  = -1,
	ACTION_Basis   = 0,
	ACTION_Line    = SHAPE_Line,
	ACTION_Ray     = SHAPE_Ray,
	ACTION_Segment = SHAPE_Segment,
	ACTION_Circle  = SHAPE_Circle,
	ACTION_Arc     = SHAPE_Arc,
	ACTION_Point,
} action_types;

typedef union shape_union
{
	line Line;
	circle Circle;
	arc Arc;
	uint P[3];
} shape_union;

typedef struct shape
{
	// TODO: does this guarantee a size?
	shape_types Kind;
	shape_union;
} shape;
shape gZeroShape;

// TODO: Add change of basis
typedef struct action
{
	action_types Kind;
	union {
		shape_union;
		uint iRemove;
		struct {
			uint ipo;
			v2 po;
		};
	};
} action;

internal inline b32
ShapeEq(shape S1, shape S2)
{
	b32 Result = S1.Kind == S2.Kind &&
				 S1.P[0] == S2.P[0] &&
				 S1.P[1] == S2.P[1] &&
				 S1.P[2] == S2.P[2];
	return Result;
}

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

typedef struct state
{
	memory_arena maPoints;
	memory_arena maShapes;
	memory_arena maActions; 
	memory_arena maPointStatus;
		
	basis Basis;
	basis pBasis;
	uint iAction;
	uint cActions;

	uint iLastPoint;
	uint iLastShape;
	uint cPoints;
	uint cLines;
	// TODO: should circles and arcs be consolidated?
	uint cCircles;
	uint cArcs;
	uint cShapes;

	u64 FrameCount;
	f32 dt;
	f32 tBasis;

	font DefaultFont;
	// TODO: turn bools into flags?
	b32 ShowDebugInfo;
	b32 CloseApp;

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
