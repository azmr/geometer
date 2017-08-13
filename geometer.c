#define _CRT_SECURE_NO_WARNINGS
#include "geometer.h"
#include <fonts.c>
 
// UI:
// ===
//
// Click on point to select it OR make new point (which is then selected)
// L-click on another point to draw to it, or L-click and drag to set direction at point, then release to set drag length
// OR R-click to draw circle at current diameter, or R-click and drag to set arc beginning and release at arc end
//
// Esc to cancel point
// Enter to leave lone point
// Text to add comment/label
//
// shift unlocks diameter, caps toggles it
// set diameter with R-clicks when no point selected
//
// M-click/Space+L-click + drag -> move canvas
// Scroll -> move up/down or zoom?

// TODO:
// =====
//
// - Find (and colour) lines intersecting at a given point
// - Bases and canvas movement
// - Arcs (not just full circles)
// - Unlimited undo states
//  	- save to disk for lots of them? (temporary/pich up undos on load?)
//  	- undo history (show overall/with layers)
//  	- undo by absolute and layer order
// - Change storage of intersections, so they don't all need to be recomputed on changes
// - Spatially partition(?) shapes
// - Set lengths on other lines (modulus?)
//  	- autoset to size just made for easy repeats
// - Togglable layers (should points be separate from layers, but have status set per layer?)
// - For fast movements, make sweep effect, rather than ugly multiple line effect 
// - Deal with perfect overlaps that aren't identical (i.e. one line/arc is longer)
// - One end of line/arc constrained on another shape
// - Consider making everything one long list of shapes (as a union)
// - Animate between offsets on undo/redo so that you can keep track of where you are
// - Resizable windo (maintain centre vs maintain absolute position)

#define DRAW_STATE State->Draw[State->CurrentDrawState]
#define pDRAW_STATE State->Draw[State->CurrentDrawState-1]

internal inline void
DrawClosestPtOnSegment(image_buffer *ScreenBuffer, v2 po, v2 lipoA, v2 lipoB)
{
	BEGIN_TIMED_BLOCK;
	v2 po1 = ClosestPtOnSegment(po, lipoA, V2Sub(lipoB, lipoA));
	DrawCrosshair(ScreenBuffer, po1, 5.f, RED);
	/* DEBUGDrawLine(ScreenBuffer, po1, po, LIGHT_GREY); */
	END_TIMED_BLOCK;
}

internal inline void
DrawPoint(image_buffer *ScreenBuffer, v2 po, b32 Active, colour Col)
{
	BEGIN_TIMED_BLOCK;
	DrawCircleFill(ScreenBuffer, po, 3.f, Col);
	if(Active)
	{
		CircleLine(ScreenBuffer, po, 5.f, Col);
	}
	END_TIMED_BLOCK;
}

// 0 means nothing could be found
internal uint
ClosestPointIndex(draw_state *State, v2 Comp, f32 *ClosestDist)
{
	BEGIN_TIMED_BLOCK;
	// NOTE: all valid points start at 1
	uint Result = 0;
	// TODO: better way of doing this?
	f32 Closest = 0;
	for(uint i = 1; i <= State->iLastPoint; ++i)
	{
		if(State->PointStatus[i] != POINT_Free)
		{
			if(!Result) // i.e. first iteration
			{
				// TODO: maybe put this above loop
				Result = i;
				Closest = DistSq(State->Points[i], Comp);
				continue;
			}
			f32 Test = DistSq(State->Points[i], Comp);
			if(Test < Closest)
			{
				Closest = Test;
				Result = i;
			}
		}
	}
	*ClosestDist = Closest;
	END_TIMED_BLOCK;
	return Result;
}

internal void
Reset(state *State)
{
	BEGIN_TIMED_BLOCK;
	draw_state *Draw = &DRAW_STATE;

	for(uint i = 1; i <= Draw->iLastPoint; ++i)
	{
		Draw->PointStatus[i] = POINT_Free;
	}
	// NOTE: Point index 0 is reserved for null points (not defined in lines)
	Draw->iLastPoint     = 0;
	Draw->iLastLinePoint = 0;
	Draw->iLastCircle    = 0;
	Draw->iLastArc		 = 0;

	Draw->cPoints     = 0;
	Draw->cLinePoints = 0;
	Draw->cCircles    = 0;
	Draw->cArcs		  = 0;

	Draw->Basis.XAxis  = V2(1.f, 0.f);
	Draw->Basis.Offset = ZeroV2;
	Draw->Basis.Zoom   = 1.f;

	State->tBasis      = 1.f;
	State->ipoSelect   = 0;
	State->ipoArcStart = 0;
	END_TIMED_BLOCK;
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
		{
			++Result;
		}
	}
	END_TIMED_BLOCK;
	return Result;
}

internal uint
FindPointAtPos(draw_state *State, v2 po, uint PointStatus)
{
	BEGIN_TIMED_BLOCK;
	uint Result = 0;
	for(uint i = 1; i <= State->iLastPoint; ++i)
	{
		if(V2WithinEpsilon(po, State->Points[i], POINT_EPSILON) && (State->PointStatus[i] & PointStatus))
		{
			Result = i;
			break;
		}
	}
	END_TIMED_BLOCK;
	return Result;
}

/// returns index of point (may be new or existing)
// TODO: less definite name? ProposePoint, ConsiderNewPoint...?
internal uint
AddPoint(draw_state *State, v2 po, uint PointTypes, u8 *PriorStatus)
{
	BEGIN_TIMED_BLOCK;
	uint Result = FindPointAtPos(State, po, ~(uint)POINT_Free);
	if(Result)
	{
		// NOTE: Use existing point, but add any new status (and confirm Extant)
		if(PriorStatus) *PriorStatus = State->PointStatus[Result];
		State->PointStatus[Result] |= PointTypes | POINT_Extant;
	}

	else 
	{
		if(PriorStatus) *PriorStatus = POINT_Free;
		// TODO: extract into function? ExistingFreePoint
		for(uint ipo = 1; ipo <= State->iLastPoint; ++ipo)
		{
			// NOTE: Use existing point if free
#if 0
			// NOTE: this should be disappearing with dynamic allocation
			// if(State->iLastPoint < ArrayCount(State->Points))
#endif
			if(State->PointStatus[ipo] == POINT_Free)
			{
				Result = ipo;
				break;
			}
		}
		// NOTE: Create new point if needed
		if(!Result)
		{
			Result = ++State->iLastPoint;
			++State->cPoints;
		}
		State->Points[Result] = po;
		State->PointStatus[Result] |= PointTypes | POINT_Extant;
	}

	DebugReplace("AddPoint => %u\n", Result);

	END_TIMED_BLOCK;
	return Result;
}

internal inline uint
MatchingPointIndex(uint ipo)
{
	BEGIN_TIMED_BLOCK;
	uint Result;
	if(ipo % 2)
	{
		// NOTE: first of 2 line points
		Result = ipo + 1;
	}
	else
	{
		// NOTE: second of 2 line points
		Result = ipo - 1;
	}
	END_TIMED_BLOCK;
	return Result;
}

internal inline void
AddIntersections(draw_state *State, v2 po1, v2 po2, uint cIntersections)
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

internal inline f32
ArcRadius(v2 *Points, arc Arc)
{
	BEGIN_TIMED_BLOCK;
	f32 Result = Dist(Points[Arc.ipoFocus], Points[Arc.ipoStart]);
	END_TIMED_BLOCK;
	return Result;
}

internal inline uint
IntersectPointsSegmentArc(v2 *Points, v2 po, v2 vDir, arc Arc, v2 *Intersection1, v2 *Intersection2)
{
	BEGIN_TIMED_BLOCK;
	f32 Radius = ArcRadius(Points, Arc);
	v2 vStart = V2Sub(Points[Arc.ipoStart], Points[Arc.ipoFocus]);
	v2 vEnd = V2Sub(Points[Arc.ipoEnd], Points[Arc.ipoFocus]);
	END_TIMED_BLOCK;
	return IntersectSegmentArc(po, vDir, Points[Arc.ipoFocus], Radius, vStart, vEnd, Intersection1, Intersection2);
}

internal inline uint
IntersectPointsCircleArc(v2 *Points, v2 Focus, f32 Radius, arc Arc, v2 *Intersection1, v2 *Intersection2)
{
	BEGIN_TIMED_BLOCK;
	f32 ArcRad = ArcRadius(Points, Arc);
	v2 vStart = V2Sub(Points[Arc.ipoStart], Points[Arc.ipoFocus]);
	v2 vEnd = V2Sub(Points[Arc.ipoEnd], Points[Arc.ipoFocus]);
	END_TIMED_BLOCK;
	return IntersectCircleArc(Focus, Radius, Points[Arc.ipoFocus], ArcRad, vStart, vEnd, Intersection1, Intersection2);
}

internal inline uint
IntersectPointsArcs(v2 *Points, arc Arc1, arc Arc2, v2 *Intersection1, v2 *Intersection2)
{
	BEGIN_TIMED_BLOCK;
	f32 R1 = ArcRadius(Points, Arc1);
	f32 R2 = ArcRadius(Points, Arc2);
	v2 vStart1 = V2Sub(Points[Arc1.ipoStart], Points[Arc1.ipoFocus]);
	v2 vEnd1   = V2Sub(Points[Arc1.ipoEnd],   Points[Arc1.ipoFocus]);
	v2 vStart2 = V2Sub(Points[Arc2.ipoStart], Points[Arc2.ipoFocus]);
	v2 vEnd2   = V2Sub(Points[Arc2.ipoEnd],   Points[Arc2.ipoFocus]);
	END_TIMED_BLOCK;
	return IntersectArcs(Points[Arc1.ipoFocus], R1, vStart1, vEnd1, Points[Arc2.ipoFocus], R2, vStart2, vEnd2, Intersection1, Intersection2);
}

// TODO: add all intersections without recomputing line-circle
/// returns number of intersections
internal uint
AddCircleIntersections(draw_state *State, uint ipoFocus, uint ipoRadius, uint iSkip)
{
	BEGIN_TIMED_BLOCK;
	uint Result = 0, cIntersections = 0;
	circle *Circles = State->Circles;
	arc *Arcs = State->Arcs;
	v2 *Points = State->Points;
	uint *LinePoints = State->LinePoints;
	v2 po1, po2;
	v2 poFocus = Points[ipoFocus];
	f32 Radius = Dist(poFocus, Points[ipoRadius]);

	for(uint iCircle = 1; iCircle <= State->iLastCircle; ++iCircle)
	{
		if(iCircle == iSkip) continue;
		v2 poFocus2 = Points[Circles[iCircle].ipoFocus];
		f32 Radius2 = Dist(poFocus2, Points[Circles[iCircle].ipoRadius]);
		cIntersections = IntersectCircles(poFocus, Radius, poFocus2, Radius2, &po1, &po2);
		Result += cIntersections;
		AddIntersections(State, po1, po2, cIntersections);
	}

	for(uint iArc = 1; iArc <= State->iLastArc; ++iArc)
	{
		cIntersections = IntersectPointsCircleArc(Points, poFocus, Radius, Arcs[iArc], &po1, &po2);
		Result += cIntersections;
		AddIntersections(State, po1, po2, cIntersections);
	}

	for(uint iLine = 1; iLine <= State->iLastLinePoint; iLine+=2)
	{
		po1 = Points[LinePoints[iLine]];
		po2 = Points[LinePoints[iLine+1]];
		cIntersections = IntersectSegmentCircle(po1, V2Sub(po2, po1), poFocus, Radius, &po1, &po2);
		Result += cIntersections;
		AddIntersections(State, po1, po2, cIntersections);
	}

	END_TIMED_BLOCK;
	return Result;
}

internal uint
AddArcIntersections(draw_state *State, arc Arc, uint iSkip)
{
	BEGIN_TIMED_BLOCK;
	uint Result = 0, cIntersections = 0;
	circle *Circles = State->Circles;
	arc *Arcs = State->Arcs;
	v2 *Points = State->Points;
	uint *LinePoints = State->LinePoints;
	v2 po1, po2;

	for(uint iCircle = 1; iCircle <= State->iLastCircle; ++iCircle)
	{
		v2 Focus2 = Points[Circles[iCircle].ipoFocus];
		f32 Radius2 = Dist(Focus2, Points[Circles[iCircle].ipoRadius]);
		cIntersections = IntersectPointsCircleArc(Points, Focus2, Radius2,
				Arc, &po1, &po2);
		Result += cIntersections;
		AddIntersections(State, po1, po2, cIntersections);
	}

	for(uint iArc = 1; iArc <= State->iLastArc; ++iArc)
	{
		if(iArc == iSkip) continue;
		cIntersections = IntersectPointsArcs(Points, Arc, Arcs[iArc], &po1, &po2);
		Result += cIntersections;
		AddIntersections(State, po1, po2, cIntersections);
	}

	for(uint iLine = 1; iLine <= State->iLastLinePoint; iLine+=2)
	{
		po1 = Points[LinePoints[iLine]];
		po2 = Points[LinePoints[iLine+1]];
		cIntersections = IntersectPointsSegmentArc(Points, po1, V2Sub(po2, po1), Arc, &po1, &po2);
		Result += cIntersections;
		AddIntersections(State, po1, po2, cIntersections);
	}

	END_TIMED_BLOCK;
	return Result;
}

/// returns number of intersections
internal uint
AddLineIntersections(draw_state *State, uint ipoA, uint ipoB, uint iSkip)
{
	BEGIN_TIMED_BLOCK;
	uint Result = 0, cIntersections = 0;
	circle *Circles = State->Circles;
	arc *Arcs = State->Arcs;
	v2 *Points = State->Points;
	uint *LinePoints = State->LinePoints;
	v2 po1, po2;

	for(uint iLine = 1; iLine <= State->iLastLinePoint; iLine+=2)
	{
		if(iLine == iSkip) continue;

		// NOTE: TODO? internal line between eg corners of square adds 1 intersection... sometimes?
		v2 Intersect;
		// IMPORTANT TODO: spatially separate, maybe hierarchically
		if(IntersectSegmentsWinding(Points[ipoA], Points[ipoB],
					Points[LinePoints[iLine]], Points[LinePoints[iLine+1]],
					&Intersect))
		{
			AddPoint(State, Intersect, POINT_Intersection, 0);
			++Result;
		}
		// TODO: use segments rather than lines
	}

	for(uint iArc = 1; iArc <= State->iLastArc; ++iArc)
	{
		cIntersections = IntersectPointsSegmentArc(Points, Points[ipoA], V2Sub(Points[ipoB], Points[ipoA]), 
				Arcs[iArc], &po1, &po2);
		Result += cIntersections;
		AddIntersections(State, po1, po2, cIntersections);
	}

	for(uint iCircle = 1; iCircle <= State->iLastCircle; ++iCircle)
	{
		v2 Focus2 = Points[Circles[iCircle].ipoFocus];
		f32 Radius2 = Dist(Focus2, Points[Circles[iCircle].ipoRadius]);
		cIntersections = IntersectSegmentCircle(Points[ipoA], V2Sub(Points[ipoB], Points[ipoA]),
				Focus2, Radius2, &po1, &po2);
		Result += cIntersections;
		AddIntersections(State, po1, po2, cIntersections);
	}

	END_TIMED_BLOCK;
	return Result;
}

/// returns first point of the pair that make up the line
internal uint
AddLine(draw_state *State, uint ipoA, uint ipoB)
{
	BEGIN_TIMED_BLOCK;
	// TODO: could optimise out checking indices if already known (as optional params?)
	uint Result = 0;
	// NOTE: avoids duplicate lines
	b32 ExistingLine = 0;
	for(uint ipoLine = 1; ipoLine <= State->iLastLinePoint; ipoLine += 2)
	{
			// NOTE: no ordering
		if( (State->LinePoints[ipoLine] == ipoA && State->LinePoints[ipoLine + 1] == ipoB) ||
			(State->LinePoints[ipoLine] == ipoB && State->LinePoints[ipoLine + 1] == ipoA))
		{
			ExistingLine = 1;
			break;
		}
	}

	if(!ExistingLine)
	{
		uint ipoEmptyLine = 0;
		for(uint ipoLine = 1; ipoLine <= State->iLastLinePoint; ipoLine += 2)
		{
			// NOTE: only checking first line point of each pair
			if(State->LinePoints[ipoLine] == 0)
			{
				ipoEmptyLine = ipoLine;
				break;
			}
		}

		if(ipoEmptyLine)
		{
			State->LinePoints[ipoEmptyLine]     = ipoA;
			State->LinePoints[ipoEmptyLine + 1] = ipoB;
			AddLineIntersections(State, ipoA, ipoB, ipoEmptyLine);
			Result = ipoEmptyLine;
		}
		else
		{
			State->LinePoints[++State->iLastLinePoint] = ipoA;
			State->LinePoints[++State->iLastLinePoint] = ipoB;
			AddLineIntersections(State, ipoA, ipoB, State->iLastLinePoint-1);
			Result = State->iLastLinePoint-1;
			State->cLinePoints += 2; // TODO: numlines?
		}
	}
	// TODO: make thread-safe?
	State->PointStatus[ipoA] |= POINT_Line;
	State->PointStatus[ipoB] |= POINT_Line;

	DebugReplace("AddLine => %d", Result);
	END_TIMED_BLOCK;
	return Result;
}

internal uint
AddArc(draw_state *State, uint ipoFocus, uint ipoArcStart, uint ipoArcEnd)
{
	BEGIN_TIMED_BLOCK;
	uint Result = 0;
	arc NewArc;
	NewArc.ipoFocus = ipoFocus;
	NewArc.ipoStart = ipoArcStart;
	NewArc.ipoEnd   = ipoArcEnd;
	State->PointStatus[ipoFocus]    |= POINT_Focus;
	State->PointStatus[ipoArcStart] |= POINT_Arc;
	State->PointStatus[ipoArcEnd]   |= POINT_Arc;

	// Ensure it's not a duplicate
	b32 ExistingArc = 0;
	for(uint iArc = 1; iArc <= State->iLastArc; ++iArc)
	{
		arc TestArc = State->Arcs[iArc];
		if(TestArc.ipoFocus == ipoFocus && TestArc.ipoStart == ipoArcStart && TestArc.ipoEnd == ipoArcEnd)
		{
			ExistingArc = 1;
			break;
		}
	}

	if(!ExistingArc)
	{
		Result = ++State->iLastArc;
		State->Arcs[Result] = NewArc;
		++State->cArcs;
		AddArcIntersections(State, NewArc, State->iLastArc);
	}

	END_TIMED_BLOCK;
	return Result;
}

internal uint
AddCircle(draw_state *State, uint ipoFocus, uint ipoRadius)
{
	BEGIN_TIMED_BLOCK;
	uint Result = 0;
	circle NewCircle;
	NewCircle.ipoFocus = ipoFocus;
	NewCircle.ipoRadius = ipoRadius;
	State->PointStatus[ipoFocus] |= POINT_Focus;

	// Ensure it's not a duplicate
	b32 ExistingCircle = 0;
	for(uint iCircle = 1; iCircle <= State->iLastCircle; ++iCircle)
	{
		circle TestCircle = State->Circles[iCircle];
		if(TestCircle.ipoFocus == ipoFocus && TestCircle.ipoRadius == ipoRadius)
		{
			ExistingCircle = 1;
			break;
		}
	}

	if(!ExistingCircle)
	{
		Result = ++State->iLastCircle;
		State->Circles[Result] = NewCircle;
		++State->cCircles;
		AddCircleIntersections(State, NewCircle.ipoFocus, NewCircle.ipoRadius, State->iLastCircle);
	}

	END_TIMED_BLOCK;
	return Result;
}

internal void
InvalidateLinesAtPoint(draw_state *State, uint ipo)
{
	BEGIN_TIMED_BLOCK;
	// TODO: invalidate both points or just one?
	// TODO: do I want this here or in drawing/adding..?
	for(uint i = 1; i <= State->iLastLinePoint; ++i)
	{
		if(State->LinePoints[i] == ipo)
		{
			State->LinePoints[i] = 0;
			State->LinePoints[MatchingPointIndex(i)] = 0;
		}
	}
	END_TIMED_BLOCK;
}

internal void
InvalidateCirclesWithFocusAtPoint(draw_state *State, uint ipo)
{
	BEGIN_TIMED_BLOCK;
	// TODO: invalidate both points or just one?
	// TODO: do I want this here or in drawing/adding..?
	for(uint i = 1; i <= State->iLastCircle; ++i)
	{
		if(State->Circles[i].ipoFocus == ipo)
		{
			State->Circles[i].ipoFocus = 0;
		}
	}
	END_TIMED_BLOCK;
}

internal void
InvalidatePoint(draw_state *State, uint ipo)
{
	BEGIN_TIMED_BLOCK;
	u8 Status = State->PointStatus[ipo];
	if(Status & POINT_Line)
	{
		InvalidateLinesAtPoint(State, ipo);
	}
	if(Status & POINT_Focus)
	{
		InvalidateCirclesWithFocusAtPoint(State, ipo);
	}
	State->PointStatus[ipo] = POINT_Free;
	END_TIMED_BLOCK;
}

/// returns number of points removed
internal uint
RemovePointsOfType(draw_state *State, uint PointType)
{
	BEGIN_TIMED_BLOCK;
	uint Result = 0;
	for(uint i = 1; i <= State->iLastPoint; ++i)
	{
		if(State->PointStatus[i] & PointType)
		{
			InvalidatePoint(State, i);
			++Result;
		}
	}
	END_TIMED_BLOCK;
	return Result;
}

internal inline void
SaveUndoState(state *State)
{
	BEGIN_TIMED_BLOCK;
	draw_state PrevDrawState = DRAW_STATE;

	// NOTE: Only needed for limited undo numbers
	if(State->cDrawStates == NUM_UNDO_STATES-1)
	{
		for(uint i = 0; i < NUM_UNDO_STATES-1; ++i)
		{
			State->Draw[i] = State->Draw[i+1];
		}
	}
	else
	{
		// NOTE: needed regardless of undo numbers
		++State->CurrentDrawState;
	}

	draw_state *CurrentDrawState = &DRAW_STATE;
	*CurrentDrawState = PrevDrawState;

	if(State->cDrawStates < State->CurrentDrawState)  State->cDrawStates = State->CurrentDrawState;
	State->cDrawStates = State->CurrentDrawState;
	END_TIMED_BLOCK;
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
	Result.XAxis  = V2Lerp(Start.XAxis, t, End.XAxis);
	Result.Offset = V2Lerp(Start.Offset, t, End.Offset);
	Result.Zoom   = Lerp(Start.Zoom, t, End.Zoom);
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

// TODO: implement undo/redo
UPDATE_AND_RENDER(UpdateAndRender)
{
	BEGIN_TIMED_BLOCK;
	OPEN_LOG("geometer_log.txt");
	state *State = (state *)Memory->PermanentStorage;
	memory_arena Arena;
	v2 Origin;
	Origin.X = 0;
	Origin.Y = 0;
	v2 ScreenSize;
	ScreenSize.X = (f32)ScreenBuffer->Width;
	ScreenSize.Y = (f32)ScreenBuffer->Height;
	v2 ScreenCentre = V2Mult(0.5f, ScreenSize);
	ScreenCentre;

	memory_arena TempArena;
	InitArena(&TempArena, (u8 *)Memory->TransientStorage, Memory->TransientStorageSize);

	if(!Memory->IsInitialized)
	{
		Reset(State);
		InitArena(&Arena, (u8 *)Memory->PermanentStorage + sizeof(state), Memory->PermanentStorageSize - sizeof(state));
		// NOTE: this allows use of 'previous Basis' without out-of-bounds access. It is also consistent that 0 is not
		// used as an index
		SaveUndoState(State);

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
	Assert(DRAW_STATE.cLinePoints % 2 == 0);

	if(State->FrameCount < 4)
	{
		// NOTE: prevents a point being added if double-click opening is in same region that app opens to
		// TODO: more elegant solution? clear window messages in platform layer?
		Input.New->Mouse.LMB.EndedDown = 0;
	}

	// Clear BG
	BEGIN_NAMED_TIMED_BLOCK(ClearBG);
	memset(ScreenBuffer->Memory, 0xFF, ScreenBuffer->Width * ScreenBuffer->Height * BytesPerPixel);
	END_NAMED_TIMED_BLOCK(ClearBG);
	/* DrawRectangleFilled(ScreenBuffer, Origin, ScreenSize, WHITE); */

	if(State->tBasis < 1.f)  State->tBasis += State->dt*5.f;
	else                     State->tBasis = 1.f;

	keyboard_state Keyboard;
	mouse_state Mouse;
	mouse_state pMouse;
	v2 SnapMouseP, poClosest;
	uint ipoSnap;
	uint ipoClosest = 0;
	{ LOG("INPUT");
		Keyboard = Input.New->Keyboard;
		Mouse  = Input.New->Mouse;
		pMouse = Input.Old->Mouse;

		State->PointSnap = Held(Keyboard.Shift) ? 0 : 1;

		b32 Down = Keyboard.Down.EndedDown;
		b32 Up = Keyboard.Up.EndedDown;
		b32 Left = Keyboard.Left.EndedDown;
		b32 Right = Keyboard.Right.EndedDown;
		f32 PanSpeed = 5.f;
		if(Down != Up)
		{
			if(Down)       DRAW_STATE.Basis.Offset = V2Add(DRAW_STATE.Basis.Offset, V2Mult(-PanSpeed, Perp(DRAW_STATE.Basis.XAxis)));
			else /*Up*/    DRAW_STATE.Basis.Offset = V2Add(DRAW_STATE.Basis.Offset, V2Mult( PanSpeed, Perp(DRAW_STATE.Basis.XAxis)));
		}

		if(Left != Right)
		{
			if(Left)       DRAW_STATE.Basis.Offset = V2Add(DRAW_STATE.Basis.Offset, V2Mult(-PanSpeed,      DRAW_STATE.Basis.XAxis ));
			else /*Right*/ DRAW_STATE.Basis.Offset = V2Add(DRAW_STATE.Basis.Offset, V2Mult( PanSpeed,      DRAW_STATE.Basis.XAxis ));
		}

		b32 ZoomIn  = Keyboard.PgUp.EndedDown;
		b32 ZoomOut = Keyboard.PgDn.EndedDown;
		// TODO: Make these constants?
		if(ZoomIn != ZoomOut)
		{
			f32 ZoomFactor = 0.9f;
			f32 invZoomFactor = 1.f/ZoomFactor;
			if(ZoomIn)        DRAW_STATE.Basis.Zoom *=    ZoomFactor;
			else if(ZoomOut)  DRAW_STATE.Basis.Zoom *= invZoomFactor;
		}

		if(Mouse.ScrollV)
		{
			f32 ScrollFactor = 0.8f;
			f32 invScrollFactor = 1.f/ScrollFactor;
			if(Mouse.ScrollV < 0)
			{
				ScrollFactor = invScrollFactor;
				Mouse.ScrollV = -Mouse.ScrollV;
			}
			DebugReplace("Scroll: %f", ScrollFactor);
			// NOTE: wheel delta is in multiples of 120
			for(int i = 0; i < Mouse.ScrollV/120; ++i)
			{
				DRAW_STATE.Basis.Zoom *= ScrollFactor;
			}
		}

		f32 ClosestDistSq;
		v2 CanvasMouseP = V2ScreenToCanvas(DRAW_STATE.Basis, Mouse.P, ScreenCentre);
		SnapMouseP = CanvasMouseP;
		poClosest = CanvasMouseP;
		ipoClosest = ClosestPointIndex(&DRAW_STATE, CanvasMouseP, &ClosestDistSq);
		ipoSnap = 0;
		if(ipoClosest)
		{
			poClosest = DRAW_STATE.Points[ipoClosest];
			if(ClosestDistSq/DRAW_STATE.Basis.Zoom < 5000.f)
			{
				if(State->PointSnap)
				{
					SnapMouseP = poClosest;
					ipoSnap = ipoClosest;
				}
			}

			else
			{
				ipoClosest = 0;
			}
		}
		else
		{
			// TODO: ???
		}

		// TODO: fix the halftransitioncount - when using released(button), it fires twice per release
#define DEBUGClick(button) (IsInScreenBounds(ScreenBuffer, Mouse.P) &&  \
		Input.Old->Mouse.button.EndedDown && !Input.New->Mouse.button.EndedDown)
#define DEBUGRelease(button) (Input.Old->button.EndedDown && !Input.New->button.EndedDown)
#define DEBUGPress(button)   (!Input.Old->button.EndedDown && Input.New->button.EndedDown)

		if(DEBUGPress(Keyboard.End)) State->ShowDebugInfo = !State->ShowDebugInfo;

		if(DEBUGPress(Keyboard.Home))
		{
			SaveUndoState(State);
			DRAW_STATE.Basis.Offset = ZeroV2;
			// TODO: do I want zoom to reset?
			// DRAW_STATE.Basis.Zoom   = 1.f;
			State->tBasis           = 0.f;
		}

		// TODO: fix needed for if started and space released part way?
		else if((Keyboard.Space.EndedDown && Mouse.LMB.EndedDown) || Mouse.MMB.EndedDown)
		{
			if(DEBUGPress(Mouse.LMB) || DEBUGPress(Mouse.MMB))
			{

			}
			// DRAG SCREEN AROUND
			DRAW_STATE.Basis.Offset = V2Add(DRAW_STATE.Basis.Offset,
				V2Sub(V2ScreenToCanvas(DRAW_STATE.Basis, pMouse.P, ScreenCentre),
					  V2ScreenToCanvas(DRAW_STATE.Basis,  Mouse.P, ScreenCentre)));
			// NOTE: prevents later triggers of clicks, may not be required if input scheme changes.
			Input.New->Mouse.LMB.EndedDown = 0;
		}

		/* // TODO: Do I actually want to be able to drag points? */
		/* else if(State->ipoDrag) */
		/* { */
		/* 	if(DEBUGClick(LMB)) */
		/* 	{ */
		/* 		SaveUndoState(State); */
		/* 		// Set point to mouse location and recompute intersections */
		/* 		State->ipoDrag = 0; */
		/* 		State->ipoSelect = 0; */
		/* 		// TODO: this breaks lines attached to intersections... */
		/* 		RemovePointsOfType(&DRAW_STATE, POINT_Intersection); */
		/* 		for(uint i = 1; i <= DRAW_STATE.iLastLinePoint; i+=2) */
		/* 		{ */
		/* 			// TODO: this is wasteful */
		/* 			AddLineIntersections(&DRAW_STATE, DRAW_STATE.LinePoints[i], i, 0); */
		/* 		} */
		/* 	} */

		/* 	else if(DEBUGClick(RMB) || Keyboard.Esc.EndedDown) */
		/* 	{ */
		/* 		// Cancel dragging, point returns to saved location */
		/* 		DRAW_STATE.Points[State->ipoDrag] = State->poSaved; */
		/* 		State->ipoDrag = 0; */
		/* 		State->ipoSelect = 0; */
		/* 	} */

		/* 	else */
		/* 	{ */
		/* 		DRAW_STATE.Points[State->ipoDrag] = Mouse.P; // Update dragged point to mouse location */
		/* 	} */
		/* 	// Snapping is off while dragging; TODO: maybe change this when points can be combined */
		/* 	ipoSnap = 0; */
		/* } */

		else if(State->ipoSelect)
		{
			if(Keyboard.Esc.EndedDown)
			{
				// Cancel selection, point returns to saved location
				DRAW_STATE.PointStatus[State->ipoSelect] = State->SavedStatus[0];
				DRAW_STATE.PointStatus[State->ipoArcStart] = State->SavedStatus[1];
				State->ipoSelect = 0;
				State->ipoArcStart = 0;
				--State->CurrentDrawState;
			}

			else if(Keyboard.Alt.EndedDown && DEBUGClick(LMB))
			{
				State->tBasis = 0.f;
				DRAW_STATE.Basis.XAxis = Norm(V2Sub(SnapMouseP, DRAW_STATE.Points[State->ipoSelect]));
				DRAW_STATE.PointStatus[State->ipoSelect] = State->SavedStatus[0];
				DRAW_STATE.PointStatus[State->ipoArcStart] = State->SavedStatus[1];
				State->ipoSelect = 0;
				State->ipoArcStart = 0;
			}

			else if(State->ipoArcStart)
			{
				if(!Mouse.RMB.EndedDown)
				{
					if(V2Equals(SnapMouseP, DRAW_STATE.Points[State->ipoArcStart])) // Same angle -> full circle
					{
						AddCircle(&DRAW_STATE, State->ipoSelect, AddPoint(&DRAW_STATE, SnapMouseP, POINT_Radius, 0));
					}
					else
					{
						v2 poFocus = DRAW_STATE.Points[State->ipoSelect];
						v2 poStart = DRAW_STATE.Points[State->ipoArcStart];
						v2 poEnd = V2Add(poFocus, V2WithLength(V2Sub(SnapMouseP, poFocus), Dist(poFocus, poStart))); // Attached to arc
						uint ipoArcEnd = AddPoint(&DRAW_STATE, poEnd, POINT_Arc, 0);
						AddArc(&DRAW_STATE, State->ipoSelect, State->ipoArcStart, ipoArcEnd);
					}
					State->ipoSelect = 0;
					State->ipoArcStart = 0;
				}
			}

			else if(DEBUGClick(LMB))
			{
				// NOTE: completed line, set both points' status if line does not already exist
				// and points aren't coincident
				if(!V2WithinEpsilon(DRAW_STATE.Points[State->ipoSelect], SnapMouseP, POINT_EPSILON))
				{
					// TODO: lines not adding properly..?
					AddLine(&DRAW_STATE, State->ipoSelect, AddPoint(&DRAW_STATE, SnapMouseP, POINT_Line, 0));
				}
				State->ipoSelect = 0;
			}

			else if(DEBUGPress(Mouse.RMB))
			{
				// TODO: stop snapping onto focus
				State->ipoArcStart = AddPoint(&DRAW_STATE, SnapMouseP, POINT_Arc, 0);
			}
		}

		else // normal state
		{
			// UNDO
			if(Keyboard.Ctrl.EndedDown && DEBUGRelease(Keyboard.Z) && !Keyboard.Shift.EndedDown)
			{
				// NOTE: this allows for 0 as a buffer for accessing the 'previous state'
				if(State->CurrentDrawState > 1)  --State->CurrentDrawState;
			}
			// REDO
			if((Keyboard.Ctrl.EndedDown && DEBUGRelease(Keyboard.Y)) ||
					(Keyboard.Ctrl.EndedDown && Keyboard.Shift.EndedDown && DEBUGRelease(Keyboard.Z)) )
			{
				if(State->CurrentDrawState < State->cDrawStates)  ++State->CurrentDrawState;
			}

			if(DEBUGClick(LMB))
			{
				// NOTE: Starting a line, save the first point
				/* State->SavedPoint = SnapMouseP; */
				SaveUndoState(State);
				State->ipoSelect = AddPoint(&DRAW_STATE, SnapMouseP, POINT_Extant, &State->SavedStatus[0]);
			}

			// TODO: could skip check and just write to invalid point..?
			if(ipoSnap) // point snapped to
			{
				if(DEBUGClick(RMB))
				{
					SaveUndoState(State);
					InvalidatePoint(&DRAW_STATE, ipoSnap);
				}
			}

			/* if(DEBUGClick(MMB)) */
			/* { */
				// MOVE POINT
				/* SaveUndoState(State); */
				/* State->SavedPoint = DRAW_STATE.Points[ipoSnap]; */
				/* State->ipoSelect = ipoSnap; */
				/* State->ipoDrag = ipoSnap; */
			/* } */ 

			if(DEBUGRelease(Keyboard.Backspace))
			{
				SaveUndoState(State);
				Reset(State);
				DebugClear();
			}
		}
	}

	{ LOG("RENDER");
		// NOTE: only gets odd numbers if there's an unfinished point
		uint cLines = (DRAW_STATE.iLastLinePoint)/2; // completed lines ... 1?
		v2 *Points = DRAW_STATE.Points;
		uint *LinePoints = DRAW_STATE.LinePoints;
		basis Basis = BasisLerp(pDRAW_STATE.Basis, State->tBasis, DRAW_STATE.Basis);
		v2 SSSnapMouseP = V2CanvasToScreen(Basis, SnapMouseP, ScreenCentre);

#if 0
		DrawCrosshair(ScreenBuffer, ScreenCentre, 20.f, RED);
		DEBUGDrawLine(ScreenBuffer, ScreenCentre,
			V2Add(ScreenCentre, V2Mult(50.f, V2CanvasToScreen(Basis, V2(1.f, 0.f), ScreenCentre))), CYAN);
#endif

		// NOTE: should be unchanged after this point in the frame
		LOG("\tDRAW LINES");
		for(uint iLine = 1; iLine <= cLines; ++iLine)
		{
			v2 poA = Points[LinePoints[2*iLine-1]];
			v2 poB = Points[LinePoints[2*iLine]];
			poA = V2CanvasToScreen(Basis, poA, ScreenCentre);
			poB = V2CanvasToScreen(Basis, poB, ScreenCentre);

			if((DRAW_STATE.LinePoints[2*iLine-1] && DRAW_STATE.LinePoints[2*iLine]))
			{
				DEBUGDrawLine(ScreenBuffer, poA, poB, BLACK);
				DrawClosestPtOnSegment(ScreenBuffer, Mouse.P, poA, poB);
			}
		}

		LOG("\tDRAW CIRCLES");
		for(uint iCircle = 1; iCircle <= DRAW_STATE.iLastCircle; ++iCircle)
		{
			circle Circle = DRAW_STATE.Circles[iCircle];
			if(Circle.ipoFocus)
			{
				v2 poFocus = V2CanvasToScreen(Basis, DRAW_STATE.Points[Circle.ipoFocus], ScreenCentre);
				v2 poRadius = V2CanvasToScreen(Basis, DRAW_STATE.Points[Circle.ipoRadius], ScreenCentre);
				CircleLine(ScreenBuffer, poFocus, Dist(poFocus, poRadius), BLACK);
			}
		}

 		LOG("\tDRAW ARCS");
		for(uint iArc = 1; iArc <= DRAW_STATE.iLastArc; ++iArc)
		{
			arc Arc = DRAW_STATE.Arcs[iArc];
			if(Arc.ipoFocus)
			{
				v2 poFocus = Points[Arc.ipoFocus];
				v2 poStart = Points[Arc.ipoStart];
				v2 poEnd   = Points[Arc.ipoEnd];
				poFocus = V2CanvasToScreen(Basis, poFocus, ScreenCentre);
				poStart = V2CanvasToScreen(Basis, poStart, ScreenCentre);
				poEnd   = V2CanvasToScreen(Basis, poEnd, ScreenCentre);
				ArcFromPoints(ScreenBuffer, poFocus, poStart, poEnd, BLACK); 
			}
		}

		LOG("\tDRAW POINTS");
		for(uint i = 1; i <= DRAW_STATE.iLastPoint; ++i)
		{
			if(DRAW_STATE.PointStatus[i] != POINT_Free)
			{
				v2 po = Points[i];
				po = V2CanvasToScreen(Basis, po, ScreenCentre);
				DrawPoint(ScreenBuffer, po, 0, LIGHT_GREY);
			}
		}

		poClosest = V2CanvasToScreen(Basis, poClosest, ScreenCentre);
		if(DRAW_STATE.iLastPoint) CircleLine(ScreenBuffer, poClosest, 5.f, GREY);
		if(ipoClosest) DrawCircleFill(ScreenBuffer, poClosest, 3.f, BLUE);


		if(State->ipoSelect) // A point is selected (currently drawing)
		{
			v2 poSelect = Points[State->ipoSelect];
			poSelect = V2CanvasToScreen(Basis, Points[State->ipoSelect], ScreenCentre);
		// TODO: start here, messing with mousep, snap, ss
			if(State->ipoArcStart && !V2Equals(Points[State->ipoArcStart], SnapMouseP)) // drawing an arc
			{
				LOG("\tDRAW HALF-FINISHED ARC");
				v2 poFocus = Points[State->ipoSelect];
				v2 poStart = Points[State->ipoArcStart];
				poFocus = V2CanvasToScreen(Basis, Points[State->ipoSelect], ScreenCentre);
				poStart = V2CanvasToScreen(Basis, Points[State->ipoArcStart], ScreenCentre);
				ArcFromPoints(ScreenBuffer, poFocus, poStart, SSSnapMouseP, BLACK);
				DEBUGDrawLine(ScreenBuffer, poSelect, poStart, LIGHT_GREY);
				DEBUGDrawLine(ScreenBuffer, poSelect, SSSnapMouseP, LIGHT_GREY);
			}
			else
			{
				LOG("\tDRAW SOMETHING");
				// NOTE: Mid-way through drawing a line
				DrawCircleFill(ScreenBuffer, poSelect, 3.f, RED);
				CircleLine(ScreenBuffer, poSelect, 5.f, RED);
				CircleLine(ScreenBuffer, poSelect, Dist(poSelect, SSSnapMouseP), LIGHT_GREY);
				DEBUGDrawLine(ScreenBuffer, poSelect, SSSnapMouseP, LIGHT_GREY);
				/* DebugAdd("\n\nMouse Angle: %f", MouseAngle/TAU); */
			}
		}

		if(ipoSnap)
		{
			// NOTE: Overdraws...
			DrawPoint(ScreenBuffer, poClosest, 1, BLUE);
		}
	}

	if(State->ShowDebugInfo)
	{ LOG("PRINT");
		DebugPrint();
		/* DrawSuperSlowCircleLine(ScreenBuffer, ScreenCentre, 50.f, RED); */

		CycleCountersInfo(ScreenBuffer, &State->DefaultFont);

		// TODO: Highlight status for currently selected/hovered points

		char Message[512];
		f32 TextSize = 15.f;
		stbsp_sprintf(Message, //"LinePoints: %u, TypeLine: %u, Esc Down: %u"
				"\nFrame time: %.2fms, Mouse: (%.2f, %.2f), Basis: (%.2f, %.2f), Offset: (%.2f, %.2f)",
				/* State->cLinePoints, */
				/* NumPointsOfType(DRAW_STATE.PointStatus, DRAW_STATE.iLastPoint, POINT_Line), */
				/* Keyboard.Esc.EndedDown, */
				/* Input.New->Controllers[0].Button.A.EndedDown, */
				State->dt*1000.f, Mouse.P.X, Mouse.P.Y, DRAW_STATE.Basis.XAxis.X, DRAW_STATE.Basis.XAxis.Y,
														DRAW_STATE.Basis.Offset.X, DRAW_STATE.Basis.Offset.Y);
		DrawString(ScreenBuffer, &State->DefaultFont, Message, TextSize, 10.f, TextSize, 1, BLACK);

		char LinePointInfo[512];
		stbsp_sprintf(LinePointInfo, "L#  P#\n\n");
		for(uint i = 1; i <= DRAW_STATE.cLinePoints && i <= 32; ++i)
		{
			stbsp_sprintf(LinePointInfo, "%s%02u  %02u\n", LinePointInfo, i, DRAW_STATE.LinePoints[i]);
		}
		char PointInfo[512];
		stbsp_sprintf(PointInfo, " # DARTFILE\n\n");
		for(uint i = 1; i <= DRAW_STATE.cPoints && i <= 32; ++i)
		{
			stbsp_sprintf(PointInfo, "%s%02u %08b\n", PointInfo, i, DRAW_STATE.PointStatus[i]);
		}
		TextSize = 13.f;
		DrawString(ScreenBuffer, &State->DefaultFont, LinePointInfo, TextSize,
				ScreenSize.X - 180.f, ScreenSize.Y - 30.f, 0, BLACK);
		DrawString(ScreenBuffer, &State->DefaultFont, PointInfo, TextSize,
				ScreenSize.X - 120.f, ScreenSize.Y - 30.f, 0, BLACK);
	}

	CLOSE_LOG();
	END_TIMED_BLOCK;
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
					stbsp_snprintf(DebugTextBuffer, Megabytes(8)-1,
								   /* AltFormat ? AltFormat :*/ "%s%22s%8s(%4d): %'12ucy %'8uh %'10ucy/h\n", 
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
