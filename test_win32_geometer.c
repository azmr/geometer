#define _CRT_SECURE_NO_WARNINGS
#include <sweet/sweet.h>
#include "win32_geometer.c"

int main()
{
	state State = {0};
	AllocStateArenas(&State);
	ResetNoAction(&State, 0);
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

	TestGroup("Basis transformation")
	{
		basis Basis = {0};
		Basis.XAxis = Norm(V2(41.5f, -12.f));
		Basis.Offset = V2(-106.9, -0.5);
		Basis.Zoom = 12.f;

#define BASIS_EPS 0.00001f
		compressed_basis CompressedBasis = CompressBasis(Basis);
		basis DecompressedBasis = DecompressBasis(CompressedBasis);
		TestVEqEps(Basis.XAxis.X, DecompressedBasis.XAxis.X, BASIS_EPS, "%.8f");
		TestVEqEps(Basis.XAxis.Y, DecompressedBasis.XAxis.Y, BASIS_EPS, "%.8f");
		TestVEqEps(Basis.Zoom, DecompressedBasis.Zoom,       BASIS_EPS, "%.8f");
		TestVEq(Basis.Offset.X, DecompressedBasis.Offset.X, "%.20f");
		TestVEq(Basis.Offset.Y, DecompressedBasis.Offset.Y, "%.20f");
	} EndTestGroup;

	return PrintTestResults();
}

SWEET_END_TESTS;
