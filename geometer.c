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
	for(uint i = 1; i < EndIndex; ++i)
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
		// NOTE: Point index 0 is reserved for null points (not defined in lines)
		State->PointIndex = 1;
		// TODO: make start at 1
		State->NumLinePoints = 0;
		InitArena(&Arena, (u8 *)Memory->PermanentStorage + sizeof(state), Memory->PermanentStorageSize - sizeof(state));

		Memory->IsInitialized = 1;
	}

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
	Closest = ClosestPointIndex(State->Points, State->PointIndex, Mouse.P, &ClosestDistSq);
	b32 Snapping = 0;
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
				Snapping = 1;
			}

		}
	}
	else
	{
		// TODO: ???
	}

	// TODO: fix the halftransitioncount - when using released(button), it fires twice per release
#define DEBUGClick(button) (PixelLocationInBuffer(ScreenBuffer, (size_t)Mouse.P.X, (size_t)Mouse.P.X) &&  \
		Input.Old->Mouse.Buttons[button].EndedDown && !Input.New->Mouse.Buttons[button].EndedDown)
	if(DEBUGClick(LMB))
	{
		State->Points[State->PointIndex] = SnapMouseP;
		State->LinePoints[State->NumLinePoints] = State->PointIndex;
		++State->PointIndex;
		++State->NumLinePoints;
		State->LinePoints[State->NumLinePoints] = 0;
		/* ClosestPoint = Mouse.P; */
		/* SnapMouseP = Mouse.P; */
	}

#define DEBUGPress(button) (Input.Old->Controllers[0].Button.button.EndedDown && !Input.New->Controllers[0].Button.button.EndedDown)
	if(DEBUGPress(Back))
	{
		State->PointIndex = 1;
		State->NumLinePoints = 0;
	}
	// NOTE: only gets even numbers if there's an unfinished point
	uint PointI = State->PointIndex;
	uint NumLinePoints = State->NumLinePoints;
	uint NumLines = NumLinePoints/2; // completed lines
	v2 *Points = State->Points;
	uint *LinePoints = State->LinePoints;
	line_points *Lines = State->Lines;

	for(uint i = 1; i < PointI; ++i)
	{
		DrawPoint(ScreenBuffer, Points[i], 0, LIGHT_GREY);
	}

#define LINE(lineI) Points[Lines[lineI].P1], Points[Lines[lineI].P2]
	if(NumLinePoints)
	{
		for(uint LineI = 0; LineI < NumLines; ++LineI)
		{
			/* Lines[LineI] = ZeroLineP; */
			/* Lines[LineI].P1 = 2 * LineI; */
			/* Lines[LineI].P1 = 2 * LineI + 1; */
			DEBUGDrawLine(ScreenBuffer, LINE(LineI), BLACK);
			DrawClosestPtOnSegment(ScreenBuffer, Mouse.P, LINE(LineI));

			if(LineI)
			{
				// IMPORTANT TODO: spatially separate, maybe hierarchically
				// IMPORTANT TODO: don't recompute every frame
				// IMPORTANT TODO: don't allow any duplicate points
				for(uint IntersectI = 0; IntersectI < LineI; ++IntersectI)
				{
					v2 Intersect;
					b32 LinesIntersected = IntersectSegmentsWinding(LINE(LineI),
							LINE(IntersectI), &Intersect);

					if(LinesIntersected)
					{
						b32 FoundMatch = 0;
						for(uint i = 1; i < PointI; ++i)
						{
							if(V2Equals(Intersect, Points[i]))
							{
								FoundMatch = 1;
							}
						}

						if(!FoundMatch)
						{
							State->Points[PointI] = Intersect;
							++State->PointIndex;
							PointI = State->PointIndex;
						}
					}
				}
			}
		}
	}

	if(NumLinePoints % 2)
	{
		// NOTE: Mid-way through drawing a line
		DrawCircleFill(ScreenBuffer, Points[LinePoints[NumLinePoints-1]], 3.f, LIGHT_GREY);
		CircleLine(ScreenBuffer, Points[LinePoints[NumLinePoints-1]], 5.f, LIGHT_GREY);
		DEBUGDrawLine(ScreenBuffer, Points[LinePoints[NumLinePoints-1]], SnapMouseP, LIGHT_GREY);
		if(DEBUGClick(RMB))
		{
			--State->NumLinePoints;
			--State->PointIndex;
			PointI = State->PointIndex;
		}
	}

	if(Snapping)
	{
		// NOTE: Overdraws...
		DrawPoint(ScreenBuffer, ClosestPoint, 1, BLUE);
	}

	CycleCountersInfo(ScreenBuffer, &State->DefaultFont);

	char Message[512];
	stbsp_sprintf(Message, "Frame time: %.2f, DistSq: %.2f",
			State->dt*1000.f, ClosestDistSq);
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
