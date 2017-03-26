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

typedef struct state
{
	u64 FrameCount;
	f32 dt;
	font DefaultFont;
	b32 CloseApp;

	uint DragIndex;
	v2 SavedPoint;
	b32 PointSnap;
	uint PointIndex;
	uint NumLinePoints;
	// TODO: allocate dynamically
#define NUM_POINTS 256
	union
	{
		uint LinePoints[NUM_POINTS*2];
		line_points Lines[NUM_POINTS];
	};
	v2 Points[NUM_POINTS];
} state;

#define UPDATE_AND_RENDER(name) void name(image_buffer *ScreenBuffer, memory *Memory, input Input)

DECLARE_DEBUG_FUNCTION;

#define GEOMETER_H
#endif
