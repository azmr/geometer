#ifndef GEOMETER_H
#define BASIS_DEFAULT_ZOOM 0.1f
#define BASIS State->Basis
#define pBASIS State->pBasis
#define POINTS(i)      Pull(State->maPoints, i)
#define POINTSTATUS(i) Pull(State->maPointStatus, i)
#define POINTS_OS(i)   Pull(State->maPointsOnScreen, i)
#define POINTLAYER(i)  Pull(State->maPointLayer, i)
#define SHAPES(i)      Pull(State->maShapes, i)
#define ACTIONS(i)     Pull(State->maActions, i)
#define DEBUG_LOG_ACTIONS 0
#define DEFAULT_LENGTH 20.f
#define cSTART_POINTS 32
#define BASIS_ANIMATION_SPEED 8.f
#define MOUSE_ANIMATION_SPEED 30.f
#define DRAWER_ANIMATION_SPEED 10.f
#define MAX_LAYERS 5
#define POINT_RADIUS 4.f
#define ACTIVE_POINT_RADIUS 6.f

#include <types.h>
/* #include "geometer_config.h" */
// TODO: DEBUG_TYPE_MEMBERS
#define DEBUG_TYPES \
	DEBUG_TYPE(b32, "%d") \
	DEBUG_TYPE(int, "%d") \
	DEBUG_TYPE(char, "%c") \
	DEBUG_TYPE(uint, "%u") \
	DEBUG_TYPE(f32, "%ff") \
	DEBUG_TYPE_STRUCT(v2, "{ %ff, %ff }", DEBUG_TYPE_MEMBER(v2, X), DEBUG_TYPE_MEMBER(v2, Y)) \

#include "geometer_live.h"

#define DEBUG_HIERARCHY_KINDS \
	DEBUG_HIERARCHY_KIND(debug_variable, Live) \
	DEBUG_HIERARCHY_KIND(debug_variable, Observed) \
	DEBUG_HIERARCHY_KIND(debug_record, Profiling)
#define DEBUG_HIERARCHY_MULTIPLE 4
#include <live_edit/hierarchy.h>

// minimal prefix to prevent collision with stdio
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

#define DEBUG_TWEAK_IMPLEMENTATION
#include <live_edit/live_variable.h>

#define POINT_EPSILON 0.02f

#include "geometer_filetype.h"
typedef action_v2 action;
typedef basis_v2 basis;
typedef shape_v1 shape;    shape gZeroShape;
typedef arena_type(shape);  typedef union shape_arena shape_arena;
typedef arena_type(v2);     typedef union v2_arena   v2_arena;   // repeated from macro - just for syntax highlighting
typedef arena_type(u8);     typedef union u8_arena   u8_arena;   // repeated from macro - just for syntax highlighting
typedef arena_type(uint);   typedef union uint_arena uint_arena; // repeated from macro - just for syntax highlighting
typedef arena_type(action); typedef union action_arena action_arena;


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

#define INITIAL_ZOOM 0.1f
#define DefaultBasis (basis){ (v2){1.f, 0.f}, (v2){0.f, 0.f}, INITIAL_ZOOM }

typedef enum input_mode
{
	MODE_Normal = 0,
	MODE_SetBasis,
	MODE_SetLength,
	MODE_QuickPtOrSeg,
	MODE_Draw,
		MODE_ExtendArc,
		MODE_ExtendSeg,
		MODE_SetPerp,
	MODE_BoxSelect,
	MODE_Selected,
		MODE_AddToSelection,
		MODE_DragMove,
		MODE_RmFromSelection,

	MODE_Count,
	/* MODE_START_Select = MODE_DragSelect, */
	/* MODE_END_Select   = MODE_RmFromSelection, */
	/* MODE_START_Draw   = MODE_QuickPtOrSeg, */
	/* MODE_END_Draw     = MODE_SetPerp, */
} input_mode;
char *InputModeText[] =
{
	"MODE_Normal",
	"MODE_SetBasis",
	"MODE_SetLength",
	"MODE_BoxSelect",
	"MODE_Selected",
		"MODE_AddToSelection",
		"MODE_DragMove",
		"MODE_RmFromSelection",
	"MODE_QuickPtOrSeg",
	"MODE_Draw",
		"MODE_ExtendArc",
		"MODE_ExtendSeg",
		"MODE_SetPerp",
};

internal inline b32
ShapeEq(shape S1, shape S2)
{
	b32 Result = S1.Kind == S2.Kind &&
				 S1.P[0] == S2.P[0] &&
				 S1.P[1] == S2.P[1] &&
				 S1.P[2] == S2.P[2];
	return Result;
}

// TODO: other smears

#define FN_CIRCLE(name) void name(struct draw_buffer *Draw, v2 Centre, f32 Radius, colour Colour)
#define FN_2VEC(name, v_1, v_2) void name(struct draw_buffer *Draw, v2 v_1, v2 v_2, colour Colour)

struct draw_buffer;
#define FN_DrawSeg(       name) FN_2VEC(name, Point1, Point2)
#define FN_DrawLine(      name) FN_2VEC(name, P, Dir)
#define FN_DrawCircleFill(name) FN_CIRCLE(name)
#define FN_DrawCircleLine(name) FN_CIRCLE(name)
#define FN_DrawRectFill(  name) FN_2VEC(name, vMin, vMax)
#define FN_DrawRectLine(  name) FN_2VEC(name, vMin, vMax)
#define FN_DrawArcLine(   name) void         name(struct draw_buffer *Draw, v2 Centre, f32 Radius, v2 A, v2 B, colour Colour)
#define FN_ClearBuffer(   name) void         name(image_buffer Buffer)
#define FN_ClipBuffer(    name) image_buffer name(image_buffer Buffer, v2 Offset, v2 Size)
#define FN_DrawCircleLineSmear(name) FN_CIRCLE(name)

#define FN(prefix, fn) FN_##fn (prefix ## fn)

#define DRAW_FNS \
	DRAW_FN(DrawSeg) \
	DRAW_FN(DrawLine) \
	DRAW_FN(DrawCircleFill) \
	DRAW_FN(DrawCircleLine) \
	DRAW_FN(DrawCircleLineSmear) \
	DRAW_FN(DrawArcLine) \
	DRAW_FN(DrawRectFill) \
	DRAW_FN(DrawRectLine) \
	DRAW_FN(ClearBuffer) \
	DRAW_FN(ClipBuffer) \

#define DRAW_FN(name)\
typedef FN_## name (fn_## name);
DRAW_FNS
#undef DRAW_FN

typedef struct draw_buffer
{
	enum {
		DRAW_Software,
		DRAW_Hardware,
	} Kind;
	// TODO: unions etc
	image_buffer Buffer;
	f32 StrokeWidth;
	v2 Smear;

#define DRAW_FN(name)\
	fn_##name *name;
	DRAW_FNS
#undef DRAW_FN
	// TODO: String
} draw_buffer;

typedef void drawstring(image_buffer *ImgBuffer, font *Font, char *Str, f32 SizeInEms, f32 XOffset, f32 YOffset, b32 InvDirection, colour Colour);
typedef struct debug
{
	image_buffer Buffer;
	drawstring *Print;
	font Font;
	v2 P;
	f32 FontSize;
} debug;
global_variable debug Debug;
#define DebugPrint() Debug.Print(&Debug.Buffer, &Debug.Font, DebugText.Text, Debug.FontSize, Debug.P.X, Debug.P.Y, 0, BLACK)

typedef enum {
	FX_Smear,

	FX_COUNT,
} fx;

// TODO: add prev valid shape snap point for when cursor is at circle centre
typedef struct state
{
	// TODO (opt): PointStatus now just a marker for free points...
	// could use bit vector? NaN in the point values?
	v2_arena maPoints;
	v2_arena maIntersects;
	// Could optimize by keeping sorted
	v2_arena maPointsOnScreen;
	uint_arena maSelectedPoints;
	shape_arena maShapesNearScreen;
	shape_arena maShapes;
	u8_arena maPointStatus;
	uint_arena maPointLayer;
	action_arena maActions; 

	basis Basis;
	basis pBasis;
	f32 tBasis;

	uint iCurrentLayer;
	uint iSaveAction;
	uint iCurrentAction;
	uint iLastAction;
	uint iLastPoint;
	uint iLastShape;

	u64 FrameCount;
	f32 dt;
	f32 dtWork;
	f32 Length;
	f32 pLength;
	f32 LengthStores[26];

	font DefaultFont;
	uint cchFilePath;
	char *FilePath; // allocc'd
	// TODO: turn bools into flags?
	b32 ShowHelpInfo;
	b32 ArcSwapDirection;
	b32 ShowLayerDrawer; // somewhat redundant with t
	f32 tLayerDrawer;
	input_mode InputMode;

	// TODO: Consolidate to 2 points used as determined by flags
	uint ipoDrag; // TODO: consolidate into ipoSelect
	v2 poArcStart;
	v2 poSelect;
	v2 poSaved;
	v2 PerpDir;
	v2 pSnapMouseP; // TODO (opt): remove?
	f32 tSnapMouseP;

	b8 FX[FX_COUNT];
	u8 SavedStatus[2];
	// NOTE: woefully underspecced:
	u64 OverflowTest;
} state;

#include "geometer_core.c"

#define UPDATE_AND_RENDER(name) platform_request name(draw_buffer *Draw, memory *Memory, input Input)

DECLARE_DEBUG_FUNCTION;

#define TEST_INPUT() DrawRectangleFilled(ScreenBuffer, Origin, ScreenSize, RED)
#define TEST_INPUT2() DrawRectangleFilled(ScreenBuffer, Origin, ScreenSize, GREEN)
#define TEST_INPUT3() DrawRectangleFilled(ScreenBuffer, Origin, ScreenSize, BLUE)

#define GEOMETER_H
#endif
