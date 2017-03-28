#ifndef GEOMETER_H
#include <types.h>
#include <maths.h>
#include <intrinsics.h>
#include <debug.h>
#include <geometry.h>
#include <platform.h>
#include <memory.h>
#include <gfx.h>
#include <fonts.h>
#include <input.h>
#include <stb_sprintf.h>

typedef struct line_points
{
	// NOTE: corresponds with point in index
	uint P1;
	uint P2;
} line_points;
line_points ZeroLineP = {0};

// TODO: make these as orthogonal as possible?
typedef enum
{
	POINT_Free         = 0, // also temporary points unless this becomes an issue
	POINT_Line         = (1 << 0),
	POINT_Intersection = (1 << 1),
	POINT_Centre       = (1 << 2),
} PointFlags;

typedef struct state
{
	u64 FrameCount;
	f32 dt;
	font DefaultFont;
	b32 CloseApp;

	uint DragIndex;
	v2 SavedPoint;
	b32 MidEdit;
	b32 PointSnap;

	uint LastPoint;
	uint NumPoints;
	uint LastLinePoint;
	uint NumLinePoints;
	// TODO: allocate dynamically
#define NUM_POINTS 256

	uint LinePoints[NUM_POINTS*2];
	v2 Points[NUM_POINTS];
	u8 PointStatus[NUM_POINTS];
	// NOTE: woefully underspecced:
	u64 OverflowTest;
} state;

#define UPDATE_AND_RENDER(name) void name(image_buffer *ScreenBuffer, memory *Memory, input Input)

DECLARE_DEBUG_FUNCTION;

#define GEOMETER_H
#endif
