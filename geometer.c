#define _CRT_SECURE_NO_WARNINGS
#include "geometer.h"
#include <fonts.c>

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
ClosestPointIndex(v2 *Subset, uint EndIndex, v2 Comp, f32 *ClosestDist)
{
	uint Result = 0;
	// NOTE: all valid points start at 1
	f32 Closest = 0;
	for(uint i = 1; i <= EndIndex; ++i)
	{
		// TODO: maybe put this above loop
		if(!Result)
		{
			Result = i;
			Closest = DistSq(Subset[i], Comp);
		}

		else
		{
			f32 Test = DistSq(Subset[i], Comp);
			if(Test < Closest)
			{
				Closest = Test;
				Result = i;
			}
		}
	}
	*ClosestDist = Closest;
	return Result;
}

internal void
ResetPoints(state *State)
{
	// NOTE: Point index 0 is reserved for null points (not defined in lines)
	State->LastPoint          = 0;
	State->LastLinePoint      = 0;
	State->LastIntersectPoint = 0;
	State->NumPoints          = 0;
	State->NumLinePoints      = 0;
	State->NumIntersectPoints = 0;
}


/// returns index of point (may be new or existing)
// TODO: less definite name? ProposePoint, ConsiderNewPoint...?
// TODO: find free points, not just next
internal uint
AddPoint(state *State, v2 P, uint PointTypes)
{
	uint Result = 0;
	b32 FoundMatch = 0;
	for(uint i = 1; i <= State->LastPoint; ++i)
	{
		if(V2Equals(P, State->Points[i]))
		{
			// NOTE: Use existing point
			FoundMatch = 1;
			State->PointStatus[i] |= PointTypes;
			Result = i;
			break;
		}
	}

	if(!FoundMatch)
	{
		if(State->LastPoint >= ArrayCount(State->Points))
		{
			// NOTE: No more space in array for new point
			Result = 0;
		}
		else
		{
			// NOTE: Create new point
			Result = ++State->LastPoint;
			State->Points[Result] = P;
			State->PointStatus[Result] |= PointTypes;
			++State->NumPoints;

			if(PointTypes & POINT_Intersection)
			{
				State->IntersectPoints[++State->LastIntersectPoint] = Result;
					++State->NumIntersectPoints;
				
			}

			if(PointTypes & POINT_Line)
			{
				++State->NumLinePoints;
				++State->LastLinePoint;
			}
		}
	}

	return Result;
}

UPDATE_AND_RENDER(UpdateAndRender)
{
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
	Assert(!State->OverflowTest);

	// Clear BG
	DrawRectangleFilled(ScreenBuffer, Origin, ScreenSize, WHITE);

	controller Keyboard = Input.New->Controllers[KEYBOARD];
	Keyboard;
	// TODO: move out of screen space
	mouse_state Mouse = Input.New->Mouse;

	if(Held(Keyboard.Button.DPadRight)) // 'S' on this computer
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
	Closest = ClosestPointIndex(State->Points, State->LastPoint, Mouse.P, &ClosestDistSq);
	b32 SnapIndex = 0;
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

	b32 MidEditing = State->NumLinePoints % 2;
	// TODO: fix the halftransitioncount - when using released(button), it fires twice per release
#define DEBUGClick(button) (IsInScreenBounds(ScreenBuffer, Mouse.P) &&  \
		Input.Old->Mouse.Buttons[button].EndedDown && !Input.New->Mouse.Buttons[button].EndedDown)
#define DEBUGPress(button) (Input.Old->Controllers[0].Button.button.EndedDown && !Input.New->Controllers[0].Button.button.EndedDown)
	if(DEBUGClick(LMB) && !State->DragIndex)
	{
		if(SnapIndex)
		{
			State->LinePoints[++State->LastLinePoint] = SnapIndex;
			++State->NumLinePoints;
		}
		else
		{
			State->Points[++State->LastPoint] = SnapMouseP;
			State->LinePoints[++State->LastLinePoint] = State->LastPoint;
			++State->NumPoints;
			++State->NumLinePoints;
		}

		// TODO: confirm this won't make points for duplicate lines
		if(MidEditing)
		{
			// NOTE: completed lines, set both points'
			State->PointStatus[State->LinePoints[State->LastLinePoint]]   &= POINT_Line;
			State->PointStatus[State->LinePoints[State->LastLinePoint-1]] &= POINT_Line;
		}
		// NOTE: ensures that the line is not improperly considered valid:
		State->LinePoints[State->LastLinePoint + 1] = 0;
	}

	// NOTE: put before next conditional so it doesn't turn itself off automatically.
	// TODO: Do I actually want to be able to drag points?
	if(State->DragIndex)
	{
		if(DEBUGClick(LMB))
		{
			// Set point to mouse location
			State->DragIndex = 0;
		}
		else if(DEBUGClick(RMB) || DEBUGPress(Start))
		{
			// Cancel dragging, point returns to saved location
			State->Points[State->DragIndex] = State->SavedPoint;
			State->DragIndex = 0;
		}
		else
		{
			State->Points[State->DragIndex] = Mouse.P; // Update dragged point to mouse location
		}
		// Snapping is off while dragging; TODO: maybe change this when points can be combined
		SnapIndex = 0;
	}
	else if(DEBUGClick(MMB) && SnapIndex && !MidEditing)
	{
		// Move point
		State->SavedPoint = State->Points[SnapIndex];
		State->DragIndex = SnapIndex;
	}

	if(DEBUGPress(Back))
	{
		ResetPoints(State);
	}
	// NOTE: only gets odd numbers if there's an unfinished point
	uint NumLines = (State->LastLinePoint)/2; // completed lines ... 1?
	v2 *Points = State->Points;
	uint *LinePoints = State->LinePoints;

	for(uint i = 1; i <= State->LastPoint; ++i)
	{
		DrawPoint(ScreenBuffer, Points[i], 0, LIGHT_GREY);
	}

#define LINE(lineI) Points[LinePoints[2*lineI-1]], Points[LinePoints[2*lineI]]
	if(State->LastLinePoint)
	{
		for(uint LineI = 1; LineI <= NumLines; ++LineI)
		{
			DEBUGDrawLine(ScreenBuffer, LINE(LineI), BLACK);
			DrawClosestPtOnSegment(ScreenBuffer, Mouse.P, LINE(LineI));

			if(LineI && !State->DragIndex)
			{
				// IMPORTANT TODO: spatially separate, maybe hierarchically
				// IMPORTANT TODO: don't recompute every frame, but don't create excess
				// IMPORTANT TODO: don't allow any duplicate points
				for(uint IntersectCheckI = 1; IntersectCheckI <= LineI; ++IntersectCheckI)
				{
					// NOTE: TODO? internal line between eg corners of square adds 1 intersection... sometimes?
					v2 Intersect;
					if(IntersectSegmentsWinding(LINE(LineI), LINE(IntersectCheckI), &Intersect))
					{
						AddPoint(State, Intersect, POINT_Intersection);
					}
				}
			}
		}
	}

	if(MidEditing)
	{
		// NOTE: Mid-way through drawing a line
		DrawCircleFill(ScreenBuffer, Points[LinePoints[State->LastLinePoint]], 3.f, LIGHT_GREY);
		CircleLine(ScreenBuffer, Points[LinePoints[State->LastLinePoint]], 5.f, LIGHT_GREY);
		DEBUGDrawLine(ScreenBuffer, Points[LinePoints[State->LastLinePoint]], SnapMouseP, LIGHT_GREY);
		if(DEBUGClick(RMB))
		{
			--State->LastLinePoint;
			--State->NumLinePoints;
			--State->LastPoint;
			--State->NumPoints;
		}
	}

	if(SnapIndex)
	{
		// NOTE: Overdraws...
		DrawPoint(ScreenBuffer, ClosestPoint, 1, BLUE);
	}

	CycleCountersInfo(ScreenBuffer, &State->DefaultFont);

	char Message[512];
	stbsp_sprintf(Message, "Frame time: %.2f, (%.2f, %.2f), Lines: %u, Intersections: %u",
			State->dt*1000.f, Mouse.P.X, Mouse.P.Y, NumLines, State->NumIntersectPoints);
	DrawString(ScreenBuffer, &State->DefaultFont, Message, 15, 10, 0, BLACK);
}

DECLARE_DEBUG_RECORDS;
DECLARE_DEBUG_FUNCTION
{
	/* debug_state *DebugState = Memory->DebugStorage; */
	int Offset = 0;
	if(1)//DebugState)
	{
		u32 HitI = 0;
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
				char TextBuffer[257];
				Offset +=
					stbsp_snprintf(TextBuffer, 256,
								   /* AltFormat ? AltFormat :*/ "%22s(%4d): %'12ucy %'8uh %'10ucy/h", 
								   Counter->FunctionName,
								   Counter->LineNumber,
								   CycleCount,
								   HitCount,
								   CycleCount / HitCount);
				/* TextBuffer[Offset] = '\n'; */
				f32 TextHeight = 13.f;
				DrawString(Buffer, Font, TextBuffer, TextHeight, 0, 150 - (HitI*TextHeight), BLACK);
				++HitI;
			}
		}
	}
}
