internal void AddAction(state *State, action Action);
internal void CountActionPointsShapes(state *State, uint iActionLow, uint iActionHigh, uint *cPointsOut, uint *cShapesOut);

internal void
ResetNoAction(state *State, uint iAction)
{
	BEGIN_TIMED_BLOCK;
 
	uint cPoints = 0;
	uint cShapes = 0;
	if(iAction)
	{ CountActionPointsShapes(State, 1, iAction, &cPoints, &cShapes); }
	State->iLastPoint = cPoints;
	State->iLastShape = cPoints;
	State->maPointStatus.Used      = sizeof(u8)    * (1+cPoints);
	State->maPoints.Used           = sizeof(v2)    * (1+cPoints);
	State->maShapes.Used           = sizeof(shape) * (1+cShapes);
	State->maIntersects.Used       = sizeof(v2);
	State->maShapesNearScreen.Used = 0;
	State->maPointsOnScreen.Used   = 0;
	State->maSelectedPoints.Used   = 0;

	for(uint i = cPoints+1; i <= State->iLastPoint; ++i)
	{ POINTSTATUS(i) = POINT_Free; }

	State->Basis = DefaultBasis;
	State->tBasis = 1.f;

	State->Length = DEFAULT_LENGTH;

	END_TIMED_BLOCK;
}

internal void
Reset(state *State, uint iAction)
{
	ResetNoAction(State, iAction);
	action Action;
	Action.Kind = ACTION_Reset;
	Action.i = iAction;
	AddAction(State, Action);
}

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
	// TODO (opt): look at only those on screen
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
		// NOTE: Create new point
		Push1(&State->maPoints);
		Push1(&State->maPointStatus);
		ipo = ++State->iLastPoint;
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

internal inline action
InvalidatePointOnly(state *State, uint ipo, action_types ActionType)
{
	action Action = {0};
	Action.Kind = ActionType;
	Action.i = ipo;
	Action.po = POINTS(ipo);
	POINTSTATUS(ipo) = POINT_Free;
	return Action;
}

internal inline void
InvalidatePoint(state *State, uint ipo, action_types ActionType)
{
	// TODO (ui): may not be obvious if deleting a point on a circle circumference
	// that it's the radius point rather than just a point on the line...
	// Don't surprise the user!
	// So... transition back to circles defined by focus and radius

	action Action = InvalidatePointOnly(State, ipo, ActionType);
	InvalidateShapesAtPoint(State, ipo);
	AddAction(State, Action);
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
			InvalidatePoint(State, i, ACTION_RemovePt);
			++Result;
		}
	}
	END_TIMED_BLOCK;
	return Result;
}

internal inline b32
IsDrawing(state *State)
{ return State->InputMode > MODE_Normal; }

internal inline b32
IsDrawingArc(state *State)
{ return State->InputMode == MODE_ExtendArc; }

///////////////////////////////////////////////////////////////////////////////
//  INTERSECTIONS  ////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

internal uint
ClosestIntersectIndex(state *State, v2 Comp, f32 *ClosestDistSq)
{
	BEGIN_TIMED_BLOCK;
	// TODO (opt): look at only those on screen
	v2 *Intersects = State->maIntersects.Items;
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
	if(S1.Kind <= SHAPE_Free || S2.Kind <= SHAPE_Free) goto end;
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
		Push(&State->maIntersects, po);
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
	if(Shape.Kind > SHAPE_Free)
	{
		for(uint i = 0; i < cShapes; ++i)
		{
			if(i == iShape) continue;

			cIntersections = IntersectShapes(State->maPoints.Items, Shape, Shapes[i], &po1, &po2);
			AddIntersections(State, po1, po2, cIntersections);
			Result += cIntersections;
		}
	}
	END_TIMED_BLOCK;
	return Result;
}

internal inline uint
AddNearScreenShapeIntersects(state *State, uint iShape)
{
	shape_arena Arena = State->maShapesNearScreen;
	uint Result = AddShapeIntersections(State, Arena.Items + 1, (uint)Len(Arena) - 1, iShape);
	return Result;
}
internal inline uint
AddAllShapeIntersects(state *State, uint iShape)
{
	uint Result = AddShapeIntersections(State, State->maShapes.Items + 1, State->iLastShape, iShape - 1);
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
	uint Result = RecalcIntersects(State, State->maShapes.Items + 1, State->iLastShape);
	return Result;
}
internal inline uint
RecalcNearScreenIntersects(state *State)
{
	shape_arena Arena = State->maShapesNearScreen;
	uint ArenaLen = (uint)Len(Arena);
	ArenaLen = ArenaLen > 0 ? ArenaLen - 1 : 0; // NOTE: avoid an underflow
	uint Result = RecalcIntersects(State, Arena.Items + 1, ArenaLen);
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
	Assert(!(Shape.Kind == SHAPE_Arc && Shape.P[0] == Shape.P[2]));
	Assert(!(Shape.Kind == SHAPE_Arc && Shape.P[1] == Shape.P[2]));
	b32 Result = 0;
	b32 ExistingShape = 0;
	shape *Shapes = State->maShapes.Items;
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
	{ // NOTE: new shape
		Push(&State->maShapes, Shape);
		iShape = ++State->iLastShape;
		AddAllShapeIntersects(State, iShape);

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
		action Action = {0};
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

internal inline uint
AddSegmentAtPoints(state *State, v2 po1, v2 po2)
{
	uint ipo1 = AddPoint(State, po1, POINT_Line, 0, ACTION_NonUserPoint);
	uint ipo2 = AddPoint(State, po2, POINT_Line, 0, ACTION_NonUserPoint);
	return AddSegment(State, ipo1, ipo2);
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

internal inline uint
AddCircleAtPoints(state *State, v2 poFocus, v2 poRadius)
{
	uint ipoFocus  = AddPoint(State, poFocus,  POINT_Focus,  0, ACTION_NonUserPoint);
	uint ipoRadius = AddPoint(State, poRadius, POINT_Radius, 0, ACTION_NonUserPoint);
	return AddCircle(State, ipoFocus, ipoRadius);
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

internal inline uint
AddArcAtPoints(state *State, v2 poFocus, v2 poStart, v2 poEnd)
{
	uint ipoFocus = AddPoint(State, poFocus, POINT_Focus,  0, ACTION_NonUserPoint);
	uint ipoStart = AddPoint(State, poStart, POINT_Radius, 0, ACTION_NonUserPoint);
	uint ipoEnd   = AddPoint(State, poEnd,   POINT_Radius, 0, ACTION_NonUserPoint);
	return AddArc(State, ipoFocus, ipoStart, ipoEnd);
}

internal inline b32
ShapeUsesPoint(shape Shape, uint ipo)
{
	b32 Result = 0;
	switch(Shape.Kind)
	{
		case SHAPE_Segment:
		{
			if(Shape.Line.P1 == ipo || Shape.Line.P2 == ipo)
			{ Result = 1; }
		} break;

		case SHAPE_Circle:
		{
			if(Shape.Circle.ipoFocus == ipo || Shape.Circle.ipoRadius == ipo)
			{ Result = 1; }
		} break;

		case SHAPE_Arc:
		{
			if(Shape.Arc.ipoFocus == ipo ||
			   Shape.Arc.ipoStart == ipo ||
			   Shape.Arc.ipoEnd   == ipo)
			{ Result = 1; }
		} break;

		case -SHAPE_Segment:
		case -SHAPE_Circle:
		case -SHAPE_Arc:
		{} break;

		default:
		{ Assert(!"Unknown shape type"); }
	}

	return Result;
}

// TODO: allow user to make automatic point 'user-set' by overriding?
internal inline b32
InvalidatePointIfUnusedAndAutomatic(state *State, uint ipo)
{
	b32 Used = 0;
	b32 Result = 0;
	if(POINTSTATUS(ipo) != POINT_Free)
	{
		for(uint iShape = State->iLastShape; iShape > 0; --iShape)
		{
			if(ShapeUsesPoint(Pull(State->maShapes, iShape), ipo))
			{ Used = 1; break; }
		}
		if( ! Used)
		{
			b32 PlacedByUser = 0;
			for(uint iAction = State->iLastAction; iAction > 0; --iAction)
			{
				action Action = Pull(State->maActions, iAction);
				if(Action.i == ipo && Action.Kind == ACTION_Point) // the user-intended variant
				{ PlacedByUser = 1; break; }
			}

			Result = ! PlacedByUser;
			if(Result)
			{
				action Action = InvalidatePointOnly(State, ipo, ACTION_NonUserRemovePt);
				AddAction(State, Action);
			}
		}
	}
	return Result;
}

internal inline void
InvalidateShapesAtPoint(state *State, uint ipo)
{
	BEGIN_TIMED_BLOCK;
	shape *Shapes = State->maShapes.Items;
	for(uint iShape = 1; iShape <= State->iLastShape; ++iShape)
	{ // NOTE: if any of the shape's points match, remove it
		shape Shape = Shapes[iShape];
		if(ShapeUsesPoint(Shape, ipo))
		{
			Assert(Shape.Kind > SHAPE_Free || !"Shape should be valid before invalidating it");
			Shapes[iShape].Kind = Shape.Kind > SHAPE_Free ? -Shape.Kind : Shape.Kind;

			switch(Shape.Kind)
			{ // invalidate any points unused by other shapes and not user-placed
				case SHAPE_Segment:
				{
					InvalidatePointIfUnusedAndAutomatic(State, Shape.Line.P1);
					InvalidatePointIfUnusedAndAutomatic(State, Shape.Line.P2);
				} break;                     

				case SHAPE_Circle:           
				{                            
					InvalidatePointIfUnusedAndAutomatic(State, Shape.Circle.ipoFocus);
					InvalidatePointIfUnusedAndAutomatic(State, Shape.Circle.ipoRadius);
				} break;                     

				case SHAPE_Arc:              
				{                            
					InvalidatePointIfUnusedAndAutomatic(State, Shape.Arc.ipoFocus);
					InvalidatePointIfUnusedAndAutomatic(State, Shape.Arc.ipoStart);
					InvalidatePointIfUnusedAndAutomatic(State, Shape.Arc.ipoEnd);
				} break;

				default:
				{ Assert(!"Unknown shape type"); }
			}

			action Action = {0};
			Action.Kind = ACTION_NonUserRemoveShape;
			Action.i = iShape;
			AddAction(State, Action);
		}
	}

// Should we reduce the shape array?
/* 	Assert(State->iLastShape == Len(State->maShapes)-1); */
/* 	for(uint i = State->iLastShape; i > 0 && Shapes[i].Kind == SHAPE_Free; --i) */
/* 	{ */
/* 		--State->iLastShape; */
/* 		PopDiscard(&State->maShapes); */
/* 	} */
	END_TIMED_BLOCK;
}

internal aabb
AABBFromShape(v2 *Points, shape Shape)
{
	aabb Result = {0};
	switch(Shape.Kind)
	{
		case SHAPE_Segment:
		{
			v2 po1 = Points[Shape.Line.P1];
			v2 po2 = Points[Shape.Line.P2];
			minmaxf32 x = MinMaxF32(po1.X, po2.X);
			minmaxf32 y = MinMaxF32(po1.Y, po2.Y);
			Result.MinX = x.Min;
			Result.MaxX = x.Max;
			Result.MinY = y.Min;
			Result.MaxY = y.Max;
		} break;

		// TODO (optimize): arc AABB may be smaller than circle
		case SHAPE_Arc:
		case SHAPE_Circle:
		{
			v2 Focus = Points[Shape.Circle.ipoFocus];
			f32 Radius = Dist(Focus, Points[Shape.Circle.ipoRadius]);
			Result.MinX = Focus.X - Radius;
			Result.MaxX = Focus.X + Radius;
			Result.MinY = Focus.Y - Radius;
			Result.MaxY = Focus.Y + Radius;
		} break;

		default:
		{
			Assert(0);
		}
	}
	return Result;
}

///////////////////////////////////////////////////////////////////////////////
//  ACTIONS  //////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

#if INTERNAL && DEBUG_LOG_ACTIONS
internal void
LogActionsToFile(state *State, char *FilePath)
{
	FILE *ActionFile = fopen(FilePath, "w");

	action_arena maActions = State->maActions;
	// NOTE: account for initial offset
	Assert(State->iLastAction == Len(maActions)-1);
	uint iLastAction = State->iLastAction;
	uint iCurrentAction = State->iCurrentAction;

	for(uint iAction = 1; iAction <= iLastAction; ++iAction)
	{
		action Action = Pull(maActions, iAction);

		fprintf(ActionFile,
				"Action %2u: %s",
				iAction,
				ActionTypesStrings[Action.Kind]);

		if(Action.i)
		{ fprintf(ActionFile, " -> [%u]", Action.i); }

		if(iAction == iCurrentAction)
		{ fprintf(ActionFile, "\t\t<-- CURRENT"); }

		fprintf(ActionFile, "\n");


		// NOTE: not doing boundary checks (i.e. `Pull`) as I'm accessing
		// outside the Len - this is only because this is for debugging!
		uint ipo1 = Action.P[0];
		uint ipo2 = Action.P[1];
		uint ipo3 = Action.P[2];
		switch(USERIFY_ACTION(Action.Kind))
		{
			case ACTION_Basis:
			{
				basis B = Action.Basis;
				fprintf(ActionFile,
						"\tx-axis: (%.3f, %.3f)\n"
						"\toffset: (%.3f, %.3f)\n"
						"\tzoom: %.3f\n",
						B.XAxis.X, B.XAxis.Y,
						B.Offset.X, B.Offset.Y,
						B.Zoom);
			} break;

			case ACTION_Segment:
			{
				v2 po1 = State->maPoints.Items[ipo1];
				v2 po2 = State->maPoints.Items[ipo2];
				fprintf(ActionFile,
						"\tPoint 1: %u (%.3f, %.3f)\n"
						"\tPoint 2: %u (%.3f, %.3f)\n",
						ipo1, po1.X, po1.Y,
						ipo2, po2.X, po2.Y);
			} break;

			case ACTION_Circle:
			{
				v2 po1 = State->maPoints.Items[ipo1];
				v2 po2 = State->maPoints.Items[ipo2];
				fprintf(ActionFile,
						"\tFocus:  %u (%.3f, %.3f)\n"
						"\tRadius: %u (%.3f, %.3f)\n",
						ipo1, po1.X, po1.Y,
						ipo2, po2.X, po2.Y);
			} break;

			case ACTION_Arc:
			{
				v2 po1 = State->maPoints.Items[ipo1];
				v2 po2 = State->maPoints.Items[ipo2];
				v2 po3 = State->maPoints.Items[ipo3];
				fprintf(ActionFile,
						"\tFocus:  %u (%.3f, %.3f)\n"
						"\tStart:  %u (%.3f, %.3f)\n"
						"\tEnd:    %u (%.3f, %.3f)\n",
						ipo1, po1.X, po1.Y,
						ipo2, po2.X, po2.Y,
						ipo3, po3.X, po3.Y);
			} break;

			case ACTION_Point:
			{
				char Types[] = "DARTFILE";
				char Status[sizeof(Types)];
				ssprintf(Status, "%08b", Action.PointStatus);
				v2 po1 = Action.po;
				fprintf(ActionFile,
						"\t(%f, %f)\n"
						"\t        %s\n"
						"\tStatus: %s\n",
						po1.X, po1.Y,
						Types, Status);
			} break;

			default: {}
		}

		fprintf(ActionFile, "\n");
	}

	fclose(ActionFile);
}
#endif//INTERNAL && DEBUG_LOG_ACTIONS

internal void
AddAction(state *State, action Action)
{
	++State->iCurrentAction;
	// NOTE: prevent redoing to future history
	State->maActions.Used = State->iCurrentAction * sizeof(action);
	Push(&State->maActions, Action);
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
		{ ResetNoAction(State, 0); } break;

		case ACTION_RemovePt:
		{ Pull(State->maPointStatus, Action.i) = POINT_Free; } break;

		case ACTION_RemoveShape:
		{
			shape *Shape = &Pull(State->maShapes, Action.i);
			Assert(Shape->Kind > SHAPE_Free);
			Shape->Kind = Shape->Kind > SHAPE_Free ? -Shape->Kind : Shape->Kind;
		} break;

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

internal inline b32 ActionIsShape(action_types Action)
{
	Action = USERIFY_ACTION(Action);
	b32 Result = (Action >= ACTION_SHAPE_START && Action <= ACTION_SHAPE_END);
	return Result;
}

internal inline b32 ActionIsPoint(action_types Action)
{
	b32 Result = USERIFY_ACTION(Action) == ACTION_Point;
	return Result;
}

internal void
CountActionPointsShapes(state *State, uint iActionLow, uint iActionHigh, uint *cPointsOut, uint *cShapesOut)
{
	uint cPoints = 0, cShapes = 0;
	for(uint iAction = iActionHigh; iAction >= iActionLow && iAction; --iAction)
	{
		     if(ActionIsPoint(Pull(State->maActions, iAction).Kind)) {++cPoints;}
		else if(ActionIsShape(Pull(State->maActions, iAction).Kind)) {++cShapes;}
		// TODO: what to do when encountering Resets?
		/* else if(Action.Kind == ACTION_Reset && iAction.i < iActionLow)
		 * { iActionAction.i; } */
	}
	*cPointsOut = cPoints;
	*cShapesOut = cShapes;
}
