internal void AddAction(state *State, action Action);

///////////////////////////////////////////////////////////////////////////////
//  BASIS  ////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

internal void
SetBasis(state *State, basis NewBasis)
{
	State->tBasis = 0.f;
	NewBasis.XAxis = Norm(NewBasis.XAxis);
	pBASIS = BASIS;
	BASIS = NewBasis;
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


///////////////////////////////////////////////////////////////////////////////
//  POINTS  ///////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

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
		{ ++Result; }
	}
	END_TIMED_BLOCK;
	return Result;
}
/// returns the first free point up to the last point used
/// returns 0 if none found
internal inline uint
FirstFreePoint(state *State)
{
	uint Result = 0;
	for(uint ipo = 1; ipo <= State->iLastPoint; ++ipo)
	{
		if(POINTSTATUS(ipo) == POINT_Free)
		{
			Result = ipo;
			break;
		}
	}
	return Result;
}

/// returns true if a point is added/updated (i.e. an action is needed)
internal b32
AddPointNoAction(state *State, v2 po, uint PointStatus, u8 *PriorStatus, uint *ipoOut)
{
	BEGIN_TIMED_BLOCK;
	/* gDebugV2 = po; */
	b32 Result = 0;
	uint ipo = FindPointAtPos(State, po, ~(uint)POINT_Free);
	if(ipo) // point exists already
	{
		// NOTE: Use existing point, but add any new status (and confirm Extant)
		if(PriorStatus) { *PriorStatus = POINTSTATUS(ipo); }
		if((POINTSTATUS(ipo) & (PointStatus | POINT_Extant))) // with full status
		{ goto end; } // no changes needed; exit
		else // status needs updating
		{ POINTSTATUS(ipo) |= PointStatus | POINT_Extant; }
	}

	else 
	{ // add a new point
		if(PriorStatus) { *PriorStatus = POINT_Free; }
		ipo = FirstFreePoint(State);
		// NOTE: Create new point if needed
		if(!ipo)
		{
			PushStruct(&State->maPoints, v2);
			PushStruct(&State->maPointStatus, u8);
			ipo = ++State->iLastPoint;
		}
		POINTS(ipo) = po;
		POINTSTATUS(ipo) |= PointStatus | POINT_Extant;
	}

	Result = 1;
end:
	*ipoOut = ipo;
	END_TIMED_BLOCK;
	return Result;
}

/// returns index of point (may be new or existing)
internal uint
AddPoint(state *State, v2 po, uint PointStatus, u8 *PriorStatus, action_types PointType)
{
	uint Result = 0;
	if(AddPointNoAction(State, po, PointStatus, PriorStatus, &Result))
	{
		action Action;
		Action.Kind = PointType;
		Action.i = Result;
		Action.po = po;
		Action.PointStatus = POINTSTATUS(Result);
		AddAction(State, Action);
	}
	DebugReplace("AddPoint => %u\n", Result);
	return Result;
}

internal inline void InvalidateShapesAtPoint(state *State, uint ipo);
internal inline void
InvalidatePoint(state *State, uint ipo)
{
	BEGIN_TIMED_BLOCK;
	InvalidateShapesAtPoint(State, ipo);
	POINTSTATUS(ipo) = POINT_Free;
	action Action;
	Action.Kind = ACTION_RemovePt;
	Action.i = ipo;
	AddAction(State, Action);
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


///////////////////////////////////////////////////////////////////////////////
//  INTERSECTIONS  ////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

internal uint
ClosestIntersectIndex(state *State, v2 Comp, f32 *ClosestDistSq)
{
	BEGIN_TIMED_BLOCK;
	v2 *Intersects = (v2 *)State->maIntersects.Base;
	uint iLast = (uint)ArenaCount(State->maIntersects, v2) - 1;
	uint Result = 0;
	f32 Closest = 0;
	if(iLast)
	{
		Result = 1;
		Closest = DistSq(Intersects[1], Comp);
		for(uint i = 2; i <= iLast; ++i)
		{
			f32 Test = DistSq(Intersects[i], Comp);
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

/// For 2 lines, Intersection2 can be 0
internal inline uint
IntersectShapes(v2 *Points, shape S1, shape S2, v2 *Intersection1, v2 *Intersection2)
{
	BEGIN_TIMED_BLOCK;
	uint Result = 0;
	if(S1.Kind == SHAPE_Free || S2.Kind == SHAPE_Free) goto end;
	// NOTE: should be valid for circles or arcs
	f32 S1Radius = Dist(Points[S1.Circle.ipoFocus], Points[S1.Circle.ipoRadius]);
	f32 S2Radius = Dist(Points[S2.Circle.ipoFocus], Points[S2.Circle.ipoRadius]);
	v2 S1Dir = V2Sub(Points[S1.Line.P2], Points[S1.Line.P1]);
	v2 S2Dir = V2Sub(Points[S2.Line.P2], Points[S2.Line.P1]);
#define LPOINTS(x) Points[S##x.Line.P1], Points[S##x.Line.P2]
#define LINE(x) Points[S##x.Line.P1], S##x##Dir
#define CIRCLE(x) Points[S##x.Circle.ipoFocus], S##x##Radius
#define ARC(x) Points[S##x.Arc.ipoFocus], S##x##Radius, Points[S##x.Arc.ipoStart], Points[S##x.Arc.ipoEnd]
	switch(S1.Kind)
	{
		case SHAPE_Segment:
		switch(S2.Kind)
		{
			case SHAPE_Segment:
				Result = IntersectSegmentsWinding (LPOINTS(1), LPOINTS(2),Intersection1);                 break;
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


internal inline uint
CountShapeIntersects(v2 *Points, shape *Shapes, uint cShapes)
{
	uint cIntersects = 0;
	v2 po1, po2;
	for(uint i = 0; i < cShapes; ++i)
	{
		for(uint j = 0; j < cShapes; ++j)
		{
			if(j == i) { continue; }
			cIntersects += IntersectShapes(Points, Shapes[i], Shapes[j], &po1, &po2);
		}
	}
	return cIntersects;
}

// TODO: don't auto-add intersections - only suggest when mouse is near
internal inline void
AddIntersection(state *State, v2 po)
{
	// TODO (opt): only add if on screen
	if( ! FindPointAtPos(State, po, ~(uint)POINT_Free))
	{ // given that there isn't a point already, add an intersection
		AppendStruct(&State->maIntersects, v2, po);
	}
}

internal inline void
AddIntersections(state *State, v2 po1, v2 po2, uint cIntersections)
{
	BEGIN_TIMED_BLOCK;
	if(cIntersections == 1)
	{
		AddIntersection(State, po1);
	}
	else if(cIntersections == 2)
	{
		AddIntersection(State, po1);
		AddIntersection(State, po2);
	}
	else { Assert(!cIntersections); }
	END_TIMED_BLOCK;
}

internal inline void
AddIntersectionsAsPoints(state *State, v2 po1, v2 po2, uint cIntersections)
{
	BEGIN_TIMED_BLOCK;
	if(cIntersections == 1)
	{
		AddPoint(State, po1, POINT_Intersection, 0, ACTION_NonUserPoint);
	}
	else if(cIntersections == 2)
	{
		AddPoint(State, po1, POINT_Intersection, 0, ACTION_NonUserPoint);
		AddPoint(State, po2, POINT_Intersection, 0, ACTION_NonUserPoint);
	}
	else { Assert(!cIntersections); }
	END_TIMED_BLOCK;
}

/// returns number of intersections
internal uint
AddShapeIntersections(state *State, shape *Shapes, uint cShapes, uint iShape)
{
	// NOTE: iShape is there to prevent checking intersects against itself
	BEGIN_TIMED_BLOCK;
	uint Result = 0, cIntersections;
	shape Shape = Shapes[iShape];
	Assert(Shape.Kind != SHAPE_Free);
	v2 po1, po2;

	// NOTE: TODO? internal line between eg corners of square adds 1 intersection... sometimes?
	// IMPORTANT TODO: spatially separate, maybe hierarchically
	for(uint i = 0; i < cShapes; ++i)
	{
		if(i == iShape) continue;

		cIntersections = IntersectShapes(State->Points, Shape, Shapes[i], &po1, &po2);
		AddIntersections(State, po1, po2, cIntersections);
		Result += cIntersections;
	}
	END_TIMED_BLOCK;
	return Result;
}

internal inline uint
AddNearScreenShapeIntersects(state *State, uint iShape)
{
	memory_arena Arena = State->maShapesNearScreen;
	uint Result = AddShapeIntersections(State, (shape *)Arena.Base, (uint)Arena.Used/sizeof(shape), iShape);
	return Result;
}
internal inline uint
AddAllShapeIntersects(state *State, uint iShape)
{
	uint Result = AddShapeIntersections(State, State->Shapes + 1, State->iLastShape, iShape - 1);
	return Result;
}

internal uint
RecalcIntersects(state *State, shape *Shapes, uint cShapes)
{
	State->maIntersects.Used = sizeof(v2);
	uint Result = 0;
	for(uint iShape = 0; iShape < cShapes; ++iShape)
	{
		Result += AddShapeIntersections(State, Shapes, cShapes, iShape);
	}
	return Result;
}

internal inline uint
RecalcAllIntersects(state *State)
{
	uint Result = RecalcIntersects(State, State->Shapes + 1, State->iLastShape);
	return Result;
}
internal inline uint
RecalcNearScreenIntersects(state *State)
{
	memory_arena Arena = State->maShapesNearScreen;
	uint Result = RecalcIntersects(State, (shape *)Arena.Base, (uint)ArenaCount(Arena, shape));
	return Result;
}



///////////////////////////////////////////////////////////////////////////////
//  SHAPES  ///////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

// returns if new shape (i.e. action to be added)
internal b32
AddShapeNoAction(state *State, shape Shape, uint *iShapeOut)
{
	BEGIN_TIMED_BLOCK;
	Assert(Shape.P[0] != Shape.P[1]);
	Assert(Shape.P[0] != Shape.P[2]);
	b32 Result = 0;
	b32 ExistingShape = 0;
	shape *Shapes = State->Shapes;
	uint iShape;
	// NOTE: check if exists already
	// TODO (opt): any reason why you can't only check onscreen shapes?
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
			AddAllShapeIntersects(State, iEmptyShape);
		}
		else
		{ // NOTE: new shape
			AppendStruct(&State->maShapes, shape, Shape);
			iShape = ++State->iLastShape;
			AddAllShapeIntersects(State, iShape);
		}

		Result = 1;
	}

	*iShapeOut = iShape;
	END_TIMED_BLOCK;
	return Result;
}

/// returns position in Shapes array
internal uint
AddShape(state *State, shape Shape)
{
	uint Result = 0;
	if(AddShapeNoAction(State, Shape, &Result))
	{
		action Action;
		Action.Kind = Shape.Kind;
		Action.i = Result;
		Action.P[0] = Shape.P[0];
		Action.P[1] = Shape.P[1];
		Action.P[2] = Shape.P[2];
		AddAction(State, Action);
	}
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

///////////////////////////////////////////////////////////////////////////////
//  ACTIONS  //////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

internal void
AddAction(state *State, action Action)
{
	++State->iCurrentAction;
	// NOTE: prevent redoing to future history
	State->maActions.Used = State->iCurrentAction * sizeof(action);
	AppendStruct(&State->maActions, action, Action);
	State->iLastAction = State->iCurrentAction;
#if INTERNAL && DEBUG_LOG_ACTIONS
	LogActionsToFile(State, "ActionLog.txt");
#endif
}

internal inline void
ApplyAction(state *State, action Action)
{
	switch(USERIFY_ACTION(Action.Kind))
	{
		case ACTION_Reset:
		{ ResetNoAction(State); } break;

		case ACTION_RemovePt:
		{ Assert(!"TODO: RemovePt"); } break;

		case ACTION_Basis:
		{ SetBasis(State, Action.Basis); } break;

		case ACTION_Segment:
		case ACTION_Circle:
		case ACTION_Arc:
		{
			shape Shape;
			Shape.Kind = Action.Kind;
			Shape.AllPoints = Action.AllPoints;
			uint iShape = 0;
			AddShapeNoAction(State, Shape, &iShape);
			Assert(Action.i == iShape);
		} break;

		case ACTION_Point:
		{
			uint ipo = 0;
			AddPointNoAction(State, Action.po, Action.PointStatus, 0, &ipo);
			Assert(Action.i == ipo);
		} break;

		default:
		{ Assert(!"Unknown action type"); }
	}
}