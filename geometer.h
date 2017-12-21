#ifndef GEOMETER_H
#define DRAW_STATE State->Draw
#define BASIS State->Basis
#define pBASIS State->pBasis
#define POINTS(i) (State->Points[i])
#define POINTSTATUS(i) (State->PointStatus[i])
#define SHAPES(i) (State->Shapes[i])
#define ACTIONS(i) (*PullEl(State->maActions, i, action))
#define DEFAULT_LENGTH 20.f

#include <types.h>

typedef struct debug_text
{
#define DEBUG_TEXT_SIZE 16384
	uint Length;
	char Text[DEBUG_TEXT_SIZE];
} debug_text;
static debug_text DebugText;
#define DebugAdd(txt, ...) DebugText.Length += ssnprintf(DebugText.Text, DEBUG_TEXT_SIZE, "%s"txt, DebugText.Text, __VA_ARGS__)
#define DebugClear() if(DebugText.Length > DEBUG_TEXT_SIZE) DebugText.Length = DEBUG_TEXT_SIZE;\
					 for(unsigned int i = 0; i < DebugText.Length; ++i)  DebugText.Text[i] = 0
#define DebugReplace(txt, ...) DebugClear(); DebugAdd(txt, __VA_ARGS__)

// no prefix
#define STB_SPRINTF_DECORATE(fn) s##fn
#include <stb_sprintf.h>
#include <maths.h>
#include <intrinsics.h>
#define LOGGING 0
#include <debug.h>
#include <geometry.h>
#include <platform.h>
#include <memory.h>
#include "gfx.h"
#include <fonts.h>
#include <input.h>
#include <misc.h>

#define POINT_EPSILON 0.02f

typedef enum file_action
{
	FILE_None = 0,
	FILE_Save,
	FILE_Open,
	FILE_New,
	// TODO (feature): FILE_ExportPNG,
	FILE_ExportSVG,
	FILE_Close
} file_action;

typedef struct platform_request
{
	b32 NewWindow;
	file_action Action;
	b32 Pan;
} platform_request;

typedef struct basis
{
	v2 XAxis; // NOTE: describes X-axis. Y-axis is the perp.
	v2 Offset;
	// TODO: include in XAxis
	f32 Zoom;
} basis;
#define INITIAL_ZOOM 0.1f
#define DefaultBasis (basis){ (v2){1.f, 0.f}, (v2){0.f, 0.f}, INITIAL_ZOOM }

typedef struct line
{
	// NOTE: corresponds with point in index
	uint P1;
	uint P2;
} line;
line ZeroLine = {0};

// TODO: should circles and arcs be consolidated?
typedef struct circle
{
	uint ipoFocus;
	uint ipoRadius;
} circle;

typedef struct arc
{
	uint ipoFocus;
	uint ipoStart;
	uint ipoEnd;
} arc;

typedef enum shape_types
{
	SHAPE_Free = 0,
	SHAPE_Line,
	SHAPE_Ray,
	SHAPE_Segment,
	SHAPE_Circle,
	SHAPE_Arc,
} shape_types;

// TODO (opt): define the statements to redo each action here?
#define ACTION_TYPES \
	ACTION_TYPE(ACTION_Reset   = 0,             "Reset"                  ) \
	ACTION_TYPE(ACTION_Line    = SHAPE_Line,    "Add line"               ) \
	ACTION_TYPE(ACTION_Ray     = SHAPE_Ray,     "Add ray"                ) \
	ACTION_TYPE(ACTION_Segment = SHAPE_Segment, "Add segment"            ) \
	ACTION_TYPE(ACTION_Circle  = SHAPE_Circle,  "Add circle"             ) \
	ACTION_TYPE(ACTION_Arc     = SHAPE_Arc,     "Add arc"                ) \
	ACTION_TYPE(ACTION_Point,                   "Add point"              ) \
	ACTION_TYPE(ACTION_RemovePt,                "Remove Pt"              ) \
	ACTION_TYPE(ACTION_Basis,                   "Change Basis"           ) \
	ACTION_TYPE(ACTION_NON_USER,                "**NON-USER**"           ) \
	ACTION_TYPE(ACTION_NonUserLine,             "Add line (non-user)"    ) \
	ACTION_TYPE(ACTION_NonUserRay,              "Add ray (non-user)"     ) \
	ACTION_TYPE(ACTION_NonUserSegment,          "Add segment (non-user)" ) \
	ACTION_TYPE(ACTION_NonUserCircle,           "Add circle (non-user)"  ) \
	ACTION_TYPE(ACTION_NonUserArc,              "Add arc (non-user)"     ) \
	ACTION_TYPE(ACTION_NonUserPoint,            "Add point (non-user)"   )

#define USERIFY_ACTION(a) ((a) < ACTION_NON_USER ? (a) : (a) - ACTION_NON_USER)

typedef enum action_types
{
#define ACTION_TYPE(a, b) a,
	ACTION_TYPES
#undef ACTION_TYPE
	ACTION_Count
} action_types;

char *ActionTypesStrings[] =
{
#define ACTION_TYPE(a, b) b,
	ACTION_TYPES
#undef ACTION_TYPE
};
#undef ACTION_TYPES

typedef enum input_mode
{
	MODE_Normal = 0,
	MODE_SetBasis,
	MODE_SetLength,
		MODE_DrawArc,
			MODE_ExtendArc,
	MODE_QuickSeg,
		MODE_DrawSeg,
			MODE_SetPerp,
			MODE_ExtendSeg,
			MODE_ExtendLinePt,
} input_mode;
char *InputModeText[] =
{
	"MODE_Normal",
	"MODE_SetBasis",
	"MODE_SetLength",
		"MODE_DrawArc",
			"MODE_ExtendArc",
	"MODE_QuickSeg",
		"MODE_DrawSeg",
			"MODE_SetPerp",
			"MODE_ExtendSeg",
			"MODE_ExtendLinePt",
};

typedef union shape_union
{
#define NUM_SHAPE_POINTS 3
	line Line;
	circle Circle;
	arc Arc;
	uint P[NUM_SHAPE_POINTS];
} shape_union;

typedef struct shape
{
	// TODO: does this guarantee a size?
	shape_types Kind;
	shape_union;
} shape;
shape gZeroShape;

typedef struct action
{
	action_types Kind;
	uint i;
	union {
		shape_union;
		basis Basis;
		struct
		{
			v2 po;
			u8 PointStatus;
		};
	};
} action;

internal inline b32
ShapeEq(shape S1, shape S2)
{
	b32 Result = S1.Kind == S2.Kind &&
				 S1.P[0] == S2.P[0] &&
				 S1.P[1] == S2.P[1] &&
				 S1.P[2] == S2.P[2];
	return Result;
}

typedef void drawstring(image_buffer *ImgBuffer, font *Font, char *Str, f32 SizeInEms, f32 XOffset, f32 YOffset, b32 InvDirection, colour Colour);
typedef struct debug
{
	image_buffer *Buffer;
	drawstring *Print;
	font Font;
	v2 P;
	f32 FontSize;
} debug;
global_variable debug Debug;
#define DebugPrint() Debug.Print(Debug.Buffer, &Debug.Font, DebugText.Text, Debug.FontSize, Debug.P.X, Debug.P.Y, 0, BLACK)

// TODO: make these as orthogonal as possible?
typedef enum point_flags
{
	POINT_Free         = 0,
	POINT_Extant       = (1 << 0),
	POINT_Line         = (1 << 1),
	POINT_Intersection = (1 << 2),
	POINT_Focus        = (1 << 3),
	POINT_Text         = (1 << 4),
	POINT_Radius       = (1 << 5), // maybe POINT_Dist | POINT_Arc ?
	// POINT_Dir
	POINT_Arc          = (1 << 6),
	POINT_Dist         = (1 << 7), 
} point_flags;

// TODO: add prev valid shape snap point for when cursor is at circle centre
typedef struct state
{
	v2 *Points;
	shape *Shapes;
	u8 *PointStatus;
	// TODO (opt): PointStatus now just a marker for free points...
	// could use bit vector? NaN in the point values?
	memory_arena maPoints;
	memory_arena maPointStatus;
	memory_arena maShapes;
	memory_arena maIntersects;
	memory_arena maActions; 
	memory_arena maShapesNearScreen;
	memory_arena maPointsOnScreen;

	basis Basis;
	basis pBasis;

	uint iSaveAction;
	uint iCurrentAction;
	uint iLastAction;
	uint iLastPoint;
	uint iLastShape;

	u64 FrameCount;
	f32 dt;
	f32 tBasis;
	f32 Length;
	f32 pLength;
	// TODO IMPORTANT (feature): include these in save
	f32 LengthStores[52];

	font DefaultFont;
	uint cchFilePath;
	char *FilePath; // allocc'd
	// TODO: turn bools into flags?
	b32 ShowDebugInfo;
	b32 ShowHelpInfo;
	input_mode InputMode;

	// TODO: Consolidate to 2 points used as determined by flags
	uint ipoDrag; // TODO: consolidate into ipoSelect
	uint ipoSelect;
	union
	{
		uint ipoArcStart; // Non-zero -> drawing arc
		uint ipoLength; // Lines at length aways from start
	};
	v2 poSaved;
	v2 PerpDir;

	u8 SavedStatus[2];
	// NOTE: woefully underspecced:
	u64 OverflowTest;
} state;

#if INTERNAL
internal void
LogActionsToFile(state *State, char *FilePath)
{
	FILE *ActionFile = fopen(FilePath, "w");

	memory_arena maActions = State->maActions;
	// TODO (refactor): ignores initial Reset, make consistent
	Assert(State->iLastAction == maActions.Used/sizeof(action) - 1);
	// NOTE: account for initial offset
	action *Actions = (action *)maActions.Base;
	uint iLastAction = State->iLastAction;
	uint iCurrentAction = State->iCurrentAction;

	for(uint iAction = 1; iAction <= iLastAction; ++iAction)
	{
		action Action = Actions[iAction];

		fprintf(ActionFile,
				"Action %2u: %s",
				iAction,
				ActionTypesStrings[Action.Kind]);

		if(Action.i)
		{ fprintf(ActionFile, " -> [%u]", Action.i); }

		if(iAction == iCurrentAction)
		{ fprintf(ActionFile, "\t\t<-- CURRENT"); }

		fprintf(ActionFile, "\n");


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
				v2 po1 = POINTS(ipo1);
				v2 po2 = POINTS(ipo2);
				fprintf(ActionFile,
						"\tPoint 1: %u (%.3f, %.3f)\n"
						"\tPoint 2: %u (%.3f, %.3f)\n",
						ipo1, po1.X, po1.Y,
						ipo2, po2.X, po2.Y);
			} break;

			case ACTION_Circle:
			{
				v2 po1 = POINTS(ipo1);
				v2 po2 = POINTS(ipo2);
				fprintf(ActionFile,
						"\tFocus:  %u (%.3f, %.3f)\n"
						"\tRadius: %u (%.3f, %.3f)\n",
						ipo1, po1.X, po1.Y,
						ipo2, po2.X, po2.Y);
			} break;

			case ACTION_Arc:
			{
				v2 po1 = POINTS(ipo1);
				v2 po2 = POINTS(ipo2);
				v2 po3 = POINTS(ipo3);
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
#endif // INTERNAL

internal inline void
UpdateDrawPointers(state *State)
{
	State->Points      = (v2 *)State->maPoints.Base;
	State->PointStatus = (u8 *)State->maPointStatus.Base;
	State->Shapes      = (shape *)State->maShapes.Base;
	// NOTE: ignore space for empty zeroth pos
	State->iLastPoint = (uint)State->maPoints.Used / sizeof(v2) - 1;
	State->iLastShape = (uint)State->maShapes.Used / sizeof(shape) - 1;
}

internal void
ResetNoAction(state *State)
{
	BEGIN_TIMED_BLOCK;
	for(uint i = 1; i <= State->iLastPoint; ++i)
	{ POINTSTATUS(i) = POINT_Free; }
	// NOTE: Point index 0 is reserved for null points (not defined in lines)

	State->maPoints.Used           = sizeof(v2);
	State->maPointStatus.Used      = sizeof(u8);
	State->maShapes.Used           = sizeof(shape);
	State->maIntersects.Used       = sizeof(v2);
	State->maShapesNearScreen.Used = 0;
	State->maPointsOnScreen.Used   = 0;
	UpdateDrawPointers(State);

	State->Basis = DefaultBasis;

	State->tBasis        = 1.f;
	State->ipoSelect     = 0;
	State->ipoArcStart   = 0;

	State->Length = DEFAULT_LENGTH;

	END_TIMED_BLOCK;
}

internal void
Reset(state *State)
{
	ResetNoAction(State);
	action Action;
	Action.Kind = ACTION_Reset;
	AppendStruct(&State->maActions, action, Action);
	++State->iLastAction;
}

// TODO (internal): for some reason these aren't found by the compiler
// when they're in `geometry.h`, but that's where they should live!
internal aabb
AABBExpand(aabb Expandee, aabb Expander)
{
	aabb Result = Expandee;
	if(Expander.MinX < Result.MinX) { Result.MinX = Expander.MinX; }
	if(Expander.MaxX > Result.MaxX) { Result.MaxX = Expander.MaxX; }
	if(Expander.MinY < Result.MinY) { Result.MinY = Expander.MinY; }
	if(Expander.MaxY > Result.MaxY) { Result.MaxY = Expander.MaxY; }
	return Result;
}

internal b32
PointInAABB(v2 P, aabb AABB)
{
	b32 InsideHorz = P.X >= AABB.MinX && P.X <= AABB.MaxX; 
	b32 InsideVert = P.Y >= AABB.MinY && P.Y <= AABB.MaxY;
	b32 Result = InsideHorz && InsideVert;
	return Result;
}

internal b32
AABBOverlaps(aabb A, aabb B)
{
	b32 HorizontOverlap = (A.MinX >= B.MinX && A.MinX <= B.MaxX) ||
	                      (A.MaxX >= B.MinX && A.MaxX <= B.MaxX) ||
	                      (B.MinX >= A.MinX && B.MinX <= A.MaxX) ||
	                      (B.MinX >= A.MinX && B.MinX <= A.MaxX);
	b32 VerticalOverlap = (A.MinY >= B.MinY && A.MinY <= B.MaxY) ||
	                      (A.MaxY >= B.MinY && A.MaxY <= B.MaxY) ||
	                      (B.MaxY >= A.MinY && B.MaxY <= A.MaxY) ||
	                      (B.MaxY >= A.MinY && B.MaxY <= A.MaxY);
	b32 Result = HorizontOverlap && VerticalOverlap;
	return Result;
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



#define UPDATE_AND_RENDER(name) platform_request name(image_buffer *ScreenBuffer, memory *Memory, input Input)

DECLARE_DEBUG_FUNCTION;

#define TEST_INPUT() DrawRectangleFilled(ScreenBuffer, Origin, ScreenSize, RED)
#define TEST_INPUT2() DrawRectangleFilled(ScreenBuffer, Origin, ScreenSize, GREEN)
#define TEST_INPUT3() DrawRectangleFilled(ScreenBuffer, Origin, ScreenSize, BLUE)

#define GEOMETER_H
#endif
