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
// - Arcs
// - Change storage of intersections, so they don't all need to be recomputed on changes
// - Spatially partition(?) shapes

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
ClosestPointIndex(state *State, v2 Comp, f32 *ClosestDist)
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
			if(!Result)
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
	for(uint i = 1; i <= State->LastPoint; ++i)
	{
		State->PointStatus[i] = POINT_Free;
	}
	// NOTE: Point index 0 is reserved for null points (not defined in lines)
	State->LastPoint     = 0;
	State->LastLinePoint = 0;
	State->LastCircle    = 0;
	State->NumPoints     = 0;
	State->NumLinePoints = 0;
	State->NumCircles    = 0;
	State->SelectIndex   = 0;
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
FindPointAtPos(state *State, v2 P, uint PointStatus)
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
AddPoint(state *State, v2 P, uint PointTypes, u8 *PriorStatus)
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

// TODO: add all intersections without recomputing line-circle
/// returns number of intersections
internal uint
AddCircleIntersections(state *State, uint Point, f32 Radius, uint SkipIndex)
{
	uint Result = 0;
	circle *Circles = State->Circles;
	v2 *Points = State->Points;
	uint *LinePoints = State->LinePoints;

	for(uint CircleIndex = 1; CircleIndex <= State->LastCircle; ++CircleIndex)
	{
		if(CircleIndex == SkipIndex) continue;
		v2 P1, P2;
		uint NumIntersections = IntersectCircles(Points[Point], Radius,
				Points[Circles[CircleIndex].Focus], Circles[CircleIndex].Radius, &P1, &P2);
		Result += NumIntersections;
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

	for(uint LineIndex = 1; LineIndex <= State->LastLinePoint; LineIndex+=2)
	{
		v2 P1 = Points[LinePoints[LineIndex]];
		v2 P2 = Points[LinePoints[LineIndex+1]];
		uint NumIntersections = IntersectSegmentCircle(P1, V2Sub(P2, P1), Points[Point], Radius, &P1, &P2);
		Result += NumIntersections;
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

	return Result;
}

/// returns number of intersections
internal uint
AddLineIntersections(state *State, uint PointA, uint PointB, uint SkipIndex)
{
	uint Result = 0;
	circle *Circles = State->Circles;
	v2 *Points = State->Points;
	uint *LinePoints = State->LinePoints;

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
	for(uint CircleIndex = 1; CircleIndex <= State->LastCircle; ++CircleIndex)
	{
		v2 P1, P2;
		uint NumIntersections = IntersectSegmentCircle(Points[PointA], V2Sub(Points[PointB], Points[PointA]),
				Points[Circles[CircleIndex].Focus], Circles[CircleIndex].Radius, &P1, &P2);
		Result += NumIntersections;
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

	return Result;
}

/// returns first point of the pair that make up the line
internal uint
AddLine(state *State, uint IndexA, uint IndexB)
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

	return Result;
}

internal uint
AddCircle(state *State, uint FocusIndex, f32 Radius)
{
	uint Result = 0;
	circle NewCircle;
	NewCircle.Focus = FocusIndex;
	NewCircle.Radius = Radius;
	State->PointStatus[FocusIndex] |= POINT_Focus;

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
		State->Circles[++State->LastCircle] = NewCircle;
		++State->NumCircles;
		AddCircleIntersections(State, NewCircle.Focus, NewCircle.Radius, State->LastCircle);
	}

	return Result;
}

internal void
InvalidateLinesAtPoint(state *State, uint PointIndex)
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
InvalidateCirclesWithFocusAtPoint(state *State, uint PointIndex)
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
InvalidatePoint(state *State, uint PointIndex)
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
RemovePointsOfType(state *State, uint PointType)
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

// TODO: implement undo/redo
UPDATE_AND_RENDER(UpdateAndRender)
{
	BEGIN_TIMED_BLOCK;
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
	Assert(State->OverflowTest == 0);
	Assert(State->NumLinePoints % 2 == 0);

	// Clear BG
	DrawRectangleFilled(ScreenBuffer, Origin, ScreenSize, WHITE);

	keyboard_state Keyboard = Input.New->Keyboard;
	// TODO: move out of screen space
	mouse_state Mouse = Input.New->Mouse;

	if(Held(Keyboard.Shift)) // 'S' on this computer
	{
		State->PointSnap = 0;
	}
	else
	{
		State->PointSnap = 1;
	}

	uint Closest = 0;
	f32 ClosestDistSq;
	v2 SnapMouseP = Mouse.P;
	v2 ClosestPoint = Mouse.P;
	Closest = ClosestPointIndex(State, Mouse.P, &ClosestDistSq);
	uint SnapIndex = 0;
	if(Closest)
	{
		ClosestPoint = State->Points[Closest];
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
#define DEBUGPress(button) (Input.Old->Keyboard.button.EndedDown && !Input.New->Keyboard.button.EndedDown)

	// TODO: Do I actually want to be able to drag points?
	if(State->DragIndex)
	{
		if(DEBUGClick(LMB))
		{
			// Set point to mouse location and recompute intersections
			State->DragIndex = 0;
			State->SelectIndex = 0;
			// TODO: this breaks lines attached to intersections...
			RemovePointsOfType(State, POINT_Intersection);
			for(uint i = 1; i <= State->LastLinePoint; i+=2)
			{
				// TODO: this is wasteful
				AddLineIntersections(State, State->LinePoints[i], i, 0);
			}
		}

		else if(DEBUGClick(RMB) || Keyboard.Esc.EndedDown)
		{
			// Cancel dragging, point returns to saved location
			State->Points[State->DragIndex] = State->SavedPoint;
			State->DragIndex = 0;
			State->SelectIndex = 0;
		}

		else
		{
			State->Points[State->DragIndex] = Mouse.P; // Update dragged point to mouse location
		}
		// Snapping is off while dragging; TODO: maybe change this when points can be combined
		SnapIndex = 0;
	}
	
	else if(State->SelectIndex)
	{
		if(DEBUGClick(LMB))
		{
			// NOTE: completed line, set both points' status if line does not already exist
			// and points aren't coincident
			if(!V2WithinEpsilon(State->Points[State->SelectIndex], SnapMouseP, POINT_EPSILON))
			{
				// TODO: lines not adding properly..?
				AddLine(State, State->SelectIndex, AddPoint(State, SnapMouseP, POINT_Line, 0));
			}
			State->SelectIndex = 0;
		}

		else if(DEBUGClick(RMB))
		{
			AddCircle(State, State->SelectIndex, Dist(State->Points[State->SelectIndex], SnapMouseP));
		}

		else if(Keyboard.Esc.EndedDown)
		{
			// Cancel selection, point returns to saved location
			State->PointStatus[State->SelectIndex] = State->SavedStatus;
			State->SelectIndex = 0;
		}
	}

	else // normal state
	{
		if(DEBUGClick(LMB))
		{
			// NOTE: Starting a line, save the first point
			/* State->SavedPoint = SnapMouseP; */
			State->SelectIndex = AddPoint(State, SnapMouseP, POINT_Extant, &State->SavedStatus);
		}

		// TODO: could skip check and just write to invalid point..?
		if(SnapIndex) // point snapped to
		{
			if(DEBUGClick(RMB))
			{
				InvalidatePoint(State, SnapIndex);
			}
		}

		if(DEBUGClick(MMB))
		{
		// Move point
		/* State->SavedPoint = State->Points[SnapIndex]; */
			State->SelectIndex = SnapIndex;
			State->DragIndex = SnapIndex;
		} 

		if(DEBUGPress(Backspace))
		{
			ResetPoints(State);
		}
	}

	// NOTE: only gets odd numbers if there's an unfinished point
	uint NumLines = (State->LastLinePoint)/2; // completed lines ... 1?
	v2 *Points = State->Points;
	uint *LinePoints = State->LinePoints;

#define LINE(lineI) Points[LinePoints[2*lineI-1]], Points[LinePoints[2*lineI]]
	for(uint LineI = 1; LineI <= NumLines; ++LineI)
	{
		if((State->LinePoints[2*LineI-1] && State->LinePoints[2*LineI]))
		{
			DEBUGDrawLine(ScreenBuffer, LINE(LineI), BLACK);
			DrawClosestPtOnSegment(ScreenBuffer, Mouse.P, LINE(LineI));
		}

	}
	for(uint CircleI = 1; CircleI <= State->LastCircle; ++CircleI)
	{
		circle Circle = State->Circles[CircleI];
		if(Circle.Focus)
		{
			CircleLine(ScreenBuffer, State->Points[Circle.Focus], Circle.Radius, BLACK);
		}
	}

	for(uint i = 1; i <= State->LastPoint; ++i)
	{
		if(State->PointStatus[i] != POINT_Free)
		{
			DrawPoint(ScreenBuffer, Points[i], 0, LIGHT_GREY);
		}
	}

	if(State->SelectIndex)
	{
		// NOTE: Mid-way through drawing a line
		DrawCircleFill(ScreenBuffer, State->Points[State->SelectIndex], 3.f, RED);
		CircleLine(ScreenBuffer, State->Points[State->SelectIndex], 5.f, RED);
		CircleLine(ScreenBuffer, State->Points[State->SelectIndex],
				Dist(State->Points[State->SelectIndex], SnapMouseP), LIGHT_GREY);
		DEBUGDrawLine(ScreenBuffer, State->Points[State->SelectIndex], SnapMouseP, LIGHT_GREY);
		if(DEBUGClick(RMB))
		{
			State->SelectIndex = 0;
		}
	}

	if(SnapIndex)
	{
		// NOTE: Overdraws...
		DrawPoint(ScreenBuffer, ClosestPoint, 1, BLUE);
	}

	CycleCountersInfo(ScreenBuffer, &State->DefaultFont);

	char Message[512];
	f32 TextSize = 15.f;
	stbsp_sprintf(Message, "LinePoints: %u, TypeLine: %u, Esc Down: %u"
				"\nFrame time: %.2f, (%.2f, %.2f)",
				State->NumLinePoints,
				NumPointsOfType(State->PointStatus, State->LastPoint, POINT_Line),
				Keyboard.Esc.EndedDown,
				/* Input.New->Controllers[0].Button.A.EndedDown, */
				State->dt*1000.f, Mouse.P.X, Mouse.P.Y);
	DrawString(ScreenBuffer, &State->DefaultFont, Message, TextSize, 10.f, TextSize, 1, BLACK);

	char LinePointInfo[512];
	stbsp_sprintf(LinePointInfo, "L#  P#\n\n");
	for(uint i = 1; i <= State->NumLinePoints && i <= 32; ++i)
	{
		stbsp_sprintf(LinePointInfo, "%s%02u  %02u\n", LinePointInfo, i, State->LinePoints[i]);
	}
	char PointInfo[512];
	stbsp_sprintf(PointInfo, " # DARTFILE\n\n");
	for(uint i = 1; i <= State->NumPoints && i <= 32; ++i)
	{
		stbsp_sprintf(PointInfo, "%s%02u %08b\n", PointInfo, i, State->PointStatus[i]);
	}
	TextSize = 13.f;
	DrawString(ScreenBuffer, &State->DefaultFont, LinePointInfo, TextSize,
			ScreenSize.X - 180.f, ScreenSize.Y - 30.f, 0, BLACK);
	DrawString(ScreenBuffer, &State->DefaultFont, PointInfo, TextSize,
			ScreenSize.X - 120.f, ScreenSize.Y - 30.f, 0, BLACK);
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
