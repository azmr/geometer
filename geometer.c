#define _CRT_SECURE_NO_WARNINGS
#include "geometer.h"
#include <fonts.c>

// TODO:
// =====
//
// - Find (and colour) lines intersecting at a given point
// - Unlimited undo states
//  	- save to disk for lots of them? (temporary/pich up undos on load?)
//  	- undo history (show overall/with layers)
//  	- undo by absolute and layer order
// - Change storage of intersections, so they don't all need to be recomputed on changes
// - Spatially partition(?) shapes
// - Togglable layers (should points be separate from layers, but have status set per layer?)
// - For fast movements, make sweep effect, rather than ugly multiple line effect 
// - Deal with perfect overlaps that aren't identical (i.e. one line/arc is longer)
// - Resizable windo (maintain centre vs maintain absolute position)
// - Constraint system? Macros? Paid version?
// - Make custom cursors

// UI that allows modification:
//	- LMB-drag - quick seg
//	- LMB - start drawing
//	(Alt + anything for point version)
// 		- LMB on pt - quick circle (allows double click)
// 		- LMB drag on pt - set length
// 		- LMB - circle
// 		- LMB-drag - arc
//		- RMB drag on point - set perpendicular
//		- RMB - seg
//		- RMB-drag - extend seg
//	- RMB - select point/shape
//	- RMB-drag marquee-select points/shapes
//	- Alt-RMB for +/- selection
//	...

// CONTROLS: ////////////////////////////
#define C_Cancel       Keyboard.Esc
#define CB_StartShape  LMB
#define CB_FullShape   LMB
#define CB_PointOnly   RMB
#define CB_Arc         LMB
#define CB_Line        RMB
#define CB_Length      LMB

#define C_StartShape   Mouse.CB_StartShape
#define C_FullShape    Mouse.CB_FullShape
#define C_PointOnly    Mouse.CB_PointOnly
#define C_Arc          Mouse.CB_Arc
#define C_Line         Mouse.CB_Line
#define C_Length       Mouse.CB_Length
// divide length       1-0
// mult length         Alt + 1-0
// get store length    a-z,A-Z
// set store length    Alt + a-z,A-Z

#define C_NoSnap       Keyboard.Shift
#define C_ShapeLock    Keyboard.Ctrl

#define C_BasisSet     Mouse.RMB
#define C_BasisMod     Keyboard.Space
#define C_Pan          Mouse.MMB
#define C_PanDown      Keyboard.Down
#define C_PanUp        Keyboard.Up
#define C_PanLeft      Keyboard.Left
#define C_PanRight     Keyboard.Right
#define C_PanMod       Keyboard.Space
#define C_Zoom         Mouse.ScrollV
#define C_ZoomIn       Keyboard.PgUp
#define C_ZoomOut      Keyboard.PgDn

#define C_Delete       Keyboard.Del
#define C_Reset        Keyboard.Backspace
#define C_CanvasHome   Keyboard.Home
#define C_DebugInfo    Keyboard.End
#define C_PrevLength   Keyboard.Tab
/////////////////////////////////////////

v2 gDebugV2;
v2 gDebugPoint;

internal inline void
DrawClosestPtOnSegment(image_buffer *ScreenBuffer, v2 po, v2 lipoA, v2 lipoB)
{
	BEGIN_TIMED_BLOCK;
	v2 po1 = ClosestPtOnSegment(po, lipoA, V2Sub(lipoB, lipoA));
	DrawCrosshair(ScreenBuffer, po1, 5.f, RED);
	END_TIMED_BLOCK;
}

internal inline void
DrawClosestPtOnCircle(image_buffer *ScreenBuffer, v2 po, v2 poFocus, f32 Radius)
{
	BEGIN_TIMED_BLOCK;
	v2 po1 = ClosestPtOnCircle(po, poFocus, Radius);
	DrawCrosshair(ScreenBuffer, po1, 5.f, RED);
	END_TIMED_BLOCK;
}

internal inline void
DrawClosestPtOnArc(image_buffer *ScreenBuffer, v2 po, v2 poFocus, v2 poStart, v2 poEnd)
{
	BEGIN_TIMED_BLOCK;
	v2 po1 = ClosestPtOnArc(po, poFocus, poStart, poEnd);
	DrawCrosshair(ScreenBuffer, po1, 5.f, RED);
	END_TIMED_BLOCK;
}

internal inline void
DrawActivePoint(image_buffer *ScreenBuffer, v2 po, colour Col)
{
	BEGIN_TIMED_BLOCK;
	DrawCircleFill(ScreenBuffer, po, 3.f, Col);
	CircleLine(ScreenBuffer, po, 5.f, Col);
	END_TIMED_BLOCK;
}

// 0 means nothing could be found
// NOTE: Dist is in canvas-space
internal uint
ClosestPointIndex(state *State, v2 Comp, f32 *ClosestDistSq)
{
	BEGIN_TIMED_BLOCK;
	// NOTE: all valid points start at 1
	uint Result = 0;
	// TODO: better way of doing this?
	f32 Closest = 0;
	for(uint i = 1; i <= State->iLastPoint; ++i)
	{
		if(POINTSTATUS(i) != POINT_Free)
		{
			if(!Result)
			{ // first iteration
				// TODO: maybe put this above loop
				Result = i;
				Closest = DistSq(POINTS(i), Comp);
				continue;
			}
			f32 Test = DistSq(POINTS(i), Comp);
			if(Test < Closest)
			{
				Closest = Test;
				Result = i;
			}
		}
	}
	*ClosestDistSq = Closest;
	END_TIMED_BLOCK;
	return Result;
}

// NOTE: less than numlinepoints if any points are reused
internal uint
NumPointsOfType(u8 *Statuses, uint iEnd, uint PointTypes)
{
	BEGIN_TIMED_BLOCK;
	// TODO: could be done 16x faster with SIMD (maybe just for practice)
	uint Result = 0;
	for(uint i = 1; i <= iEnd; ++i)
	{
		// TODO: Do I want inclusive as well as exclusive?
		if(Statuses[i] == PointTypes)
		{ ++Result; }
	}
	END_TIMED_BLOCK;
	return Result;
}

internal uint
FindPointAtPos(state *State, v2 po, uint PointStatus)
{
	BEGIN_TIMED_BLOCK;
	uint Result = 0;
	for(uint i = 1; i <= State->iLastPoint; ++i)
	{
		if(V2WithinEpsilon(po, POINTS(i), POINT_EPSILON) && (POINTSTATUS(i) & PointStatus))
		{
			Result = i;
			break;
		}
	}
	END_TIMED_BLOCK;
	return Result;
}

/// returns the first free point up to the last point used
/// returns 0 if none found
internal inline uint
FirstFreePoint(state *State)
{
	uint Result = 0;
	for(uint ipo = 1; ipo <= State->iLastPoint; ++ipo)
	{
		if(POINTSTATUS(ipo) == POINT_Free)
		{
			Result = ipo;
			break;
		}
	}
	return Result;
}

/// returns index of point (may be new or existing)
internal uint
AddPoint(state *State, v2 po, uint PointTypes, u8 *PriorStatus)
{
	BEGIN_TIMED_BLOCK;
	/* gDebugV2 = po; */
	uint Result = FindPointAtPos(State, po, ~(uint)POINT_Free);
	if(Result) // point exists already
	{
		// NOTE: Use existing point, but add any new status (and confirm Extant)
		if(PriorStatus) { *PriorStatus = POINTSTATUS(Result); }
		if((POINTSTATUS(Result) & (PointTypes | POINT_Extant))) // with full status
		{ goto end; } // no changes needed; exit
		else // status needs updating
		{ POINTSTATUS(Result) |= PointTypes | POINT_Extant; }
	}

	else 
	{ // add a new point
		if(PriorStatus) { *PriorStatus = POINT_Free; }
		Result = FirstFreePoint(State);
		// NOTE: Create new point if needed
		if(!Result)
		{
			PushStruct(&DRAW_STATE.maPoints, v2);
			PushStruct(&DRAW_STATE.maPointStatus, u8);
			Result = ++State->iLastPoint;
			/* ++State->cPoints; */
		}
		POINTS(Result) = po;
		POINTSTATUS(Result) |= PointTypes | POINT_Extant;
	}

	DebugReplace("AddPoint => %u\n", Result);

	PushStruct(&State->maActions, action);
	action Action;
	Action.Kind = ACTION_Point;
	Action.i = Result;
	Action.po = po;
	Action.PointStatus = POINTSTATUS(Result);
	ACTIONS(++State->iLastAction) = Action;

end:
	END_TIMED_BLOCK;
	return Result;
}

// TODO: don't auto-add intersections - only suggest when mouse is near
internal inline void
AddIntersections(state *State, v2 po1, v2 po2, uint cIntersections)
{
	BEGIN_TIMED_BLOCK;
	if(cIntersections == 1)
	{
		AddPoint(State, po1, POINT_Intersection, 0);
	}
	else if(cIntersections == 2)
	{
		AddPoint(State, po1, POINT_Intersection, 0);
		AddPoint(State, po2, POINT_Intersection, 0);
	}
	END_TIMED_BLOCK;
}

/// For 2 lines, Intersection2 can be 0
internal inline uint
IntersectShapes(state *State, shape S1, shape S2, v2 *Intersection1, v2 *Intersection2)
{
	BEGIN_TIMED_BLOCK;
	uint Result = 0;
	if(S1.Kind == SHAPE_Free || S2.Kind == SHAPE_Free) goto end;
	// NOTE: should be valid for circles or arcs
	f32 S1Radius = Dist(POINTS(S1.Circle.ipoFocus), POINTS(S1.Circle.ipoRadius));
	f32 S2Radius = Dist(POINTS(S2.Circle.ipoFocus), POINTS(S2.Circle.ipoRadius));
	v2 S1Dir = V2Sub(POINTS(S1.Line.P2), POINTS(S1.Line.P1));
	v2 S2Dir = V2Sub(POINTS(S2.Line.P2), POINTS(S2.Line.P1));
#define LPOINTS(x) POINTS(S##x.Line.P1), POINTS(S##x.Line.P2)
#define LINE(x) POINTS(S##x.Line.P1), S##x##Dir
#define CIRCLE(x) POINTS(S##x.Circle.ipoFocus), S##x##Radius
#define ARC(x) POINTS(S##x.Arc.ipoFocus), S##x##Radius, POINTS(S##x.Arc.ipoStart), POINTS(S##x.Arc.ipoEnd)
	switch(S1.Kind)
	{
		case SHAPE_Segment:
		switch(S2.Kind)
		{
			case SHAPE_Segment:
				Result = IntersectSegmentsWinding (LPOINTS(1), LPOINTS(2),Intersection1);                 break;
			case SHAPE_Circle:
				Result = IntersectSegmentCircle   (LINE(1),    CIRCLE(2), Intersection1, Intersection2);  break;
			case SHAPE_Arc:
				Result = IntersectSegmentArc	  (LINE(1),    ARC(2),    Intersection1, Intersection2);  break;
			default:
				Assert(SHAPE_Free);
		} break;

		case SHAPE_Circle:
		switch(S2.Kind)
		{													     				  
			case SHAPE_Segment:
				Result = IntersectSegmentCircle   (LINE(2),    CIRCLE(1), Intersection1, Intersection2);  break;
			case SHAPE_Circle:
				Result = IntersectCircles		  (CIRCLE(1),  CIRCLE(2), Intersection1, Intersection2);  break;
			case SHAPE_Arc:
				Result = IntersectCircleArc	      (CIRCLE(1),  ARC(2),    Intersection1, Intersection2);  break;
			default:
				Assert(SHAPE_Free);
		} break;

		case SHAPE_Arc:
		switch(S2.Kind)
		{
			case SHAPE_Segment:
				Result = IntersectSegmentArc	  (LINE(2),   ARC(1),   Intersection1, Intersection2);   break;
			case SHAPE_Circle:
				Result = IntersectCircleArc	      (CIRCLE(2), ARC(1),   Intersection1, Intersection2);   break;
			case SHAPE_Arc:
				Result = IntersectArcs			  (ARC(1),    ARC(2),   Intersection1, Intersection2);   break;
			default:
				Assert(SHAPE_Free);
		} break;

		default:
		{
			Assert(SHAPE_Free);
		}
	}
#undef LINE
#undef CIRCLE
#undef ARC

end:
	END_TIMED_BLOCK;
	return Result;
}

/// returns number of intersections
internal uint
AddShapeIntersections(state *State, uint iShape)
{
	BEGIN_TIMED_BLOCK;
	uint Result = 0, cIntersections;
	shape Shape = SHAPES(iShape);
	shape *Shapes = State->Shapes;
	v2 po1, po2;

	// NOTE: TODO? internal line between eg corners of square adds 1 intersection... sometimes?
	// IMPORTANT TODO: spatially separate, maybe hierarchically
	for(uint i = 1; i <= State->iLastShape; ++i)
	{
		if(i == iShape) continue;

		cIntersections = IntersectShapes(State, Shape, Shapes[i], &po1, &po2);
		AddIntersections(State, po1, po2, cIntersections);
		Result += cIntersections;
	}
	END_TIMED_BLOCK;
	return Result;
}

/// returns position in Shapes array
internal uint
AddShape(state *State, shape Shape)
{
	BEGIN_TIMED_BLOCK;
	uint Result = 0;
	b32 ExistingShape = 0;
	shape *Shapes = State->Shapes;
	uint iShape;
	// NOTE: check if exists already
	for(iShape = 1; iShape <= State->iLastShape; ++iShape)
	{
		if(ShapeEq(Shape, Shapes[iShape]))
		{
			ExistingShape = 1;
			break;
		}
	}

	if(!ExistingShape)
	{
		// NOTE: check for free shape to fill
		uint iEmptyShape = 0;
		for(iShape = 1; iShape <= State->iLastShape; ++iShape)
		{
			if(Shapes[iShape].Kind == SHAPE_Free)
			{
				iEmptyShape = iShape;
				break;
			}
		}

		if(iEmptyShape)
		{ // NOTE: fill empty shape
			Shapes[iEmptyShape] = Shape;
			AddShapeIntersections(State, iEmptyShape);
			Result = iEmptyShape;
		}
		else
		{ // NOTE: new shape
			PushStruct(&DRAW_STATE.maShapes, shape);
			Shapes[++State->iLastShape] = Shape;
			Result = State->iLastShape;
			AddShapeIntersections(State, Result);
			/* ++State->cShapes; */
		}

		PushStruct(&State->maActions, action);
		action Action;
		Action.Kind = Shape.Kind;
		Action.i = Result;
		Action.P[0] = Shape.P[0];
		Action.P[1] = Shape.P[1];
		Action.P[2] = Shape.P[2];
		ACTIONS(++State->iLastAction) = Action;
	}
	END_TIMED_BLOCK;
	return Result;
}

/// returns position in Shapes array
internal inline uint
AddSegment(state *State, uint P1, uint P2)
{
	shape Shape;
	Shape.Kind = SHAPE_Segment;
	Shape.Line.P1 = P1;
	Shape.Line.P2 = P2;
	uint Result = AddShape(State, Shape);
	return Result;
}

/// returns position in Shapes array
internal inline uint
AddCircle(state *State, uint ipoFocus, uint ipoRadius)
{
	shape Shape;
	Shape.Kind = SHAPE_Circle;
	Shape.Circle.ipoFocus  = ipoFocus;
	Shape.Circle.ipoRadius = ipoRadius;
	uint Result = AddShape(State, Shape);
	return Result;
}

/// returns position in Shapes array
internal inline uint
AddArc(state *State, uint ipoFocus, uint ipoStart, uint ipoEnd)
{
	shape Shape;
	Shape.Kind = SHAPE_Arc;
	Shape.Arc.ipoFocus = ipoFocus;
	Shape.Arc.ipoStart = ipoStart;
	Shape.Arc.ipoEnd   = ipoEnd;
	uint Result = AddShape(State, Shape);
	return Result;
}

internal inline void
InvalidateShapesAtPoint(state *State, uint ipo)
{
	BEGIN_TIMED_BLOCK;
	shape *Shapes = State->Shapes;
	for(uint i = 1; i <= State->iLastShape; ++i)
	{ // NOTE: if any of the shape's points match, remove it
		if(Shapes[i].P[0] == ipo || Shapes[i].P[1] == ipo || Shapes[i].P[2] == ipo)
			Shapes[i].Kind = SHAPE_Free;
	}
	END_TIMED_BLOCK;
}

internal inline void
InvalidatePoint(state *State, uint ipo)
{
	BEGIN_TIMED_BLOCK;
	InvalidateShapesAtPoint(State, ipo);
	POINTSTATUS(ipo) = POINT_Free;
	PushStruct(&State->maActions, action);
	action Action;
	Action.Kind =  ACTION_Remove;
	Action.i = ipo;
	ACTIONS(++State->iLastAction) = Action;
	END_TIMED_BLOCK;
}

/// returns number of points removed
internal uint
RemovePointsOfType(state *State, uint PointType)
{
	BEGIN_TIMED_BLOCK;
	uint Result = 0;
	for(uint i = 1; i <= State->iLastPoint; ++i)
	{
		if(POINTSTATUS(i) & PointType)
		{
			InvalidatePoint(State, i);
			++Result;
		}
	}
	END_TIMED_BLOCK;
	return Result;
}

internal inline void
ArcFromPoints(image_buffer *Buffer, v2 Centre, v2 A, v2 B, colour Colour)
{
	ArcLine(Buffer, Centre, Dist(Centre, A), V2Sub(A, Centre), V2Sub(B, Centre), Colour);
}

internal inline b32
SameAngle(v2 A, v2 B)
{
	b32 Result = WithinEpsilon(Dot(A, B), 1.f, POINT_EPSILON);
	return Result;
}

internal inline basis
BasisLerp(basis Start, f32 t, basis End)
{
	basis Result;
	Result.XAxis  = Norm(V2Lerp(Start.XAxis, t, End.XAxis));
	Result.Offset = V2Lerp(Start.Offset, t, End.Offset);
	Result.Zoom   = Lerp(Start.Zoom, t, End.Zoom);
	return Result;
}

internal inline v2
V2RotateToAxis(v2 XAxis, v2 V)
{
	BEGIN_TIMED_BLOCK;
	v2 Result = V;
	// NOTE: based on working out for 2x2 matrix where j = perp(i)
	// x and y are for the i axis; a and b for operand
	f32 x = XAxis.X;
	f32 y = XAxis.Y;
	f32 a = Result.X;
	f32 b = Result.Y;
	Result.X = a * x - b * y;
	Result.Y = a * y + b * x;
	END_TIMED_BLOCK;
	return Result;
}
internal inline v2
V2ScreenToCanvas(basis Basis, v2 V, v2 ScreenCentre)
{
	BEGIN_TIMED_BLOCK;
	v2 Result = V2Sub(V, ScreenCentre);
	// NOTE: based on working out for 2x2 matrix where j = perp(i)
	// x and y are for the i axis; a and b for operand
	f32 x = Basis.XAxis.X * Basis.Zoom;
	f32 y = Basis.XAxis.Y * Basis.Zoom;
	f32 a = Result.X;
	f32 b = Result.Y;
	Result.X = a * x - b * y;
	Result.Y = a * y + b * x;
	Result = V2Add(Result, Basis.Offset);
	END_TIMED_BLOCK;
	return Result;
}

internal inline v2
V2CanvasToScreen(basis Basis, v2 V, v2 ScreenCentre)
{
	BEGIN_TIMED_BLOCK;
	v2 Result = V2Sub(V, Basis.Offset);
	// NOTE: based on working out for inverse of 2x2 matrix where j = perp(i)
	// x and y are for the i axis; a and b for operand
	f32 x = Basis.XAxis.X * Basis.Zoom;
	f32 y = Basis.XAxis.Y * Basis.Zoom;
	f32 invXSqPlusYSq = 1.f/(x*x + y*y);
	f32 a = Result.X;
	f32 b = Result.Y;
	Result.X = (a*x + b*y) * invXSqPlusYSq;
	Result.Y = (b*x - a*y) * invXSqPlusYSq;
	Result = V2Add(Result, ScreenCentre);
	END_TIMED_BLOCK;
	return Result;
}

internal inline void
OffsetDraw(state *State, int Offset)
{
	uint iPrevDraw = State->iCurrentDraw;
	State->iCurrentDraw = iDrawOffset(State, Offset);
	State->cDraws += Offset;
	UpdateDrawPointers(State, iPrevDraw);
}

internal inline v2
ExtendSegment(v2 poStart, v2 poDir, v2 poLength)
{
	v2 LineAxis = Norm(V2Sub(poDir, poStart));
	v2 RelLength = V2Sub(poLength, poStart);
	// Project RelLength onto LineAxis
	f32 ExtendLength = Dot(LineAxis, RelLength);
	v2 Result = V2WithDist(poStart, poDir, ExtendLength);
	return Result;
}

internal uint
ClosestPtIntersectingCircle(v2 *Points, shape *Shapes, uint iLastShape, v2 P, v2 TestFocus, f32 TestRadius, v2 *poClosest)
{
	// TODO: necessary?
	*poClosest = TestFocus; 

	v2 poIntersect1 = ZeroV2, poIntersect2 = ZeroV2;
	uint cTotalIntersects = 0;
	for(uint iShape = 1; iShape <= iLastShape; ++iShape)
	{
		shape Shape = Shapes[iShape];
		uint cIntersects = 0;
		switch(Shape.Kind)
		{ // find the intersects with a circle from poSaved to mouse
			case SHAPE_Circle:
				{
					v2 poFocus = Points[Shape.Circle.ipoFocus];
					f32 Radius = Dist(poFocus, Points[Shape.Circle.ipoRadius]);
					cIntersects = IntersectCircles(TestFocus, TestRadius, poFocus, Radius,
							&poIntersect1, &poIntersect2);
				} break;

			case SHAPE_Arc:
				{
					v2 poFocus = Points[Shape.Arc.ipoFocus];
					v2 poStart = Points[Shape.Arc.ipoStart];
					v2 poEnd   = Points[Shape.Arc.ipoEnd];
					f32 Radius = Dist(poFocus, poStart);
					cIntersects = IntersectCircleArc(TestFocus, TestRadius, poFocus, Radius, poStart, poEnd,
							&poIntersect1, &poIntersect2);
				} break;

			case SHAPE_Segment:
				{
					v2 po = Points[Shape.Line.P1];
					v2 Dir = V2Sub(Points[Shape.Line.P2], po);
					cIntersects = IntersectSegmentCircle(po, Dir, TestFocus, TestRadius,
							&poIntersect1, &poIntersect2);
				} break;

			default: { /* do nothing */ }
		}
		cTotalIntersects += cIntersects;

		// update closest candidate
		if(cIntersects	  && DistSq(P, poIntersect1) < DistSq(P, *poClosest))
		{ *poClosest = poIntersect1; }
		if(cIntersects == 2 && DistSq(P, poIntersect2) < DistSq(P, *poClosest))
		{ *poClosest = poIntersect2; }
	}
	return cTotalIntersects;
}

internal uint
ClosestPtIntersectingLine(v2 *Points, shape *Shapes, uint iLastShape, v2 P, v2 TestStart, v2 TestDir, v2 *poClosest)
{
	*poClosest = TestStart; 

	v2 poIntersect1 = ZeroV2, poIntersect2 = ZeroV2;
	uint cTotalIntersects = 0;
	// TODO (opt): extract to function with function params
	for(uint iShape = 1; iShape <= iLastShape; ++iShape)
	{
		shape Shape = Shapes[iShape];
		uint cIntersects = 0;
		switch(Shape.Kind)
		{ // find the intersects with a line going through poExtend
			case SHAPE_Circle:
				{
					v2 poFocus = Points[Shape.Circle.ipoFocus];
					f32 Radius = Dist(poFocus, Points[Shape.Circle.ipoRadius]);
					cIntersects = IntersectLineCircle(TestStart, TestDir, poFocus, Radius,
							&poIntersect1, &poIntersect2);
				} break;

			case SHAPE_Arc:
				{
					v2 poFocus = Points[Shape.Arc.ipoFocus];
					v2 poStart = Points[Shape.Arc.ipoStart];
					v2 poEnd   = Points[Shape.Arc.ipoEnd];
					f32 Radius = Dist(poFocus, poStart);
					cIntersects = IntersectLineArc(TestStart, TestDir, poFocus, Radius, poStart, poEnd,
							&poIntersect1, &poIntersect2);
				} break;

			case SHAPE_Segment:
				{
					v2 po = Points[Shape.Line.P1];
					v2 Dir = V2Sub(Points[Shape.Line.P2], po);
					cIntersects = IntersectLineSegment(TestStart, TestDir, po, Dir, &poIntersect1);
				} break;

			default: { /* do nothing */ }
		}

		cTotalIntersects += cIntersects;

		// update closest candidate
		if(cIntersects	  && DistSq(P, poIntersect1) < DistSq(P, *poClosest))
		{ *poClosest = poIntersect1; }
		if(cIntersects == 2 && DistSq(P, poIntersect2) < DistSq(P, *poClosest))
		{ *poClosest = poIntersect2; }
	}

	return cTotalIntersects;
}

/* internal uint */
/* ClosestPtIntersectingShape(v2 *Points, shape *Shapes, uint iLastShape, v2 P, shape TestShape, v2 *poClosest) */
/* { */
/* 	*poClosest = TestShape.P[0]; // either focus or line start */

/* 	v2 poIntersect1 = ZeroV2, poIntersect2 = ZeroV2; */
/* 	uint cTotalIntersects = 0; */
/* 	// TODO (opt): extract to function with function params */
/* 	for(uint iShape = 1; iShape <= iLastShape; ++iShape) */
/* 	{ */
/* 		shape Shape = Shapes[iShape]; */
/* 		uint cIntersects = 0; */
/* 		switch(Shape.Kind) */
/* 		{ // find the intersects with a line going through poExtend */
/* 			case SHAPE_Circle: */
/* 				{ */
/* 					v2 poFocus = Points[Shape.Circle.ipoFocus]; */
/* 					f32 Radius = Dist(poFocus, Points[Shape.Circle.ipoRadius]); */
/* 					if(TestShape.Kind == SHAPE_Line) */
/* 					{ cIntersects = IntersectLineCircle(TestStart, TestDir, poFocus, Radius, */
/* 									&poIntersect1, &poIntersect2); } */
/* 					else if(TestShape.Kind == SHAPE_Circle) */
/* 					{ cIntersects = IntersectCircles(TestFocus, TestRadius, poFocus, Radius, */
/* 									&poIntersect1, &poIntersect2); } */
/* 					else { Assert(0); } */
/* 				} break; */

/* 			case SHAPE_Arc: */
/* 				{ */
/* 					v2 poFocus = Points[Shape.Arc.ipoFocus]; */
/* 					v2 poStart = Points[Shape.Arc.ipoStart]; */
/* 					v2 poEnd   = Points[Shape.Arc.ipoEnd]; */
/* 					f32 Radius = Dist(poFocus, poStart); */
/* 					if(TestShape.Kind == SHAPE_Line) */
/* 					{ cIntersects = IntersectLineArc(TestStart, TestDir, poFocus, Radius, poStart, poEnd, */
/* 								&poIntersect1, &poIntersect2); } */
/* 					else if(TestShape.Kind == SHAPE_Circle) */
/* 					{ cIntersects = IntersectCircleArc(TestFocus, TestRadius, poFocus, Radius, poStart, poEnd, */
/* 								&poIntersect1, &poIntersect2); } */
/* 					else { Assert(0); } */
/* 				} break; */

/* 			case SHAPE_Segment: */
/* 				{ */
/* 					v2 po = Points[Shape.Line.P1]; */
/* 					v2 Dir = V2Sub(Points[Shape.Line.P2], po); */
/* 					if(TestShape.Kind == SHAPE_Line) */
/* 					{ cIntersects = IntersectLineSegment(TestStart, TestDir, po, Dir, &poIntersect1); } */
/* 					else if(TestShape.Kind == SHAPE_Circle) */
/* 					{ cIntersects = IntersectCircles(TestFocus, TestRadius, poFocus, Radius, */
/* 									&poIntersect1, &poIntersect2); } */
/* 					else { Assert(0); } */
/* 				} break; */

/* 			default: { /1* do nothing *1/ } */
/* 		} */

/* 		cTotalIntersects += cIntersects; */

/* 		// update closest candidate */
/* 		if(cIntersects	  && DistSq(P, poIntersect1) < DistSq(P, *poClosest)) */
/* 		{ *poClosest = poIntersect1; } */
/* 		if(cIntersects == 2 && DistSq(P, poIntersect2) < DistSq(P, *poClosest)) */
/* 		{ *poClosest = poIntersect2; } */
/* 	} */

/* 	return cTotalIntersects; */
/* } */

internal inline v2
ChooseCirclePoint(state *State, v2 MouseP, v2 SnapMouseP, b32 ShapeLock)
{ // find point on shape closest to mouse along circumference
	v2 poFocus = POINTS(State->ipoSelect);
	f32 Radius = State->Length;
	v2 Result;
	if(ShapeLock)
	{
		uint cIntersects = ClosestPtIntersectingCircle(State->Points, State->Shapes, State->iLastShape, MouseP,
				poFocus, Radius, &Result);
		if(cIntersects == 0) { Result = ClosestPtOnCircle(MouseP, poFocus, Radius); }
		DebugReplace("cIntersects: %u\n", cIntersects);
	}
	else
	{
		Result = ClosestPtOnCircle(SnapMouseP, poFocus, Radius);
	}
	return Result;
}

UPDATE_AND_RENDER(UpdateAndRender)
{
	BEGIN_TIMED_BLOCK;
	OPEN_LOG("geometer_log",".txt");
	state *State = (state *)Memory->PermanentStorage;
	memory_arena Arena;
	v2 Origin;
	Origin.X = 0;
	Origin.Y = 0;
	v2 ScreenSize;
	ScreenSize.X = (f32)ScreenBuffer->Width;
	ScreenSize.Y = (f32)ScreenBuffer->Height;
	v2 ScreenCentre = V2Mult(0.5f, ScreenSize);
	platform_request File = {0};

	// REMOVE
	static int testcharindex = 0;

	memory_arena TempArena;
	InitArena(&TempArena, (u8 *)Memory->TransientStorage, Memory->TransientStorageSize);

	if(!Memory->IsInitialized)
	{
		InitArena(&Arena, (u8 *)Memory->PermanentStorage + sizeof(state), Memory->PermanentStorageSize - sizeof(state));

		// NOTE: need initial save state to undo to
		SaveUndoState(State);
		State->iSaveDraw = State->iCurrentDraw;

		Memory->IsInitialized = 1;
	}
	{ // DEBUG
		Debug.Buffer = ScreenBuffer;
		Debug.Print = DrawString;
		Debug.Font = State->DefaultFont;
		Debug.FontSize = 11.f;
		Debug.P = V2(2.f, ScreenSize.Y-(2.f*Debug.FontSize));
	}
	Assert(State->OverflowTest == 0);

	if(V2InvalidDir(BASIS->XAxis))
	{
		LOG("Invalid basis");
		BASIS->XAxis.X = 1.f;
		BASIS->XAxis.Y = 0.f;
		// TODO (UI): should I reset zoom here as well?
	}

	// Clear BG
	BEGIN_NAMED_TIMED_BLOCK(ClearBG);
	memset(ScreenBuffer->Memory, 0xFF, ScreenBuffer->Width * ScreenBuffer->Height * BytesPerPixel);
	END_NAMED_TIMED_BLOCK(ClearBG);
	/* DrawRectangleFilled(ScreenBuffer, Origin, ScreenSize, WHITE); */

	if(State->tBasis < 1.f)  State->tBasis += State->dt*4.f;
	else                     State->tBasis = 1.f;

	keyboard_state Keyboard;
	mouse_state Mouse;
	mouse_state pMouse;
	v2 SnapMouseP, poClosest;
	v2 poAtDist = ZeroV2;
	v2 poOnLine = ZeroV2;
	uint ipoSnap;
	uint ipoClosest = 0;
	{ LOG("INPUT");
		Keyboard = Input.New->Keyboard;
		Mouse  = Input.New->Mouse;
		pMouse = Input.Old->Mouse;

		// Pan with arrow keys
		b32 Down  = C_PanDown.EndedDown;
		b32 Up    = C_PanUp.EndedDown;
		b32 Left  = C_PanLeft.EndedDown;
		b32 Right = C_PanRight.EndedDown;
		f32 PanSpeed = 5.f;
		if(Down != Up)
		{
			if(Down)      { BASIS->Offset = V2Add(BASIS->Offset, V2Mult(-PanSpeed, Perp(BASIS->XAxis))); }
			else/*Up*/    { BASIS->Offset = V2Add(BASIS->Offset, V2Mult( PanSpeed, Perp(BASIS->XAxis))); }
		}

		if(Left != Right)
		{
			if(Left)      { BASIS->Offset = V2Add(BASIS->Offset, V2Mult(-PanSpeed,      BASIS->XAxis )); }
			else/*Right*/ { BASIS->Offset = V2Add(BASIS->Offset, V2Mult( PanSpeed,      BASIS->XAxis )); }
		}

		// Zoom with PgUp/PgDn
		b32 ZoomIn  = C_ZoomIn.EndedDown;
		b32 ZoomOut = C_ZoomOut.EndedDown;
		// TODO: Make these constants?
		if(ZoomIn != ZoomOut)
		{
			f32 ZoomFactor = 0.9f;
			f32 invZoomFactor = 1.f/ZoomFactor;
			if(ZoomIn)        BASIS->Zoom *=    ZoomFactor;
			else if(ZoomOut)  BASIS->Zoom *= invZoomFactor;
		}

		if(C_Zoom)
		{ // scroll to change zoom level
			f32 ScrollFactor = 0.8f;
			f32 invScrollFactor = 1.f/ScrollFactor;
			v2 dMouseP = V2Sub(Mouse.P, ScreenCentre);
			if(C_Zoom < 0)
			{ // Zooming out
				C_Zoom = -C_Zoom;
				ScrollFactor = invScrollFactor;
#define ROTATED_OFFSET() V2RotateToAxis(BASIS->XAxis, V2Mult((1.f-ScrollFactor) * BASIS->Zoom, dMouseP))
				// NOTE: keep canvas under pointer in same screen location
				BASIS->Offset = V2Add(BASIS->Offset, ROTATED_OFFSET());
			}
			else
			{ // Zooming in
				// NOTE: keep canvas under pointer in same screen location
				BASIS->Offset = V2Add(BASIS->Offset, ROTATED_OFFSET());
#undef ROTATED_OFFSET
			}
			// NOTE: wheel delta is in multiples of 120
			for(int i = 0; i < C_Zoom/120; ++i)
			{
				BASIS->Zoom *= ScrollFactor;
			}
		}

		// SNAPPING
		if(State->InputMode == MODE_ExtendSeg || State->InputMode == MODE_ExtendLinePt)
		{ // Temp point added so that lines can be extended to the given length
			v2 poAtLength = ClosestPtOnCircle(State->poSaved, POINTS(State->ipoSelect), State->Length);
			State->ipoLength = AddPoint(State, poAtLength, POINT_Arc, 0);
		}

		f32 ClosestDistSq;
		v2 CanvasMouseP = V2ScreenToCanvas(*BASIS, Mouse.P, ScreenCentre);
		SnapMouseP = CanvasMouseP;
		poClosest = CanvasMouseP;
		ipoClosest = ClosestPointIndex(State, CanvasMouseP, &ClosestDistSq);
		ipoSnap = 0;
		if(ipoClosest)
		{
			poClosest = POINTS(ipoClosest);
#define POINT_SNAP_DIST 5000.f
			// NOTE: BASIS->Zoom needs to be squared to match ClosestDistSq
			if(ClosestDistSq/(BASIS->Zoom * BASIS->Zoom) < POINT_SNAP_DIST)
			{ // closest point within range
				if( ! C_NoSnap.EndedDown)
				{ // point snapping still on
					SnapMouseP = poClosest;
					ipoSnap = ipoClosest;
				}
			}

			else
			{ // closest point outside range
				ipoClosest = 0;
			}
		}
		if(C_ShapeLock.EndedDown)
		{
			for(uint iShape = 1; iShape <= State->iLastShape; ++iShape)
			{
				v2 TestP = ZeroV2;
				shape Shape = State->Shapes[iShape];
				switch(Shape.Kind)
				{
					case SHAPE_Circle:
					{
						circle Circle = Shape.Circle;
						v2 poFocus = POINTS(Circle.ipoFocus);
						f32 Radius = Dist(poFocus, POINTS(Circle.ipoRadius));
						TestP = ClosestPtOnCircle(CanvasMouseP, poFocus, Radius);
					} break;

					case SHAPE_Arc:
					{
						arc Arc = Shape.Arc;
						v2 poFocus = POINTS(Arc.ipoFocus);
						v2 poStart = POINTS(Arc.ipoStart);
						v2 poEnd = POINTS(Arc.ipoEnd);
						TestP = ClosestPtOnArc(CanvasMouseP, poFocus, poStart, poEnd);
					} break;

					case SHAPE_Segment:
					{
						line Seg = Shape.Line;
						v2 po1 = POINTS(Seg.P1);
						v2 Dir = V2Sub(POINTS(Seg.P2), po1);
						TestP = ClosestPtOnSegment(CanvasMouseP, po1, Dir);
					} break;

					default:
					{
						// do nothing
					}
				}
				if(iShape == 1 || DistSq(TestP, CanvasMouseP) < DistSq(CanvasMouseP, SnapMouseP))
				{ SnapMouseP = TestP; }
			}
		}

		// TODO: fix the halftransitioncount - when using released(button), it fires twice per release
#define DEBUGClick(button) (IsInScreenBounds(ScreenBuffer, Mouse.P) && DEBUGPress(button))
#define DEBUGRelease(button) (Input.Old->button.EndedDown && !Input.New->button.EndedDown)
#define DEBUGPress(button)   (!Input.Old->button.EndedDown && Input.New->button.EndedDown)

		if(DEBUGPress(C_DebugInfo)) // toggle debug info
		{ State->ShowDebugInfo = !State->ShowDebugInfo; }

		if(DEBUGPress(C_CanvasHome))
		{ // reset canvas position
			SaveUndoState(State);
			BASIS->Offset = ZeroV2;
			// TODO: do I want zoom to reset?
			// BASIS->Zoom   = 1.f;
			State->tBasis = 0.f;
		}

		if(DEBUGPress(C_PrevLength))
		{ // swap to previous length
			f32 tLength = State->Length;
			State->Length = State->pLength;
			State->pLength = tLength;
		}

#define KEYBOARD_LENGTH_FACTOR(keynum, factor) \
		if(DEBUGPress(Keyboard.n ## keynum)) \
		{ \
			State->pLength = State->Length; \
			/* TODO (UI): choose modifier key */ \
			if(Keyboard.Shift.EndedDown) { State->Length *= factor; } \
			else                         { State->Length /= factor; } \
		}
		// TODO (UI): something more interesting with 1...
		// return to previous store? ...?
		/* KEYBOARD_LENGTH_FACTOR(1, 1.f) */
		KEYBOARD_LENGTH_FACTOR(2, 2.f)
		KEYBOARD_LENGTH_FACTOR(3, 3.f)
		KEYBOARD_LENGTH_FACTOR(4, 4.f)
		KEYBOARD_LENGTH_FACTOR(5, 5.f)
		KEYBOARD_LENGTH_FACTOR(6, 6.f)
		KEYBOARD_LENGTH_FACTOR(7, 7.f)
		KEYBOARD_LENGTH_FACTOR(8, 8.f)
		KEYBOARD_LENGTH_FACTOR(9, 9.f)
		KEYBOARD_LENGTH_FACTOR(0, 10.f)
#undef KEYBOARD_LENGTH_FACTOR

#if 1
#define KEYBOARD_LENGTH_STORE(key, index) \
		if(DEBUGPress(Keyboard.key)) \
		{ \
			int Index; \
			/* TODO (feature): what if caps lock is on?*/ \
			if(Keyboard.Shift.EndedDown) { Index = index; } \
			else /*straight after caps*/ { Index = index + 26; } \
			testcharindex = Index; \
			Assert(Index >= 0 && Index < 52) \
			if(Keyboard.Alt.EndedDown) { State->LengthStores[Index] = State->Length; } \
			else if(State->LengthStores[Index] > 0.f && \
					State->LengthStores[Index] != State->Length) \
			{ \
				State->pLength = State->Length; \
				State->Length = State->LengthStores[Index]; \
			} \
		}
		KEYBOARD_LENGTH_STORE(A, 0)
		KEYBOARD_LENGTH_STORE(B, 1)
		KEYBOARD_LENGTH_STORE(C, 2)
		KEYBOARD_LENGTH_STORE(D, 3)
		KEYBOARD_LENGTH_STORE(E, 4)
		KEYBOARD_LENGTH_STORE(F, 5)
		KEYBOARD_LENGTH_STORE(G, 6)
		KEYBOARD_LENGTH_STORE(H, 7)
		KEYBOARD_LENGTH_STORE(I, 8)
		KEYBOARD_LENGTH_STORE(J, 9)
		KEYBOARD_LENGTH_STORE(K, 10)
		KEYBOARD_LENGTH_STORE(L, 11)
		KEYBOARD_LENGTH_STORE(M, 12)
		KEYBOARD_LENGTH_STORE(N, 13)
		KEYBOARD_LENGTH_STORE(O, 14)
		KEYBOARD_LENGTH_STORE(P, 15)
		KEYBOARD_LENGTH_STORE(Q, 16)
		KEYBOARD_LENGTH_STORE(R, 17)
		KEYBOARD_LENGTH_STORE(S, 18)
		KEYBOARD_LENGTH_STORE(T, 19)
		KEYBOARD_LENGTH_STORE(U, 20)
		KEYBOARD_LENGTH_STORE(V, 21)
		KEYBOARD_LENGTH_STORE(W, 22)
		KEYBOARD_LENGTH_STORE(X, 23)
		KEYBOARD_LENGTH_STORE(Y, 24)
		KEYBOARD_LENGTH_STORE(Z, 25)
#undef KEYBOARD_LENGTH_STORE
#endif

		{ // unwanted?
			/* // TODO: Do I actually want to be able to drag points? */
			/* else if(State->ipoDrag) */
			/* { */
			/*	 if(DEBUGClick(C_Drag)) */
			/*	 { */
			/*		 SaveUndoState(State); */
			/*		 // Set point to mouse location and recompute intersections */
			/*		 State->ipoDrag = 0; */
			/*		 State->ipoSelect = 0; */
			/*		 // TODO: this breaks lines attached to intersections... */
			/*		 RemovePointsOfType(State, POINT_Intersection); */
			/*		 for(uint i = 1; i <= State->iLastLinePoint; i+=2) */
			/*		 { */
			/*			 // TODO: this is wasteful */
			/*			 AddLineIntersections(State, State->LinePoints[i], i, 0); */
			/*		 } */
			/*	 } */

			/*	 else if(DEBUGClick(RMB) || Keyboard.Esc.EndedDown) */
			/*	 { */
			/*		 // Cancel dragging, point returns to saved location */
			/*		 State->Points[State->ipoDrag] = State->poSaved; */
			/*		 State->ipoDrag = 0; */
			/*		 State->ipoSelect = 0; */
			/*	 } */

			/*	 else */
			/*	 { */
			/*		 State->Points[State->ipoDrag] = Mouse.P; // Update dragged point to mouse location */
			/*	 } */
			/*	 // Snapping is off while dragging; TODO: maybe change this when points can be combined */
			/*	 ipoSnap = 0; */
			/* } */
		}

		// TODO (UI): if panning, ignore input but still do preview
		// TODO: fix needed for if started and space released part way?
		if((C_PanMod.EndedDown && Mouse.LMB.EndedDown) || C_Pan.EndedDown)
		{ // pan canvas with mouse
			BASIS->Offset = V2Add(BASIS->Offset,
				V2Sub(V2ScreenToCanvas(*BASIS, pMouse.P, ScreenCentre),
					  V2ScreenToCanvas(*BASIS,  Mouse.P, ScreenCentre)));
			// NOTE: prevents later triggers of clicks, may not be required if input scheme changes.
			Input.New->Mouse.LMB.EndedDown = 0;
			File.Pan = 1;
		}

		else if(DEBUGPress(C_Cancel) && State->InputMode != MODE_Normal)
		{ // cancel selection, point returns to saved location
			POINTSTATUS(State->ipoSelect) = State->SavedStatus[0];
			POINTSTATUS(State->ipoArcStart) = State->SavedStatus[1];
			OffsetDraw(State, -1);
			State->ipoSelect = 0;
			State->ipoArcStart = 0;
			State->InputMode = MODE_Normal;
		}

		else if(DEBUGPress(Keyboard.F1))
		{
			State->ShowHelpInfo = !State->ShowHelpInfo;
		}

		else
		{
			uint InputButton = LMB;

			// TODO (opt): jump straight to the right mode.
			switch(State->InputMode)
			{
				case MODE_Normal:
				{
					if(Keyboard.Ctrl.EndedDown)
					{
						if(DEBUGPress(Keyboard.S))
						{ // SAVE (AS)
							File.Action = FILE_Save;
							if(Keyboard.Shift.EndedDown) { File.NewWindow = 1; }
						}
						else if(DEBUGPress(Keyboard.O))
						{ // OPEN (AS)
							// TODO IMPORTANT: seems to trap the 'o' down,
							// so it needs to be pressed again before it's registered properly
							File.Action = FILE_Open;
							if(Keyboard.Shift.EndedDown) { File.NewWindow = 1; }
						}
						else if(DEBUGPress(Keyboard.N))
						{ // NEW (AS)
							File.Action = FILE_New;
							if(Keyboard.Shift.EndedDown) { File.NewWindow = 1; }
						}
						else if(DEBUGPress(Keyboard.E))
						{
							if(Keyboard.Shift.EndedDown) { File.Action = FILE_ExportSVG; }
							// else { File.Action = FILE_ExportPNG; }
						}
					}

					if((Keyboard.Ctrl.EndedDown && DEBUGPress(Keyboard.Z) && !Keyboard.Shift.EndedDown) &&
						// NOTE: making sure that there is a state available to undo into
						((State->cDraws >= NUM_UNDO_STATES && State->iLastDraw != iDrawOffset(State, -1)) ||
							(State->cDraws <  NUM_UNDO_STATES && State->iCurrentDraw > 1)))
					{ OffsetDraw(State, -1); } // UNDO
					if(((Keyboard.Ctrl.EndedDown && DEBUGPress(Keyboard.Y)) ||
						(Keyboard.Ctrl.EndedDown && Keyboard.Shift.EndedDown && DEBUGPress(Keyboard.Z))) &&
						(State->iCurrentDraw < State->iLastDraw))
					{ OffsetDraw(State, 1); } // REDO

					if(C_BasisMod.EndedDown && DEBUGClick(C_BasisSet))
					{
						State->poSaved = SnapMouseP;
						State->InputMode = MODE_SetBasis;
					}

					else if(DEBUGClick(C_Arc))
					{ // start drawing arc
						// NOTE: Starting a shape, save the first point
						SaveUndoState(State);
						State->ipoSelect = AddPoint(State, SnapMouseP, POINT_Extant, &State->SavedStatus[0]);
						State->InputMode = MODE_SetLength;
					}

					else if(DEBUGClick(C_Line))
					{ // start drawing line
						// NOTE: Starting a shape, save the first point
						SaveUndoState(State);
						State->ipoSelect = AddPoint(State, SnapMouseP, POINT_Extant, &State->SavedStatus[0]);
						State->InputMode = MODE_QuickSeg;
					}

					else if(DEBUGPress(C_Reset))
					{ // reset canvas
						SaveUndoState(State);
						Reset(State);
						DebugClear();
					}
					{// unwanted?

						// TODO: could skip check and just write to invalid point..?
						/* else if(ipoSnap) */
						/* { // point snapped to */
						/*	 if(DEBUGPress(C_Delete)) */
						/*	 { */
						/*		 // TODO: deleting points */
						/*		 SaveUndoState(State); */
						/*		 InvalidatePoint(State, ipoSnap); */
						/*	 } */
						/* } */

						/* if(DEBUGClick(MMB)) */
						/* { */
						// MOVE POINT
						/* SaveUndoState(State); */
						/* State->SavedPoint = State->Points[ipoSnap]; */
						/* State->ipoSelect = ipoSnap; */
						/* State->ipoDrag = ipoSnap; */
						/* } */ 
					}
				} break;


				case MODE_SetBasis:
				{ // rotate canvas basis
					// TODO: if RMB held, set zoom as well - box aligned to new axis showing where screen will end up
					// TODO (fix): don't move if same axis is set again (starting from a fixed pBasis?)
					if(!C_BasisSet.EndedDown)
					{ // set basis on release
						// SetAnimatedBasis
						{
							State->tBasis = 0.f;
							pBASIS = *BASIS;
							BASIS->XAxis = Norm(V2Sub(SnapMouseP, State->poSaved));
						}

						State->ipoSelect = 0;
						State->ipoArcStart = 0;
						State->InputMode = MODE_Normal;
					}
				} break;


				case MODE_SetLength:
				{
					if(!C_Length.EndedDown)
					{ // set length on release
						// TODO (optimize): use DistSq
						f32 Length = Dist(POINTS(State->ipoSelect), SnapMouseP);
						if(Length > POINT_EPSILON)
						{
							State->pLength = State->Length;
							State->Length = Length;
						}
						// TODO (UI): do I want it automatically continuing to draw here?
						State->InputMode = MODE_DrawArc;
						goto case_mode_drawarc;
					}
				} break;


				case MODE_DrawArc:
				{
case_mode_drawarc:
					// TODO (opt): there is a 1 frame lag even if pressed and released within 1...
					Assert(State->ipoSelect);
					poAtDist = ChooseCirclePoint(State, CanvasMouseP, SnapMouseP, C_ShapeLock.EndedDown);

					if(DEBUGClick(C_FullShape))
					{ // start drawing arc/circle
						v2 poFocus = POINTS(State->ipoSelect);
						f32 Radius = State->Length;
						if(V2WithinEpsilon(SnapMouseP, poFocus, POINT_EPSILON))
						{
							AddCircle(State, State->ipoSelect, AddPoint(State, V2(poFocus.X + Radius, poFocus.Y), POINT_Radius, 0));
							State->InputMode = MODE_Normal;
						}
						else
						{
							State->ipoArcStart = AddPoint(State, poAtDist, POINT_Arc, 0);
							State->InputMode = MODE_ExtendArc;
						}
					}

					else if(DEBUGClick(C_PointOnly))
					{ // add point intersecting shape 
						SaveUndoState(State);
						// TODO IMPORTANT (fix): don't add when no shape intersected
						AddPoint(State, poAtDist, POINT_Extant, 0);
						State->InputMode = MODE_Normal;
					}
				} break;


				case MODE_ExtendArc:
				{
					Assert(State->ipoSelect);
					Assert(State->ipoArcStart);
					poAtDist = ChooseCirclePoint(State, CanvasMouseP, SnapMouseP, C_ShapeLock.EndedDown);

					// NOTE: Not using button released in case it's missed for some reason
					// also possible this fires at a weird time when focus returns or something...
					if(!C_Arc.EndedDown)
					{ // finish drawing arc/circle
						v2 poFocus = POINTS(State->ipoSelect);
						v2 poExtend = ClosestPtOnCircle(SnapMouseP, poFocus, State->Length);
						if(V2WithinEpsilon(SnapMouseP, poFocus, POINT_EPSILON) ||
						   V2WithinEpsilon(poExtend, POINTS(State->ipoArcStart), POINT_EPSILON))
						{ // Same angle -> full circle
							AddCircle(State, State->ipoSelect, State->ipoArcStart);
						}

						else
						{ // arc
							uint ipoArcEnd = AddPoint(State, poAtDist, POINT_Arc, 0);
							AddArc(State, State->ipoSelect, State->ipoArcStart, ipoArcEnd);
						}
						State->ipoSelect = 0;
						State->ipoArcStart = 0;
						State->InputMode = MODE_Normal;
					}
				} break;


				case MODE_SetPerp:
				{
					v2 poSelect = POINTS(State->ipoSelect);
					if(V2WithinEpsilon(poSelect, SnapMouseP, POINT_EPSILON)) // mouse on seg start
					{ State->PerpDir = ZeroV2; }
					else // set perpendicular
					{ State->PerpDir = Perp(V2Sub(SnapMouseP, poSelect)); }

					if( ! C_FullShape.EndedDown)
					{
						State->InputMode = MODE_DrawSeg;
					}
				} break;


				case MODE_QuickSeg:
				{
					if( ! C_Line.EndedDown) // RMB released
					{ // draw quick segment or move to full segment drawing
						v2 poSelect = POINTS(State->ipoSelect);
						if(V2WithinEpsilon(poSelect, SnapMouseP, POINT_EPSILON))
						{ // mouse not moved, go to full seg drawing
							State->InputMode = MODE_DrawSeg;
						}
						else
						{ // draw quick seg
							AddSegment(State, State->ipoSelect, AddPoint(State, SnapMouseP, POINT_Line, 0));
							State->ipoSelect = 0;
							State->InputMode = MODE_Normal;
						}
					}
				} break;


#define DRAW_PERP_OR_NORMAL \
				if(V2Equals(State->PerpDir, ZeroV2)) /* perpendicular not set */ \
				{ State->poSaved = SnapMouseP; } /* set current mouse as point to extend through */ \
				else /* extend through the perpendicular */ \
				{ State->poSaved = V2Add(poSelect, State->PerpDir); } \
				State->PerpDir = ZeroV2;

				case MODE_DrawSeg:
				{ // start drawing line
					v2 poSelect = POINTS(State->ipoSelect);
					if(DEBUGClick(C_FullShape))
					{
						if(V2WithinEpsilon(poSelect, SnapMouseP, POINT_EPSILON))
						{ // NOTE: don't  want to extend a line with no direction!
							// leave a point and return to normal mode
							State->InputMode = MODE_SetPerp;
						}
						else
						{ // extend segment
							DRAW_PERP_OR_NORMAL
							State->InputMode = MODE_ExtendSeg;
						}
					}

					else if(DEBUGClick(C_PointOnly))
					{ // continue to extending shape if valid
						if(V2WithinEpsilon(poSelect, SnapMouseP, POINT_EPSILON))
						{
							// TODO (ui): cancel point?
							State->ipoSelect = 0;
							State->InputMode = MODE_Normal;
						}
						else // ClickPointOnly
						{ // extend line point
							DRAW_PERP_OR_NORMAL
							State->InputMode = MODE_ExtendLinePt;
						}
					}
				} break;
#undef DRAW_PERP


				// TODO (fix): shape snapping not working
				case MODE_ExtendLinePt: // fallthrough
				InputButton = CB_PointOnly;
				case MODE_ExtendSeg:
				{ // find point on shape closest to mouse along line
					// remove temp point in case length changes for next frame
					// NOTE: currently relies on not being overwritten...
					// may want to move after draw
					POINTSTATUS(State->ipoLength) = POINT_Free;

					// TODO (fix): preview point pulling away from shape
					v2 TestStart = POINTS(State->ipoSelect); 
					v2 poExtend = State->poSaved;
					v2 TestDir = V2Sub(poExtend, TestStart);
					uint cIntersects = 0;
					if(C_ShapeLock.EndedDown)
					{
						cIntersects =
							ClosestPtIntersectingLine(State->Points, State->Shapes, State->iLastShape, CanvasMouseP,
									TestStart, TestDir, &poOnLine);
						if(cIntersects == 0)  { poOnLine = ClosestPtOnLine(CanvasMouseP, TestStart, TestDir); }
					}
					else
					{ poOnLine = ClosestPtOnLine(SnapMouseP, TestStart, TestDir); }

					if( ! Mouse.Buttons[InputButton].EndedDown)
					{ // add point along line (and maybe add segment)
						SaveUndoState(State);
						uint ipoNew = AddPoint(State, poOnLine, POINT_Extant, 0);
						if(State->InputMode == MODE_ExtendSeg)  { AddSegment(State, State->ipoSelect, ipoNew); }
						State->ipoSelect = 0;
						State->InputMode = MODE_Normal;
					}
				} break;

				default:
				{
					// TODO (feature): error handle? reset flags and return to normal mode
					Assert(0);
				}
			}
		}
	}

	{ LOG("RENDER");
		DrawCrosshair(ScreenBuffer, ScreenCentre, 5.f, LIGHT_GREY);

		basis EndBasis = *BASIS;
		/* pBASIS = &pDRAW_STATE.Basis; */
		basis StartBasis = pBASIS;
		f32 tBasis = State->tBasis;
		// TODO: animate on undos
		if(Dot(EndBasis.XAxis, StartBasis.XAxis) < 0)
		{ // Not within 90° either side
			if(tBasis < 0.5f)
			{ // first half of transition
				if(PerpDot(EndBasis.XAxis, StartBasis.XAxis) < 0)
				{ EndBasis.XAxis = Perp(StartBasis.XAxis); }
				else
				{ EndBasis.XAxis = Perp(EndBasis.XAxis); }
				tBasis *= 2.f;
			}

			else
			{ // second half of transition
				if(PerpDot(EndBasis.XAxis, StartBasis.XAxis) < 0)
				{ StartBasis.XAxis = Perp(StartBasis.XAxis); }
				else
				{ StartBasis.XAxis = Perp(EndBasis.XAxis); }
				tBasis = (tBasis-0.5f) * 2.f;
			}
		}
		else
		{ // do small transition with smooth in/out
			// NOTE: fine for one transition, not 2
			tBasis = SmoothStep(tBasis);
		}

#if 1
		basis Basis = BasisLerp(StartBasis, tBasis, EndBasis);
#else
		basis Basis = BasisLerp(pBASIS, State->tBasis, BASIS);
#endif
		v2 SSSnapMouseP = V2CanvasToScreen(Basis, SnapMouseP, ScreenCentre);

#if 0
		DrawCrosshair(ScreenBuffer, ScreenCentre, 20.f, RED);
		DEBUGDrawLine(ScreenBuffer, ScreenCentre,
			V2Add(ScreenCentre, V2Mult(50.f, V2CanvasToScreen(Basis, V2(1.f, 0.f), ScreenCentre))), CYAN);
#endif

		// NOTE: should be unchanged after this point in the frame
		uint iLastShape = State->iLastShape;
		shape *Shapes = State->Shapes;
		v2 *Points = State->Points;

		LOG("\tDRAW SHAPES");
		for(uint iShape = 1; iShape <= iLastShape; ++iShape)
		{
			shape Shape = Shapes[iShape];
			switch(Shape.Kind)
			{
				case SHAPE_Segment:
				{
					v2 poA = V2CanvasToScreen(Basis, Points[Shape.Line.P1], ScreenCentre);
					v2 poB = V2CanvasToScreen(Basis, Points[Shape.Line.P2], ScreenCentre);
					DEBUGDrawLine(ScreenBuffer, poA, poB, BLACK);
					if(State->ShowDebugInfo)
					{ DrawClosestPtOnSegment(ScreenBuffer, Mouse.P, poA, poB); }
				} break;
				
				case SHAPE_Circle:
				{
					v2 poFocus  = V2CanvasToScreen(Basis, Points[Shape.Circle.ipoFocus], ScreenCentre);
					v2 poRadius = V2CanvasToScreen(Basis, Points[Shape.Circle.ipoRadius], ScreenCentre);
					f32 Radius = Dist(poFocus, poRadius);
					CircleLine(ScreenBuffer, poFocus, Radius, BLACK);
					if(State->ShowDebugInfo)
					{ DrawClosestPtOnCircle(ScreenBuffer, Mouse.P, poFocus, Radius); }
				} break;

				case SHAPE_Arc:
				{
					v2 poFocus = V2CanvasToScreen(Basis, Points[Shape.Arc.ipoFocus], ScreenCentre);
					v2 poStart = V2CanvasToScreen(Basis, Points[Shape.Arc.ipoStart], ScreenCentre);
					v2 poEnd   = V2CanvasToScreen(Basis, Points[Shape.Arc.ipoEnd],   ScreenCentre);
					ArcFromPoints(ScreenBuffer, poFocus, poStart, poEnd, BLACK); 
					if(State->ShowDebugInfo)
					{ DrawClosestPtOnArc(ScreenBuffer, Mouse.P, poFocus, poStart, poEnd); }
				} break;
			}
		}

		LOG("\tDRAW POINTS");
		for(uint i = 1; i <= State->iLastPoint; ++i)
		{
			if(POINTSTATUS(i) != POINT_Free)
			{
				v2 po = V2CanvasToScreen(Basis, Points[i], ScreenCentre);
				DrawCircleFill(ScreenBuffer, po, 3.f, LIGHT_GREY);
			}
		}

		v2 poSSClosest = V2CanvasToScreen(Basis, poClosest, ScreenCentre);
		if(State->iLastPoint)  { CircleLine(ScreenBuffer, poSSClosest, 5.f, GREY); }
		if(ipoClosest)  { DrawCircleFill(ScreenBuffer, poSSClosest, 3.f, BLUE); }
		if(ipoSnap)
		{ // draw snapped point
			// NOTE: Overdraws...
			DrawActivePoint(ScreenBuffer, poSSClosest, BLUE);
		}


		LOG("\tDRAW PREVIEW");
		// TODO (UI): animate previews in and out by smoothstepping alpha over a few frames
		// so that they don't pop too harshly when only seen briefly
		v2 poSSSelect = ZeroV2;
		f32 SSLength = State->Length/BASIS->Zoom; 
		v2 poSelect = Points[State->ipoSelect];
		if(State->ipoSelect)  { poSSSelect = V2CanvasToScreen(Basis, poSelect, ScreenCentre); }
		switch(State->InputMode)
		{
			case MODE_Normal:
			{
				// TODO (UI): animate when (un)snapping
				CircleLine(ScreenBuffer, SSSnapMouseP, SSLength, LIGHT_GREY);
				if(C_ShapeLock.EndedDown)
				{ CircleLine(ScreenBuffer, SSSnapMouseP, 3.f, LIGHT_GREY); }
			} break;


			case MODE_SetBasis:
			{
				v2 poSSSaved = V2CanvasToScreen(Basis, State->poSaved, ScreenCentre);
				DEBUGDrawLine(ScreenBuffer, poSSSaved, SSSnapMouseP, RED);
			} break;


			case MODE_SetLength:
			{
				if( ! V2WithinEpsilon(SnapMouseP, poSelect, POINT_EPSILON) && ! C_PanMod.EndedDown)
				{ SSLength = Dist(poSSSelect, SSSnapMouseP); }
				CircleLine(ScreenBuffer, poSSSelect, SSLength, LIGHT_GREY);
				DEBUGDrawLine(ScreenBuffer, poSSSelect, SSSnapMouseP, LIGHT_GREY);
			} break;


			case MODE_DrawArc:
			{
				DrawActivePoint(ScreenBuffer, poSSSelect, RED);
				// preview circle at given length
				CircleLine(ScreenBuffer, poSSSelect, SSLength, BLUE);
				// preview new point dist and position
				v2 poSSAtDist = V2CanvasToScreen(Basis, poAtDist, ScreenCentre);
				DEBUGDrawLine(ScreenBuffer, poSSSelect, poSSAtDist, LIGHT_GREY);
				DrawActivePoint(ScreenBuffer, poSSAtDist, RED);
			} break;


			case MODE_ExtendArc:
			{ // preview drawing arc
				DrawActivePoint(ScreenBuffer, poSSSelect, RED);
				LOG("\tDRAW HALF-FINISHED ARC");
				v2 poStart = Points[State->ipoArcStart];
				v2 poSSStart = V2CanvasToScreen(Basis, poStart, ScreenCentre);
				DEBUGDrawLine(ScreenBuffer, poSSSelect, poSSStart, LIGHT_GREY);
				if(V2WithinEpsilon(poStart, poAtDist, POINT_EPSILON))
				{
					CircleLine(ScreenBuffer, poSSSelect, SSLength, BLACK);
				}
				else
				{
					v2 poSSAtDist = V2CanvasToScreen(Basis, poAtDist, ScreenCentre);
					ArcFromPoints(ScreenBuffer, poSSSelect, poSSStart, poSSAtDist, BLACK);
					DEBUGDrawLine(ScreenBuffer, poSSSelect, poSSAtDist, LIGHT_GREY);
				}
			} break;


			case MODE_SetPerp:
			{
				v2 PerpDir = State->PerpDir;
				if( ! V2Equals(PerpDir, ZeroV2))
				{
					v2 poSSStart = V2CanvasToScreen(Basis, poSelect, ScreenCentre);
					v2 poSSPerp = V2CanvasToScreen(Basis, V2Add(poSelect, PerpDir), ScreenCentre);
					v2 poSSNPerp = V2CanvasToScreen(Basis, V2Add(poSelect, V2Neg(PerpDir)), ScreenCentre);
					DEBUGDrawLine(ScreenBuffer, poSSStart, SSSnapMouseP, LIGHT_GREY);
					DEBUGDrawLine(ScreenBuffer, poSSNPerp, poSSPerp, LIGHT_GREY);
				}
			} break;


			case MODE_QuickSeg:
			case MODE_DrawSeg:
			{
				v2 poSSEnd;
				if(V2Equals(State->PerpDir, ZeroV2))
				{
					poSSEnd = SSSnapMouseP;
				}
				else
				{ // draw perpendicular segment
					v2 poSSDir = V2CanvasToScreen(Basis, V2Add(poSelect, State->PerpDir), ScreenCentre);
					poSSEnd = ExtendSegment(poSSSelect, poSSDir, SSSnapMouseP);
				}
				DrawActivePoint(ScreenBuffer, poSSSelect, RED);
				DEBUGDrawLine(ScreenBuffer, poSSSelect, poSSEnd, BLUE);
			} break;


			case MODE_ExtendSeg:
			case MODE_ExtendLinePt:
			{ // preview extending a line
				v2 poSSDir = V2CanvasToScreen(Basis, State->poSaved, ScreenCentre);
				v2 poSSOnLine = V2CanvasToScreen(Basis, poOnLine, ScreenCentre);
				DrawFullScreenLine(ScreenBuffer, poSSSelect, V2Sub(poSSDir, poSSSelect), LIGHT_GREY);
				if(State->InputMode == MODE_ExtendSeg)
				{ DEBUGDrawLine(ScreenBuffer, poSSSelect, poSSOnLine, BLACK); }
				CircleLine(ScreenBuffer, poSSSelect, SSLength, LIGHT_GREY);
				DrawActivePoint(ScreenBuffer, poSSOnLine, RED);
			} break;
		}


		if(!V2Equals(gDebugV2, ZeroV2))
		{ // draw debug vector
			DEBUGDrawLine(ScreenBuffer, ScreenCentre, V2Add(ScreenCentre, gDebugV2), ORANGE);
		}
		if(!V2Equals(gDebugPoint, ZeroV2))
		{ // draw debug point
			DrawActivePoint(ScreenBuffer, V2CanvasToScreen(*BASIS, gDebugV2, ScreenCentre), ORANGE);
		}
	}

	f32 TextSize = ScreenSize.Y/38;
	if(TextSize > 20.f)  { TextSize = 20.f; }

	if(State->ShowHelpInfo)
	{ LOG("PRINT HELP");
		DrawRectangleFilled(ScreenBuffer, Origin, ScreenSize, PreMultiplyColour(WHITE, 0.8f));
		char LeftHelpBuffer[] =
			"Drawing\n"
			"=======\n"
			"Arcs (compass)\n"
			"--------------\n"
			"LMB-drag - set length/radius\n"
			"LMB      - add point, start drawing arcs\n"
			" -> LMB      - circle\n"
			" -> LMB-onPt - circle\n"
			" -> LMB-drag - arc\n"
			" -> RMB      - point at distance/radius\n"
			" -> RMB-onPt - leave one point\n"

			"\n"
			"Segments/lines (straight-edge)\n"
			"------------------------------\n"
			"RMB-drag - quick draw segment\n"
			"RMB      - add point, start drawing lines\n"
			" -> LMB      - line\n"
			" -> LMB-drag - extend line\n"
			"   -> fromPt - set perpendicular\n"
			" -> RMB      - another point\n"
			" -> RMB-onPt - leave first point\n" // TODO (UI): change to cancel?
			" -> RMB-drag - point along line\n"

			"\n"
			"Modifiers\n"
			"=========\n"
			"Esc      - cancel current shape\n"
			"Ctrl     - snap to shape\n"
			"Shift    - no snapping\n"
			"Alt      - general modifier (number store...)\n"
			"Space    - canvas modifier (pan, basis)";

		char RightHelpBuffer[] =
			"Canvas/view manipulation\n"
			"========================\n"
			"MMB-drag       - pan viewport around canvas\n"
			"Space+LMB-drag - pan viewport around canvas\n"
			"Space+RMB-drag - set horizontal of viewport (rotate)\n"
			"Scroll         - zoom to cursor\n"
			"PgUp/PgDn      - zoom to centre\n"
			"Home           - return to centre\n"
			"Backspace      - reset canvas drawing\n"
			"Alt+Enter      - fullscreen\n"

			"\n"
			"Length/radius manipulation\n"
			"==========================\n"
			"2-0      - divide length by 2-10\n"
			"Alt+2-0  - multiply length by 2-10\n"
			"a-z,A-Z  - get stored length/radius\n"
			"Alt+a-Z  - set stored length/radius\n"
			"Tab      - swap to previously used length\n"

			"\n"
			"File manipulation\n"
			"=================\n"
			"Ctrl+Z    - undo\n"
			"Ctrl+Y    - redo\n"
			"Ctrl+Sh+Z - redo\n"
			"Ctrl+S    - save file\n"
			"Ctrl+Sh+S - save file as...\n"
			"Ctrl+O    - open file\n"
			"Ctrl+Sh+O - open file in new window\n"
			"Ctrl+N    - new file\n"
			"Ctrl+Sh+N - new file in new window"
			;

		DrawString(ScreenBuffer, &State->DefaultFont, LeftHelpBuffer,  TextSize, 10.f, ScreenSize.Y-2.f*TextSize, 0, BLACK);
		DrawString(ScreenBuffer, &State->DefaultFont, RightHelpBuffer, TextSize, ScreenSize.X - 32.f*TextSize, ScreenSize.Y-2.f*TextSize, 0, BLACK);
	}

	if(State->ShowDebugInfo)
	{ LOG("PRINT DEBUG");
		DebugPrint();
		/* DrawSuperSlowCircleLine(ScreenBuffer, ScreenCentre, 50.f, RED); */

		CycleCountersInfo(ScreenBuffer, &State->DefaultFont);

		// TODO: Highlight status for currently selected/hovered points

		char Message[512];
		TextSize = 15.f;
		ssprintf(Message, //"LinePoints: %u, TypeLine: %u, Esc Down: %u"
				"\nFrame time: %.2fms, "
				"Mouse: (%.2f, %.2f), "
				/* "Request: {As: %u, Action: %s, Pan: %u}" */
				/* "Basis: (%.2f, %.2f), " */
				/* "Char: %d (%c), " */
				"Mode: %s, "
				"draw (iC/c/iL/iS): %u/%u/%u/%u, "
				/* "pBasis: (%.2f, %.2f)" */
				/* "Draw Index: %u" */
				/* "Offset: (%.2f, %.2f), " */
				/* "iLastPoint: %u" */
				,
				/* State->cLinePoints, */
				/* NumPointsOfType(State->PointStatus, State->iLastPoint, POINT_Line), */
				/* C_Cancel.EndedDown, */
				State->dt*1000.f,
				Mouse.P.X, Mouse.P.Y,
				/* File.NewWindow, FileActionText[File.Action], File.Pan, */
				/* BASIS->XAxis.X, BASIS->XAxis.Y, */
				/* testcharindex + 65, testcharindex + 65, */
				InputModeText[State->InputMode],
				/* State->pBasis.XAxis.X, State->pBasis.XAxis.Y, */
				State->iCurrentDraw, State->cDraws, State->iLastDraw, State->iSaveDraw
				/* BASIS->Offset.X, BASIS->Offset.Y, */
				/* State->iLastPoint */
				);
		DrawString(ScreenBuffer, &State->DefaultFont, Message, TextSize, 10.f, TextSize, 1, BLACK);

		char ShapeInfo[512];
		ssprintf(ShapeInfo, "L#  P#\n\n");
		for(uint i = 1; i <= State->iLastShape && i <= 32; ++i)
		{
			ssprintf(ShapeInfo, "%s%02u  %04b\n", ShapeInfo, i, SHAPES(i).Kind);
		}
		char PointInfo[512];
		ssprintf(PointInfo, " # DARTFILE\n\n");
		for(uint i = 1; i <= State->iLastPoint && i <= 32; ++i)
		{
			ssprintf(PointInfo, "%s%02u %08b\n", PointInfo, i, POINTSTATUS(i));
		}
		char BasisInfo[512];
		BasisInfo[0] = '\0';
		for(uint i = 0; i <= NUM_UNDO_STATES && i <= 32; ++i)
		{
			ssprintf(BasisInfo, "%s%u) %x (%.2f, %.2f)\n", BasisInfo, i,
					&State->Draw[i].Basis, State->Draw[i].Basis.XAxis.X, State->Draw[i].Basis.XAxis.Y);
		}
		TextSize = 13.f;
		DrawString(ScreenBuffer, &State->DefaultFont, ShapeInfo, TextSize,
				ScreenSize.X - 180.f, ScreenSize.Y - 30.f, 0, BLACK);
		DrawString(ScreenBuffer, &State->DefaultFont, PointInfo, TextSize,
				ScreenSize.X - 120.f, ScreenSize.Y - 30.f, 0, BLACK);
		DrawString(ScreenBuffer, &State->DefaultFont, BasisInfo, TextSize,
				ScreenSize.X - 420.f, ScreenSize.Y - 30.f, 0, BLACK);
	}

	CLOSE_LOG();
	END_TIMED_BLOCK;
	return File;
}

global_variable char DebugTextBuffer[Megabytes(8)];

DECLARE_DEBUG_RECORDS;
DECLARE_DEBUG_FUNCTION
{
	DebugTextBuffer[0] = '\0';
	/* debug_state *DebugState = Memory->DebugStorage; */
	int Offset = 0;
	if(1)//DebugState)
	{
		/* u32 HitI = 0; */
		for(u32 i = 0;
			/* i < cCounters; */
			/* i < ArrayCount(DEBUG_RECORDS); */
			i < DEBUG_RECORDS_ENUM;
			++i)
		{
			debug_record *Counter = DEBUG_RECORD(i);

			u64 HitCount_CycleCount = AtomicExchangeU64(&Counter->HitCount_CycleCount, 0);
			u32 HitCount = (u32)(HitCount_CycleCount >> 32);
			u32 CycleCount = (u32)(HitCount_CycleCount & 0xFFFFFFFF);

			if(HitCount)
			{
				Offset +=
					ssnprintf(DebugTextBuffer, Megabytes(8)-1,
								   /* AltFormat ? AltFormat :*/ "%s%28s%8s(%4d): %'12ucy %'8uh %'10ucy/h\n", 
								   DebugTextBuffer,
								   Counter->FunctionName,
								   Counter->Name,
								   Counter->LineNumber,
								   CycleCount,
								   HitCount,
								   CycleCount / HitCount);
				/* TextBuffer[Offset] = '\n'; */
				/* ++HitI; */
			}
		}
		f32 TextHeight = 13.f;
		DrawString(Buffer, Font, DebugTextBuffer, TextHeight, 0, 2.f*(f32)Buffer->Height/3.f /*- ((HitI+2)*TextHeight)*/, 0, GREY);
	}
}
