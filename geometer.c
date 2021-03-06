#define _CRT_SECURE_NO_WARNINGS
#include "geometer.h"
#include <fonts.c>
#include "geometer_debug.c"

// TODO:
// =====
//
// - Find (and colour) lines intersecting at a given point
// - Undos
//  	- should saves remove 'superfluous' actions - i.e. take out remove points/indirections?
//  	- undo history (show overall/with layers)
//  	- undo by absolute and layer order
// - Change storage of intersections, so they don't all need to be recomputed on changes
// 	    - only compute those from shapesnear mouse?
// - Spatially partition(?) shapes
// - Layers
// 		- Togglable layers 
// 		- Infinite layers
// 		- Scrollable layers
// - For fast movements, make sweep effect, rather than ugly multiple line effect 
// - Deal with perfect overlaps that aren't identical (i.e. one line/arc is longer)
// - Resizable windo (maintain centre vs maintain absolute position)
// - Constraint system? Macros? Paid version?
// - Make custom cursors
// - F keys to open toolbars (layers/minimap/action history/...)
// - ... or have them pop up by 'slamming' mouse to canvas edge... or both
// - Copy and paste (between running apps - clipboard?) without NEEDING clipboard
// - What else between apps - lengths?
// - Consolidate history
// - Cancel actions with other mouseclick e.g. LMB while extending line with RMB
// - Select shapes, then move that shape's points - extend/arc
// 		- select via intersection with line allows weird angle selections...
// - Add arc creation behaviour to dragging
// - Regression test actions by comparing end state to version reconstructed by actions

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
//	- ??? - move to background layer/another layer...
//	Once points are selected:
//	- Alt-RMB for +/- selection
//	OR
//	- RMB for + selection, Alt-RMB for - selection
//	...

// CONTROLS: ////////////////////////////
#define C_Cancel       Keyboard.Esc

#define C_StartShape   Mouse.LMB
#define C_Arc          Mouse.LMB
#define C_Line         Mouse.RMB
#define C_Length       Mouse.LMB
// divide length       1-0
// mult length         Alt + 1-0
// get store length    a-z,A-Z
// set store length    Alt + a-z,A-Z

#define C_NoSnap       Keyboard.Shift
#define C_ShapeLock    Keyboard.Ctrl
// IDEA (ui): would this be better as full stop? obvious semantic connection
// further from resting position, but probably used infrequently..?
#define C_PointOnly    Keyboard.Alt

#define C_Drag         Mouse.LMB
#define C_Select       Mouse.RMB
#define C_SelectMod    Keyboard.Alt

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

// TODO: ctrl-tab? mouse special buttons?
#define C_LayerMod     Keyboard.Ctrl
#define C_LayerRev     Keyboard.Shift
#define C_LayerChange  Keyboard.Tab // TODO: change to 1?
#define C_LayerDrawer  Keyboard.T

/////////////////////////////////////////

#define Col_DrawPreview   BLUE
#define Col_Preview       LIGHT_GREY
#define Col_ArcCircle     LIGHT_GREY
#define Col_ArcLines      LIGHT_GREY
#define Col_LineExtend    LIGHT_GREY

#define Col_Shape         BLACK
#define Col_ShapeOffLayer LIGHT_GREY

#define Col_Perp          LIGHT_GREY
#define Col_SetLength     LIGHT_GREY
#define Col_BasisLine     RED

#define Col_ThumbOutline  LIGHT_GREY
#define Col_ThumbSelected BLUE
#define Col_Text          BLACK

#define Col_Pt            LIGHT_GREY
#define Col_ActivePt      RED
#define Col_SelectedPt    ORANGE
#define Col_UnselectedPt  MAGENTA
#define Col_SelectBox     GREY

v2 gDebugV2;
v2 gDebugPoint;

#define DRAW_FN(name)\
fn_##name *name;
DRAW_FNS
#undef DRAW_FN

internal inline void
#define DrawCrosshair GeoDrawCrosshair
DrawCrosshair(draw_buffer *Draw, v2 Centre, f32 Radius, colour Colour)
{
	v2 X1 = {2.f, 0.f};
	v2 Y1 = {0.f, 2.f};
	v2 XRad = {Radius, 0.f};
	v2 YRad = {0.f, Radius};
	draw_buffer tDraw = *Draw;
	tDraw.StrokeWidth = 1.f;
	DrawSeg(&tDraw, V2Sub(Centre, X1), V2Sub(Centre, XRad), Colour);
	DrawSeg(&tDraw, V2Add(Centre, X1), V2Add(Centre, XRad), Colour);
	DrawSeg(&tDraw, V2Add(Centre, Y1), V2Add(Centre, YRad), Colour);
	DrawSeg(&tDraw, V2Sub(Centre, Y1), V2Sub(Centre, YRad), Colour);
}

internal inline void
DrawClosestPtOnSegment(draw_buffer *Draw, v2 po, v2 lipoA, v2 lipoB)
{
	BEGIN_TIMED_BLOCK;
	v2 po1 = ClosestPtOnSegment(po, lipoA, V2Sub(lipoB, lipoA));
	DrawCrosshair(Draw, po1, ACTIVE_POINT_RADIUS, RED);
	END_TIMED_BLOCK;
}

internal inline void
DrawClosestPtOnCircle(draw_buffer *Draw, v2 po, v2 poFocus, f32 Radius)
{
	BEGIN_TIMED_BLOCK;
	v2 po1 = ClosestPtOnCircle(po, poFocus, Radius);
	DrawCrosshair(Draw, po1, ACTIVE_POINT_RADIUS, RED);
	END_TIMED_BLOCK;
}

internal inline void
DrawClosestPtOnArc(draw_buffer *Draw, v2 po, v2 poFocus, v2 poStart, v2 poEnd)
{
	BEGIN_TIMED_BLOCK;
	v2 po1 = ClosestPtOnArc(po, poFocus, poStart, poEnd);
	DrawCrosshair(Draw, po1, ACTIVE_POINT_RADIUS, RED);
	END_TIMED_BLOCK;
}

internal inline void
DrawActivePoint(draw_buffer *Draw, v2 po, colour Col)
{
	BEGIN_TIMED_BLOCK;
	draw_buffer tDraw = *Draw;
	tDraw.StrokeWidth = 1.f;
	DrawCircleFill(&tDraw, po, POINT_RADIUS, Col);
	DrawCircleLine(&tDraw, po, ACTIVE_POINT_RADIUS, Col);
	END_TIMED_BLOCK;
}


internal inline void
DrawArcFromPoints(draw_buffer *Draw, v2 Centre, v2 A, v2 B, colour Colour)
{
	DrawArcLine(Draw, Centre, Dist(Centre, A), V2Sub(A, Centre), V2Sub(B, Centre), Colour);
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
	switch(USERIFY_ACTION(Action.Kind)) // whether or not it is user-initiated is dealt with by UserUndo
	{
		case ACTION_Reset:
		{ // reapply all actions from scratch
			// TODO: add checkpoints so it doesn't have to start right from beginning
			uint iCurrentAction = State->iCurrentAction;
			for(uint i = Action.Reset.i; i < iCurrentAction; ++i)
			{ ApplyAction(State, Actions[i]); }
		} break;

		case ACTION_RemovePt:
		{
			POINTSTATUS(Action.Point.ipo) = 1;
		} break;

		case ACTION_RemoveShape:
		{
			shape *Shape = &Pull(State->maShapes, Action.Shape.i);
			Assert(Shape->Kind < SHAPE_Free);
			Shape->Kind = Shape->Kind < SHAPE_Free ? -Shape->Kind : Shape->Kind;
		} break;

		case ACTION_Basis:
		{ // find the previous basis and apply that
			basis PrevBasis = DefaultBasis; // in case no previous basis found
			for(uint i = State->iCurrentAction; i > 0; --i)
			{ // find previous basis
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
			Pull(State->maShapes, Action.Shape.i).Kind = SHAPE_Free;
			if(Action.Shape.i == State->iLastShape)
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
			Pull(State->maPointLayer, Action.Point.ipo) = 0;
			if(Action.Point.ipo == State->iLastPoint)
			{
				--State->iLastPoint;
				PopDiscard(&State->maPoints);
				PopDiscard(&State->maPointStatus);
				PopDiscard(&State->maPointLayer);
			}
			else
			{ Assert(!"TODO: undo point additions mid-array"); }
			// does anything actually need to be done?
		} break;

		case ACTION_Move:
		{
		/* 	POINTS(Action.Move.ipo[0])   = V2Sub(POINTS(Action.Move.ipo[0]), Action.Move.Dir); */
		/* 	if(Action.Move.ipo[1]) */
		/* 	{ POINTS(Action.Move.ipo[1]) = V2Sub(POINTS(Action.Move.ipo[1]), Action.Move.Dir); } */
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

			default: { Assert(!"Unknown shape type"); }
		}

		cTotalIntersects += cIntersects;

		// update closest candidate
		if(cIntersects      && DistSq(P, poIntersect1) < DistSq(P, *poClosest))
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
		// TODO: remove? (inc assert)
		if(cIntersects == 0) { Result = ClosestPtOnCircle(MouseP, poFocus, Radius); }
		/* v2 TestPoint = ClosestPtOnCircle(MouseP, poFocus, Radius); */
		/* Assert(cIntersects || V2WithinEpsilon(Result, TestPoint, POINT_EPSILON)); */
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
DrawAABB(draw_buffer *Draw, aabb AABB, colour Col)
{
	// TODO (fix): missing pixel in bottom right
	v2 TopLeft     = V2(AABB.MinX, AABB.MaxY);
	v2 TopRight    = V2(AABB.MaxX, AABB.MaxY);
	v2 BottomLeft  = V2(AABB.MinX, AABB.MinY);
	v2 BottomRight = V2(AABB.MaxX, AABB.MinY);

	DrawSeg(Draw, TopLeft,    TopRight,    Col);
	DrawSeg(Draw, TopLeft,    BottomLeft,  Col);
	DrawSeg(Draw, TopRight,   BottomRight, Col);
	DrawSeg(Draw, BottomLeft, BottomRight, Col);
}

internal inline aabb
AABBCanvasToScreen(basis Basis, aabb AABB, v2 ScreenCentre)
{
	aabb Result;
	Result.Min = V2CanvasToScreen(Basis, AABB.Min, ScreenCentre);
	Result.Max = V2CanvasToScreen(Basis, AABB.Max, ScreenCentre);
	return Result;
}

internal inline b32
CurrentActionIsByUser(state *State)
{
	action Action = Pull(State->maActions, State->iCurrentAction);
	b32 Result = Action.Kind >= 0;
	return Result;
}

internal inline b32
PointIsSelected(state *State, uint ipo)
{
	b32 Result = 0;
	foreachf(uint, ipoSelect, State->maSelectedPoints)
	{
		if(ipoSelect == ipo)
		{ Result = 1; break; }
	}
	return Result;
}

internal void
AdjustMatchingArcPoint(shape_arena Shapes, v2_arena Points, uint ipo)
{
	foreachf(shape, Shape, Shapes)
	{
		if(Shape.Kind == SHAPE_Arc)
		{
			if(Shape.Arc.ipoStart == ipo)
			{
				v2 Focus  =  Pull(Points,Shape.Arc.ipoFocus); 
				v2 Start  =  Pull(Points,Shape.Arc.ipoStart); 
				v2 *End   = &Pull(Points,Shape.Arc.ipoEnd); 
				*End = ClosestPtOnCircle(*End, Focus, Dist(Focus, Start));
			}
			else if(Shape.Arc.ipoEnd == ipo)
			{
				v2 Focus  =  Pull(Points, Shape.Arc.ipoFocus); 
				v2 *Start = &Pull(Points, Shape.Arc.ipoStart); 
				v2 End    =  Pull(Points, Shape.Arc.ipoEnd); 
				*Start = ClosestPtOnCircle(*Start, Focus, Dist(Focus, End));
			}
		}
	}
}

// this is for the occasions when an arc/circle is fully around the screen
// trying to draw the massive shape is very expensive, and nothing gets drawn
internal b32
ScreenIsInsideCircle(aabb ScreenBB, v2 poSSFocus, f32 SSRadiusSq)
{
	b32 Result = 0;
	if( DistSq(poSSFocus, ScreenBB.Min) < SSRadiusSq &&
		DistSq(poSSFocus, ScreenBB.Max) < SSRadiusSq &&
		DistSq(poSSFocus, V2(ScreenBB.MinX, ScreenBB.MaxY)) < SSRadiusSq &&
		DistSq(poSSFocus, V2(ScreenBB.MaxX, ScreenBB.MinY)) < SSRadiusSq)
	{ Result = 1; }
	return Result;
}

#define ToScreen(p) V2CanvasToScreen(Basis, p, ScreenCentre)
internal void
RenderDrawing(draw_buffer Draw, state *State, basis Basis, v2 AreaOffset, v2 AreaSize, uint iLayer, f32 PtRadius)
{
	enum { MainCanvasDrawing };
	LOG("\tDRAW SHAPES");
	shape_arena maShapesNearScreen = State->maShapesNearScreen;
	// changes buffer dimensions if software rendering, otherwise does OpenGL scissor
	Draw.Buffer = ClipBuffer(Draw.Buffer, AreaOffset, AreaSize);
	v2 ScreenCentre = V2Add(AreaOffset, V2Mult(0.5, AreaSize));
	AreaSize = V2( (f32)Draw.Buffer.Width, (f32)Draw.Buffer.Height );
	if(Draw.Kind == DRAW_Software)
	{ ScreenCentre = V2Mult(0.5, AreaSize); }

	if(iLayer != MainCanvasDrawing)
	{ Draw.StrokeWidth = 1.0f; }
	foreachf(shape, Shape, maShapesNearScreen)
	{ // DRAW SHAPES

		uint ShapeLayer = POINTLAYER(Shape.P[0]);
		Assert(ShapeLayer != 0);
		if(iLayer == MainCanvasDrawing || ShapeLayer == iLayer)
		{
			colour LayerColour = BLACK;
			if(ShapeLayer != State->iCurrentLayer)
			{ LayerColour = PreMultiplyColour(LayerColour, 0.25f); }

			switch(Shape.Kind)
			{
				case SHAPE_Segment:
				{
					v2 poA = ToScreen(POINTS_OS(Shape.Line.P1));
					v2 poB = ToScreen(POINTS_OS(Shape.Line.P2));
					DrawSeg(&Draw, poA, poB, LayerColour);
				} break;

				case SHAPE_Circle:
				{
					v2 poFocus  = ToScreen(POINTS_OS(Shape.Circle.ipoFocus));
					v2 poRadius = ToScreen(POINTS_OS(Shape.Circle.ipoRadius));
					f32 Radius  = Dist(poFocus, poRadius);
					Assert(Radius);
					DrawCircleLine(&Draw, poFocus, Radius, LayerColour);
				} break;

				case SHAPE_Arc:
				{
					v2 poFocus = ToScreen(POINTS_OS(Shape.Arc.ipoFocus));
					v2 poStart = ToScreen(POINTS_OS(Shape.Arc.ipoStart));
					v2 poEnd   = ToScreen(POINTS_OS(Shape.Arc.ipoEnd));
					DrawArcFromPoints(&Draw, poFocus, poStart, poEnd, LayerColour); 
				} break;

				default:
				{ Assert(! "Tried to draw unknown shape"); }
			}
		}
	}

	v2_arena maPointsOnScreen = State->maPointsOnScreen;
	LOG("\tDRAW POINTS");
	char PointIndex[32] = {0};
	foreachf1(v2, po, maPointsOnScreen)
	if(POINTSTATUS(ipo) && (! iLayer || POINTLAYER(ipo) == iLayer))
	{ // draw on-screen points
		v2 SSPoint = ToScreen(po);
		DrawCircleFill(&Draw, SSPoint, PtRadius, Col_Pt);
	}

	if(iLayer == MainCanvasDrawing)
	DEBUG_LIVE_if(Points_Numbering)
	{ // write index number next to points and intersections
		foreachf(v2, po, State->maPoints)
		{
			v2 SSPoint = ToScreen(po);
			ssnprintf(PointIndex, sizeof(PointIndex), "%u (L%u)", ipo, POINTLAYER(ipo));
			DrawString(&Draw.Buffer, &State->DefaultFont, PointIndex, 15.f, SSPoint.X + 5.f, SSPoint.Y - 5.f, 0, BLACK);
		}
		foreachf(v2, P, State->maIntersects)
		{
			v2 SSP = ToScreen(P);
			ssnprintf(PointIndex, sizeof(PointIndex), "%u", iP);
			DrawCrosshair(&Draw, SSP, 6.f, GREEN);
			DrawString(&Draw.Buffer, &State->DefaultFont, PointIndex, 15.f, SSP.X + 5.f, SSP.Y - 5.f, 0, GREEN);
		}
		foreachf(shape, Shape, State->maShapes)
		{
			v2 SSP = ToScreen(POINTS(Shape.P[1]));
			ssnprintf(PointIndex, sizeof(PointIndex), "%u (L%u)", Shape.P[1], POINTLAYER(Shape.P[2]));
			DrawString(&Draw.Buffer, &State->DefaultFont, PointIndex, 15.f, SSP.X + 5.f, SSP.Y + 5.f, 0, MAGENTA);
		}
	}

	// reset scissor for OpenGL
	ClipBuffer(Draw.Buffer, ZeroV2, AreaSize);
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
	ScreenSize.X = (f32)Draw->Buffer.Width;
	ScreenSize.Y = (f32)Draw->Buffer.Height;
	v2 ScreenCentre = V2Mult(0.5f, ScreenSize);
	platform_request File = {0};

	{
#define DRAW_FN(name)\
		name = Draw->name;
		DRAW_FNS
#undef DRAW_FN
	}

	memory_arena TempArena;
	InitArena(&TempArena, Memory->TransientStorage, Memory->TransientStorageSize);

	if(!Memory->IsInitialized)
	{
		InitArena(&Arena, (state *)Memory->PermanentStorage + 1, Memory->PermanentStorageSize - sizeof(state));

		State->iSaveAction = State->iCurrentAction;
		State->iCurrentLayer = 1;
		State->FX[FX_Smear] = 1;

		Memory->IsInitialized = 1;
	}
	{ // DEBUG
		Debug.Buffer = Draw->Buffer;
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
		BASIS.Zoom = BASIS_DEFAULT_ZOOM;
		// TODO (UI): should I reset zoom here as well?
	}

	// Clear BG
	BEGIN_NAMED_TIMED_BLOCK(ClearBG);
	ClearBuffer(Draw->Buffer);
	END_NAMED_TIMED_BLOCK(ClearBG);
	/* DrawRectangleFilled(Draw.Buffer, Origin, ScreenSize, WHITE); */

	if(State->tBasis < 1.f)  { State->tBasis += State->dt*BASIS_ANIMATION_SPEED; }
	else					 { State->tBasis = 1.f; }
	basis Basis = AnimateBasis(pBASIS, State->tBasis, BASIS);

	keyboard_state Keyboard = Input.New->Keyboard;
	mouse_state Mouse  = Input.New->Mouse;
	mouse_state pMouse = Input.Old->Mouse;
	v2 SnapMouseP = State->pSnapMouseP, poClosest;
	v2 poAtDist = ZeroV2;
	v2 poOnLine = ZeroV2;
	/* v2 DragDir = ZeroV2; */
	v2 poArcStart = ZeroV2;
	v2 poArcEnd   = ZeroV2;
	b32 IsSnapped = 0;
	uint ipoClosest = 0;
	uint ipoClosestIntersect = 0;
	uint ipoSnap = 0;
	aabb SelectionAABB = {0};
	b32 RecalcNeeded = 0;
#if 1
	b32 IsTakingDebugInput = 0;
	for(int i = 0; i < 9; ++i) {
		if(Keyboard.Num[i].EndedDown)
		{ IsTakingDebugInput = 1; break; }
	}
	if(!IsTakingDebugInput)
#endif
	{ LOG("INPUT");
		// Pan with arrow keys
		b32 BasisIsChanged = 0;
		basis NewBasis = Basis;
		b32 Down  = C_PanDown.EndedDown;
		b32 Up    = C_PanUp.EndedDown;
		b32 Left  = C_PanLeft.EndedDown;
		b32 Right = C_PanRight.EndedDown;
		f32 PanSpeed = 15.f * Basis.Zoom;
		if(Down != Up)
		{
			if   (Down)   { NewBasis.Offset = V2Add(NewBasis.Offset, V2Mult(-PanSpeed, Perp(NewBasis.XAxis))); }
			else/*Up*/    { NewBasis.Offset = V2Add(NewBasis.Offset, V2Mult( PanSpeed, Perp(NewBasis.XAxis))); }
			BasisIsChanged = 1;
		}

		if(Left != Right)
		{
			if   (Left)   { NewBasis.Offset = V2Add(NewBasis.Offset, V2Mult(-PanSpeed,      NewBasis.XAxis )); }
			else/*Right*/ { NewBasis.Offset = V2Add(NewBasis.Offset, V2Mult( PanSpeed,      NewBasis.XAxis )); }
			BasisIsChanged = 1;
		}

		// Zoom with PgUp/PgDn
		b32 ZoomIn  = C_ZoomIn.EndedDown;
		b32 ZoomOut = C_ZoomOut.EndedDown;
		// TODO: Make these constants?
		if(ZoomIn != ZoomOut)
		{
			f32 ZoomFactor = 0.9f;
			f32 invZoomFactor = 1.f/ZoomFactor;
			if(ZoomIn)        { NewBasis.Zoom *=    ZoomFactor; }
			else if(ZoomOut)  { NewBasis.Zoom *= invZoomFactor; }
			BasisIsChanged = 1;
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
			v2 RotatedOffset = V2RotateToAxis(NewBasis.XAxis, V2Mult((1.f-ScrollFactor) * NewBasis.Zoom, dMouseP));
			NewBasis.Offset = V2Add(NewBasis.Offset, RotatedOffset);

			// NOTE: wheel delta is in multiples of 120
			for(int i = 0; i < C_Zoom/120; ++i)
			{ NewBasis.Zoom *= ScrollFactor; }
			BasisIsChanged = 1;
		}

		if(BasisIsChanged) { SetBasis(State, NewBasis); }

		// SNAPPING
		v2 CanvasMouseP = V2ScreenToCanvas(Basis, Mouse.P, ScreenCentre);
		{
			f32 ClosestDistSq;
			f32 ClosestIntersectDistSq = 0.f;
			b32 ClosestPtOrIntersect = 0;
			SnapMouseP = CanvasMouseP;
			poClosest = CanvasMouseP;
			ipoClosest = ClosestPointIndex(State, CanvasMouseP, &ClosestDistSq);
			// TODO (ui): consider ignoring intersections while selecting
			/* if( ! (MODE_START_Select <= State->InputMode && State->InputMode <= MODE_END_Select)) */
			{ ipoClosestIntersect = ClosestIntersectIndex(State, CanvasMouseP, &ClosestIntersectDistSq); }

			ClosestPtOrIntersect = ipoClosest || ipoClosestIntersect;
			DebugReplace("Pt: %u, Isct: %u, Drawing: %u\n", ipoClosest, ipoClosestIntersect, IsDrawing(State));
			if(ClosestPtOrIntersect || IsDrawing(State))
			{
				// decide whether to use point or intersect
				if(ipoClosest && ipoClosestIntersect)
				{
					if(ClosestDistSq <= ClosestIntersectDistSq)
					{
						poClosest = POINTS(ipoClosest);
						ipoSnap = ipoClosest;
						DebugAdd("PI - Point\n");
					}
					else
					{
						poClosest = Pull(State->maIntersects, ipoClosestIntersect);
						ClosestDistSq = ClosestIntersectDistSq;
						DebugAdd("PI - Intersect\n");
					}
				}
				else if(ipoClosest)
				{
					poClosest = POINTS(ipoClosest);
					ipoSnap = ipoClosest;
					DebugAdd("Point\n");
				}
				else if(ipoClosestIntersect)
				{
					poClosest = Pull(State->maIntersects, ipoClosestIntersect);
					ClosestDistSq = ClosestIntersectDistSq;
					DebugAdd("Intersect\n");
				}

				// compare against state-based temporary points
				// assumes that IsDrawingArc is implied by IsDrawing
				if(State->InputMode == MODE_ExtendArc)
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
						ipoSnap = 0;
					}
				}
				// don't snap to focus while extending arc
				else if(IsDrawing(State))
				{
					f32 SelectDistSq = DistSq(State->poSelect, CanvasMouseP);
					DebugAdd("Test: %f, Current: %f\n", SelectDistSq, ClosestDistSq);
					if( ! ClosestPtOrIntersect || SelectDistSq < ClosestDistSq)
					{
						poClosest = State->poSelect;
						DebugAdd("Adding poClosest: %.2f, %.2f\n", poClosest.X, poClosest.Y);
						ClosestDistSq = SelectDistSq;
						ipoSnap = 0;
					}
				}
				DebugAdd("poClosest: %.2f, %.2f\n", poClosest.X, poClosest.Y);
				/* gDebugPoint = poClosest; */

#define POINT_SNAP_DIST 5000.f
				// NOTE: BASIS->Zoom needs to be squared to match ClosestDistSq
				if(ClosestDistSq/(Basis.Zoom * Basis.Zoom) < POINT_SNAP_DIST && // closest point is within range
				   ! C_NoSnap.EndedDown && // point snapping is still on
				   ! V2Equals(poClosest, CanvasMouseP)) // found something to snap to
				{
					SnapMouseP = poClosest;
					DebugAdd("SnapMouseP: %.2f, %.2f\n", SnapMouseP.X, SnapMouseP.Y);
					IsSnapped = 1;
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
						// TODO: IsSnapped == 0? bring
					{ SnapMouseP = TestP; }
				}
			}
			if(IsSnapped == 0) { ipoSnap = 0; }
			else if(! V2Equals(SnapMouseP, State->pSnapMouseP))
			{ State->tSnapMouseP = 0.f; }
		}

		// TODO IMPORTANT (fix): stop unwanted clicks from registering. e.g. on open/save
		// TODO: fix the halftransitioncount - when using released(button), it fires twice per release
		b32 MouseInScreenBounds = IsInScreenBounds(Draw->Buffer, Mouse.P); 
#define DEBUGClick(button) (MouseInScreenBounds && DEBUGPress(button))
#define DEBUGRelease(button) (Input.Old->button.EndedDown && !Input.New->button.EndedDown)
#define DEBUGPress(button)   (!Input.Old->button.EndedDown && Input.New->button.EndedDown)
#define DEBUGdMouse()   (V2Sub(Input.New->Mouse.P, Input.Old->Mouse.P))

		if(DEBUGPress(C_DebugInfo)) // toggle debug info
		{ DEBUG_LIVE_VAR_Debug_ShowInfo = !DEBUG_LIVE_VAR_Debug_ShowInfo; }

		if(DEBUGPress(C_CanvasHome))
		{ // reset canvas position
			NewBasis.Offset = ZeroV2;
			NewBasis.Zoom   = BASIS_DEFAULT_ZOOM;
			SetBasis(State, NewBasis);
		}

		if(DEBUGPress(C_PrevLength)) // TODO: don't do this when e.g. changing layers
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

		// TODO (UI): if panning, ignore input but still do preview
		// TODO: fix needed for if started and space released part way?
		if((C_PanMod.EndedDown && Mouse.LMB.EndedDown) || C_Pan.EndedDown)
		{ // pan canvas with mouse
			BASIS.Offset = V2Add(BASIS.Offset,
				V2Sub(V2ScreenToCanvas(BASIS, pMouse.P, ScreenCentre),
					  V2ScreenToCanvas(BASIS,  Mouse.P, ScreenCentre)));
			File.Pan = 1;
		}

		else if(DEBUGPress(C_Cancel))
		{
			State->maSelectedPoints.Used = 0;
			State->PerpDir = ZeroV2;
			// TODO: this is a bit hacky, will go when basing length intersection on shape
			if(State->InputMode == MODE_ExtendSeg)
			{ PopDiscard(&State->maIntersects); }
			State->InputMode = MODE_Normal;
		}

		else if(DEBUGPress(Keyboard.F1))
		{ State->ShowHelpInfo = !State->ShowHelpInfo; }

		else
		{ // process normal input
			// TODO (opt): jump straight to the right mode.
			switch(State->InputMode)
			{ // process input based on current mode
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

					int dLayer = DEBUGPress(C_LayerChange)*(C_LayerMod.EndedDown - 2*C_LayerRev.EndedDown);

					if((Keyboard.Ctrl.EndedDown && DEBUGPress(Keyboard.Z) && !Keyboard.Shift.EndedDown) &&
							(State->iCurrentAction > 0))
					{ // UNDO
						do { SimpleUndo(State); }
						while( ! IsUserAction(Pull(State->maActions, State->iCurrentAction).Kind));
						RecalcNeeded = 1;
					}
					if(((Keyboard.Ctrl.EndedDown && DEBUGPress(Keyboard.Y)) ||
						(Keyboard.Ctrl.EndedDown && Keyboard.Shift.EndedDown && DEBUGPress(Keyboard.Z))) &&
						(State->iCurrentAction < State->iLastAction))
					{ // REDO
						do { SimpleRedo(State); }
						while( ! IsUserAction(Pull(State->maActions, State->iCurrentAction).Kind));
						RecalcNeeded = 1;
					}

					if(C_BasisMod.EndedDown && DEBUGClick(C_BasisSet))
					{
						// TODO: should basis be settable during selection?
						State->poSaved = SnapMouseP;
						State->InputMode = MODE_SetBasis;
					}

					else if(Keyboard.Ctrl.EndedDown && DEBUGPress(Keyboard.A))
					{ // select all points on current layer
						State->maSelectedPoints.Used = 0;
						foreachf1(uint, PtLayer, State->maPointLayer)
							if(PtLayer == State->iCurrentLayer && POINTSTATUS(iPtLayer))
							{ Push(&State->maSelectedPoints, iPtLayer); }
						if(Len(State->maSelectedPoints))
						{ State->InputMode = MODE_Selected; }
					}

					else if(DEBUGClick(C_StartShape))
					{ // start drawing arc
						// NOTE: Starting a shape, save the first point
						State->poSelect = SnapMouseP;
						State->ArcSwapDirection = 0;
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

					else if(DEBUGClick(C_Select))
					{
						State->poSaved = ToScreen(SnapMouseP);
						State->maSelectedPoints.Used = 0;
						State->InputMode = MODE_BoxSelect;
					}
					
					else if(C_LayerMod.EndedDown && DEBUGPress(C_LayerDrawer))
					{
						State->tLayerDrawer = (f32)State->ShowLayerDrawer;
						State->ShowLayerDrawer = ! State->ShowLayerDrawer;
					}

					else if(dLayer)
					{ State->iCurrentLayer += dLayer; }

					if     (State->iCurrentLayer == 0)         { ++State->iCurrentLayer; }
					else if(State->iCurrentLayer > MAX_LAYERS) { --State->iCurrentLayer; }
				} break;


				case MODE_BoxSelect:
				case MODE_AddToSelection:
				{
					SelectionAABB = AABBFromPoints(State->poSaved, ToScreen(SnapMouseP));

					if( ! C_Select.EndedDown)
					{ // add points in selection box to selection
						foreachf1(v2,P, State->maPoints)
						if(POINTSTATUS(iP) && POINTLAYER(iP) == State->iCurrentLayer)
						{ // add indexes of selected points to that array
							// TODO: use animated basis rather than state
							P = ToScreen(P);
							if(PointInAABB(P, SelectionAABB))
							{
								foreachf(uint,ipoSelected, State->maSelectedPoints)
								{ // insert into array, keeping it in ascending order
									// TODO: make InsertSorted macro
									Assert(ipoSelected);
									if(iP == ipoSelected) { goto dont_append_selection; } // already in array
									else if(ipoSelected > iP)
									{ // found larger element, insert before it
										Insert(&State->maSelectedPoints, iipoSelected, iP);
										goto dont_append_selection;
									}
								}
								Push(&State->maSelectedPoints, iP);
dont_append_selection:;
							}
						}

						Assert(Len(State->maSelectedPoints) <= Len(State->maPoints));

						State->InputMode = MODE_Selected;
						goto case_mode_selected;
					}
				} break;


				case MODE_RmFromSelection:
				{
					SelectionAABB = AABBFromPoints(State->poSaved, ToScreen(SnapMouseP));
					if( ! C_Select.EndedDown)
					{
						foreach(uint,ipoTest, State->maSelectedPoints)
						{ // add indexes of selected points to that array
							if(PointInAABB(POINTS(ipoTest), SelectionAABB))
							{ // point inside box. If already selected, remove it.
								Remove(&State->maSelectedPoints, iipoTest--);
							}
						}

						State->InputMode = MODE_Selected;
						goto case_mode_selected;
					}
				} break;


				case MODE_Selected:
				{
case_mode_selected:
					if(Len(State->maSelectedPoints) == 0) { State->InputMode = MODE_Normal; break; }

					/* if(DEBUGClick(C_Drag)) */
					/* { */
					/* 	// TODO (feature): move to SnapMouseP (and combine coincident points) */
					/* 	State->poSaved = CanvasMouseP; */
					/* 	State->InputMode = MODE_DragMove; */
					/* } */

					if(DEBUGClick(C_Select))
					{ // add or remove points to/from selected array
						State->poSaved = ToScreen(SnapMouseP);
						if(C_SelectMod.EndedDown) // remove points from selected array
						{ State->InputMode = MODE_RmFromSelection; }
						else // add points to selected array
						{ State->InputMode = MODE_AddToSelection; }
					}

					else if(DEBUGPress(C_Delete))
					{
						// TODO: UI: to determine whether or not to keep the other point(s) in a shape:
						// find in action list, see if the actions before were non-user points.
						// If they were, check other shapes to see if they're used, if not, then delete.

						uint cPointsSelected = (uint)Len(State->maSelectedPoints);
						if(cPointsSelected)
						{
							uint iLastPoint = cPointsSelected-1;
							for(uint i = 0; i < iLastPoint; ++i)
							{ InvalidatePoint(State, Pull(State->maSelectedPoints, i), -ACTION_RemovePt); }
							InvalidatePoint  (State, Pull(State->maSelectedPoints, iLastPoint), ACTION_RemovePt);
						}

						RecalcNeeded = 1;
						State->maSelectedPoints.Used = 0;
						State->InputMode = MODE_Normal;
					}
				} break;


				// TODO: make sure you can mix and match DEBUG_LIVE defines and globals
				/* case MODE_DragMove: */
				/* { */
				/* 	// TODO: change to SnapMouseP (after point consolidation) */
				/* 	// TODO (fix): decide what to do with arc endpoint dragging */
				/* 	// probably worth asserting that their DistSq from the focus never differs (+/- eps) */
				/* 	DragDir = V2Sub(CanvasMouseP, State->poSaved); */

				/* 	if( ! C_Drag.EndedDown) */
				/* 	{ */
				/* 		foreachf(uint, ipo, State->maSelectedPoints) */
				/* 		{ // set action to non user move */
				/* 			POINTS(ipo) = V2Add(POINTS(ipo), DragDir); */
				/* 			AdjustMatchingArcPoint(State->maShapes, State->maPoints, ipo); */

				/* 			action Action      = {0}; */
				/* 			Action.Kind        = -ACTION_Move; */
				/* 			Action.Move.ipo[0] = ipo; */
				/* 			Action.Move.Dir    = DragDir; */
				/* 			if(++iipo < (uint)Len(State->maSelectedPoints)) */
				/* 			{ */
				/* 				ipo = Pull(State->maSelectedPoints, iipo); */
				/* 				POINTS(ipo) = V2Add(POINTS(ipo), DragDir); */
				/* 				AdjustMatchingArcPoint(State->maShapes, State->maPoints, ipo); */
				/* 				Action.Move.ipo[1] = ipo; */
				/* 			} */
				/* 			AddAction(State, Action); */
				/* 		} // change the last one to a user move */
				/* 		Pull(State->maActions, State->iCurrentAction).Kind = ACTION_Move; */

				/* 		// TODO (opt): could buffer the move until returning to normal mode */
				/* 		// so lots of partial moves look like only one */
				/* 		RecalcNearScreenIntersects(State); */
				/* 		if(C_SelectMod.EndedDown) */
				/* 		{ State->InputMode = MODE_Selected; } */
				/* 		else */
				/* 		{ State->InputMode = MODE_Normal; } */
				/* 	} */
				/* } break; */


				case MODE_SetBasis:
				{ // rotate canvas basis
					// TODO (UI): if MMB(?) held, set zoom as well - box aligned to new axis showing where screen will end up
					if(!C_BasisSet.EndedDown)
					{ // set basis on release
						if( ! V2Equals(SnapMouseP, State->poSaved)) // prevents XAxis with no length
						{ // set new basis and start animation
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
						{ AddPoint(State, State->poSelect, ACTION_Point); }
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
								AddPoint(State, State->poSelect, ACTION_Point); 
								State->InputMode = MODE_Normal;
							}
							else
							{ State->InputMode = MODE_SetPerp; }
						}
						else
						{ // extend segment, possibly through perpendicular
							// TODO (UI): this should be based on shape snapping rather than intersection
							// otherwise can accidentally 'extend' to circumference near intended point
							v2 poAtLength = ClosestPtOnCircle(SnapMouseP, State->poSelect, State->Length);
							AddIntersection(State, poAtLength);

							if(V2Equals(State->PerpDir, ZeroV2)) // perpendicular not set
							{ State->poSaved = SnapMouseP; }     // set current mouse as point to extend through
							else                                 // extend through the perpendicular
							{ State->poSaved = V2Add(State->poSelect, State->PerpDir); }
							State->PerpDir = ZeroV2;

							State->InputMode = MODE_ExtendSeg;
							goto input_mode_extendseg;
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
							goto case_mode_extend_arc;
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


				case MODE_ExtendArc:
				{
case_mode_extend_arc:
					poAtDist = ChooseCirclePoint(State, CanvasMouseP, SnapMouseP, C_ShapeLock.EndedDown);

					v2 poFocus = State->poSelect;
					v2 poArcInit = State->poArcStart;
					{ // arc is smallest, unless goes round the back of focus
						v2 RelMouse  = V2Sub(SnapMouseP,  poFocus);
						v2 pRelMouse = V2Sub(State->pSnapMouseP, poFocus);

						v2 ArcDirX   = V2Sub(poArcInit, poFocus);
						v2 ArcDirY   = Perp(ArcDirX);
						b32 IsForward  = Dot(ArcDirX, RelMouse)  >= 0; // mouse is forward of focus
						b32 IsLeft     = Dot(ArcDirY, RelMouse)  >= 0;
						b32 WasLeft    = Dot(ArcDirY, pRelMouse) >= 0;

						// TODO (ui): modifier to reverse direction?
						if( ! IsForward && WasLeft != IsLeft)
						{ State->ArcSwapDirection = !State->ArcSwapDirection; }

						b32 DrawCW = IsLeft != State->ArcSwapDirection;
						if(DrawCW)
						{
							poArcStart = poArcInit;
							poArcEnd   = poAtDist;
						}
						else
						{
							poArcStart = poAtDist;
							poArcEnd   = poArcInit;
						}
					}

					// NOTE: Not using button released in case it's missed for some reason
					// also possible this fires at a weird time when focus returns or something...
					if(!C_Arc.EndedDown)
					{ // finish drawing arc/circle
						// TODO IMPORTANT (fix): don't add when no shape intersected
						if(V2WithinEpsilon(SnapMouseP, poFocus, POINT_EPSILON) ||
						   V2WithinEpsilon(poAtDist, State->poArcStart, POINT_EPSILON))
						{ // Same angle -> full circle
							if(C_PointOnly.EndedDown)
							{ // add point intersecting shape 
								AddPoint(State, poFocus,  -ACTION_Point);
								AddPoint(State, poAtDist,  ACTION_Point);
							}
							else
							{ AddCircleAtPoints(State, poFocus, State->poArcStart); }
						}

						else
						{ // set points for arc
							if(C_PointOnly.EndedDown)
							{ // add point intersecting shape 
								AddPoint(State, poFocus,    -ACTION_Point);
								AddPoint(State, poArcStart, -ACTION_Point);
								AddPoint(State, poArcEnd,    ACTION_Point);
							}
							else
							{ AddArcAtPoints(State, poFocus, poArcStart, poArcEnd); }
						}
						State->InputMode = MODE_Normal;
						Assert(CurrentActionIsByUser(State)); 
					}
				} break;


				case MODE_ExtendSeg:
				{ // find point on shape closest to mouse along line
input_mode_extendseg:
					// TODO (fix): preview point pulling away from shape on shape snap
					// TODO (fix): should be snapping to opposite side of circle
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
							AddPoint(State, State->poSelect, -ACTION_Point);
							AddPoint(State, poOnLine,         ACTION_Point);
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

	shape_arena *maShapesNearScreen = &State->maShapesNearScreen;
	uint_arena  *maSelectedPoints   = &State->maSelectedPoints;
	// provides temporary buffer so that points can be dragged around
	// and the shapes that rely on them will follow live.
	v2_arena    *maPointsOnScreen   = &State->maPointsOnScreen;
	maShapesNearScreen->Used = 0;
	maPointsOnScreen->Used = State->maPoints.Used;
	uint cShapesNearScreen = 0;
	aabb ScreenBB;
	{ LOG("CULL");
	/////////////////////
		uint iLastShape = State->iLastShape;
		uint iLastPoint = State->iLastPoint;
		DEBUG_LIVE_if(Rendering_SmallScreenBoundary)
		{
			v2 FakeScreenMin = V2Mult(0.2f, ScreenSize);
			v2 FakeScreenMax = V2Mult(0.8f, ScreenSize);
			ScreenBB.MinX = FakeScreenMin.X;
			ScreenBB.MinY = FakeScreenMin.Y;
			ScreenBB.MaxX = FakeScreenMax.X;
			ScreenBB.MaxY = FakeScreenMax.Y;
			DrawAABB(Draw, ScreenBB, GREEN);
		}
		else // use a full size screen
		{
			ScreenBB.MinX = 0.f;
			ScreenBB.MinY = 0.f;
			ScreenBB.MaxX = ScreenSize.X;
			ScreenBB.MaxY = ScreenSize.Y;
		}

		v2 *Points = State->maPoints.Items;
		shape *Shapes = State->maShapes.Items;
		for(uint iShape = 1; iShape <= iLastShape; ++iShape)
		{
			shape Shape = Shapes[iShape];
			if(Shape.Kind > SHAPE_Free)
			{
				// create local copy of shape with basis applied
				shape LocalShape = Shape;
				v2 ShapePoints[NUM_SHAPE_POINTS];
				for(uint i = 0; i < NUM_SHAPE_POINTS; ++i)
				{
					// TODO (opt): init all shape points to 0 so I don't need to check
					// that point is inside mem bounds?
					uint ipo = Shape.P[i];
					v2 P = ipo <= iLastPoint ? Points[ipo] : ZeroV2;
					ShapePoints[i] = ToScreen(P);
					LocalShape.P[i] = i;
				}
				aabb ShapeBB = AABBFromShape(ShapePoints, LocalShape);
				/* DrawAABB(Draw.Buffer, ShapeBB, ORANGE); */
				b32 ScreenIsInsideShape = 0;
				if(Shape.Kind == SHAPE_Arc || Shape.Kind == SHAPE_Circle)
				{
					v2 SSFocus = ShapePoints[LocalShape.Circle.ipoFocus];
					f32 SSRadiusSq = DistSq(SSFocus, ShapePoints[LocalShape.Circle.ipoRadius]);
					ScreenIsInsideShape = ScreenIsInsideCircle(ScreenBB, SSFocus, SSRadiusSq);
				}

				if(AABBOverlaps(ScreenBB, ShapeBB) && ! ScreenIsInsideShape)
				{ // add shape to array of shapes on screen
					Push(maShapesNearScreen, Shape);
					++cShapesNearScreen;
				}
			}
		}
		if(RecalcNeeded)
		{ RecalcNearScreenIntersects(State); }
		Assert(cShapesNearScreen == maShapesNearScreen->Used/sizeof(*maShapesNearScreen->Items));

		// TODO (opt): probably don't need to do every frame
		// alternatively, could get maShapesNearScreen to change their indices...
		foreachf1(v2, po, State->maPoints)
		{ 
			// NOTE: needed in canvas form later
			/* if(POINTSTATUS(ipo) != POINT_Free && */
			/*    PointInAABB(ToScreen(po), ScreenBB)) */
			{
				Pull(*maPointsOnScreen, ipo) = po;
			}
		}
	}

	if(State->tSnapMouseP < 1.f)
	{ State->tSnapMouseP += State->dt*MOUSE_ANIMATION_SPEED; }
	State->tSnapMouseP = Clamp01(State->tSnapMouseP);
	v2 AnimSnapMouseP = V2Lerp(State->pSnapMouseP, State->tSnapMouseP, SnapMouseP);
	{ LOG("RENDER");
	////////////////
		DrawCrosshair(Draw, ScreenCentre, ACTIVE_POINT_RADIUS, LIGHT_GREY);
		// TODO: move up for other things
		v2 pSSSnapMouseP = ToScreen(State->pSnapMouseP);
		v2 SSSnapMouseP = ToScreen(AnimSnapMouseP);
		DrawCrosshair(Draw, SSSnapMouseP, ACTIVE_POINT_RADIUS, GREY);

		/* if(State->InputMode == MODE_DragMove) */
		/* { // offset dragged points and arc counterparts */
		/* 	foreachf(uint, ipo, *maSelectedPoints) */
		/* 	{ */
		/* 		v2 *P = &Pull(*maPointsOnScreen, ipo); */
		/* 		*P = V2Add(*P, DragDir); */
		/* 	} */
		/* 	foreachf(shape, Shape, State->maShapes) */
		/* 	{ // ensure arc lengths are always consistent */
		/* 		if(Shape.Kind == SHAPE_Arc) */
		/* 		{ */
		/* 			if(PointIsSelected(State, Shape.Arc.ipoStart)) */
		/* 			{ */
		/* 				v2 Focus = POINTS_OS(Shape.Arc.ipoFocus); */
		/* 				v2 *P = &Pull(*maPointsOnScreen, Shape.Arc.ipoEnd); */
		/* 				*P = ClosestPtOnCircle(*P, Focus, Dist(Focus, POINTS_OS(Shape.Arc.ipoStart))); */
		/* 			} */
		/* 			else if(PointIsSelected(State, Shape.Arc.ipoEnd)) */
		/* 			{ */
		/* 				v2 Focus = POINTS_OS(Shape.Arc.ipoFocus); */
		/* 				v2 *P = &Pull(*maPointsOnScreen, Shape.Arc.ipoStart); */
		/* 				*P = ClosestPtOnCircle(*P, Focus, Dist(Focus, POINTS_OS(Shape.Arc.ipoEnd))); */
		/* 			} */
		/* 		} */
		/* 	} */
		/* } */

		RenderDrawing(*Draw, State, Basis, Origin, ScreenSize, 0, POINT_RADIUS);
		DEBUG_LIVE_if(Shapes_ShowClosestPoint)
		{
			foreachf(shape, Shape, *maShapesNearScreen)
			{
				v2 Pts[ArrayCount(Shape.P)] = {0};
				for(uint i = ArrayCount(Pts); i--;) { Pts[i] = ToScreen(POINTS(Shape.P[i])); }
				switch(Shape.Kind) {
					case SHAPE_Segment: DrawClosestPtOnSegment(Draw, Mouse.P, Pts[0], Pts[1]); break;
					case SHAPE_Circle:  DrawClosestPtOnCircle( Draw, Mouse.P, Pts[0], Dist(Pts[0],Pts[1])); break;
					case SHAPE_Arc:		DrawClosestPtOnArc(	   Draw, Mouse.P, Pts[0], Pts[1], Pts[2]); break;
					default:            Assert(! "Tried to debug unknown shape");
				}
			}
		}

		// TODO: consider separating dragmove's logic entirely
		/* if(State->InputMode != MODE_DragMove) */
		/* { // snapping preview */
		/* 	v2 poSSClosest = ToScreen(poClosest); */
		/* 	b32 ValidPointExists = 0; // TODO: may be able to determine at calculation of poClosest */
		/* 	for(uint i = 1; i <= State->iLastPoint; ++i) */
		/* 	{ if(POINTLAYER(i) != POINT_Free) { ValidPointExists = 1; break; } } */
		/* 	if(ValidPointExists) */
		/* 	{ CircleLine(Draw->BufferDraw.Buffer, poSSClosest, ACTIVE_POINT_RADIUS, GREY); } */

		/* 	if(IsSnapped) */
		/* 	{ // draw snapped point */
		/* 		DrawCircleFill(Draw->BufferDraw.Buffer, poSSClosest, POINT_RADIUS, BLUE); */ 
		/* 		// NOTE: Overdraws... */
		/* 		DrawActivePoint(DrawDraw.Buffer, poSSClosest, BLUE); */
		/* 	} */
		/* } */


		LOG("\tDRAW PREVIEW");
		// TODO (UI): animate previews in and out by smoothstepping alpha over a few frames
		// so that they don't pop too harshly when only seen briefly
		// TODO QUICK (UI): when mid basis-change, use that value rather than the new one...
		DEBUG_WATCH(f32, SSLength) = State->Length/Basis.Zoom; 
		DEBUG_WATCHED_EQ(v2, Vectors_Points, poSelect, State->poSelect);

		v2 poSSSelect = ToScreen(poSelect);
		v2 poSSSaved  = ToScreen(State->poSaved);
		b32 DrawPreviewCircle = ! ScreenIsInsideCircle(ScreenBB, poSSSelect, SSLength * SSLength);
		Draw->Smear = V2Sub(pSSSnapMouseP, SSSnapMouseP);
		switch(State->InputMode)
		{ // draw mode-dependent preview
			case MODE_Normal:
			{
				// TODO (UI): animate when (un)snapping
				if(DrawPreviewCircle)
				{
					if(State->FX[FX_Smear])
					{ DrawCircleLineSmear(Draw, SSSnapMouseP, SSLength, Col_Preview); }
					DrawCircleLine(Draw, SSSnapMouseP, SSLength,     Col_Preview);
				}
				if(C_ShapeLock.EndedDown)
				{ DrawCircleLine(Draw, SSSnapMouseP, POINT_RADIUS, Col_Preview); }
			} break;


			case MODE_SetBasis:
			{
				DrawSeg(Draw, poSSSaved, SSSnapMouseP, Col_BasisLine);
			} break;


			case MODE_SetLength:
			{
				if( ! V2WithinEpsilon(SnapMouseP, poSelect, POINT_EPSILON) && ! C_PanMod.EndedDown)
				{ SSLength = Dist(poSSSelect, SSSnapMouseP); }
				if(DrawPreviewCircle)
				{ DrawCircleLine(Draw, poSSSelect, SSLength, Col_Preview); }
				DrawSeg(Draw, poSSSelect, SSSnapMouseP, Col_SetLength);
			} break;


			case MODE_QuickPtOrSeg:
			{
				DrawSeg(Draw, poSSSelect, SSSnapMouseP, Col_DrawPreview);
				DrawActivePoint(Draw, poSSSelect, Col_ActivePt);
			} break;


			case MODE_BoxSelect:
			case MODE_AddToSelection:
			{
				foreachf1(v2, P, State->maPoints) if(POINTLAYER(iP))
				{
					P = ToScreen(P);
					if(PointInAABB(P, SelectionAABB))
					{ DrawActivePoint(Draw, P, Col_SelectedPt); }
				}
			} // fallthrough
			case MODE_RmFromSelection:
			case MODE_Selected:
			{
				DrawAABB(Draw, SelectionAABB, Col_SelectBox);
				foreachf(uint, ipo, *maSelectedPoints)    if(ipo)
				{
					v2 P = POINTS(ipo);
					if(State->InputMode == MODE_RmFromSelection && PointInAABB(P, SelectionAABB))
					{ DrawActivePoint(Draw, ToScreen(P), Col_UnselectedPt); }
					else
					{ DrawActivePoint(Draw, ToScreen(P), Col_SelectedPt); }
				}
			} break;

			/* case MODE_DragMove: */
			/* { */
			/* 	foreachf(uint, ipo, *maSelectedPoints)    if(ipo) */
			/* 	{ */
			/* 		v2 P = POINTS(ipo); */
			/* 		v2 PMoved = V2Add(P, DragDir); */
			/* 		v2 SSP = ToScreen(P); */
			/* 		v2 SSPMoved = ToScreen(PMoved); */
			/* 		DrawSeg(Draw, SSP, SSPMoved, LIGHT_GREY); */
			/* 		DrawCircleFill(DrawBuffer, SSPMoved, POINT_RADIUS, BLUE); */
			/* 	} */
			/* } break; */

			case MODE_Draw:
			{
				v2 poSSEnd = SSSnapMouseP;
				v2 poSSAtDist = ToScreen(poAtDist);
				if( ! V2Equals(State->PerpDir, ZeroV2))
				{ // draw perpendicular segment
					v2 poSSDir = ToScreen(V2Add(poSelect, State->PerpDir));
					poSSEnd = ExtendSegment(poSSSelect, poSSDir, SSSnapMouseP);
				}
				// preview circle at given length and segment
				if(DrawPreviewCircle)
				{ DrawCircleLine(Draw, poSSSelect, SSLength, Col_DrawPreview); }
				DrawActivePoint(Draw, poSSAtDist, Col_ActivePt);
				DrawActivePoint(Draw, poSSSelect, Col_ActivePt);
				DrawSeg(Draw, poSSSelect, poSSEnd, Col_DrawPreview);
			} break;


			case MODE_ExtendArc:
			{ // preview drawing arc
				DrawActivePoint(Draw, poSSSelect, Col_ActivePt);
				LOG("\tDRAW HALF-FINISHED ARC");
				v2 poStart = State->poArcStart;
				v2 poSSStart = ToScreen(poArcStart);
				DrawSeg(Draw, poSSSelect, poSSStart, Col_ArcLines);
				if(DrawPreviewCircle && V2WithinEpsilon(poStart, poAtDist, POINT_EPSILON))
				{ DrawCircleLine(Draw, poSSSelect, SSLength, Col_Shape); }
				else
				{
					v2 poSSEnd   = ToScreen(poArcEnd);
					DrawSeg(Draw, poSSSelect, poSSEnd, Col_ArcLines);
					DrawArcFromPoints(Draw, poSSSelect, poSSStart, poSSEnd, Col_Shape);
				}
			} break;


			case MODE_ExtendSeg:
			{ // preview extending a line
				v2 poSSDir = ToScreen(State->poSaved);
				v2 poSSOnLine = ToScreen(poOnLine);
				if(DrawPreviewCircle)
				{ DrawCircleLine(Draw, poSSSelect, SSLength, Col_Preview); }
				DrawLine(Draw, poSSSelect, V2Sub(poSSDir, poSSSelect), Col_LineExtend);
				DrawSeg(Draw, poSSSelect, poSSOnLine, Col_Shape);
				DrawActivePoint(Draw, poSSOnLine, Col_ActivePt);
			} break;


			case MODE_SetPerp:
			{
				DEBUG_WATCH(v2, PerpDir) = State->PerpDir;
				if( ! V2Equals(PerpDir, ZeroV2))
				{
					v2 poSSStart = ToScreen(poSelect);
					v2 poSSPerp  = ToScreen(V2Add(poSelect, PerpDir));
					v2 poSSNPerp = ToScreen(V2Add(poSelect, V2Neg(PerpDir)));
					DrawSeg(Draw, poSSStart, SSSnapMouseP, Col_Perp);
					DrawSeg(Draw, poSSNPerp, poSSPerp,     Col_Perp);
				}
			} break;
		}

		if(!V2Equals(gDebugV2, ZeroV2))
		{ // draw debug vector
			DrawSeg(Draw, ScreenCentre, V2Add(ScreenCentre, gDebugV2), ORANGE);
		}
		if(!V2Equals(gDebugPoint, ZeroV2))
		{ // draw debug point
			if(IsDrawing(State))
			{ DrawActivePoint(Draw, ToScreen(gDebugPoint), ORANGE); }
		}

		{ // Animate layer drawer
			f32 tLayerDrawer = State->tLayerDrawer;
			if(State->ShowLayerDrawer && tLayerDrawer < 1.f)
			{ tLayerDrawer += State->dt*DRAWER_ANIMATION_SPEED; }
			else if(! State->ShowLayerDrawer && tLayerDrawer > 0.f)
			{ tLayerDrawer -= State->dt*DRAWER_ANIMATION_SPEED; }
			tLayerDrawer = Clamp01(tLayerDrawer);
			State->tLayerDrawer = tLayerDrawer;
			// Draw layer thumbnails
			uint cThumbs = MAX_LAYERS;
			f32 ThumbScreenFraction = 1.f/cThumbs;
			basis ThumbBasis = Basis;
			ThumbBasis.Zoom *= (f32)cThumbs;
			v2 TargetThumbSize = V2Mult(ThumbScreenFraction, ScreenSize);
			v2 ThumbSize = { Lerp(0, tLayerDrawer, TargetThumbSize.X), TargetThumbSize.Y };
			v2 ThumbBL = { ScreenSize.X - ThumbSize.X, 0.f };
			// TODO (fix): sometimes clips before the edge of the thumbnail)
			for(uint iThumb = 0; iThumb++ < cThumbs;)
			{
				v2 ThumbTR = { ScreenSize.X, ThumbBL.Y + ThumbSize.Y }; // V2Add(ThumbBL, ThumbSize);
				DrawRectFill(Draw, ThumbBL, ThumbTR, PreMultiplyColour(WHITE, 0.8f));
				RenderDrawing(*Draw, State, ThumbBasis, ThumbBL, ThumbSize, iThumb, 2.f);
				if(iThumb != State->iCurrentLayer)
				{ DrawRectLine(Draw, ThumbBL, ThumbTR, Col_ThumbOutline); }
				ThumbBL.Y += ThumbSize.Y;
			}

			{
				ThumbBL.Y = ThumbSize.Y * (State->iCurrentLayer - 1);
				v2 ThumbTR = { ScreenSize.X, ThumbBL.Y + ThumbSize.Y };
				v2 One = {1.f, 1.f};
				v2 Two = {2.f, 2.f};
				DrawRectLine(Draw,             ThumbBL,             ThumbTR, Col_ThumbSelected);
				DrawRectLine(Draw, V2Add(ThumbBL, One), V2Sub(ThumbTR, One), Col_ThumbSelected);
				DrawRectLine(Draw, V2Add(ThumbBL, Two), V2Sub(ThumbTR, Two), Col_ThumbSelected);
			}
		}

	}


	/* f32 TextSize = ScreenSize.Y/40.f; */
	DEBUG_WATCHED(f32, Text, TextSize) = ScreenSize.Y/40.f;
	if(TextSize > 20.f)  { TextSize = 20.f; }

	if(State->ShowHelpInfo)
	{ LOG("PRINT HELP");
		DrawRectFill(Draw, Origin, ScreenSize, PreMultiplyColour(WHITE, 0.8f));
		char LeftHelpBuffer[] =
			"Drawing\n"
			"=======\n"
			" LMB-drag - set length/radius\n"
			" Alt-LMB  - quick draw point\n"
			"  -> drag - quick draw segment\n"
			" LMB      - start drawing shape:\n"
			"\n"
			" Arcs (compass)\n"
			" --------------\n"
			"   -> LMB      - circle\n"
			"   -> LMB-drag - arc\n"
			"\n"
			" Segments/lines (straight-edge)\n"
			" ------------------------------\n"
			"   -> RMB      - line\n"
			"   -> RMB-drag - extend line\n"
			"     -> fromPt - set perpendicular\n"
			"\n"
			" Alt+^^^       - only draw points, not shapes\n"
			"\n"
			"Selection\n"
			"=========\n"
			" RMB       - add point(s) to selection\n"
			"   -> drag - box select\n"
			" Alt+^^^   - remove point(s) from selection\n"
			" Delete    - delete selected points (+shapes)\n"
			" Ctrl+A    - select all on current layer\n"
			"\n"
			"Modifiers\n"
			"=========\n"
			" Esc      - cancel\n"
			" Ctrl     - snap to shapes\n"
			" Shift    - no snapping\n"
			" Alt      - general modifier (number store...)\n"
			" Space    - canvas modifier (pan, basis)";

		char RightHelpBuffer[] =
			"Canvas/view manipulation\n"
			"========================\n"
			" MMB-drag       - pan viewport around canvas\n"
			" Arrow keys     - pan viewport around canvas\n"
			" Space+LMB-drag - pan viewport around canvas\n"
			" Space+RMB-drag - set horizontal of viewport (rotate)\n"
			" Scroll         - zoom to cursor\n"
			" PgUp/PgDn      - zoom to centre\n"
			" Home           - return to centre\n"
			" Backspace      - reset canvas drawing\n"
			" Alt+Enter      - fullscreen\n"
			" Ctrl+Tab       - up a layer\n"
			" Ctrl+Shift+Tab - down a layer\n"
			" Ctrl+T         - toggle layer thumbnails\n"
			"\n"
			"Length/radius manipulation\n"
			"==========================\n"
			" 2-0      - divide length by 2-10\n"
			" Alt+2-0  - multiply length by 2-10\n"
			" a-z,A-Z  - get stored length/radius\n"
			" Alt+a-Z  - set stored length/radius\n"
			" Tab      - swap to previously used length\n"
			"\n"
			"File manipulation\n"
			"=================\n"
			" Ctrl+Z    - undo\n"
			" Ctrl+Y    - redo\n"
			" Ctrl+Sh+Z - redo\n"
			" Ctrl+S    - save file\n"
			" Ctrl+Sh+S - save file as...\n"
			" Ctrl+O    - open file\n"
			" Ctrl+Sh+O - open file in new window\n"
			" Ctrl+N    - new file\n"
			" Ctrl+Sh+N - new file in new window" ;

		DrawString(&Draw->Buffer, &State->DefaultFont, LeftHelpBuffer,  TextSize, 10.f, ScreenSize.Y-2.f*TextSize, 0, BLACK);
		DrawString(&Draw->Buffer, &State->DefaultFont, RightHelpBuffer, TextSize, ScreenSize.X - 32.f*TextSize, ScreenSize.Y-2.f*TextSize, 0, BLACK);
	}

	DEBUG_LIVE_if(Debug_ShowInfo)
	{ LOG("PRINT DEBUG");
#if !SINGLE_EXECUTABLE
		PrintDebugHierarchy(Draw->Buffer, Input);
#endif//!SINGLE_EXECUTABLE
		DEBUG_LIVE_if(Debug_PrintMidFrameInfo)
		{ DebugPrint(); }

		DEBUG_WATCHED(f32, Profiling, FrameWork) = State->dtWork * 1000.f;
		DEBUG_WATCH(v2, Input_Mouse) = Mouse.P;
		DEBUG_WATCH(uint, CurrentLayer) = State->iCurrentLayer;

		DEBUG_LIVE_if(Debug_PrintPointDetails)
		{
			char TextInfoBuffer[512];
			*TextInfoBuffer = 0;
			/* ssprintf(TextInfoBuffer, "L#  P#\n\n"); */
			/* for(uint i = 1; i <= State->iLastShape && i <= 32; ++i) */
			/* { */
			/*	 ssprintf(TextInfoBuffer, "%s%02u  %04b\n", TextInfoBuffer, i, SHAPES(i).Kind); */
			/* } */
			/* DrawString(Draw->Buffer, &State->DefaultFont, TextInfoBuffer, TextSize, */
			/*		 ScreenSize.X - 180.f, ScreenSize.Y - 30.f, 0, BLACK); */

			*TextInfoBuffer = 0;
			for(uint i = 1; i <= State->iLastPoint && i <= 32; ++i)
			{
				v2 po = Pull(State->maPoints, i);
				ssprintf(TextInfoBuffer, "%s%02u (%f, %f)\n", TextInfoBuffer, i, po.X, po.Y);
			}
			DrawString(&Draw->Buffer, &State->DefaultFont, TextInfoBuffer, TextSize,
					ScreenSize.X - 320.f, ScreenSize.Y - 4.5f*TextSize, 0, Col_Text);
		}
	}

	State->pSnapMouseP = AnimSnapMouseP;

	CLOSE_LOG();
	END_TIMED_BLOCK;
	return File;
}

#define GEOMETER_DEBUG_IMPLEMENTATION
#include "geometer_debug.c"
#undef ToScreen
