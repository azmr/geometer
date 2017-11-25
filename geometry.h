#ifndef GEOMETRY_H
#include <maths.h>

// TODO: Should these be f32?
internal inline v2
Proj(v2 V, v2 W)
{
	// NOTE: does not assume normalized
	v2 Result = V2Mult((Dot(V, W)/LenSq(W)), W);
	return Result;
}

internal inline v2
ProjN(v2 V, v2 W)
{
	// NOTE: assumes normalized(?)
	v2 Result = V2Mult(Dot(V, W), W);
	return Result;
}

// NOTE: must be less than 180°
// order matters - CCW of B1, CW of B2
// inclusive at start, exclusive at end?
internal inline b32
V2WithinNarrowBoundaries(v2 A, v2 B1, v2 B2)
{
	b32 CCW_OfB1 = PerpDot(B1, A) >= 0.f;
	b32 CW__OfB2 = PerpDot(B2, A) <  0.f;
	b32 Result = CCW_OfB1 && CW__OfB2;
	return Result;
}

internal inline b32
V2WithinBoundaries(v2 A, v2 B1, v2 B2)
{
	b32 Result;
	v2 PerpB1 = Perp(B1);
	// Boundaries span less than half-circle
	if(Dot(PerpB1, B2) >= 0.f)
		Result = V2WithinNarrowBoundaries(A, B1, B2);
	// TestPoint is within the fully-covered half-circle
	else if(Dot(PerpB1, A) >= 0.f)
		Result = 1;
	// B2 is still CCW of -B1, so check if A is between those
	else
		Result = V2WithinNarrowBoundaries(A, V2Neg(B1), B2);
	return Result;
}

internal inline v2
ClosestPtOnCircle(v2 P, v2 Focus, f32 Radius)
{
	BEGIN_TIMED_BLOCK;
	v2 Dir;
	if(V2Equals(P, Focus)) // centre -> right (failing is unhelpful)
	{ Dir = V2(Radius, 0.f); }
	else // normal path
	{ Dir = V2Sub(P, Focus); }
	v2 poRel = V2WithLength(Dir, Radius);
	v2 Result = V2Add(Focus, poRel);
	END_TIMED_BLOCK;
	return Result;
}

internal inline v2
ClosestPtOnArc(v2 P, v2 Focus, v2 ArcStart, v2 ArcEnd)
{
	v2 Result;
	v2 RelP     = V2Sub(P,        Focus);
	v2 RelStart = V2Sub(ArcStart, Focus);
	v2 RelEnd   = V2Sub(ArcEnd,   Focus);
	if(V2WithinBoundaries(RelP, RelStart, RelEnd))
	{
		Result = ClosestPtOnCircle(P, Focus, Dist(Focus, ArcStart));
	}
	else
	{
		f32 DistSqStart = DistSq(P, ArcStart);
		f32 DistSqEnd   = DistSq(P, ArcEnd);
		Result = (DistSqStart <= DistSqEnd) ? ArcStart : ArcEnd;
	}
	return Result;
}

/// AB = B - A
internal inline v2
ClosestPtOnLine(v2 P, v2 a, v2 ab)
{
	BEGIN_TIMED_BLOCK;
	// Project P onto ab, computing parameterized position d(t) = a + t*(b - a)
	// TODO: Dot(ab, ab) better as Len(ab)?
	f32 t = Dot(V2Sub(P, a), ab) / Dot(ab, ab);
	// TODO: is t wanted as well?
	v2 Result = V2Add(a, V2Mult(t, ab));
	END_TIMED_BLOCK;
	return Result;
}

/// AB = B - A
internal inline v2
ClosestPtOnSegment(v2 P, v2 a, v2 ab)
{
	BEGIN_TIMED_BLOCK;
	// Project P onto ab, computing parameterized position d(t) = a + t*(b - a)
	// TODO: Dot(ab, ab) better as Len(ab)?
	f32 t = Dot(V2Sub(P, a), ab) / Dot(ab, ab);
	// If outside segment, closest point is an endpoint
	t = Clamp(t, 0.f, 1.f);
	// TODO: is t wanted as well?
	v2 Result = V2Add(a, V2Mult(t, ab));
	END_TIMED_BLOCK;
	return Result;
}

/// ab = b - a
internal inline v2
ClosestPtOnSegmentDeferred(v2 P, v2 a, v2 ab)
{
	BEGIN_TIMED_BLOCK;
	// Project P onto AB, computing parameterized position d(t) = a + t*(b - a)
	f32 t = Dot(V2Sub(P, a), ab);
	v2 Result;
	// If outside segment on (left), closest point is left endpoint
	if(t <= 0.f)
	{
		// t = 0.f;
		Result = a;
	}

	else
	{
		f32 Denominator = Dot(ab, ab); // Always nonnegative since denom = ||AB||^2
		if(t >= Denominator)
		{
			// t = 1.f;
			Result = V2Add(a, ab);
		}

		else
		{
			t = t / Denominator;
			// TODO: is t wanted as well?
			Result = V2Add(a, V2Mult(t, ab));
		}
	}
	END_TIMED_BLOCK;
	return Result;
}

internal inline f32
DistSqPtSegment(v2 P, v2 A, v2 B)
{
	BEGIN_TIMED_BLOCK;
	f32 Result;
	v2 ab = V2Sub(B, A), ap = V2Sub(P, A), bp = V2Sub(P, B);
	f32 E = Dot(ap, ab);
	if(E <= 0.f)
	{
		// NOTE: P to 'left' of A
		Result = Dot(ap, ap);
	}

	else
	{
		f32 F = Dot(ab, ab);
		if(E >=F)
		{
		// NOTE: P to 'right' of B
			Result = Dot(bp, bp);
		}

		else
		{
			Result = Dot(ap, ap) - E*E / F;
		}
	}
	END_TIMED_BLOCK;
	return Result;
}

// Returns 2 times the signed triangle area. The result is positive if
// abc is ccw, negative if abc is cw, zero if abc is degenerate.
internal f32
Signed2DTriAreaDoubled(v2 A, v2 B, v2 C)
{
	f32 Result = (A.X - C.X) * (B.Y - C.Y) - (A.Y - C.Y) * (B.X - C.X); 
	return Result;
}

// TODO (feature/fix): deal with collinear lines...
/// Info: https://stackoverflow.com/questions/563198/how-do-you-detect-where-two-line-segments-intersect
// t is number of times Dir1 away from P1
internal b32
IntersectLinesForT(v2 P1, v2 Dir1, v2 P2, v2 Dir2, f32 *t)
{
	BEGIN_TIMED_BLOCK;
	b32 Result = 0;
	// t = (P2 − P1) × Dir2 / (Dir1 × Dir2)
	f32 Dir1_X_Dir2 = Cross(Dir1, Dir2);
	// TODO: should this have an epsilon?
	if(Dir1_X_Dir2 != 0) // lines are not parallel/collinear
	{ 
		*t = Cross(V2Sub(P2, P1), Dir2)  /  Dir1_X_Dir2;
		Result = 1;
	}
	END_TIMED_BLOCK;
	return Result;
}

internal b32
IntersectLinesForTAndU(v2 P1, v2 Dir1, v2 P2, v2 Dir2, f32 *t, f32 *u)
{
	BEGIN_TIMED_BLOCK;
	b32 Result = 0;
	// t = (P2 − P1) × Dir2 / (Dir1 × Dir2)
	// u = (P2 − P1) × Dir1 / (Dir1 × Dir2)
	f32 Dir1_X_Dir2 = Cross(Dir1, Dir2);
	v2 P2_Sub_1 = V2Sub(P2, P1);
	// TODO: should this have an epsilon?
	if(Dir1_X_Dir2 != 0) // lines are not parallel/collinear
	{
		*t = Cross(P2_Sub_1, Dir2)  /  Dir1_X_Dir2;
		*u = Cross(P2_Sub_1, Dir1)  /  Dir1_X_Dir2;
		Result = 1;
	}
	END_TIMED_BLOCK;
	return Result;
}

internal inline b32
IntersectLines(v2 P1, v2 Dir1, v2 P2, v2 Dir2, v2 *Intersection)
{
	BEGIN_TIMED_BLOCK;
	f32 t = 0.f;
	b32 Result = IntersectLinesForT(P1, Dir1, P2, Dir2, &t);
	*Intersection = V2Add(P1, V2Mult(t, Dir1));
	END_TIMED_BLOCK;
	return Result;
}

/// SegDir should be encompass the entire segment (i.e. SegQ-SegP)
internal inline b32
IntersectLineSegmentAndT(v2 LineP, v2 LineDir, v2 SegP, v2 SegDir, v2 *Intersection, f32 *LineT)
{
	BEGIN_TIMED_BLOCK;
	f32 t = 0.f;
	f32 u = 0.f;
	b32 Result = IntersectLinesForTAndU(SegP, SegDir, LineP, LineDir, &t, &u);
	if(Result && t >= 0.f && t <= 1.f)  // intersection within segment
	{
		*Intersection = V2Add(SegP, V2Mult(t, SegDir));
		*LineT = u;
	}
	else
	{ Result = 0; }
	DebugReplace("Intersection: %f, %f", Intersection->X, Intersection->Y);

	END_TIMED_BLOCK;
	return Result;
}
internal inline b32
IntersectLineSegment(v2 LineP, v2 LineDir, v2 SegP, v2 SegDir, v2 *Intersection)
{
	BEGIN_TIMED_BLOCK;
	f32 t = 0.f;
	b32 Result = IntersectLinesForT(SegP, SegDir, LineP, LineDir, &t);
	if(Result && t >= 0.f && t <= 1.f)  // intersection within segment
	{ *Intersection = V2Add(SegP, V2Mult(t, SegDir)); }
	else
	{ Result = 0; }
	DebugReplace("Intersection: %f, %f", Intersection->X, Intersection->Y);

	END_TIMED_BLOCK;
	return Result;
}

// NOTE: From Ericson - Real Time Collision (p153)
// TODO: compare dot/cross product based intersection
internal b32
IntersectSegmentsWinding(v2 A, v2 B, v2 C, v2 D, v2 *Out)
{
	BEGIN_TIMED_BLOCK;
	b32 Result = 0;
	f32 ABC = Signed2DTriAreaDoubled(A, B, C);
	f32 ABD = Signed2DTriAreaDoubled(A, B, D);
	// NOTE: Different sides -> different signs
	if(ABC * ABD < 0.f)
	{
		f32 CDA = Signed2DTriAreaDoubled(C, D, A);
		// NOTE: Since area is constant ABD - ABC = CDA - CDB, or CDB = CDA + ABC - ABD
		// f32 CDB = Signed2DTriArea(c, d, b); // Must have opposite sign of a3
		f32 CDB = CDA + ABC - ABD;

		if(CDA * CDB < 0.f)
		{
			// Segments intersect. Find intersection point along L(t) = a + t * (b - a).
			// Given height h1 of an over cd and height h2 of b over cd,
			// t = h1 / (h1 - h2) = (b*h1/2) / (b*h1/2 - b*h2/2) = CDA / (CDA - CDB),
			// where b (the base of the triangles cda and cdb, i.e., the length of cd) cancels out.
			Result = 1;
			// TODO: maybe return t?
			f32 t = CDA / (CDA - CDB);
			*Out = V2Add(A, V2Mult(t, V2Sub(B, A)));
		}
	}
	END_TIMED_BLOCK;
	return Result;
}

// TODO: convert to RadiusSq. Ensure callers are using proper format
/// expects d to be normalised
/// returns number of intersections
internal uint
IntersectLineCircleForT(v2 P, v2 d, v2 poFocus, f32 Radius, f32 *t1, f32 *t2)
{
	BEGIN_TIMED_BLOCK;
	uint Result = 0;
	// Distance from point on line to circle centre == radius:
	// 		(P + td - C) DOT (P + td - C) == radius^2
	// m=P-C
	v2 m = V2Sub(P, poFocus);
	// Simplifies to t^2 + 2(m DOT d)t + (m DOT m) - r^2 == 0
	// Solving quadratic equation  -b +/- sqrt(b^2 - c)
	f32 b = Dot(m, d);
	f32 Dotmm = Dot(m, m);
	f32 c = Dotmm - Radius*Radius;
	f32 Discriminant = b*b - c;

	if(Discriminant < 0.f) return Result;
	else if(Discriminant == 0.f)
	{
		Result = 1;
		*t1 = -b;
	}

	else // if(Discriminant > 0.f)
	{
		// TODO: check if t makes ray/segment start/end inside circle
		Result = 2;
		f32 RootDisc = QSqrt(Discriminant);
		*t1 = -b - RootDisc;
		*t2 = -b + RootDisc;
	}

	END_TIMED_BLOCK;
	return Result;
}

/// returns number of intersections
internal uint
IntersectLineCircle(v2 P, v2 Dir, v2 poFocus, f32 Radius, v2 *Intersection1, v2 *Intersection2)
{
	BEGIN_TIMED_BLOCK;
	f32 t1, t2;
	v2 d = Norm(Dir);
	uint Result = IntersectLineCircleForT(P, d, poFocus, Radius, &t1, &t2);

	if(Result)
	{
		*Intersection1 = V2Add(P, V2Mult(t1, d));
		//if(Result == 2)
			*Intersection2 = V2Add(P, V2Mult(t2, d));
	}
	END_TIMED_BLOCK;
	return Result;
}

/// returns number of intersections
internal uint
IntersectRayCircle(v2 P, v2 Dir, v2 poFocus, f32 Radius, v2 *Intersection1, v2 *Intersection2)
{
	BEGIN_TIMED_BLOCK;
	f32 t1, t2;
	v2 d = Norm(Dir);
	uint Result = IntersectLineCircleForT(P, d, poFocus, Radius, &t1, &t2);

	if(Result)
	{
		b32 FirstPoint = 1;
		if(t1 < 0.f) FirstPoint = 0;
		if(FirstPoint)
		{
			*Intersection1 = V2Add(P, V2Mult(t1, d));
			*Intersection2 = V2Add(P, V2Mult(t2, d));
		}

		else
		{
			*Intersection1 = V2Add(P, V2Mult(t2, d));
		}
	}
	END_TIMED_BLOCK;
	return Result;
}

/// returns number of intersections
internal uint
IntersectSegmentCircle(v2 P, v2 Dir, v2 poFocus, f32 Radius, v2 *Intersection1, v2 *Intersection2)
{
	BEGIN_TIMED_BLOCK;
	f32 t1, t2;
	v2 d = Norm(Dir);
	// If the tangent is a line, how many times would it go intersect the circle?
	uint Result = IntersectLineCircleForT(P, d, poFocus, Radius, &t1, &t2);

	if(Result)
	{ 
		b32 FirstPoint = 1;
		b32 SecondPoint = 1;
		// Are any line intersections inside the segment?
		if(t1 < 0.f || LenSq(V2Mult(t1, d)) > LenSq(Dir))  FirstPoint  = 0;
		if(t2 < 0.f || LenSq(V2Mult(t2, d)) > LenSq(Dir))  SecondPoint = 0;
		Result = FirstPoint + SecondPoint;

		if(FirstPoint)
		{
			*Intersection1 = V2Add(P, V2Mult(t1, d));
			// This will be ignored if only one intersection:
			*Intersection2 = V2Add(P, V2Mult(t2, d));
		}
		else if(SecondPoint)
		{
			*Intersection1 = V2Add(P, V2Mult(t2, d));
		}
	}
	END_TIMED_BLOCK;
	return Result;
}

/// returns number of intersections
internal uint
IntersectCircles(v2 poFocus1, f32 R1, v2 poFocus2, f32 R2, v2 *Intersection1, v2 *Intersection2)
{
	BEGIN_TIMED_BLOCK;
	v2 Dir = V2Sub(poFocus2, poFocus1);
	// TODO: optimise out
	f32 dSq = LenSq(Dir);
	f32 RadAdd = R1 + R2;
	uint Result = 0;
	if(dSq == RadAdd * RadAdd)
	{
		// NOTE: early out for tangents
		Result = 1;
		*Intersection1 = V2Add(poFocus1, V2Mult(R1, Norm(Dir)));
	}

	else
	{
		f32 Fraction = 0.5f * ((R1*R1 / dSq) - (R2*R2 / dSq) + 1);
		v2 ChordCross = V2Add(poFocus1, V2Mult(Fraction, Dir));
		Result = IntersectLineCircle(ChordCross, Perp(Dir), poFocus1, R1, Intersection1, Intersection2);
	}
	END_TIMED_BLOCK;
	return Result;
}

internal inline uint
CheckCircleCollisionsForArc(v2 poArcFocus, v2 poArcStart, v2 poArcEnd, v2 *Intersection1, v2 *Intersection2, uint NumCircleCollisions)
{
	BEGIN_TIMED_BLOCK;
	uint Result = NumCircleCollisions;
	if(Result)
	{
		v2 RelArcStart   = V2Sub(poArcStart,     poArcFocus);
		v2 RelArcEnd     = V2Sub(poArcEnd,       poArcFocus);
		v2 RelIntersect1 = V2Sub(*Intersection1, poArcFocus);
		b32 ArcIntersect1 = V2WithinBoundaries(RelIntersect1, RelArcStart, RelArcEnd);
		b32 ArcIntersect2 = 0;
		if(Result == 2)
		{
			v2 RelIntersect2 = V2Sub(*Intersection2, poArcFocus);
			ArcIntersect2 = V2WithinBoundaries(RelIntersect2, RelArcStart, RelArcEnd);

			if(ArcIntersect2 && !ArcIntersect1)
			{
				*Intersection1 = *Intersection2;
			}
		}
		Result = ArcIntersect1 + ArcIntersect2;
	}
	END_TIMED_BLOCK;
	return Result;
}

internal inline uint
IntersectLineArc(v2 P, v2 Dir, v2 poFocus, f32 Radius, v2 poArcStart, v2 poArcEnd, v2 *Intersection1, v2 *Intersection2)
{
	BEGIN_TIMED_BLOCK;
	Assert( ! V2Equals(Dir, ZeroV2));
	uint NumCircleCollisions = IntersectLineCircle(P, Dir, poFocus, Radius, Intersection1, Intersection2);
	uint Result = CheckCircleCollisionsForArc(poFocus, poArcStart, poArcEnd, Intersection1, Intersection2, NumCircleCollisions);
	END_TIMED_BLOCK;
	return Result;
}

internal inline uint
IntersectRayArc(v2 P, v2 Dir, v2 poFocus, f32 Radius, v2 poArcStart, v2 poArcEnd, v2 *Intersection1, v2 *Intersection2)
{
	BEGIN_TIMED_BLOCK;
	Assert( ! V2Equals(Dir, ZeroV2));
	uint NumCircleCollisions = IntersectRayCircle(P, Dir, poFocus, Radius, Intersection1, Intersection2);
	uint Result = CheckCircleCollisionsForArc(poFocus, poArcStart, poArcEnd, Intersection1, Intersection2, NumCircleCollisions);
	END_TIMED_BLOCK;
	return Result;
}

internal inline uint
IntersectSegmentArc(v2 P, v2 Dir, v2 poFocus, f32 Radius, v2 poArcStart, v2 poArcEnd, v2 *Intersection1, v2 *Intersection2)
{
	BEGIN_TIMED_BLOCK;
	Assert( ! V2Equals(Dir, ZeroV2));
	uint NumCircleCollisions = IntersectSegmentCircle(P, Dir, poFocus, Radius, Intersection1, Intersection2);
	uint Result = CheckCircleCollisionsForArc(poFocus, poArcStart, poArcEnd, Intersection1, Intersection2, NumCircleCollisions);
	END_TIMED_BLOCK;
	return Result;
}

internal inline uint
IntersectCircleArc(v2 poFocus1, f32 R1, v2 poFocus2, f32 R2, v2 poArcStart, v2 poArcEnd, v2 *Intersection1, v2 *Intersection2)
{
	BEGIN_TIMED_BLOCK;
	uint NumCircleCollisions = IntersectCircles(poFocus1, R1, poFocus2, R2, Intersection1, Intersection2);
	uint Result = CheckCircleCollisionsForArc(poFocus2, poArcStart, poArcEnd, Intersection1, Intersection2, NumCircleCollisions);
	END_TIMED_BLOCK;
	return Result;
}

internal inline uint
IntersectArcs(v2 poFocus1, f32 R1, v2 poArcStart1, v2 poArcEnd1, v2 poFocus2, f32 R2, v2 poArcStart2, v2 poArcEnd2, v2 *Intersection1, v2 *Intersection2)
{
	BEGIN_TIMED_BLOCK;
	uint NumCircleCollisions = IntersectCircles(poFocus1, R1, poFocus2, R2, Intersection1, Intersection2);
	uint Result = CheckCircleCollisionsForArc(poFocus1, poArcStart1, poArcEnd1, Intersection1, Intersection2, NumCircleCollisions);
	     Result = CheckCircleCollisionsForArc(poFocus2, poArcStart2, poArcEnd2, Intersection1, Intersection2, Result);
	END_TIMED_BLOCK;
	return Result;
}
#define GEOMETRY_H
#endif
