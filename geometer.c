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
// set diameter with L-click + drag
//
// M-click/Space+L-click + drag -> move canvas
// Scroll -> zoom

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
// - Animate between offsets on undo/redo so that you can keep track of where you are
// - Resizable windo (maintain centre vs maintain absolute position)
// - New file w/ ctrl-n
// - Extendable lines
// - Mark point on line at distance
// - Break up current line into n parts

// CONTROLS: ////////////////////////////
#define C_Cancel       Keyboard.Esc
#define C_StartShape   Mouse.LMB
#define C_Arc          Mouse.LMB
#define C_PointOnShape Mouse.LMB
#define C_Line         Mouse.RMB
#define C_Length       Mouse.RMB
// divide length       1-0
// mult length         Shift + 1-0
// get store length    a-z,A-Z
// set store length    Alt + a-z,A-Z

#define C_BasisSet     Mouse.LMB
#define C_BasisMod     Keyboard.Alt
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
	/* DEBUGDrawLine(ScreenBuffer, po1, po, LIGHT_GREY); */
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

internal inline void
SaveUndoState(state *State)
{
	BEGIN_TIMED_BLOCK;
	uint iPrevDraw = State->iCurrentDraw;
	State->iCurrentDraw = iDrawOffset(State, 1);
	// NOTE: prevents redos
	State->iLastDraw = State->iCurrentDraw;

	draw_state *Draw = State->Draw;
	CopyArenaContents(Draw[iPrevDraw].maPoints, &Draw[State->iCurrentDraw].maPoints);
	CopyArenaContents(Draw[iPrevDraw].maShapes, &Draw[State->iCurrentDraw].maShapes);
	CopyArenaContents(Draw[iPrevDraw].maPointStatus, &Draw[State->iCurrentDraw].maPointStatus);
	Draw[State->iCurrentDraw].Basis = Draw[iPrevDraw].Basis;
	
	UpdateDrawPointers(State, iPrevDraw);

	++State->cDraws;
	State->Modified = 1;
	END_TIMED_BLOCK;
}

internal void
Reset(state *State)
{
	BEGIN_TIMED_BLOCK;
	for(uint i = 1; i <= State->iLastPoint; ++i)
	{
		POINTSTATUS(i) = POINT_Free;
	}
	// NOTE: Point index 0 is reserved for null points (not defined in lines)
	DRAW_STATE.maPoints.Used  = sizeof(v2);
	DRAW_STATE.maPointStatus.Used  = sizeof(u8);
	DRAW_STATE.maShapes.Used  = sizeof(shape);
	UpdateDrawPointers(State, 1);

	/* State->cPoints  = 0; */
	/* State->cLines   = 0; */
	/* State->cCircles = 0; */
	/* State->cArcs    = 0; */
	/* State->cShapes  = 0; */

	State->Basis->XAxis  = V2(1.f, 0.f);
	State->Basis->Offset = ZeroV2;
	State->Basis->Zoom   = 0.1f;

	State->tBasis      = 1.f;
	State->ipoSelect   = 0;
	State->ipoArcStart = 0;
	State->ExtendLine  = 0;

	State->Length = 20.f;

	PushStruct(&State->maActions, action);
	action Action;
	Action.Kind = ACTION_Reset;
	ACTIONS(++State->iLastAction) = Action;

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

/// returns index of point (may be new or existing)
internal uint
AddPoint(state *State, v2 po, uint PointTypes, u8 *PriorStatus)
{
	BEGIN_TIMED_BLOCK;
	/* gDebugV2 = po; */
	uint Result = FindPointAtPos(State, po, ~(uint)POINT_Free);
	if(Result)
	{
		// NOTE: Use existing point, but add any new status (and confirm Extant)
		if(PriorStatus) *PriorStatus = POINTSTATUS(Result);
		if((POINTSTATUS(Result) & (PointTypes | POINT_Extant)))
		{
			goto end;
		}
		else
		{
			POINTSTATUS(Result) |= PointTypes | POINT_Extant;
		}
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
			if(POINTSTATUS(ipo) == POINT_Free)
			{
				Result = ipo;
				break;
			}
		}
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
				Result = IntersectSegmentsWinding (LPOINTS(1), LPOINTS(2),   Intersection1);                 break;
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
	UpdateDrawPointers(State, iPrevDraw);

}

internal inline v2
ExtendSegment(v2 poStart, v2 poAngle, v2 poLength)
{
	v2 LineAxis = Norm(V2Sub(poAngle, poStart));
	v2 RelLength = V2Sub(poLength, poStart);
	// Project RelLength onto LineAxis
	f32 ExtendLength = Dot(LineAxis, RelLength);
	v2 Result = V2WithDist(poStart, poAngle, ExtendLength);
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
	ScreenCentre;

	// REMOVE
	static int testcharindex = 0;

	memory_arena TempArena;
	InitArena(&TempArena, (u8 *)Memory->TransientStorage, Memory->TransientStorageSize);

	if(!Memory->IsInitialized)
	{
		if(!State->OpenFile)
		{
			Reset(State);
		}
		else
		{
			State->tBasis = 1.f;
			State->OpenFile = 0;
		}
		State->cDraws = 0;
		InitArena(&Arena, (u8 *)Memory->PermanentStorage + sizeof(state), Memory->PermanentStorageSize - sizeof(state));

		// NOTE: need initial save state to undo to
		SaveUndoState(State);
		State->Modified = 0;

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

	if(V2Equals(BASIS->XAxis, ZeroV2) || isnan(BASIS->XAxis.X) || isnan(BASIS->XAxis.Y))
	{
		LOG("Invalid basis");
		BASIS->XAxis.X = 1.f;
		BASIS->XAxis.Y = 0.f;
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
	uint ipoSnap;
	uint ipoClosest = 0;
	{ LOG("INPUT");
		Keyboard = Input.New->Keyboard;
		Mouse  = Input.New->Mouse;
		pMouse = Input.Old->Mouse;

		State->PointSnap = Held(Keyboard.Shift) ? 0 : 1;

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

		f32 ClosestDistSq;
		v2 CanvasMouseP = V2ScreenToCanvas(*BASIS, Mouse.P, ScreenCentre);
		SnapMouseP = CanvasMouseP;
		poClosest = CanvasMouseP;
		ipoClosest = ClosestPointIndex(State, CanvasMouseP, &ClosestDistSq);
		ipoSnap = 0;
		if(ipoClosest)
		{
			poClosest = POINTS(ipoClosest);
			// NOTE: BASIS->Zoom needs to be squared to match ClosestDistSq
			if(ClosestDistSq/(BASIS->Zoom * BASIS->Zoom) < 5000.f)
			{ // closest point within range
				if(State->PointSnap)
				{
					SnapMouseP = poClosest;
					ipoSnap = ipoClosest;
				}
			}

			else
			{ // closest point outside range
				ipoClosest = 0;
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
		KEYBOARD_LENGTH_FACTOR(1, 2.f)
		KEYBOARD_LENGTH_FACTOR(2, 3.f)
		KEYBOARD_LENGTH_FACTOR(3, 4.f)
		KEYBOARD_LENGTH_FACTOR(4, 5.f)
		KEYBOARD_LENGTH_FACTOR(5, 6.f)
		KEYBOARD_LENGTH_FACTOR(6, 7.f)
		KEYBOARD_LENGTH_FACTOR(7, 8.f)
		KEYBOARD_LENGTH_FACTOR(8, 9.f)
		KEYBOARD_LENGTH_FACTOR(9, 10.f)
		KEYBOARD_LENGTH_FACTOR(0, 11.f)
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



		// TODO: fix needed for if started and space released part way?
		if((C_PanMod.EndedDown && Mouse.LMB.EndedDown) || C_Pan.EndedDown)
		{ // pan canvas with mouse
			if(DEBUGPress(Mouse.LMB) || DEBUGPress(C_Pan))
			{

			}
			// DRAG SCREEN AROUND
			BASIS->Offset = V2Add(BASIS->Offset,
				V2Sub(V2ScreenToCanvas(*BASIS, pMouse.P, ScreenCentre),
					  V2ScreenToCanvas(*BASIS,  Mouse.P, ScreenCentre)));
			// NOTE: prevents later triggers of clicks, may not be required if input scheme changes.
			Input.New->Mouse.LMB.EndedDown = 0;
		}

		/* // TODO: Do I actually want to be able to drag points? */
		/* else if(State->ipoDrag) */
		/* { */
		/* 	if(DEBUGClick(C_Drag)) */
		/* 	{ */
		/* 		SaveUndoState(State); */
		/* 		// Set point to mouse location and recompute intersections */
		/* 		State->ipoDrag = 0; */
		/* 		State->ipoSelect = 0; */
		/* 		// TODO: this breaks lines attached to intersections... */
		/* 		RemovePointsOfType(State, POINT_Intersection); */
		/* 		for(uint i = 1; i <= State->iLastLinePoint; i+=2) */
		/* 		{ */
		/* 			// TODO: this is wasteful */
		/* 			AddLineIntersections(State, State->LinePoints[i], i, 0); */
		/* 		} */
		/* 	} */

		/* 	else if(DEBUGClick(RMB) || Keyboard.Esc.EndedDown) */
		/* 	{ */
		/* 		// Cancel dragging, point returns to saved location */
		/* 		State->Points[State->ipoDrag] = State->poSaved; */
		/* 		State->ipoDrag = 0; */
		/* 		State->ipoSelect = 0; */
		/* 	} */

		/* 	else */
		/* 	{ */
		/* 		State->Points[State->ipoDrag] = Mouse.P; // Update dragged point to mouse location */
		/* 	} */
		/* 	// Snapping is off while dragging; TODO: maybe change this when points can be combined */
		/* 	ipoSnap = 0; */
		/* } */

		else if(State->ipoSelect)
		{ // started drawing
			if(C_Cancel.EndedDown)
			{ // cancel selection, point returns to saved location
				// TODO: should this be an undo? ...then Modified could remain unchanged
				POINTSTATUS(State->ipoSelect) = State->SavedStatus[0];
				POINTSTATUS(State->ipoArcStart) = State->SavedStatus[1];
				State->ipoSelect = 0;
				State->ipoArcStart = 0;
				OffsetDraw(State, -1);
			}

			else if(C_BasisMod.EndedDown && DEBUGClick(C_BasisSet))
			{ // rotate canvas basis
				// TODO IMPORTANT: don't add point if pressed in free space
				// TODO: Alt-click and drag rather than 2 clicks: if alt is down on LMB down, set on LMB release
				// otherwise set on alt-click
				// TODO: if RMB held, set zoom as well - box aligned to new axis showing where screen will end up
				State->tBasis = 0.f;
				BASIS->XAxis = Norm(V2Sub(SnapMouseP, POINTS(State->ipoSelect)));
				POINTSTATUS(State->ipoSelect) = State->SavedStatus[0];
				POINTSTATUS(State->ipoArcStart) = State->SavedStatus[1];
				State->ipoSelect = 0;
				State->ipoArcStart = 0;
			}

			else if(State->ipoArcStart)
			{ // drawing an arc
				// NOTE: Not using button released in case it's missed for some reason
				// also possible this fires at a weird time when focus returns or something...
				if(!C_Arc.EndedDown)
				{ // finish drawing arc/circle
					v2 poSelect = POINTS(State->ipoSelect);
					v2 poNew = V2WithDist(poSelect, SnapMouseP, State->Length);
					if(V2WithinEpsilon(poNew, POINTS(State->ipoArcStart), POINT_EPSILON))
					{ // Same angle -> full circle
						AddCircle(State, State->ipoSelect, AddPoint(State, poNew, POINT_Radius, 0));
					}

					else
					{ // arc
						/* v2 poFocus = poSelect; */
						/* v2 poStart = POINTS(State->ipoArcStart); */
						v2 poEnd = poNew; //V2WithDist(poFocus, SnapMouseP, Dist(poFocus, poStart)); // Attached to arc
						uint ipoArcEnd = AddPoint(State, poEnd, POINT_Arc, 0);
						AddArc(State, State->ipoSelect, State->ipoArcStart, ipoArcEnd);
					}
					State->ipoSelect = 0;
					State->ipoArcStart = 0;
				}
			}

			else if(State->ExtendLine)
			{ // drawing a line
				if(!C_Line.EndedDown)
				{ // finish drawing a line
					v2 poSelect = POINTS(State->ipoSelect);
					v2 poExtend = State->poSaved;
					v2 poNew = ExtendSegment(poSelect, poExtend, SnapMouseP);
					AddSegment(State, State->ipoSelect, AddPoint(State, poNew, POINT_Line, 0));
					State->ipoSelect = 0;
					State->ExtendLine = 0;
				}
			}

			else if(DEBUGRelease(C_Length))
			{ // set length
				// TODO: use DistSq
				 f32 Length = Dist(POINTS(State->ipoSelect), SnapMouseP);
				 if(Length > POINT_EPSILON)
				 {
				 	 State->pLength = State->Length;
				 	 State->Length = Length;
				 }
			}

			else if(DEBUGClick(C_Line))
			{ // start drawing line
				// NOTE: completed line, set both points' status if line does not already exist
				// and points aren't coincident
				if(V2WithinEpsilon(POINTS(State->ipoSelect), SnapMouseP, POINT_EPSILON))
				{ // NOTE: don't  want to extend a line with no direction!
					State->ipoSelect = 0;
				}
				else
				{ // (expected behaviour)
					State->poSaved = SnapMouseP;
					State->ExtendLine = 1;
				}
			}

			else if(DEBUGClick(C_Arc))
			{ // start drawing arc/circle
				// TODO: stop snapping onto focus - add exceptions to snapping
				v2 poNew = V2WithDist(POINTS(State->ipoSelect), SnapMouseP, State->Length);
				// TODO (optimise): there is a 1 frame lag even if pressed and released within 1...
				State->ipoArcStart = AddPoint(State, poNew, POINT_Arc, 0);
			}
		}

		else
		{ // normal state
			if(Keyboard.Ctrl.EndedDown && DEBUGPress(Keyboard.S))
			{ // SAVE (AS)
				State->SaveFile = 1;
				if(Keyboard.Shift.EndedDown) { State->SaveAs = 1; }
			}
			else if(Keyboard.Ctrl.EndedDown && DEBUGPress(Keyboard.O))
			{ // OPEN (AS)
				// TODO IMPORTANT: seems to trap the 'o' down,
				// so it needs to be pressed again before it's registered properly
				State->OpenFile = 1;
				if(Keyboard.Shift.EndedDown) { State->SaveAs = 1; }
			}
			// TODO (feature): new file
			
			if((Keyboard.Ctrl.EndedDown && DEBUGPress(Keyboard.Z) && !Keyboard.Shift.EndedDown) &&
				// NOTE: making sure that there is a state available to undo into
			  	((State->cDraws >= NUM_UNDO_STATES && State->iLastDraw != iDrawOffset(State, -1)) ||
				 (State->cDraws <  NUM_UNDO_STATES && State->iCurrentDraw > 1)))
			{ OffsetDraw(State, -1); } // UNDO
			if(((Keyboard.Ctrl.EndedDown && DEBUGPress(Keyboard.Y)) ||
				(Keyboard.Ctrl.EndedDown && Keyboard.Shift.EndedDown && DEBUGPress(Keyboard.Z))) &&
					(State->iCurrentDraw < State->iLastDraw))
			{ OffsetDraw(State, 1); } // REDO

			if(DEBUGClick(C_StartShape))
			{ // start drawing
				// NOTE: Starting a shape, save the first point
				/* State->SavedPoint = SnapMouseP; */
				SaveUndoState(State);
				State->ipoSelect = AddPoint(State, SnapMouseP, POINT_Extant, &State->SavedStatus[0]);
			}

			// TODO: could skip check and just write to invalid point..?
			if(ipoSnap)
			{ // point snapped to
				if(DEBUGPress(C_Delete))
				{
					// TODO: deleting points
					/* SaveUndoState(State); */
					/* InvalidatePoint(State, ipoSnap); */
				}
			}

			/* if(DEBUGClick(MMB)) */
			/* { */
				// MOVE POINT
				/* SaveUndoState(State); */
				/* State->SavedPoint = State->Points[ipoSnap]; */
				/* State->ipoSelect = ipoSnap; */
				/* State->ipoDrag = ipoSnap; */
			/* } */ 

			if(DEBUGPress(C_Reset))
			{ // reset canvas
				SaveUndoState(State);
				Reset(State);
				DebugClear();
			}
		}
	}

	{ LOG("RENDER");
		DrawCrosshair(ScreenBuffer, ScreenCentre, 5.f, LIGHT_GREY);

		basis EndBasis = *BASIS;
		basis StartBasis = *pBASIS;
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
					DrawClosestPtOnSegment(ScreenBuffer, Mouse.P, poA, poB);
				} break;
				
				case SHAPE_Circle:
				{
					v2 poFocus  = V2CanvasToScreen(Basis, Points[Shape.Circle.ipoFocus], ScreenCentre);
					v2 poRadius = V2CanvasToScreen(Basis, Points[Shape.Circle.ipoRadius], ScreenCentre);
					CircleLine(ScreenBuffer, poFocus, Dist(poFocus, poRadius), BLACK);
				} break;

				case SHAPE_Arc:
				{
					v2 poFocus = V2CanvasToScreen(Basis, Points[Shape.Arc.ipoFocus], ScreenCentre);
					v2 poStart = V2CanvasToScreen(Basis, Points[Shape.Arc.ipoStart], ScreenCentre);
					v2 poEnd   = V2CanvasToScreen(Basis, Points[Shape.Arc.ipoEnd],   ScreenCentre);
					ArcFromPoints(ScreenBuffer, poFocus, poStart, poEnd, BLACK); 
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

		poClosest = V2CanvasToScreen(Basis, poClosest, ScreenCentre);
		if(State->iLastPoint)  { CircleLine(ScreenBuffer, poClosest, 5.f, GREY); }
		if(ipoClosest)  { DrawCircleFill(ScreenBuffer, poClosest, 3.f, BLUE); }


		if(State->ipoSelect)
		{ // A point is selected (currently drawing)
			v2 poSelect = V2CanvasToScreen(Basis, Points[State->ipoSelect], ScreenCentre);
			if(State->ipoArcStart && !V2Equals(Points[State->ipoArcStart], SnapMouseP))
			{ // preview drawing arc
				LOG("\tDRAW HALF-FINISHED ARC");
				v2 poFocus = V2CanvasToScreen(Basis, Points[State->ipoSelect], ScreenCentre);
				v2 poStart = V2CanvasToScreen(Basis, Points[State->ipoArcStart], ScreenCentre);
				ArcFromPoints(ScreenBuffer, poFocus, poStart, SSSnapMouseP, BLACK);
				DEBUGDrawLine(ScreenBuffer, poSelect, poStart, LIGHT_GREY);
				DEBUGDrawLine(ScreenBuffer, poSelect, SSSnapMouseP, LIGHT_GREY);
			}
			else if(State->ExtendLine)
			{ // preview extending a line
				// TODO (feature): draw a light grey ray to edge of screen
				// TODO (feature): draw a light grey perpendicular line to mouse pointer/SnapMouseP?
				v2 poAngle = V2CanvasToScreen(Basis, State->poSaved, ScreenCentre);
				v2 poExtend = ExtendSegment(poSelect, poAngle, SSSnapMouseP);
				DEBUGDrawLine(ScreenBuffer, poSelect, poExtend, BLACK);
				DrawActivePoint(ScreenBuffer, poExtend, RED);
			}
			else
			{ // standard preview
				LOG("\tDRAW PREVIEW");
				// NOTE: Mid-way through drawing a line
				DrawCircleFill(ScreenBuffer, poSelect, 3.f, RED);
				CircleLine(ScreenBuffer, poSelect, 5.f, RED);

				f32 SSLength = State->Length/BASIS->Zoom; 
				if(C_Arc.EndedDown && !C_PanMod.EndedDown)
				{ SSLength = Dist(poSelect, SSSnapMouseP); }
				CircleLine(ScreenBuffer, poSelect,  SSLength, LIGHT_GREY);
				DEBUGDrawLine(ScreenBuffer, poSelect, SSSnapMouseP, LIGHT_GREY);
				/* DebugAdd("\n\nMouse Angle: %f", MouseAngle/TAU); */
			}
		}

		if(ipoSnap)
		{ // draw snapped point
			// NOTE: Overdraws...
			DrawActivePoint(ScreenBuffer, poClosest, BLUE);
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

	if(State->ShowDebugInfo)
	{ LOG("PRINT");
		DebugPrint();
		/* DrawSuperSlowCircleLine(ScreenBuffer, ScreenCentre, 50.f, RED); */

		CycleCountersInfo(ScreenBuffer, &State->DefaultFont);

		// TODO: Highlight status for currently selected/hovered points

		char Message[512];
		f32 TextSize = 15.f;
		stbsp_sprintf(Message, //"LinePoints: %u, TypeLine: %u, Esc Down: %u"
				"\nFrame time: %.2fms, "
				"Mouse: (%.2f, %.2f), "
				"Basis: (%.2f, %.2f), "
				"Char: %d (%c)"
				/* "Offset: (%.2f, %.2f), " */
				/* "Zoom: %.2f" */
				/* "iLastPoint: %u" */
				/* "S: %u, SA: %u, O: %u" */
				,
				/* State->cLinePoints, */
				/* NumPointsOfType(State->PointStatus, State->iLastPoint, POINT_Line), */
				/* C_Cancel.EndedDown, */
				State->dt*1000.f,
				Mouse.P.X, Mouse.P.Y,
				BASIS->XAxis.X, BASIS->XAxis.Y,
				testcharindex, testcharindex
				/* BASIS->Offset.X, BASIS->Offset.Y, */
				/* BASIS->Zoom */
				/* State->iLastPoint */
				/* State->SaveFile, State->SaveAs, State->OpenFile */
				);
		DrawString(ScreenBuffer, &State->DefaultFont, Message, TextSize, 10.f, TextSize, 1, BLACK);

		char ShapeInfo[512];
		stbsp_sprintf(ShapeInfo, "L#  P#\n\n");
		for(uint i = 1; i <= State->iLastShape && i <= 32; ++i)
		{
			stbsp_sprintf(ShapeInfo, "%s%02u  %04b\n", ShapeInfo, i, SHAPES(i).Kind);
		}
		char PointInfo[512];
		stbsp_sprintf(PointInfo, " # DARTFILE\n\n");
		for(uint i = 1; i <= State->iLastPoint && i <= 32; ++i)
		{
			stbsp_sprintf(PointInfo, "%s%02u %08b\n", PointInfo, i, POINTSTATUS(i));
		}
		TextSize = 13.f;
		DrawString(ScreenBuffer, &State->DefaultFont, ShapeInfo, TextSize,
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
