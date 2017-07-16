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

#define DRAW_STATE State->Draw[State->CurrentDrawState]

internal inline void
DrawClosestPtOnSegment(image_buffer *ScreenBuffer, v2 P, v2 A, v2 B)
{
	v2 P1 = ClosestPtOnSegment(P, A, V2Sub(B, A));
	DrawCrosshair(ScreenBuffer, P1, 5.f, RED);
	/* DEBUGDrawLine(ScreenBuffer, P1, P, LIGHT_GREY); */
}

internal inline void
DrawPoint(image_buffer *ScreenBuffer, v2 P, b32 Active, colour Col)
{
	DrawCircleFill(ScreenBuffer, P, 3.f, Col);
	if(Active)
	{
		CircleLine(ScreenBuffer, P, 5.f, Col);
	}
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
	for(uint i = 1; i <= State->LastPoint; ++i)
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
ResetPoints(state *State)
{
	BEGIN_TIMED_BLOCK;
	draw_state *Draw = &DRAW_STATE;

	for(uint i = 1; i <= Draw->LastPoint; ++i)
	{
		Draw->PointStatus[i] = POINT_Free;
	}
	// NOTE: Point index 0 is reserved for null points (not defined in lines)
	Draw->LastPoint     = 0;
	Draw->LastLinePoint = 0;
	Draw->LastCircle    = 0;
	Draw->LastArc	    = 0;

	Draw->NumPoints     = 0;
	Draw->NumLinePoints = 0;
	Draw->NumCircles    = 0;
	Draw->NumArcs	    = 0;

	State->SelectIndex  = 0;
	State->ArcStartIndex  = 0;
	END_TIMED_BLOCK;
}

// NOTE: less than numlinepoints if any points are reused
internal uint
NumPointsOfType(u8 *Statuses, uint EndIndex, uint PointTypes)
{
	BEGIN_TIMED_BLOCK;
	// TODO: could be done 16x faster with SIMD (maybe just for practice)
	uint Result = 0;
	for(uint i = 1; i <= EndIndex; ++i)
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
FindPointAtPos(draw_state *State, v2 P, uint PointStatus)
{
	BEGIN_TIMED_BLOCK;
	uint Result = 0;
	for(uint i = 1; i <= State->LastPoint; ++i)
	{
		if(V2WithinEpsilon(P, State->Points[i], POINT_EPSILON) && (State->PointStatus[i] & PointStatus))
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
AddPoint(draw_state *State, v2 P, uint PointTypes, u8 *PriorStatus)
{
	BEGIN_TIMED_BLOCK;
	uint Result = FindPointAtPos(State, P, ~(uint)POINT_Free);
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
		for(uint PointIndex = 1; PointIndex <= State->LastPoint; ++PointIndex)
		{
			// NOTE: Use existing point if free
#if 0
			// NOTE: this should be disappearing with dynamic allocation
			// if(State->LastPoint < ArrayCount(State->Points))
#endif
			if(State->PointStatus[PointIndex] == POINT_Free)
			{
				Result = PointIndex;
				break;
			}
		}
		// NOTE: Create new point if needed
		if(!Result)
		{
			Result = ++State->LastPoint;
			++State->NumPoints;
		}
		State->Points[Result] = P;
		State->PointStatus[Result] |= PointTypes | POINT_Extant;
	}

	DebugReplace("AddPoint => %u\n", Result);

	END_TIMED_BLOCK;
	return Result;
}

internal inline uint
MatchingPointIndex(uint PointIndex)
{
	uint Result;
	if(PointIndex % 2)
	{
		// NOTE: first of 2 line points
		Result = PointIndex + 1;
	}
	else
	{
		// NOTE: second of 2 line points
		Result = PointIndex - 1;
	}
	return Result;
}

internal inline void
AddIntersections(draw_state *State, v2 P1, v2 P2, uint NumIntersections)
{
	if(NumIntersections == 1)
	{
		AddPoint(State, P1, POINT_Intersection, 0);
	}
	else if(NumIntersections == 2)
	{
		AddPoint(State, P1, POINT_Intersection, 0);
		AddPoint(State, P2, POINT_Intersection, 0);
	}
}

internal inline f32
ArcRadius(v2 *Points, arc Arc)
{
	f32 Result = Dist(Points[Arc.Focus], Points[Arc.Start]);
	return Result;
}

internal inline uint
IntersectPointsSegmentArc(v2 *Points, v2 P, v2 Dir, arc Arc, v2 *Intersection1, v2 *Intersection2)
{
	f32 Radius = ArcRadius(Points, Arc);
	v2 Start = V2Sub(Points[Arc.Start], Points[Arc.Focus]);
	v2 End = V2Sub(Points[Arc.End], Points[Arc.Focus]);
	return IntersectSegmentArc(P, Dir, Points[Arc.Focus], Radius, Start, End, Intersection1, Intersection2);
}

internal inline uint
IntersectPointsCircleArc(v2 *Points, v2 Focus, f32 Radius, arc Arc, v2 *Intersection1, v2 *Intersection2)
{
	f32 ArcRad = ArcRadius(Points, Arc);
	v2 Start = V2Sub(Points[Arc.Start], Points[Arc.Focus]);
	v2 End = V2Sub(Points[Arc.End], Points[Arc.Focus]);
	return IntersectCircleArc(Focus, Radius, Points[Arc.Focus], ArcRad, Start, End, Intersection1, Intersection2);
}

internal inline uint
IntersectPointsArcs(v2 *Points, arc Arc1, arc Arc2, v2 *Intersection1, v2 *Intersection2)
{
	f32 R1 = ArcRadius(Points, Arc1);
	f32 R2 = ArcRadius(Points, Arc2);
	v2 Start1 = V2Sub(Points[Arc1.Start], Points[Arc1.Focus]);
	v2 End1 = V2Sub(Points[Arc1.End], Points[Arc1.Focus]);
	v2 Start2 = V2Sub(Points[Arc2.Start], Points[Arc2.Focus]);
	v2 End2 = V2Sub(Points[Arc2.End], Points[Arc2.Focus]);
	return IntersectArcs(Points[Arc1.Focus], R1, Start1, End1, Points[Arc2.Focus], R2, Start2, End2, Intersection1, Intersection2);
}

// TODO: add all intersections without recomputing line-circle
/// returns number of intersections
internal uint
AddCircleIntersections(draw_state *State, uint Point, f32 Radius, uint SkipIndex)
{
	uint Result = 0, NumIntersections = 0;
	circle *Circles = State->Circles;
	arc *Arcs = State->Arcs;
	v2 *Points = State->Points;
	uint *LinePoints = State->LinePoints;
	v2 P1, P2;

	for(uint CircleIndex = 1; CircleIndex <= State->LastCircle; ++CircleIndex)
	{
		if(CircleIndex == SkipIndex) continue;
		NumIntersections = IntersectCircles(Points[Point], Radius,
				Points[Circles[CircleIndex].Focus], Circles[CircleIndex].Radius, &P1, &P2);
		Result += NumIntersections;
		AddIntersections(State, P1, P2, NumIntersections);
	}

	for(uint ArcIndex = 1; ArcIndex <= State->LastArc; ++ArcIndex)
	{
		NumIntersections = IntersectPointsCircleArc(Points, Points[Point], Radius, Arcs[ArcIndex],	&P1, &P2);
		Result += NumIntersections;
		AddIntersections(State, P1, P2, NumIntersections);
	}

	for(uint LineIndex = 1; LineIndex <= State->LastLinePoint; LineIndex+=2)
	{
		P1 = Points[LinePoints[LineIndex]];
		P2 = Points[LinePoints[LineIndex+1]];
		NumIntersections = IntersectSegmentCircle(P1, V2Sub(P2, P1), Points[Point], Radius, &P1, &P2);
		Result += NumIntersections;
		AddIntersections(State, P1, P2, NumIntersections);
	}

	return Result;
}

internal uint
AddArcIntersections(draw_state *State, arc Arc, uint SkipIndex)
{
	uint Result = 0, NumIntersections = 0;
	circle *Circles = State->Circles;
	arc *Arcs = State->Arcs;
	v2 *Points = State->Points;
	uint *LinePoints = State->LinePoints;
	v2 P1, P2;

	for(uint CircleIndex = 1; CircleIndex <= State->LastCircle; ++CircleIndex)
	{
		NumIntersections = IntersectPointsCircleArc(Points, Points[Circles[CircleIndex].Focus], Circles[CircleIndex].Radius,
				Arc, &P1, &P2);
		Result += NumIntersections;
		AddIntersections(State, P1, P2, NumIntersections);
	}

	for(uint ArcIndex = 1; ArcIndex <= State->LastArc; ++ArcIndex)
	{
		if(ArcIndex == SkipIndex) continue;
		NumIntersections = IntersectPointsArcs(Points, Arc, Arcs[ArcIndex], &P1, &P2);
		Result += NumIntersections;
		AddIntersections(State, P1, P2, NumIntersections);
	}

	for(uint LineIndex = 1; LineIndex <= State->LastLinePoint; LineIndex+=2)
	{
		P1 = Points[LinePoints[LineIndex]];
		P2 = Points[LinePoints[LineIndex+1]];
		NumIntersections = IntersectPointsSegmentArc(Points, P1, V2Sub(P2, P1), Arc, &P1, &P2);
		Result += NumIntersections;
		AddIntersections(State, P1, P2, NumIntersections);
	}

	return Result;
}

/// returns number of intersections
internal uint
AddLineIntersections(draw_state *State, uint PointA, uint PointB, uint SkipIndex)
{
	uint Result = 0, NumIntersections = 0;
	circle *Circles = State->Circles;
	arc *Arcs = State->Arcs;
	v2 *Points = State->Points;
	uint *LinePoints = State->LinePoints;
	v2 P1, P2;

	for(uint LineIndex = 1; LineIndex <= State->LastLinePoint; LineIndex+=2)
	{
		if(LineIndex == SkipIndex) continue;

		// NOTE: TODO? internal line between eg corners of square adds 1 intersection... sometimes?
		v2 Intersect;
		// IMPORTANT TODO: spatially separate, maybe hierarchically
		if(IntersectSegmentsWinding(Points[PointA], Points[PointB],
					Points[LinePoints[LineIndex]], Points[LinePoints[LineIndex+1]],
					&Intersect))
		{
			AddPoint(State, Intersect, POINT_Intersection, 0);
			++Result;
		}
		// TODO: use segments rather than lines
	}

	for(uint ArcIndex = 1; ArcIndex <= State->LastArc; ++ArcIndex)
	{
		NumIntersections = IntersectPointsSegmentArc(Points, Points[PointA], V2Sub(Points[PointB], Points[PointA]), 
				Arcs[ArcIndex], &P1, &P2);
		Result += NumIntersections;
		AddIntersections(State, P1, P2, NumIntersections);
	}

	for(uint CircleIndex = 1; CircleIndex <= State->LastCircle; ++CircleIndex)
	{
		NumIntersections = IntersectSegmentCircle(Points[PointA], V2Sub(Points[PointB], Points[PointA]),
				Points[Circles[CircleIndex].Focus], Circles[CircleIndex].Radius, &P1, &P2);
		Result += NumIntersections;
		AddIntersections(State, P1, P2, NumIntersections);
	}

	return Result;
}

/// returns first point of the pair that make up the line
internal uint
AddLine(draw_state *State, uint IndexA, uint IndexB)
{
	// TODO: could optimise out checking indices if already known (as optional params?)
	uint Result = 0;
	// NOTE: avoids duplicate lines
	b32 ExistingLine = 0;
	for(uint LinePointIndex = 1; LinePointIndex <= State->LastLinePoint; LinePointIndex += 2)
	{
			// NOTE: no ordering
		if( (State->LinePoints[LinePointIndex] == IndexA && State->LinePoints[LinePointIndex + 1] == IndexB) ||
			(State->LinePoints[LinePointIndex] == IndexB && State->LinePoints[LinePointIndex + 1] == IndexA))
		{
			ExistingLine = 1;
			break;
		}
	}

	if(!ExistingLine)
	{
		uint EmptyLinePoint = 0;
		for(uint LinePointIndex = 1; LinePointIndex <= State->LastLinePoint; LinePointIndex += 2)
		{
			// NOTE: only checking first line point of each pair
			if(State->LinePoints[LinePointIndex] == 0)
			{
				EmptyLinePoint = LinePointIndex;
				break;
			}
		}

		if(EmptyLinePoint)
		{
			State->LinePoints[EmptyLinePoint]     = IndexA;
			State->LinePoints[EmptyLinePoint + 1] = IndexB;
			AddLineIntersections(State, IndexA, IndexB, EmptyLinePoint);
			Result = EmptyLinePoint;
		}
		else
		{
			State->LinePoints[++State->LastLinePoint] = IndexA;
			State->LinePoints[++State->LastLinePoint] = IndexB;
			AddLineIntersections(State, IndexA, IndexB, State->LastLinePoint-1);
			Result = State->LastLinePoint-1;
			State->NumLinePoints += 2; // TODO: numlines?
		}
	}
	// TODO: make thread-safe?
	State->PointStatus[IndexA] |= POINT_Line;
	State->PointStatus[IndexB] |= POINT_Line;

	DebugReplace("AddLine => %d", Result);
	return Result;
}

internal uint
AddArc(draw_state *State, uint FocusIndex, uint ArcStartIndex, uint ArcEndIndex)
{
	uint Result = 0;
	arc NewArc;
	NewArc.Focus = FocusIndex;
	NewArc.Start = ArcStartIndex;
	NewArc.End   = ArcEndIndex;
	State->PointStatus[FocusIndex]    |= POINT_Focus;
	State->PointStatus[ArcStartIndex] |= POINT_Arc;
	State->PointStatus[ArcEndIndex]   |= POINT_Arc;

	// Ensure it's not a duplicate
	b32 ExistingArc = 0;
	for(uint ArcIndex = 1; ArcIndex <= State->LastArc; ++ArcIndex)
	{
		arc TestArc = State->Arcs[ArcIndex];
		if(TestArc.Focus == FocusIndex && TestArc.Start == ArcStartIndex && TestArc.End == ArcEndIndex)
		{
			ExistingArc = 1;
			break;
		}
	}

	if(!ExistingArc)
	{
		Result = ++State->LastArc;
		State->Arcs[Result] = NewArc;
		++State->NumArcs;
		AddArcIntersections(State, NewArc, State->LastArc);
	}

	return Result;
}

internal uint
AddCircle(draw_state *State, uint FocusIndex, f32 Radius)
{
	uint Result = 0;
	circle NewCircle;
	NewCircle.Focus = FocusIndex;
	NewCircle.Radius = Radius;
	State->PointStatus[FocusIndex] |= POINT_Focus;

	// Ensure it's not a duplicate
	b32 ExistingCircle = 0;
	for(uint CircleIndex = 1; CircleIndex <= State->LastCircle; ++CircleIndex)
	{
		circle TestCircle = State->Circles[CircleIndex];
		if(TestCircle.Focus == FocusIndex && TestCircle.Radius == Radius)
		{
			ExistingCircle = 1;
			break;
		}
	}

	if(!ExistingCircle)
	{
		Result = ++State->LastCircle;
		State->Circles[Result] = NewCircle;
		++State->NumCircles;
		AddCircleIntersections(State, NewCircle.Focus, NewCircle.Radius, State->LastCircle);
	}

	return Result;
}

internal void
InvalidateLinesAtPoint(draw_state *State, uint PointIndex)
{
	// TODO: invalidate both points or just one?
	// TODO: do I want this here or in drawing/adding..?
	for(uint i = 1; i <= State->LastLinePoint; ++i)
	{
		if(State->LinePoints[i] == PointIndex)
		{
			State->LinePoints[i] = 0;
			State->LinePoints[MatchingPointIndex(i)] = 0;
		}
	}
}

internal void
InvalidateCirclesWithFocusAtPoint(draw_state *State, uint PointIndex)
{
	// TODO: invalidate both points or just one?
	// TODO: do I want this here or in drawing/adding..?
	for(uint i = 1; i <= State->LastCircle; ++i)
	{
		if(State->Circles[i].Focus == PointIndex)
		{
			State->Circles[i].Focus = 0;
		}
	}
}

internal void
InvalidatePoint(draw_state *State, uint PointIndex)
{
	u8 Status = State->PointStatus[PointIndex];
	if(Status & POINT_Line)
	{
		InvalidateLinesAtPoint(State, PointIndex);
	}
	if(Status & POINT_Focus)
	{
		InvalidateCirclesWithFocusAtPoint(State, PointIndex);
	}
	State->PointStatus[PointIndex] = POINT_Free;
}

/// returns number of points removed
internal uint
RemovePointsOfType(draw_state *State, uint PointType)
{
	uint Result = 0;
	for(uint i = 1; i <= State->LastPoint; ++i)
	{
		if(State->PointStatus[i] & PointType)
		{
			InvalidatePoint(State, i);
			++Result;
		}
	}
	return Result;
}

internal inline void
SaveUndoState(state *State)
{
	draw_state PrevDrawState = DRAW_STATE;

	// NOTE: Only needed for limited undo numbers
	if(State->NumDrawStates == NUM_UNDO_STATES-1)
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

	if(State->NumDrawStates < State->CurrentDrawState)  State->NumDrawStates = State->CurrentDrawState;
	State->NumDrawStates = State->CurrentDrawState;
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
		ResetPoints(State);
		InitArena(&Arena, (u8 *)Memory->PermanentStorage + sizeof(state), Memory->PermanentStorageSize - sizeof(state));

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
	Assert(DRAW_STATE.NumLinePoints % 2 == 0);

	// Clear BG
	DrawRectangleFilled(ScreenBuffer, Origin, ScreenSize, WHITE);

	keyboard_state Keyboard;
	mouse_state Mouse;
	v2 SnapMouseP, ClosestPoint;
	uint SnapIndex;
	{ LOG("INPUT");
		Keyboard = Input.New->Keyboard;
		// TODO: move out of screen space
		Mouse = Input.New->Mouse;

		if(Held(Keyboard.Shift))
		{
			State->PointSnap = 0;
		}
		else
		{
			State->PointSnap = 1;
		}

		uint Closest = 0;
		f32 ClosestDistSq;
		SnapMouseP = Mouse.P;
		ClosestPoint = Mouse.P;
		Closest = ClosestPointIndex(&DRAW_STATE, Mouse.P, &ClosestDistSq);
		SnapIndex = 0;
		if(Closest)
		{
			ClosestPoint = DRAW_STATE.Points[Closest];
			CircleLine(ScreenBuffer, ClosestPoint, 5.f, GREY);
			if(ClosestDistSq < 5000.f)
			{
				DrawCircleFill(ScreenBuffer, ClosestPoint, 3.f, BLUE);
				// TODO: change to shift
				if(State->PointSnap)
				{
					SnapMouseP = ClosestPoint;
					SnapIndex = Closest;
				}

			}
		}
		else
		{
			// TODO: ???
		}

		// TODO: fix the halftransitioncount - when using released(button), it fires twice per release
#define DEBUGClick(button) (IsInScreenBounds(ScreenBuffer, Mouse.P) &&  \
		Input.Old->Mouse.Buttons[button].EndedDown && !Input.New->Mouse.Buttons[button].EndedDown)
#define DEBUGRelease(button) (Input.Old->button.EndedDown && !Input.New->button.EndedDown)
#define DEBUGPress(button)   (!Input.Old->button.EndedDown && Input.New->button.EndedDown)

		// TODO: Do I actually want to be able to drag points?
		if(State->DragIndex)
		{
			if(DEBUGClick(LMB))
			{
				SaveUndoState(State);
				// Set point to mouse location and recompute intersections
				State->DragIndex = 0;
				State->SelectIndex = 0;
				// TODO: this breaks lines attached to intersections...
				RemovePointsOfType(&DRAW_STATE, POINT_Intersection);
				for(uint i = 1; i <= DRAW_STATE.LastLinePoint; i+=2)
				{
					// TODO: this is wasteful
					AddLineIntersections(&DRAW_STATE, DRAW_STATE.LinePoints[i], i, 0);
				}
			}

			else if(DEBUGClick(RMB) || Keyboard.Esc.EndedDown)
			{
				// Cancel dragging, point returns to saved location
				DRAW_STATE.Points[State->DragIndex] = State->SavedPoint;
				State->DragIndex = 0;
				State->SelectIndex = 0;
			}

			else
			{
				DRAW_STATE.Points[State->DragIndex] = Mouse.P; // Update dragged point to mouse location
			}
			// Snapping is off while dragging; TODO: maybe change this when points can be combined
			SnapIndex = 0;
		}

		else if(State->SelectIndex)
		{
			if(Keyboard.Esc.EndedDown)
			{
				// Cancel selection, point returns to saved location
				DRAW_STATE.PointStatus[State->SelectIndex] = State->SavedStatus[0];
				DRAW_STATE.PointStatus[State->ArcStartIndex] = State->SavedStatus[1];
				State->SelectIndex = 0;
				State->ArcStartIndex = 0;
				--State->CurrentDrawState;
			}

			else if(State->ArcStartIndex)
			{
				if(!Mouse.Buttons[RMB].EndedDown)
				{
					if(V2Equals(SnapMouseP, DRAW_STATE.Points[State->ArcStartIndex])) // Same angle -> full circle // TODO: is this the right epsilon?
					{
						AddCircle(&DRAW_STATE, State->SelectIndex, Dist(DRAW_STATE.Points[State->SelectIndex], SnapMouseP));
					}
					else
					{
						v2 FocusPoint = DRAW_STATE.Points[State->SelectIndex];
						v2 StartPoint = DRAW_STATE.Points[State->ArcStartIndex];
						v2 EndPoint = V2Add(FocusPoint, V2WithLength(V2Sub(SnapMouseP, FocusPoint), Dist(FocusPoint, StartPoint))); // Attached to arc
						uint ArcEndIndex = AddPoint(&DRAW_STATE, EndPoint, POINT_Arc, 0);
						AddArc(&DRAW_STATE, State->SelectIndex, State->ArcStartIndex, ArcEndIndex);
					}
					State->SelectIndex = 0;
					State->ArcStartIndex = 0;
				}
			}

			else if(DEBUGClick(LMB))
			{
				// NOTE: completed line, set both points' status if line does not already exist
				// and points aren't coincident
				if(!V2WithinEpsilon(DRAW_STATE.Points[State->SelectIndex], SnapMouseP, POINT_EPSILON))
				{
					// TODO: lines not adding properly..?
					AddLine(&DRAW_STATE, State->SelectIndex, AddPoint(&DRAW_STATE, SnapMouseP, POINT_Line, 0));
				}
				State->SelectIndex = 0;
			}

			else if(DEBUGPress(Mouse.Buttons[RMB]))
			{
				// TODO: stop snapping onto focus
				State->ArcStartIndex = AddPoint(&DRAW_STATE, SnapMouseP, POINT_Arc, 0);
			}
		}

		else // normal state
		{
			// UNDO
			if(Keyboard.Ctrl.EndedDown && DEBUGRelease(Keyboard.Z) && !Keyboard.Shift.EndedDown)
			{
				if(State->CurrentDrawState > 0)  --State->CurrentDrawState;
			}
			// REDO
			if((Keyboard.Ctrl.EndedDown && DEBUGRelease(Keyboard.Y)) ||
					(Keyboard.Ctrl.EndedDown && Keyboard.Shift.EndedDown && DEBUGRelease(Keyboard.Z)) )
			{
				if(State->CurrentDrawState < State->NumDrawStates)  ++State->CurrentDrawState;
			}

			if(DEBUGClick(LMB))
			{
				// NOTE: Starting a line, save the first point
				/* State->SavedPoint = SnapMouseP; */
				SaveUndoState(State);
				State->SelectIndex = AddPoint(&DRAW_STATE, SnapMouseP, POINT_Extant, &State->SavedStatus[0]);
			}

			// TODO: could skip check and just write to invalid point..?
			if(SnapIndex) // point snapped to
			{
				if(DEBUGClick(RMB))
				{
					SaveUndoState(State);
					InvalidatePoint(&DRAW_STATE, SnapIndex);
				}
			}

			if(DEBUGClick(MMB))
			{
				// MOVE POINT
				SaveUndoState(State);
				/* State->SavedPoint = DRAW_STATE.Points[SnapIndex]; */
				State->SelectIndex = SnapIndex;
				State->DragIndex = SnapIndex;
			} 

			if(DEBUGRelease(Keyboard.Backspace))
			{
				SaveUndoState(State);
				ResetPoints(State);
				DebugClear();
			}
		}
	}

	{ LOG("RENDER");
		// NOTE: only gets odd numbers if there's an unfinished point
		uint NumLines = (DRAW_STATE.LastLinePoint)/2; // completed lines ... 1?
		v2 *Points = DRAW_STATE.Points;
		uint *LinePoints = DRAW_STATE.LinePoints;

#define LINE(lineI) Points[LinePoints[2*lineI-1]], Points[LinePoints[2*lineI]]
		// NOTE: should be unchanged after this point in the frame
		LOG("\tDRAW LINES");
		for(uint LineI = 1; LineI <= NumLines; ++LineI)
		{
			if((DRAW_STATE.LinePoints[2*LineI-1] && DRAW_STATE.LinePoints[2*LineI]))
			{
				DEBUGDrawLine(ScreenBuffer, LINE(LineI), BLACK);
				DrawClosestPtOnSegment(ScreenBuffer, Mouse.P, LINE(LineI));
			}
		}

		LOG("\tDRAW CIRCLES");
		for(uint CircleI = 1; CircleI <= DRAW_STATE.LastCircle; ++CircleI)
		{
			circle Circle = DRAW_STATE.Circles[CircleI];
			if(Circle.Focus)
			{
				CircleLine(ScreenBuffer, DRAW_STATE.Points[Circle.Focus], Circle.Radius, BLACK);
			}
		}

 		LOG("\tDRAW ARCS");
		for(uint ArcI = 1; ArcI <= DRAW_STATE.LastArc; ++ArcI)
		{
			arc Arc = DRAW_STATE.Arcs[ArcI];
			if(Arc.Focus)
			{
				v2 Focus = Points[Arc.Focus];
				v2 Start = Points[Arc.Start];
				v2 End   = Points[Arc.End];
				ArcFromPoints(ScreenBuffer, Focus, Start, End, BLACK); 
			}
		}

		LOG("\tDRAW POINTS");
		for(uint i = 1; i <= DRAW_STATE.LastPoint; ++i)
		{
			if(DRAW_STATE.PointStatus[i] != POINT_Free)
			{
				DrawPoint(ScreenBuffer, Points[i], 0, LIGHT_GREY);
			}
		}

		if(State->SelectIndex) // A point is selected (currently drawing)
		{
			v2 SelectedPoint = Points[State->SelectIndex];
			if(State->ArcStartIndex && !V2Equals(Points[State->ArcStartIndex], SnapMouseP)) // drawing an arc
			{
				LOG("\tDRAW HALF-FINISHED ARC");
				v2 FocusPoint = Points[State->SelectIndex];
				v2 StartPoint = Points[State->ArcStartIndex];
				ArcFromPoints(ScreenBuffer, FocusPoint, StartPoint, SnapMouseP, BLACK);
				DEBUGDrawLine(ScreenBuffer, SelectedPoint, StartPoint, LIGHT_GREY);
				DEBUGDrawLine(ScreenBuffer, SelectedPoint, SnapMouseP, LIGHT_GREY);
			}
			else
			{
				LOG("\tDRAW SOMETHING");
				// NOTE: Mid-way through drawing a line
				DrawCircleFill(ScreenBuffer, SelectedPoint, 3.f, RED);
				CircleLine(ScreenBuffer, SelectedPoint, 5.f, RED);
				CircleLine(ScreenBuffer, SelectedPoint, Dist(SelectedPoint, SnapMouseP), LIGHT_GREY);
				DEBUGDrawLine(ScreenBuffer, SelectedPoint, SnapMouseP, LIGHT_GREY);
				/* DebugAdd("\n\nMouse Angle: %f", MouseAngle/TAU); */
			}
		}

		if(SnapIndex)
		{
			// NOTE: Overdraws...
			DrawPoint(ScreenBuffer, ClosestPoint, 1, BLUE);
		}
	}

	{ LOG("PRINT");
		DebugPrint();
		/* DrawSuperSlowCircleLine(ScreenBuffer, ScreenCentre, 50.f, RED); */

		//	CycleCountersInfo(ScreenBuffer, &State->DefaultFont);

		// TODO: Highlight status for currently selected/hovered points

		char Message[512];
		f32 TextSize = 15.f;
		stbsp_sprintf(Message, //"LinePoints: %u, TypeLine: %u, Esc Down: %u"
				"\nFrame time: %.2fms, Mouse: (%.2f, %.2f), Undo Number: %u, Num undo states: %u",
				/* State->NumLinePoints, */
				/* NumPointsOfType(DRAW_STATE.PointStatus, DRAW_STATE.LastPoint, POINT_Line), */
				/* Keyboard.Esc.EndedDown, */
				/* Input.New->Controllers[0].Button.A.EndedDown, */
				State->dt*1000.f, Mouse.P.X, Mouse.P.Y, State->CurrentDrawState, State->NumDrawStates);
		DrawString(ScreenBuffer, &State->DefaultFont, Message, TextSize, 10.f, TextSize, 1, BLACK);

		char LinePointInfo[512];
		stbsp_sprintf(LinePointInfo, "L#  P#\n\n");
		for(uint i = 1; i <= DRAW_STATE.NumLinePoints && i <= 32; ++i)
		{
			stbsp_sprintf(LinePointInfo, "%s%02u  %02u\n", LinePointInfo, i, DRAW_STATE.LinePoints[i]);
		}
		char PointInfo[512];
		stbsp_sprintf(PointInfo, " # DARTFILE\n\n");
		for(uint i = 1; i <= DRAW_STATE.NumPoints && i <= 32; ++i)
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
			/* i < NumCounters; */
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
								   /* AltFormat ? AltFormat :*/ "%s%24s(%4d): %'12ucy %'8uh %'10ucy/h\n", 
								   DebugTextBuffer,
								   Counter->FunctionName,
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
