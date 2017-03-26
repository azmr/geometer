#define _CRT_SECURE_NO_WARNINGS
#include "geometer.h"
#include <fonts.c>

internal void
DrawClosestPtOnSegment(image_buffer *ScreenBuffer, v2 P, v2 A, v2 B)
{
	v2 P1 = ClosestPtOnSegment(P, A, V2Sub(B, A));
	DrawCrosshair(ScreenBuffer, P1, 5.f, RED);
	DEBUGDrawLine(ScreenBuffer, P1, P, LIGHT_GREY);
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
		State->PointsIndex = 0;
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
		State->Points[State->PointsIndex] = Mouse.P;
		++State->PointsIndex;
	}
#define DEBUGPress(button) (Input.Old->Controllers[0].Button.button.EndedDown && !Input.New->Controllers[0].Button.button.EndedDown)
	if(DEBUGPress(Back))
	{
		State->PointsIndex = 0;
	}
	// NOTE: only gets even numbers if there's an unfinished point
	uint PointI = State->PointsIndex;
	v2 *Points = State->Points;
	if(PointI)
	{
		for(uint LineI = 0; LineI < PointI-1; LineI+=2)
		{
			DEBUGDrawLine(ScreenBuffer, Points[LineI], Points[LineI+1], BLACK);
			DrawClosestPtOnSegment(ScreenBuffer, Mouse.P, Points[LineI], Points[LineI+1]);

			if(LineI > 1)
			{
				// IMPORTANT TODO: spatially separate, maybe hierarchically
				// IMPORTANT TODO: don't recompute every frame
				for(uint IntersectI = 0; IntersectI < LineI-1; IntersectI+=2)
				{
					v2 Intersect;
					b32 LinesIntersected = IntersectSegmentsWinding(Points[LineI], Points[LineI+1],
							Points[IntersectI], Points[IntersectI+1], &Intersect);
					if(LinesIntersected)
					{
						DrawCircleFill(ScreenBuffer, Intersect, 3.f, BLUE);
						CircleLine(ScreenBuffer, Intersect, 5.f, BLUE);
					}
				}
			}
		}
		if(PointI % 2)
		{
			DrawCircleFill(ScreenBuffer, Points[PointI-1], 3.f, LIGHT_GREY);
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
