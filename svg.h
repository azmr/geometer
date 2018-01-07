#ifndef SVG_H
#include <stdio.h>
#ifndef STB_SPRINTF_DECORATE
#define STB_SPRINTF_DECORATE(fn) s##fn
#include <stb_sprintf.h>
#endif /* ifndef  */

#define FWRITE(str, file) fwrite((str), 1, sizeof(str)-1, (file))

#define BUFSIZE 512

internal FILE *
NewSVG(char *FilePath, char *Defaults)
{
	// TODO (feature): add size up front
	FILE *File = fopen(FilePath, "w");
	char Buffer[BUFSIZE];
	int cch = ssnprintf(Buffer, BUFSIZE, "<svg version='1.1' xmlns='http://www.w3.org/2000/svg' "
			"xmlns:xlink='http://www.w3.org/1999/xlink' xml:space='preserve' %s>\n", Defaults);
	fwrite(Buffer, 1, cch, File);
	Assert(!ferror(File));
	return File;
}

internal int
EndSVG(FILE *File)
{
	FWRITE("</svg>", File);
	int Result = fclose(File);
	return Result;
}
#undef FWRITE

internal void
SVGRect(FILE *File, f64 x, f64 y, f64 w, f64 h)
{
	// what point on rect do x & y map to? bottom-left I think...
	// return "<rect x='" + x + "' y='" + y + "' width='" + w + "' height='" + h + "'/>"; // fill='#BBC42A'
	char Buffer[BUFSIZE];
	int cch = ssnprintf(Buffer, BUFSIZE, "<rect fill='none' stroke-width='%f' stroke='lightgrey' "
			                             "x='%f' y='%f' width='%f' height='%f'/>\n", x, y, w, h);
	Assert(cch > 0 && cch <= BUFSIZE);
	fwrite(Buffer, 1, cch, File);
	Assert(!ferror(File));
}

internal void
SVGCircle(FILE *File, f64 x, f64 y, f64 Radius)
{
	char Buffer[BUFSIZE];
	int cch = ssnprintf(Buffer, BUFSIZE, "<circle cx='%f' cy='%f' r='%f'/>\n", x, y, Radius);
	Assert(cch > 0 && cch <= BUFSIZE);
	fwrite(Buffer, 1, cch, File);
	Assert(!ferror(File));
}

internal void
SVGArc(FILE *File, f64 Radius, f64 StartX, f64 StartY, f64 EndX, f64 EndY, b32 LargeArc)
{
	char Buffer[2*BUFSIZE];
	int cch = ssnprintf(Buffer, 2*BUFSIZE,
		"<path "
		"d='M %f %f " // move to start point
		"A "          // Arc (absolute position)
		"%f %f "      // with equal x&y radius
		"0 "          // with no rotation (irrelevant for circular arc anyway)
		"%u "         // > or < 180 degrees
		"0 "          // which of 2 circles is drawn around (https://developer.mozilla.org/@api/deki/files/345/=SVGArcs_Flags.png)
		"%f, %f"      // endpoints
		"'/>\n",
		StartX, StartY,
		Radius, Radius,
		LargeArc, // 1 ? 0 : LargeArc,
		EndX, EndY);
	Assert(cch > 0 && cch <= 2*BUFSIZE);
	fwrite(Buffer, 1, cch, File);
	Assert(!ferror(File));
}

internal void
SVGLine(FILE *File, f64 x1, f64 y1, f64 x2, f64 y2)
{
	char Buffer[BUFSIZE];
	int cch = ssnprintf(Buffer, BUFSIZE, "<line x1='%f' y1='%f' x2='%f' y2='%f'/>\n", x1, y1, x2, y2);
	Assert(cch > 0 && cch <= BUFSIZE);
	fwrite(Buffer, 1, cch, File);
	Assert(!ferror(File));
}

#undef BUFSIZE

#define SVG_H
#endif
