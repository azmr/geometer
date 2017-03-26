#define _CRT_SECURE_NO_WARNINGS
#include "geometer.h"
#include <fonts.c>

internal void
DrawClosestPtOnSegment(image_buffer *ScreenBuffer, v2 P, v2 A, v2 B)
{
	v2 P1 = ClosestPtOnSegment(P, A, V2Sub(B, A));
	DrawCrosshair(ScreenBuffer, P1, 5.f, RED);
	/* DEBUGDrawLine(ScreenBuffer, P1, P, LIGHT_GREY); */
}

internal void
DrawPoint(image_buffer *ScreenBuffer, v2 P, colour Col)
{
	DrawCircleFill(ScreenBuffer, P, 3.f, Col);
	CircleLine(ScreenBuffer, P, 5.f, Col);
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
	// TODO: fix the halftransitioncount - when using released(button), it fires twice per release
#define DEBUGClick(button) (Input.Old->Mouse.Buttons[button].EndedDown && !Input.New->Mouse.Buttons[button].EndedDown)
	if(DEBUGClick(LMB))
	{
		State->Points[State->PointIndex] = Mouse.P;
		State->LinePoints[State->NumLinePoints] = State->PointIndex;
		++State->PointIndex;
		++State->NumLinePoints;
		State->LinePoints[State->NumLinePoints] = 0;
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
		DrawPoint(ScreenBuffer, Points[i], LIGHT_GREY);
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
		if(NumLinePoints % 2)
		{
			DrawCircleFill(ScreenBuffer, Points[LinePoints[NumLinePoints]], 3.f, LIGHT_GREY);
			CircleLine(ScreenBuffer, Points[PointI-1], 5.f, LIGHT_GREY);
			DEBUGDrawLine(ScreenBuffer, Points[PointI-1], Mouse.P, LIGHT_GREY);
		}
	}

	char Message[512];
	stbsp_sprintf(Message, "Frame time: %.2f",
			State->dt*1000.f);
	DrawString(ScreenBuffer, &State->DefaultFont, Message, 15, 10, 0, BLACK);
}

END_OF_DEBUG
