#define Assert(x) Test(x)
#define _CRT_SECURE_NO_WARNINGS
#include <sweet/sweet.h>
#include "geometer.c"

int main()
{
	state State = {0};
	v2 Points[16] = {0};
	u8 PointStatus[16] = {0};
	State.Points = Points;
	State.PointStatus = PointStatus;
	State.iLastPoint = 1;
	TestEq(Points[FirstFreePoint(&State)], ZeroV2);
	return PrintTestResults();
}

SWEET_END_TESTS;
