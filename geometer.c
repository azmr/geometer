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
// - F keys to open toolbars (layers/minimap/action history/...)

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
//	- ??? - move to background layer/another layer...
//	...

// CONTROLS: ////////////////////////////
#define C_Cancel       Keyboard.Esc
#define CB_StartShape  LMB
#define CB_Arc         LMB
#define CB_Line        RMB
#define CB_Length      LMB

#define C_StartShape   Mouse.CB_StartShape
#define C_Arc          Mouse.CB_Arc
#define C_Line         Mouse.CB_Line
#define C_Length       Mouse.CB_Length
// divide length       1-0
// mult length         Alt + 1-0
// get store length    a-z,A-Z
// set store length    Alt + a-z,A-Z

#define C_NoSnap       Keyboard.Shift
#define C_ShapeLock    Keyboard.Ctrl
#define C_PointOnly    Keyboard.Alt

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


internal inline void
DrawArcFromPoints(image_buffer *Buffer, v2 Centre, v2 A, v2 B, colour Colour)
{
	ArcLine(Buffer, Centre, Dist(Centre, A), V2Sub(A, Centre), V2Sub(B, Centre), Colour);
}

internal inline b32
SameAngle(v2 A, v2 B)
{
	b32 Result = WithinEpsilon(Dot(A, B), 1.f, POINT_EPSILON);
	return Result;
}

internal inline void
SimpleRedo(state *State)
{
	BEGIN_TIMED_BLOCK;
	Assert(State->iCurrentAction < State->iLastAction);
	ApplyAction(State, Pull(State->maActions, ++State->iCurrentAction));
	Assert(State->iCurrentAction <= State->iLastAction);
	END_TIMED_BLOCK;
}

internal void
SimpleUndo(state *State)
{
	BEGIN_TIMED_BLOCK;
	Assert(State->iCurrentAction > 0);
	action *Actions = State->maActions.Items;
	action Action = Actions[State->iCurrentAction];
	switch(USERIFY_ACTION(Action.Kind))
	{
		case ACTION_Reset:
		{ // reapply all actions from scratch
			// TODO: add checkpoints so it doesn't have to start right from beginning
			uint iCurrentAction = State->iCurrentAction;
			for(uint i = Action.i; i < iCurrentAction; ++i)
			{ ApplyAction(State, Actions[i]); }
		} break;

		case ACTION_RemovePt:
		{ Assert(Action.i == AddPoint(State, Action.po, Action.PointStatus, 0, ACTION_Point)); } break;

		case ACTION_Basis:
		{ // find the previous basis and apply that
			basis PrevBasis = DefaultBasis; // in case no previous basis found
			for(uint i = State->iCurrentAction; i > 0; --i)
			{
				if(Actions[i].Kind == ACTION_Basis)
				{
					PrevBasis = Actions[i].Basis;
					break;
				}
			}
			SetBasis(State, PrevBasis);
		} break;

		case ACTION_Segment:
		case ACTION_Circle:
		case ACTION_Arc:
		{
			Pull(State->maShapes, Action.i).Kind = SHAPE_Free;
			if(Action.i == State->iLastShape)
			{
				--State->iLastShape;
				PopDiscard(&State->maShapes);
			}
			else
			{ Assert(!"TODO: undo shape additions mid-array"); }
			// does anything actually need to be done?
		} break;

		case ACTION_Point:
		{
			Pull(State->maPointStatus, Action.i) = POINT_Free;
			if(Action.i == State->iLastPoint)
			{
				--State->iLastPoint;
				PopDiscard(&State->maPoints);
				PopDiscard(&State->maPointStatus);
			}
			else
			{ Assert(!"TODO: undo point additions mid-array"); }
			// does anything actually need to be done?
		} break;

		default:
		{
			Assert(!"Unknown/invalid action type");
		}
	}

	--State->iCurrentAction;
	Assert(State->iCurrentAction <= State->iLastAction);
#if INTERNAL && DEBUG_LOG_ACTIONS
	LogActionsToFile(State, "ActionLog.txt");
#endif
	END_TIMED_BLOCK;
}


internal inline void
UserUndo(state *State)
{
	do { SimpleUndo(State); }
	while(Pull(State->maActions, State->iCurrentAction).Kind > ACTION_NON_USER);
	// NOTE: shapes on screen need to be updated before this is called
	// (or you can accept an occasional frame of lag for points near the edge)
	RecalcNearScreenIntersects(State);
}
internal inline void
UserRedo(state *State)
{
	do { SimpleRedo(State); }
	while(Pull(State->maActions, State->iCurrentAction).Kind > ACTION_NON_USER);
	RecalcNearScreenIntersects(State);
}

#if 0
internal inline void
OffsetDraw(state *State, int Offset)
{
	uint iPrevDraw = State->iCurrentDraw;
	State->iCurrentDraw = iDrawOffset(State, Offset);
	State->cDraws += Offset;
	UpdateDrawPointers(State, iPrevDraw);
#if 1
	// NOTE: shapes on screen need to be updated before this is called
	// (or you can accept an occasional frame of lag)
	RecalcNearScreenIntersects(State);
#else
	RecalcAllIntersects(State);
#endif
}
#endif

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
	b32 IsSet = 0;
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
		if((cIntersects	  && DistSq(P, poIntersect1) < DistSq(P, *poClosest)) || !IsSet)
		{ *poClosest = poIntersect1; IsSet = 1; }
		if(cIntersects == 2 && DistSq(P, poIntersect2) < DistSq(P, *poClosest))
		{ *poClosest = poIntersect2; }
	}
	// only possibly true for first shape
	if(!IsSet) { *poClosest = ClosestPtOnCircle(P, TestFocus, TestRadius); }
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
	v2 poFocus = State->poSelect;
	f32 Radius = State->Length;
	v2 Result;
	if(ShapeLock)
	{
		uint cIntersects = ClosestPtIntersectingCircle(State->maPoints.Items, State->maShapes.Items,
				State->iLastShape, MouseP, poFocus, Radius, &Result);
		// TODO: remove (inc assert)
		/* if(cIntersects == 0) { Result = ClosestPtOnCircle(MouseP, poFocus, Radius); } */
		Assert(cIntersects || V2WithinEpsilon(Result, ClosestPtOnCircle(MouseP, poFocus, Radius), POINT_EPSILON));
		DebugReplace("cIntersects: %u\n", cIntersects);
	}
	else
	{
		v2 P = V2Equals(SnapMouseP, poFocus) ? MouseP : SnapMouseP;
		Result = ClosestPtOnCircle(P, poFocus, Radius);
	}
	Assert( ! V2WithinEpsilon(Result, poFocus, POINT_EPSILON));
	return Result;
}

internal void
DrawAABB(image_buffer *ScreenBuffer, aabb AABB, colour Col)
{
	v2 TopLeft     = V2(AABB.MinX, AABB.MaxY);
	v2 TopRight    = V2(AABB.MaxX, AABB.MaxY);
	v2 BottomLeft  = V2(AABB.MinX, AABB.MinY);
	v2 BottomRight = V2(AABB.MaxX, AABB.MinY);

	DEBUGDrawLine(ScreenBuffer, TopLeft,    TopRight,    Col);
	DEBUGDrawLine(ScreenBuffer, TopLeft,    BottomLeft,  Col);
	DEBUGDrawLine(ScreenBuffer, TopRight,   BottomRight, Col);
	DEBUGDrawLine(ScreenBuffer, BottomLeft, BottomRight, Col);
}

internal b32
CurrentActionIsByUser(state *State)
{
	action Action = Pull(State->maActions, State->iCurrentAction);
	b32 Result = Action.Kind < ACTION_NON_USER;
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

	memory_arena TempArena;
	InitArena(&TempArena, Memory->TransientStorage, Memory->TransientStorageSize);

	if(!Memory->IsInitialized)
	{
		InitArena(&Arena, (state *)Memory->PermanentStorage + 1, Memory->PermanentStorageSize - sizeof(state));

		State->iSaveAction = State->iCurrentAction;

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

	if(V2InvalidDir(BASIS.XAxis))
	{
		LOG("Invalid basis");
		BASIS.XAxis.X = 1.f;
		BASIS.XAxis.Y = 0.f;
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
	b32 IsSnapped;
	uint ipoClosest = 0;
	uint ipoClosestIntersect = 0;
	{ LOG("INPUT");
		Keyboard = Input.New->Keyboard;
		Mouse  = Input.New->Mouse;
		pMouse = Input.Old->Mouse;

		// TODO (ui): animate zoom (and pan?)
		// Pan with arrow keys
		b32 Down  = C_PanDown.EndedDown;
		b32 Up    = C_PanUp.EndedDown;
		b32 Left  = C_PanLeft.EndedDown;
		b32 Right = C_PanRight.EndedDown;
		f32 PanSpeed = 8.f * BASIS.Zoom;
		if(Down != Up)
		{
			if(Down)      { BASIS.Offset = V2Add(BASIS.Offset, V2Mult(-PanSpeed, Perp(BASIS.XAxis))); }
			else/*Up*/    { BASIS.Offset = V2Add(BASIS.Offset, V2Mult( PanSpeed, Perp(BASIS.XAxis))); }
		}

		if(Left != Right)
		{
			if(Left)      { BASIS.Offset = V2Add(BASIS.Offset, V2Mult(-PanSpeed,      BASIS.XAxis )); }
			else/*Right*/ { BASIS.Offset = V2Add(BASIS.Offset, V2Mult( PanSpeed,      BASIS.XAxis )); }
		}

		// Zoom with PgUp/PgDn
		b32 ZoomIn  = C_ZoomIn.EndedDown;
		b32 ZoomOut = C_ZoomOut.EndedDown;
		// TODO: Make these constants?
		if(ZoomIn != ZoomOut)
		{
			f32 ZoomFactor = 0.9f;
			f32 invZoomFactor = 1.f/ZoomFactor;
			if(ZoomIn)        BASIS.Zoom *=    ZoomFactor;
			else if(ZoomOut)  BASIS.Zoom *= invZoomFactor;
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
			}
			// NOTE: keep canvas under pointer in same screen location
			v2 RotatedOffset = V2RotateToAxis(BASIS.XAxis, V2Mult((1.f-ScrollFactor) * BASIS.Zoom, dMouseP));
			BASIS.Offset = V2Add(BASIS.Offset, RotatedOffset);

			// NOTE: wheel delta is in multiples of 120
			for(int i = 0; i < C_Zoom/120; ++i)
			{ BASIS.Zoom *= ScrollFactor; }
		}

		// SNAPPING
		v2 CanvasMouseP = V2ScreenToCanvas(BASIS, Mouse.P, ScreenCentre);
		{
			f32 ClosestDistSq;
			f32 ClosestIntersectDistSq;
			b32 ClosestPtOrIntersect = 0;
			SnapMouseP = CanvasMouseP;
			poClosest = CanvasMouseP;
			ipoClosest = ClosestPointIndex(State, CanvasMouseP, &ClosestDistSq);
			ipoClosestIntersect = ClosestIntersectIndex(State, CanvasMouseP, &ClosestIntersectDistSq);
			IsSnapped = 0;
			
			ClosestPtOrIntersect = ipoClosest || ipoClosestIntersect;
			DebugReplace("Pt: %u, Isct: %u, Drawing: %u\n", ipoClosest, ipoClosestIntersect, IsDrawing(State));
			if(ClosestPtOrIntersect || IsDrawing(State))
			{
				// decide whether to use point or intersect
				if(ipoClosest && ipoClosestIntersect)
				{
					if(ClosestDistSq <= ClosestIntersectDistSq)
					{ poClosest = POINTS(ipoClosest); }
					else
					{
						poClosest = Pull(State->maIntersects, ipoClosestIntersect);
						ClosestDistSq = ClosestIntersectDistSq;
					}
				}
				else if(ipoClosest)
				{ poClosest = POINTS(ipoClosest); }
				else if(ipoClosestIntersect)
				{
					poClosest = Pull(State->maIntersects, ipoClosestIntersect);
					ClosestDistSq = ClosestIntersectDistSq;
				}

				// compare against state-based temporary points
				// assumes that IsDrawingArc is implied by IsDrawing
				if(IsDrawing(State))
				{
					f32 SelectDistSq = DistSq(State->poSelect, CanvasMouseP);
					DebugAdd("Test: %f, Current: %f\n", SelectDistSq, ClosestDistSq);
					if( ! ClosestPtOrIntersect || SelectDistSq < ClosestDistSq)
					{
						poClosest = State->poSelect;
						DebugAdd("Adding poClosest: %.2f, %.2f\n", poClosest.X, poClosest.Y);
						ClosestDistSq = SelectDistSq;
					}
				}
				if(IsDrawingArc(State))
				{
					f32 ArcStartDistSq = DistSq(State->poArcStart, CanvasMouseP);
					DebugAdd("Test: %f, Current: %f\n", ArcStartDistSq, ClosestDistSq);
					// NOTE: very slight preference for arc start
					DebugAdd("poClosest: %.2f, %.2f\n", poClosest.X, poClosest.Y);
					if(ArcStartDistSq < ClosestDistSq)
					{
						poClosest = State->poArcStart;
						ClosestDistSq = ArcStartDistSq;
						DebugAdd("poClosest: %.2f, %.2f\n", poClosest.X, poClosest.Y);
					}
				}
				DebugAdd("poClosest: %.2f, %.2f\n", poClosest.X, poClosest.Y);
				/* gDebugPoint = poClosest; */

#define POINT_SNAP_DIST 5000.f
				// NOTE: BASIS->Zoom needs to be squared to match ClosestDistSq
				if(ClosestDistSq/(BASIS.Zoom * BASIS.Zoom) < POINT_SNAP_DIST)
				{ // closest point within range
					if( ! C_NoSnap.EndedDown)
					{ // point snapping still on
						SnapMouseP = poClosest;
						DebugAdd("SnapMouseP: %.2f, %.2f\n", SnapMouseP.X, SnapMouseP.Y);
						IsSnapped = 1;
					}
				}
			}
			if(C_ShapeLock.EndedDown)
			{
				for(uint iShape = 1; iShape <= State->iLastShape; ++iShape)
				{
					v2 TestP = ZeroV2;
					shape Shape = Pull(State->maShapes, iShape);
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
		}

		// TODO IMPORTANT (fix): stop unwanted clicks from registering. e.g. on open/save
		// TODO: fix the halftransitioncount - when using released(button), it fires twice per release
		b32 MouseInScreenBounds = IsInScreenBounds(ScreenBuffer, Mouse.P); 
#define DEBUGClick(button) (MouseInScreenBounds && DEBUGPress(button))
#define DEBUGRelease(button) (Input.Old->button.EndedDown && !Input.New->button.EndedDown)
#define DEBUGPress(button)   (!Input.Old->button.EndedDown && Input.New->button.EndedDown)

		if(DEBUGPress(C_DebugInfo)) // toggle debug info
		{ State->ShowDebugInfo = !State->ShowDebugInfo; }

		if(DEBUGPress(C_CanvasHome))
		{ // reset canvas position
			BASIS.Offset = ZeroV2;
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
		if(DEBUGPress(Keyboard.key)) { \
			Assert(index >= 0 && index < ArrayCount(State->LengthStores)); \
			if(Keyboard.Alt.EndedDown) { State->LengthStores[index] = State->Length; } \
			else if(State->LengthStores[index] > 0.f && \
					State->LengthStores[index] != State->Length) { \
				State->pLength = State->Length; \
				State->Length = State->LengthStores[index]; \
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
			/*		 // Set point to mouse location and recompute intersections */
			/*		 State->ipoDrag = 0; */
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
			BASIS.Offset = V2Add(BASIS.Offset,
				V2Sub(V2ScreenToCanvas(BASIS, pMouse.P, ScreenCentre),
					  V2ScreenToCanvas(BASIS,  Mouse.P, ScreenCentre)));
			File.Pan = 1;
		}

		else if(DEBUGPress(C_Cancel) && State->InputMode != MODE_Normal)
		{ State->InputMode = MODE_Normal; }

		else if(DEBUGPress(Keyboard.F1))
		{ State->ShowHelpInfo = !State->ShowHelpInfo; }

		else
		{
			// TODO (opt): jump straight to the right mode.
			switch(State->InputMode)
			{
				case MODE_Normal:
				{
					if(State->iCurrentAction)
					{ Assert(CurrentActionIsByUser(State)); }

					if(Keyboard.Ctrl.EndedDown)
					{ // file actions
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
							(State->iCurrentAction > 0))
					{ UserUndo(State); } // UNDO
					if(((Keyboard.Ctrl.EndedDown && DEBUGPress(Keyboard.Y)) ||
						(Keyboard.Ctrl.EndedDown && Keyboard.Shift.EndedDown && DEBUGPress(Keyboard.Z))) &&
						(State->iCurrentAction < State->iLastAction))
					{ UserRedo(State); } // REDO

					if(C_BasisMod.EndedDown && DEBUGClick(C_BasisSet))
					{
						State->poSaved = SnapMouseP;
						State->InputMode = MODE_SetBasis;
					}

					else if(DEBUGClick(C_StartShape))
					{ // start drawing arc
						// NOTE: Starting a shape, save the first point
						State->poSelect = SnapMouseP;
						if(C_PointOnly.EndedDown)
						{ State->InputMode = MODE_QuickPtOrSeg; }
						else
						{ State->InputMode = MODE_SetLength; }
					}

					else if(DEBUGPress(C_Reset))
					{ // reset canvas
						/* if(Keyboard.Alt.EndedDown) */
						Reset(State, 0);
						// TODO (ui): reset to save point
						/* else */
						/* { Reset(State, State->iSaveAction); } */
						
						DebugClear();
					}
					{// unwanted?

						// TODO: could skip check and just write to invalid point..?
						/* else if(ipoSnap) */
						/* { // point snapped to */
						/*	 if(DEBUGPress(C_Delete)) */
						/*	 { */
						/*		 // TODO: deleting points */
						/*		 InvalidatePoint(State, ipoSnap); */
						/*	 } */
						/* } */

						/* if(DEBUGClick(MMB)) */
						/* { */
						// MOVE POINT
						/* State->SavedPoint = State->Points[ipoSnap]; */
						/* State->ipoSelect = ipoSnap; */
						/* State->ipoDrag = ipoSnap; */
						/* } */ 
					}
				} break;


				case MODE_SetBasis:
				{ // rotate canvas basis
					// TODO (UI): if MMB(?) held, set zoom as well - box aligned to new axis showing where screen will end up
					if(!C_BasisSet.EndedDown)
					{ // set basis on release
						if( ! V2Equals(SnapMouseP, State->poSaved)) // prevents XAxis with no length
						{ // set new basis and start animation
							basis NewBasis = State->Basis;
							NewBasis.XAxis = V2Sub(SnapMouseP, State->poSaved);
							SetBasis(State, NewBasis);
						}

						State->InputMode = MODE_Normal;
					}
					Assert(CurrentActionIsByUser(State)); 
				} break;


				case MODE_SetLength:
				{
					if(!C_Length.EndedDown)
					{ // set length on release
						// TODO (optimize): use DistSq
						f32 Length = Dist(State->poSelect, SnapMouseP);
						if(Length > POINT_EPSILON)
						{
							State->pLength = State->Length;
							State->Length = Length;
						}
						// TODO (UI): do I want it automatically continuing to draw here?
						State->InputMode = MODE_Draw;
						goto case_mode_draw;
					}
					Assert(CurrentActionIsByUser(State)); 
				} break;


				case MODE_QuickPtOrSeg:
				{
					if( ! C_StartShape.EndedDown) // LMB released
					{ // draw quick segment or point
						if(V2WithinEpsilon(State->poSelect, SnapMouseP, POINT_EPSILON))
						{ AddPoint(State, State->poSelect, POINT_Extant, 0, ACTION_Point); }
						else // draw quick seg
						{ AddSegmentAtPoints(State, State->poSelect, SnapMouseP); }
						State->InputMode = MODE_Normal;
					}
					Assert(CurrentActionIsByUser(State)); 
				} break;


				case MODE_Draw:
				{ // start drawing line
case_mode_draw:
					// TODO (opt): there is a 1 frame lag even if pressed and released within 1...
					Assert(IsDrawing(State));
					poAtDist = ChooseCirclePoint(State, CanvasMouseP, SnapMouseP, C_ShapeLock.EndedDown);

					if(DEBUGClick(C_Line))
					{
						if(V2WithinEpsilon(State->poSelect, SnapMouseP, POINT_EPSILON))
						// NOTE: don't  want to extend a line with no direction!
						// leave a point and return to normal mode
						{
							if(C_PointOnly.EndedDown)
							{
								AddPoint(State, State->poSelect, POINT_Extant, 0, ACTION_Point); 
								State->InputMode = MODE_Normal;
							}
							else
							{ State->InputMode = MODE_SetPerp; }
						}
						else
						{ // extend segment, possibly through perpendicular
							// TODO (UI): this should be based on shape snapping rather than intersection
							v2 poAtLength = ClosestPtOnCircle(SnapMouseP, State->poSelect, State->Length);
							AddIntersection(State, poAtLength);

							if(V2Equals(State->PerpDir, ZeroV2)) // perpendicular not set
							{ State->poSaved = SnapMouseP; }     // set current mouse as point to extend through
							else                                 // extend through the perpendicular
							{ State->poSaved = V2Add(State->poSelect, State->PerpDir); }
							State->PerpDir = ZeroV2;

							State->InputMode = MODE_ExtendSeg;
						}
						Assert(CurrentActionIsByUser(State)); 
					}

					else if(DEBUGClick(C_Arc))
					{ // start drawing arc/circle
						// TODO: REMOVE
						poAtDist = ChooseCirclePoint(State, CanvasMouseP, SnapMouseP, C_ShapeLock.EndedDown);
						v2 poFocus = State->poSelect;
						f32 Radius = State->Length;
						if(V2WithinEpsilon(poAtDist, poFocus, POINT_EPSILON))
						{
							// TODO (UI): better point to default radius to? follow mouse direction?
							v2 poRad = V2(poFocus.X + Radius, poFocus.Y);
							Assert( ! V2Equals(poRad, poFocus));
							AddCircleAtPoints(State, poFocus, poRad);
							State->InputMode = MODE_Normal;
						}
						else
						{
							State->poArcStart = poAtDist;
							State->InputMode = MODE_ExtendArc;
						}
						State->PerpDir = ZeroV2;
						Assert(CurrentActionIsByUser(State)); 
					}
				} break;


				case MODE_SetPerp:
				{
					if(V2WithinEpsilon(State->poSelect, SnapMouseP, POINT_EPSILON)) // mouse on seg start
					{ State->PerpDir = ZeroV2; }
					else // set perpendicular
					{ State->PerpDir = Perp(V2Sub(SnapMouseP, State->poSelect)); }

					if( ! C_Line.EndedDown)
					{ State->InputMode = MODE_Draw; }
					Assert(CurrentActionIsByUser(State)); 
				} break;


				// TODO: only draw the smaller arc if not gone around the focus
				// use coord system based on focus to first point and its perp
				case MODE_ExtendArc:
				{
					Assert(IsDrawingArc(State));
					poAtDist = ChooseCirclePoint(State, CanvasMouseP, SnapMouseP, C_ShapeLock.EndedDown);

					// NOTE: Not using button released in case it's missed for some reason
					// also possible this fires at a weird time when focus returns or something...
					if(!C_Arc.EndedDown)
					{ // finish drawing arc/circle
						v2 poFocus    = State->poSelect;
						v2 poArcStart = State->poArcStart;
						// TODO IMPORTANT (fix): don't add when no shape intersected
						if(V2WithinEpsilon(SnapMouseP, poFocus, POINT_EPSILON) ||
						   V2WithinEpsilon(poAtDist, State->poArcStart, POINT_EPSILON))
						// Same angle -> full circle
						{
							if(C_PointOnly.EndedDown)
							{ // add point intersecting shape 
								AddPoint(State, poFocus,  POINT_Extant, 0, ACTION_NonUserPoint);
								AddPoint(State, poAtDist, POINT_Extant, 0, ACTION_Point);
							}
							else
							{ AddCircleAtPoints(State, poFocus, State->poArcStart); }
						}
						else
						{
							if(C_PointOnly.EndedDown)
							{ // add point intersecting shape 
								AddPoint(State, poFocus,    POINT_Extant, 0, ACTION_NonUserPoint);
								AddPoint(State, poArcStart, POINT_Extant, 0, ACTION_NonUserPoint);
								AddPoint(State, poAtDist,   POINT_Extant, 0, ACTION_Point);
							}
							else
							{ AddArcAtPoints(State, poFocus, State->poArcStart, poAtDist); }
						}
						State->InputMode = MODE_Normal;
						Assert(CurrentActionIsByUser(State)); 
					}
				} break;


				case MODE_ExtendSeg:
				{ // find point on shape closest to mouse along line
					// TODO (fix): preview point pulling away from shape
					v2 TestStart = State->poSelect; 
					v2 poExtend = State->poSaved;
					v2 TestDir = V2Sub(poExtend, TestStart);
					uint cIntersects = 0;
					if(C_ShapeLock.EndedDown)
					{
						cIntersects =
							ClosestPtIntersectingLine(State->maPoints.Items, State->maShapes.Items,
									State->iLastShape, CanvasMouseP, TestStart, TestDir, &poOnLine);
						if(cIntersects == 0)  { poOnLine = ClosestPtOnLine(CanvasMouseP, TestStart, TestDir); }
					}
					else
					{ poOnLine = ClosestPtOnLine(SnapMouseP, TestStart, TestDir); }

					if( ! C_Line.EndedDown)
					{ // add point along line (and maybe add segment)
						// NOTE: remove temporary intersection at user length
						PopDiscard(&State->maIntersects);
						if(C_PointOnly.EndedDown)
						{
							AddPoint(State, State->poSelect, POINT_Extant, 0, ACTION_NonUserPoint);
							AddPoint(State, poOnLine,        POINT_Extant, 0, ACTION_Point);
						}
						else
						{ AddSegmentAtPoints(State, State->poSelect, poOnLine); }
						State->InputMode = MODE_Normal;
					}
					Assert(CurrentActionIsByUser(State)); 
				} break;

				default:
				{
					// TODO (feature): error handle? reset flags and return to normal mode
					Assert(0);
				}
			}
		}
	}

	basis Basis;
	{ LOG("CALCULATE ANIMATED BASIS")
		basis EndBasis = BASIS;
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
		tBasis = Clamp01(tBasis);
#else
		// TODO: investigate why tBasis goes wildly out of bounds
		// e.g. -174660.406 (is it only during debug?)
		Assert(tBasis >= 0.f);
		Assert(tBasis <= 1.f);
#endif

#if 1
		Basis = BasisLerp(StartBasis, tBasis, EndBasis);
#else
		Basis = BasisLerp(pBASIS, State->tBasis, BASIS);
#endif
	}

	shape_arena *maShapesNearScreen = &State->maShapesNearScreen;
	v2_arena *maPointsOnScreen      = &State->maPointsOnScreen;
	maShapesNearScreen->Used = 0;
	maPointsOnScreen->Used = 0;
	uint cShapesNearScreen = 0;
	uint cPointsOnScreen = 0;
	{ LOG("CULL");
	/////////////////////
		uint iLastShape = State->iLastShape;
		uint iLastPoint = State->iLastPoint;
		aabb ScreenBB;
#if 1
		ScreenBB.MinX = 0.f;
		ScreenBB.MinY = 0.f;
		ScreenBB.MaxX = ScreenSize.X;
		ScreenBB.MaxY = ScreenSize.Y;
#else // use a smaller screen
		v2 FakeScreenMin = V2Mult(0.2f, ScreenSize);
		v2 FakeScreenMax = V2Mult(0.8f, ScreenSize);
		ScreenBB.MinX = FakeScreenMin.X;
		ScreenBB.MinY = FakeScreenMin.Y;
		ScreenBB.MaxX = FakeScreenMax.X;
		ScreenBB.MaxY = FakeScreenMax.Y;
		DrawAABB(ScreenBuffer, ScreenBB, GREEN);
#endif

		v2 *Points = State->maPoints.Items;
		shape *Shapes = State->maShapes.Items;
		for(uint iShape = 1; iShape <= iLastShape; ++iShape)
		{
			shape Shape = Shapes[iShape];
			// create local copy of shape with basis applied
			shape LocalShape = Shape;
			v2 ShapePoints[NUM_SHAPE_POINTS];
			for(uint i = 0; i < NUM_SHAPE_POINTS; ++i)
			{
				// TODO (opt): init all shape points to 0 so I don't need to check
				// that point is inside mem bounds?
				uint ipo = Shape.P[i];
				v2 P = ipo <= iLastPoint ? Points[ipo] : ZeroV2;
				ShapePoints[i] = V2CanvasToScreen(Basis, P, ScreenCentre);
				LocalShape.P[i] = i;
			}
			aabb ShapeBB = AABBFromShape(ShapePoints, LocalShape);
			/* DrawAABB(ScreenBuffer, ShapeBB, ORANGE); */
			if(AABBOverlaps(ScreenBB, ShapeBB)) // shape BB on screen
			{ // add shape to array of shapes on screen
				Push(maShapesNearScreen, Shape);
				++cShapesNearScreen;
			}
		}
		Assert(cShapesNearScreen == State->maShapesNearScreen.Used/sizeof(shape));

		for(uint ipo = 1; ipo <= iLastPoint; ++ipo)
		{
			v2 P = V2CanvasToScreen(Basis, Points[ipo], ScreenCentre);
			if(PointInAABB(P, ScreenBB) && POINTSTATUS(ipo) != POINT_Free)
			{
				Push(maPointsOnScreen, P);
				++cPointsOnScreen;
			}
		}
		Assert(cPointsOnScreen == State->maPointsOnScreen.Used/sizeof(v2));
	}

	{ LOG("RENDER");
	////////////////
		DrawCrosshair(ScreenBuffer, ScreenCentre, 5.f, LIGHT_GREY);
		v2 SSSnapMouseP = V2CanvasToScreen(Basis, SnapMouseP, ScreenCentre);

#if 0
		DrawCrosshair(ScreenBuffer, ScreenCentre, 20.f, RED);
		DEBUGDrawLine(ScreenBuffer, ScreenCentre,
			V2Add(ScreenCentre, V2Mult(50.f, V2CanvasToScreen(Basis, V2(1.f, 0.f), ScreenCentre))), CYAN);
#endif

		v2 *Points = State->maPoints.Items;
		LOG("\tDRAW SHAPES");
		for(uint iShape = 0; iShape < cShapesNearScreen; ++iShape)
		{
			shape Shape = Pull(*maShapesNearScreen, iShape);
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
					Assert(Radius);
					CircleLine(ScreenBuffer, poFocus, Radius, BLACK);
					if(State->ShowDebugInfo)
					{ DrawClosestPtOnCircle(ScreenBuffer, Mouse.P, poFocus, Radius); }
				} break;

				case SHAPE_Arc:
				{
					v2 poFocus = V2CanvasToScreen(Basis, Points[Shape.Arc.ipoFocus], ScreenCentre);
					v2 poStart = V2CanvasToScreen(Basis, Points[Shape.Arc.ipoStart], ScreenCentre);
					v2 poEnd   = V2CanvasToScreen(Basis, Points[Shape.Arc.ipoEnd],   ScreenCentre);
					DrawArcFromPoints(ScreenBuffer, poFocus, poStart, poEnd, BLACK); 
					if(State->ShowDebugInfo)
					{ DrawClosestPtOnArc(ScreenBuffer, Mouse.P, poFocus, poStart, poEnd); }
				} break;
			}
		}

		LOG("\tDRAW POINTS");
		for(uint i = 0; i < cPointsOnScreen; ++i)
		{
			// NOTE: basis already applied
			DrawCircleFill(ScreenBuffer, Pull(*maPointsOnScreen, i), 3.f, LIGHT_GREY);
		}

		v2 poSSClosest = V2CanvasToScreen(Basis, poClosest, ScreenCentre);
		if(State->iLastPoint)  { CircleLine(ScreenBuffer, poSSClosest, 5.f, GREY); }
		if(IsSnapped)
		{ // draw snapped point
			DrawCircleFill(ScreenBuffer, poSSClosest, 3.f, BLUE); 
			// NOTE: Overdraws...
			DrawActivePoint(ScreenBuffer, poSSClosest, BLUE);
		}


		LOG("\tDRAW PREVIEW");
		// TODO (UI): animate previews in and out by smoothstepping alpha over a few frames
		// so that they don't pop too harshly when only seen briefly
		// TODO QUICK (UI): when mid basis-change, use that value rather than the new one...
		f32 SSLength = State->Length/BASIS.Zoom; 
		v2 poSelect = State->poSelect;
		v2 poSSSelect = V2CanvasToScreen(Basis, State->poSelect, ScreenCentre);
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


			case MODE_QuickPtOrSeg:
			{
				DEBUGDrawLine(ScreenBuffer, poSSSelect, SSSnapMouseP, BLUE);
				DrawActivePoint(ScreenBuffer, poSSSelect, RED);
			} break;


			case MODE_Draw:
			{
				v2 poSSEnd = SSSnapMouseP;
				v2 poSSAtDist = V2CanvasToScreen(Basis, poAtDist, ScreenCentre);
				if( ! V2Equals(State->PerpDir, ZeroV2))
				{ // draw perpendicular segment
					v2 poSSDir = V2CanvasToScreen(Basis, V2Add(poSelect, State->PerpDir), ScreenCentre);
					poSSEnd = ExtendSegment(poSSSelect, poSSDir, SSSnapMouseP);
				}
				// preview circle at given length and segment
				CircleLine(ScreenBuffer, poSSSelect, SSLength, BLUE);
				DrawActivePoint(ScreenBuffer, poSSAtDist, RED);
				DrawActivePoint(ScreenBuffer, poSSSelect, RED);
				DEBUGDrawLine(ScreenBuffer, poSSSelect, poSSEnd, BLUE);
			} break;


			case MODE_ExtendArc:
			{ // preview drawing arc
				DrawActivePoint(ScreenBuffer, poSSSelect, RED);
				LOG("\tDRAW HALF-FINISHED ARC");
				v2 poStart = State->poArcStart;
				v2 poSSStart = V2CanvasToScreen(Basis, poStart, ScreenCentre);
				DEBUGDrawLine(ScreenBuffer, poSSSelect, poSSStart, LIGHT_GREY);
				if(V2WithinEpsilon(poStart, poAtDist, POINT_EPSILON))
				{
					CircleLine(ScreenBuffer, poSSSelect, SSLength, BLACK);
				}
				else
				{
					v2 poSSAtDist = V2CanvasToScreen(Basis, poAtDist, ScreenCentre);
					DrawArcFromPoints(ScreenBuffer, poSSSelect, poSSStart, poSSAtDist, BLACK);
					DEBUGDrawLine(ScreenBuffer, poSSSelect, poSSAtDist, LIGHT_GREY);
				}
			} break;


			case MODE_ExtendSeg:
			{ // preview extending a line
				v2 poSSDir = V2CanvasToScreen(Basis, State->poSaved, ScreenCentre);
				v2 poSSOnLine = V2CanvasToScreen(Basis, poOnLine, ScreenCentre);
				DrawFullScreenLine(ScreenBuffer, poSSSelect, V2Sub(poSSDir, poSSSelect), LIGHT_GREY);
				if(State->InputMode == MODE_ExtendSeg)
				{ DEBUGDrawLine(ScreenBuffer, poSSSelect, poSSOnLine, BLACK); }
				CircleLine(ScreenBuffer, poSSSelect, SSLength, LIGHT_GREY);
				DrawActivePoint(ScreenBuffer, poSSOnLine, RED);
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
		}

		uint LastIntersect = (uint)ArenaCount(State->maIntersects, v2) - 1;
		for(uint i = 1; i <= LastIntersect; ++i)
		{
			/* v2 poSS = V2CanvasToScreen(Basis, *PullEl(State->maIntersects, i, v2), ScreenCentre); */
			/* DrawActivePoint(ScreenBuffer, poSS, ORANGE); */
		}


		if(!V2Equals(gDebugV2, ZeroV2))
		{ // draw debug vector
			DEBUGDrawLine(ScreenBuffer, ScreenCentre, V2Add(ScreenCentre, gDebugV2), ORANGE);
		}
		if(!V2Equals(gDebugPoint, ZeroV2))
		{ // draw debug point
			if(IsDrawing(State))
				DrawActivePoint(ScreenBuffer, V2CanvasToScreen(Basis, gDebugPoint, ScreenCentre), ORANGE);
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

		/* CycleCountersInfo(ScreenBuffer, &State->DefaultFont); */

		// TODO: Highlight status for currently selected/hovered points

		char Message[512];
		TextSize = 15.f;
		ssprintf(Message, //"LinePoints: %u, TypeLine: %u, Esc Down: %u"
				"\nFrame time: %.2fms, "
				"Mouse: (%3.f, %3.f), "
				/* "Request: {As: %u, Action: %s, Pan: %u}" */
				/* "Basis: (%.2f, %.2f), " */
				/* "Char: %d (%c), " */
				/* "Mode: %s, " */
				/* "draw (iC/c/iL/iS): %u/%u/%u/%u, " */
				"actions (iC/iL/iS): %u/%u/%u, "
				/* "pBasis: (%.2f, %.2f)" */
				/* "Draw Index: %u" */
				/* "Offset: (%.2f, %.2f), " */
				"iLastPoint: %u, "
				"iLastShape: %u"
				,
				/* State->cLinePoints, */
				/* NumPointsOfType(State->PointStatus, State->iLastPoint, POINT_Line), */
				/* C_Cancel.EndedDown, */
				State->dt*1000.f,
				Mouse.P.X, Mouse.P.Y,
				/* File.NewWindow, FileActionText[File.Action], File.Pan, */
				/* BASIS->XAxis.X, BASIS->XAxis.Y, */
				/* testcharindex + 65, testcharindex + 65, */
				/* InputModeText[State->InputMode], */
				/* State->pBasis.XAxis.X, State->pBasis.XAxis.Y, */
				/* State->iCurrentDraw, State->cDraws, State->iLastDraw, State->iSaveDraw, */
				State->iCurrentAction, State->iLastAction, State->iSaveAction,
				/* BASIS->Offset.X, BASIS->Offset.Y, */
				State->iLastPoint,
				State->iLastShape
				);
		DrawString(ScreenBuffer, &State->DefaultFont, Message, TextSize, 10.f, TextSize, 1, BLACK);

#if 1
		TextSize = 13.f;
		char TextInfoBuffer[512];
		*TextInfoBuffer = 0;
		/* ssprintf(TextInfoBuffer, "L#  P#\n\n"); */
		/* for(uint i = 1; i <= State->iLastShape && i <= 32; ++i) */
		/* { */
		/* 	ssprintf(TextInfoBuffer, "%s%02u  %04b\n", TextInfoBuffer, i, SHAPES(i).Kind); */
		/* } */
		/* DrawString(ScreenBuffer, &State->DefaultFont, TextInfoBuffer, TextSize, */
		/* 		ScreenSize.X - 180.f, ScreenSize.Y - 30.f, 0, BLACK); */

		ssprintf(TextInfoBuffer, " # DARTFILE\n\n");
		for(uint i = 1; i <= State->iLastPoint && i <= 32; ++i)
		{
			ssprintf(TextInfoBuffer, "%s%02u %08b\n", TextInfoBuffer, i, POINTSTATUS(i));
		}
		DrawString(ScreenBuffer, &State->DefaultFont, TextInfoBuffer, TextSize,
				ScreenSize.X - 120.f, ScreenSize.Y - 30.f, 0, BLACK);

		*TextInfoBuffer = 0;
		for(uint i = 1; i <= State->iLastPoint && i <= 32; ++i)
		{
			v2 po = Pull(State->maPoints, i);
			ssprintf(TextInfoBuffer, "%s%02u (%f, %f)\n", TextInfoBuffer, i, po.X, po.Y);
		}
		DrawString(ScreenBuffer, &State->DefaultFont, TextInfoBuffer, TextSize,
				ScreenSize.X - 320.f, ScreenSize.Y - 4.5f*TextSize, 0, BLACK);
#endif
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
