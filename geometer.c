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
	State->NumPoints     = 0;
	State->NumLinePoints = 0;
	State->MidEdit       = 0;
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
		if(Statuses[i] & PointTypes)
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
		if(V2Equals(P, State->Points[i]) && (State->PointStatus[i] & PointStatus))
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
// TODO: find free points, not just next
internal uint
AddPoint(state *State, v2 P, uint PointTypes)
{
	BEGIN_TIMED_BLOCK;
	uint Result = FindPointAtPos(State, P, ~(uint)POINT_Free);
	if(Result)
	{
		// NOTE: Use existing point, but add any new status
		State->PointStatus[Result] |= PointTypes;
	}

	else 
	{
		// TODO: extract into function? ExistingFreePoint
		for(uint PointIndex = 1; PointIndex <= State->LastPoint; ++PointIndex)
		{
			// NOTE: Use existing point if free
			// NOTE: this should be disappearing with dynamic allocation
			// if(State->LastPoint < ArrayCount(State->Points))
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
		State->PointStatus[Result] |= PointTypes;

	}

	if(PointTypes & POINT_Line)
	{
		++State->NumLinePoints;
		State->LinePoints[++State->LastLinePoint] = Result;
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

// TODO: implement undo/redo
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
	Assert(State->OverflowTest == 0);
	Assert(State->NumLinePoints % 2 == 0);

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
#define DEBUGPress(button) (Input.Old->Controllers[0].Button.button.EndedDown && !Input.New->Controllers[0].Button.button.EndedDown)

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
	
	else if(DEBUGClick(LMB))
	{
		// TODO: confirm this won't make points for duplicate lines
		if(!State->MidEdit)
		{
			// NOTE: Starting a line, save the first point
			State->SavedPoint = SnapMouseP;
			State->MidEdit = 1;
		}
		else
		{
			// NOTE: completed line, set both points' status if line does not already exist
			b32 ExistingLine = 0;
			uint PointAIndex = FindPointAtPos(State, State->SavedPoint, POINT_Line);
			if(PointAIndex)
			{
				for(uint LinePointI = 1; LinePointI <= State->NumLinePoints; ++LinePointI)
				{
					if(PointAIndex == State->LinePoints[LinePointI])
					{
						uint PointBLineIndex = MatchingPointIndex(LinePointI);

						uint TestPointIndex = FindPointAtPos(State, SnapMouseP, POINT_Line); 
						if(State->LinePoints[PointBLineIndex] == TestPointIndex)
						{
							ExistingLine = 1;
							break;
						}
					}
				}
			}
			if(!ExistingLine)
			{
				AddPoint(State, State->SavedPoint, POINT_Line);
				AddPoint(State, SnapMouseP, POINT_Line);
			}
			State->MidEdit = 0;
		}
		// NOTE: ensures that the line is not improperly considered valid:
		State->LinePoints[State->LastLinePoint + 1] = 0;
	}

	else if(SnapIndex && !State->MidEdit)
	{
		if(DEBUGClick(RMB))
		{
			if(State->PointStatus[SnapIndex] & POINT_Line)
			{
				// TODO: do I want this here or in drawing/adding..?
				for(uint i = 1; i <= State->LastLinePoint; ++i)
				{
					if(State->LinePoints[i] == SnapIndex)
					{
						// NOTE: invalidates line
						// TODO: invalidate both points or just one?
						State->LinePoints[i] = 0;
						State->LinePoints[MatchingPointIndex(i)] = 0;
					}
				}
			}
			// Invalidate point
			State->PointStatus[SnapIndex] = POINT_Free;
		}

		else if(DEBUGClick(MMB))
		{
		// Move point
		State->SavedPoint = State->Points[SnapIndex];
		State->DragIndex = SnapIndex;
		} 
	}

	else if(DEBUGPress(Back))
	{
		ResetPoints(State);
	}
	// NOTE: only gets odd numbers if there's an unfinished point
	uint NumLines = (State->LastLinePoint)/2; // completed lines ... 1?
	v2 *Points = State->Points;
	uint *LinePoints = State->LinePoints;

	for(uint i = 1; i <= State->LastPoint; ++i)
	{
		if(State->PointStatus[i] != POINT_Free)
		{
			DrawPoint(ScreenBuffer, Points[i], 0, LIGHT_GREY);
		}
	}

#define LINE(lineI) Points[LinePoints[2*lineI-1]], Points[LinePoints[2*lineI]]
	for(uint LineI = 1; LineI <= NumLines; ++LineI)
	{
		if(!(State->LinePoints[2*LineI-1] && State->LinePoints[2*LineI]))
		{
			continue;
		}

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

	if(State->MidEdit)
	{
		// NOTE: Mid-way through drawing a line
		DrawCircleFill(ScreenBuffer, State->SavedPoint, 3.f, LIGHT_GREY);
		CircleLine(ScreenBuffer, State->SavedPoint, 5.f, LIGHT_GREY);
		DEBUGDrawLine(ScreenBuffer, State->SavedPoint, SnapMouseP, LIGHT_GREY);
		if(DEBUGClick(RMB))
		{
			State->MidEdit = 0;
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
	stbsp_sprintf(Message, "LinePoints: %u, TypeLine: %u, MidEdit: %b\n\n"
				"\nFrame time: %.2f, (%.2f, %.2f)",
				State->NumLinePoints,
				NumPointsOfType(State->PointStatus, State->LastPoint, POINT_Line),
				State->MidEdit,
				State->dt*1000.f, Mouse.P.X, Mouse.P.Y);
	DrawString(ScreenBuffer, &State->DefaultFont, Message, TextSize, 10.f, TextSize, 1, BLACK);

	char LinePointInfo[512];
	stbsp_sprintf(LinePointInfo, "L#  P#\n\n");
	for(uint i = 1; i <= State->NumLinePoints && i <= 32; ++i)
	{
		stbsp_sprintf(LinePointInfo, "%s%02u  %02u\n", LinePointInfo, i, State->LinePoints[i]);
	}
	char PointInfo[512];
	stbsp_sprintf(PointInfo, " #  CIL\n\n");
	for(uint i = 1; i <= State->NumPoints && i <= 32; ++i)
	{
		stbsp_sprintf(PointInfo, "%s%02u  %03b\n", PointInfo, i, State->PointStatus[i]);
	}
	TextSize = 13.f;
	DrawString(ScreenBuffer, &State->DefaultFont, LinePointInfo, TextSize,
			ScreenSize.X - 180.f, ScreenSize.Y - 30.f, 0, BLACK);
	DrawString(ScreenBuffer, &State->DefaultFont, PointInfo, TextSize,
			ScreenSize.X - 80.f, ScreenSize.Y - 30.f, 0, BLACK);
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
								   /* AltFormat ? AltFormat :*/ "%24s(%4d): %'12ucy %'8uh %'10ucy/h", 
								   Counter->FunctionName,
								   Counter->LineNumber,
								   CycleCount,
								   HitCount,
								   CycleCount / HitCount);
				/* TextBuffer[Offset] = '\n'; */
				f32 TextHeight = 13.f;
				DrawString(Buffer, Font, TextBuffer, TextHeight, 0, Buffer->Height - ((HitI+2)*TextHeight), 0, GREY);
				++HitI;
			}
		}
	}
}
