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
	uint Focus;
	f32 Radius;
} circle;

/// expects d to be normalised
internal uint
IntersectLineCircleForT(v2 P, v2 d, v2 Focus, f32 Radius, f32 *t1, f32 *t2)
{
	uint Result = 0;
	// Distance from point on line to circle centre == radius:
	// 		(P + td - C) DOT (P + td - C) == radius^2
	// m=P-C
	v2 m = V2Sub(P, Focus);
	// Simplifies to t^2 + 2(m DOT d)t + (m DOT m) - r^2 == 0
	// Solving quadratic equation  -b +/- sqrt(b^2 - c)
	f32 b = Dot(m, d);
	f32 Dotmm = Dot(m, m);
	f32 c = Dotmm - Radius*Radius;
	f32 Discriminant = b*b - c;

	if(Discriminant < 0.f) return Result;
	else if(Discriminant == 0.f)
	{
		Result = 1;
		*t1 = -b;
	}

	else // if(Discriminant > 0.f)
	{
		// TODO: check if t makes ray/segment start/end inside circle
		Result = 2;
		f32 RootDisc = QSqrt(Discriminant);
		*t1 = -b - RootDisc;
		*t2 = -b + RootDisc;
	}

	return Result;
}

internal uint
IntersectLineCircle(v2 P, v2 Dir, v2 Focus, f32 Radius, v2 *Intersection1, v2 *Intersection2)
{
	f32 t1, t2;
	v2 d = Norm(Dir);
	uint Result = IntersectLineCircleForT(P, d, Focus, Radius, &t1, &t2);

	if(Result)
	{
		*Intersection1 = V2Add(P, V2Mult(t1, d));
		//if(Result == 2)
			*Intersection2 = V2Add(P, V2Mult(t2, d));
	}
	return Result;
}

internal uint
IntersectRayCircle(v2 P, v2 Dir, v2 Focus, f32 Radius, v2 *Intersection1, v2 *Intersection2)
{
	f32 t1, t2;
	v2 d = Norm(Dir);
	uint Result = IntersectLineCircleForT(P, d, Focus, Radius, &t1, &t2);

	if(Result)
	{
		b32 FirstPoint = 1;
		if(t1 < 0.f) FirstPoint = 0;
		if(FirstPoint)
		{
			*Intersection1 = V2Add(P, V2Mult(t1, d));
			*Intersection2 = V2Add(P, V2Mult(t2, d));
		}

		else
		{
			*Intersection1 = V2Add(P, V2Mult(t2, d));
		}
	}
	return Result;
}

internal uint
IntersectSegmentCircle(v2 P, v2 Dir, v2 Focus, f32 Radius, v2 *Intersection1, v2 *Intersection2)
{
	f32 t1, t2;
	v2 d = Norm(Dir);
	uint Result = IntersectLineCircleForT(P, d, Focus, Radius, &t1, &t2);

	if(Result)
	{ 
		b32 FirstPoint = 1;
		b32 SecondPoint = 1;
		if(t1 < 0.f)                            FirstPoint = 0;
		if(LenSq(V2Mult(t2, d)) > LenSq(Dir))  SecondPoint = 0;
		Result = FirstPoint + SecondPoint;

		if(FirstPoint)
		{
			*Intersection1 = V2Add(P, V2Mult(t1, d));
			*Intersection2 = V2Add(P, V2Mult(t2, d));
		}
		else if(SecondPoint)
		{
			*Intersection1 = V2Add(P, V2Mult(t2, d));
		}
	}
	return Result;
}

internal uint
IntersectCircles(v2 Focus1, f32 R1, v2 Focus2, f32 R2, v2 *Intersection1, v2 *Intersection2)
{
	v2 Dir = V2Sub(Focus2, Focus1);
	// TODO: optimise out
	f32 dSq = LenSq(Dir);
	f32 RadAdd = R1 + R2;
	uint Result = 0;
	if(dSq == RadAdd * RadAdd)
	{
		// NOTE: early out for tangents
		Result = 1;
		*Intersection1 = V2Add(Focus1, V2Mult(R1, Norm(Dir)));
	}

	else
	{
		f32 Fraction = 0.5f * ((R1*R1 / dSq) - (R2*R2 / dSq) + 1);
		v2 ChordCross = V2Add(Focus1, V2Mult(Fraction, Dir));
		Result = IntersectLineCircle(ChordCross, Perp(Dir), Focus1, R1, Intersection1, Intersection2);
	}
	return Result;
}

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
	POINT_Arc          = (1 << 6),
	POINT_Dist         = (1 << 7), 
} PointFlags;

typedef struct draw_state
{
	uint LastPoint;
	uint NumPoints;
	uint LastLinePoint;
	uint NumLinePoints;
	uint LastCircle;
	uint NumCircles;
	// TODO: allocate dynamically
#define NUM_POINTS 256

	uint LinePoints[NUM_POINTS*2];
	circle Circles[NUM_POINTS];
	v2 Points[NUM_POINTS];
	u8 PointStatus[NUM_POINTS];
} draw_state;

typedef struct state
{
#define NUM_UNDO_STATES 16
	uint CurrentDrawState;
	uint NumDrawStates;
	draw_state Draw[NUM_UNDO_STATES];
		
	u64 FrameCount;
	f32 dt;
	font DefaultFont;
	b32 CloseApp;

	uint DragIndex; // TODO: consolidate into SelectIndex
	uint SelectIndex;
	v2 SavedPoint;
	/* b32 MidEdit; */
	b32 PointSnap;

	u8 SavedStatus;
	// NOTE: woefully underspecced:
	u64 OverflowTest;
} state;

#define UPDATE_AND_RENDER(name) void name(image_buffer *ScreenBuffer, memory *Memory, input Input)

DECLARE_DEBUG_FUNCTION;

#define GEOMETER_H
#endif
