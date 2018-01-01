#define _CRT_SECURE_NO_WARNINGS
#include <sweet/sweet.h>
#include "win32_geometer.c"

int main()
{
	state State = {0};
	AllocStateArenas(&State);
	ResetNoAction(&State);
	UpdateArenaPointers(&State);
	AddPoint(&State, V2(5.f,   8.f), POINT_Extant, 0, ACTION_Point);
	AddPoint(&State, V2(-5.f,  8.f), POINT_Extant, 0, ACTION_Point);
	AddPoint(&State, V2(0.f,   5.f), POINT_Extant, 0, ACTION_Point);
	AddPoint(&State, V2(5.f,   0.f), POINT_Extant, 0, ACTION_Point);
	AddPoint(&State, V2(0.f,   0.f), POINT_Extant, 0, ACTION_Point);
	AddPoint(&State, V2(-5.f, -8.f), POINT_Extant, 0, ACTION_Point);
	AddPoint(&State, V2(3.f,  -2.f), POINT_Extant, 0, ACTION_Point);
	AddPoint(&State, V2(-5.f,  4.f), POINT_Extant, 0, ACTION_Point);

	AddSegment(&State, 1, 2);
	AddSegment(&State, 1, 4);
	AddSegment(&State, 6, 8);
	AddCircle(&State, 3, 4);
	AddCircle(&State, 7, 4);
	AddArc(&State, 5, 3, 4);

	TestGroup("Setup")
		TestVEq(State.iLastPoint, 8, "%u");
		TestOp(State.iLastShape, ==, 6, "%u");
	EndTestGroup;


	TestGroup("CRC32 for save/open cycle")
	{
		char *Filename = "testsave.geo";
		u32 SaveCRC32 = SaveToFile(&State, 0, Filename);
		FILE *File = fopen(Filename, "rb");
		file_header FH;
		Assert(fread(&FH, sizeof(FH), 1, File));
		fclose(File);
		TestOp(SaveCRC32, ==, FH.CRC32, "%u");
		HardReset(&State, 0);
		OpenFileInCurrentWindow(&State, Filename, sizeof("testsave.geo")-1, 0);
		u32 Save2CRC32 = SaveToFile(&State, 0, Filename);
		TestOp(SaveCRC32, ==, Save2CRC32, "%u");
	} EndTestGroup;

	return PrintTestResults();
}

SWEET_END_TESTS;
