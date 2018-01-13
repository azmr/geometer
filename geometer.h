#ifndef GEOMETER_H
#define BASIS State->Basis
#define pBASIS State->pBasis
#define POINTS(i)      Pull(State->maPoints, i)
#define POINTSTATUS(i) Pull(State->maPointStatus, i)
#define SHAPES(i)      Pull(State->maShapes, i)
#define ACTIONS(i)     Pull(State->maActions, i)
#define DEBUG_LOG_ACTIONS 1
#define DEFAULT_LENGTH 20.f
#define cSTART_POINTS 32

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
#include "geometry.h"
#include <platform.h>
#include <memory.h>
#include "gfx.h"
#include <fonts.h>
#include <input.h>
#include <misc.h>

typedef arena_type(v2);   typedef union v2_arena   v2_arena; // repeated from macro - just for syntax highlighting
typedef arena_type(u8);   typedef union u8_arena   u8_arena; // repeated from macro - just for syntax highlighting
typedef arena_type(uint); typedef union uint_arena uint_arena; // repeated from macro - just for syntax highlighting

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
	SHAPE_ToggleActive = -1, // ensure negatives allowed
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
	ACTION_TYPE(ACTION_RemovePt,                "Remove point"           ) \
	ACTION_TYPE(ACTION_RemoveShape,             "Remove shape"           ) \
	ACTION_TYPE(ACTION_Basis,                   "Change basis"           ) \
	ACTION_TYPE(ACTION_NON_USER,                "**NON-USER**"           ) \
	ACTION_TYPE(ACTION_NonUserLine,             "Add line (non-user)"    ) \
	ACTION_TYPE(ACTION_NonUserRay,              "Add ray (non-user)"     ) \
	ACTION_TYPE(ACTION_NonUserSegment,          "Add segment (non-user)" ) \
	ACTION_TYPE(ACTION_NonUserCircle,           "Add circle (non-user)"  ) \
	ACTION_TYPE(ACTION_NonUserArc,              "Add arc (non-user)"     ) \
	ACTION_TYPE(ACTION_NonUserPoint,            "Add point (non-user)"   ) \
	ACTION_TYPE(ACTION_NonUserRemovePt,         "Remove point (non-user)") \
	ACTION_TYPE(ACTION_NonUserRemoveShape,      "Remove shape (non-user)")

#define USERIFY_ACTION(a) ((a) < ACTION_NON_USER ? (a) : (a) - ACTION_NON_USER)

typedef enum action_types
{
#define ACTION_TYPE(a, b) a,
	ACTION_TYPES
#undef ACTION_TYPE
	ACTION_Count,
	ACTION_SHAPE_START = ACTION_Line,
	ACTION_SHAPE_END   = ACTION_Arc
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
	MODE_QuickPtOrSeg,
	MODE_DragSelect,
	MODE_Selected,
		MODE_AddToSelection,
		MODE_RmFromSelection,
	MODE_Draw,
		MODE_ExtendArc,
		MODE_ExtendSeg,
		MODE_SetPerp,
} input_mode;
char *InputModeText[] =
{
	"MODE_Normal",
	"MODE_SetBasis",
	"MODE_SetLength",
	"MODE_QuickPtOrSeg",
	"MODE_DragSelect",
	"MODE_Selected",
		"MODE_AddToSelection",
		"MODE_RmFromSelection",
	"MODE_Draw",
		"MODE_ExtendArc",
		"MODE_ExtendSeg",
		"MODE_SetPerp",
};

typedef union shape_union
{
#define NUM_SHAPE_POINTS 3
	line Line;
	circle Circle;
	arc Arc;
	union {
		uint P[NUM_SHAPE_POINTS];
		struct { uint P[NUM_SHAPE_POINTS]; } AllPoints;
	};
} shape_union;

typedef struct shape
{
	// TODO: does this guarantee a size?
	shape_types Kind;
	shape_union;
} shape;
shape gZeroShape;
typedef arena_type(shape); typedef union shape_arena shape_arena;

internal inline b32
ShapeEq(shape S1, shape S2)
{
	b32 Result = S1.Kind == S2.Kind &&
				 S1.P[0] == S2.P[0] &&
				 S1.P[1] == S2.P[1] &&
				 S1.P[2] == S2.P[2];
	return Result;
}

typedef struct action
{
	// TODO: choose size for this instead of action_types, otherwise serializing will be unpredictable
	action_types Kind; // MSVC thinks this is 4 bytes
	u32 i;             // 4 bytes
	union {
		shape_union;   // 12 bytes
		basis Basis;   // 20 bytes - could compress to 16 by combining XAxis and Zoom
		struct         // 12?
		{
			v2 po;
			u8 PointStatus;
		};
		// TODO: add char array the size of this union... or I could have ACTION_TextStart and ACTION_TextContinue,
		// just waste the Kind each time (I think I want to be able to look at any individual action and see what it is
	};
} action;
typedef arena_type(action); typedef union action_arena action_arena;

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

// TODO: replace with better way of indicating existence
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
	// TODO (opt): PointStatus now just a marker for free points...
	// could use bit vector? NaN in the point values?
	v2_arena maPoints;
	v2_arena maIntersects;
	// Could optimize by keeping sorted
	uint_arena maPointsOnScreen;
	uint_arena maSelectedPoints;
	shape_arena maShapesNearScreen;
	shape_arena maShapes;
	u8_arena maPointStatus;
	action_arena maActions; 

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
	f32 LengthStores[26];

	font DefaultFont;
	uint cchFilePath;
	char *FilePath; // allocc'd
	// TODO: turn bools into flags?
	b32 ShowDebugInfo;
	b32 ShowHelpInfo;
	input_mode InputMode;

	// TODO: Consolidate to 2 points used as determined by flags
	uint ipoDrag; // TODO: consolidate into ipoSelect
	v2 poArcStart;
	v2 poSelect;
	v2 poSaved;
	v2 PerpDir;

	u8 SavedStatus[2];
	// NOTE: woefully underspecced:
	u64 OverflowTest;
} state;

#include "geometer_core.c"

#define UPDATE_AND_RENDER(name) platform_request name(image_buffer *ScreenBuffer, memory *Memory, input Input)

DECLARE_DEBUG_FUNCTION;

#define TEST_INPUT() DrawRectangleFilled(ScreenBuffer, Origin, ScreenSize, RED)
#define TEST_INPUT2() DrawRectangleFilled(ScreenBuffer, Origin, ScreenSize, GREEN)
#define TEST_INPUT3() DrawRectangleFilled(ScreenBuffer, Origin, ScreenSize, BLUE)

#define GEOMETER_H
#endif
