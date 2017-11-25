#ifndef GEOMETER_H
#define DRAW_STATE State->Draw[State->iCurrentDraw]
#define pDRAW_STATE State->Draw[iDrawOffset(State, -1)]
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
#define DebugAdd(txt, ...) DebugText.Length += stbsp_snprintf(DebugText.Text, DEBUG_TEXT_SIZE, "%s"txt, DebugText.Text, __VA_ARGS__)
#define DebugClear() if(DebugText.Length > DEBUG_TEXT_SIZE) DebugText.Length = DEBUG_TEXT_SIZE;\
					 for(unsigned int i = 0; i < DebugText.Length; ++i)  DebugText.Text[i] = 0
#define DebugReplace(txt, ...) DebugClear(); DebugAdd(txt, __VA_ARGS__)

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

typedef struct file_actions
{
	b32 SaveAs;
	b32 SaveFile;
	b32 OpenFile;
	b32 NewFile;
	b32 CloseApp;
} file_actions;

typedef struct basis
{
	v2 XAxis; // NOTE: describes X-axis. Y-axis is the perp.
	v2 Offset;
	// TODO: include in XAxis
	f32 Zoom;
} basis;

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

typedef enum action_types
{
	ACTION_Reset   = -2,
	ACTION_Remove  = -1,
	ACTION_Basis   = 0,
	ACTION_Line    = SHAPE_Line,
	ACTION_Ray     = SHAPE_Ray,
	ACTION_Segment = SHAPE_Segment,
	ACTION_Circle  = SHAPE_Circle,
	ACTION_Arc     = SHAPE_Arc,
	ACTION_Point,
} action_types;

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
	line Line;
	circle Circle;
	arc Arc;
	uint P[3];
} shape_union;

typedef struct shape
{
	// TODO: does this guarantee a size?
	shape_types Kind;
	shape_union;
} shape;
shape gZeroShape;

// TODO: will be more meaningful when intersections are separate
// should map to user actions -> better undo facility
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

typedef struct draw_state
{
	memory_arena maPoints;
	memory_arena maPointStatus;
	memory_arena maShapes;
	basis Basis;
} draw_state;

typedef struct state
{
	v2 *Points;
	shape *Shapes;
	u8 *PointStatus;
	memory_arena maActions; 

#define NUM_UNDO_STATES 16
	draw_state Draw[NUM_UNDO_STATES];
	uint iLastDraw;
	uint iCurrentDraw;
	// NOTE: only for when only < NUM_UNDO_STATES are used
	uint cDraws;

	basis *Basis;
	// NOTE: probably has to be a pointer to a draw state's basis
	// if you want bases to create a new undo state
	basis pBasis;

	uint iLastAction;
	uint iLastPoint;
	uint iLastShape;

	u64 FrameCount;
	f32 dt;
	f32 tBasis;
	f32 Length;
	f32 pLength;
	// TODO (feature): include these in save
	f32 LengthStores[52];

	font DefaultFont;
	uint cchFilePath;
	char *FilePath; // allocc'd
	// TODO: turn bools into flags?
	b32 ShowDebugInfo;
	b32 ShowHelpInfo;
	b32 Modified; // TODO: set to 0 when undos reach save level
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

internal inline uint
iDrawOffset(state *State, int Offset)
{
	uint Result = (State->iCurrentDraw + Offset) % NUM_UNDO_STATES;
	return Result;
}

internal inline void
UpdateDrawPointers(state *State, uint iPrevDraw)
{
	draw_state *Draw = State->Draw;
	uint iCurrentDraw = State->iCurrentDraw;
	State->Points      = (v2 *)Draw[iCurrentDraw].maPoints.Base;
	State->PointStatus = (u8 *)Draw[iCurrentDraw].maPointStatus.Base;
	State->Shapes      = (shape *)Draw[iCurrentDraw].maShapes.Base;
	// NOTE: ignore space for empty zeroth pos
	State->iLastPoint = (uint)Draw[iCurrentDraw].maPoints.Used / sizeof(v2) - 1;
	State->iLastShape = (uint)Draw[iCurrentDraw].maShapes.Used / sizeof(shape) - 1;
	State->pBasis = Draw[iPrevDraw].Basis;
	State->Basis  = &Draw[iCurrentDraw].Basis;
	State->tBasis = 0;
}

internal void
Reset(state *State)
{
	BEGIN_TIMED_BLOCK;
	for(uint i = 1; i <= State->iLastPoint; ++i)
	{ POINTSTATUS(i) = POINT_Free; }
	// NOTE: Point index 0 is reserved for null points (not defined in lines)
	DRAW_STATE.maPoints.Used  = sizeof(v2);
	DRAW_STATE.maPointStatus.Used  = sizeof(u8);
	DRAW_STATE.maShapes.Used  = sizeof(shape);
	UpdateDrawPointers(State, 1);

	State->Basis->XAxis  = V2(1.f, 0.f);
	State->Basis->Offset = ZeroV2;
	State->Basis->Zoom   = 0.1f;

	State->cDraws = 0;

	State->tBasis        = 1.f;
	State->ipoSelect     = 0;
	State->ipoArcStart   = 0;

	State->Length = DEFAULT_LENGTH;

	PushStruct(&State->maActions, action);
	action Action;
	Action.Kind = ACTION_Reset;
	ACTIONS(++State->iLastAction) = Action;

	END_TIMED_BLOCK;
}

internal inline void
SaveUndoState(state *State)
{
	BEGIN_TIMED_BLOCK;
	uint iPrevDraw = State->iCurrentDraw;
	State->iCurrentDraw = iDrawOffset(State, 1);
	// NOTE: prevents redos
	State->iLastDraw = State->iCurrentDraw;

	draw_state *Draw = State->Draw;
	CopyArenaContents(Draw[iPrevDraw].maPoints, &Draw[State->iCurrentDraw].maPoints);
	CopyArenaContents(Draw[iPrevDraw].maShapes, &Draw[State->iCurrentDraw].maShapes);
	CopyArenaContents(Draw[iPrevDraw].maPointStatus, &Draw[State->iCurrentDraw].maPointStatus);
	Draw[State->iCurrentDraw].Basis = Draw[iPrevDraw].Basis;
	
	UpdateDrawPointers(State, iPrevDraw);

	++State->cDraws;
	State->Modified = 1;
	END_TIMED_BLOCK;
}


#define UPDATE_AND_RENDER(name) file_actions name(image_buffer *ScreenBuffer, memory *Memory, input Input)

DECLARE_DEBUG_FUNCTION;

#define TEST_INPUT() DrawRectangleFilled(ScreenBuffer, Origin, ScreenSize, RED)
#define TEST_INPUT2() DrawRectangleFilled(ScreenBuffer, Origin, ScreenSize, GREEN)
#define TEST_INPUT3() DrawRectangleFilled(ScreenBuffer, Origin, ScreenSize, BLUE)

#define GEOMETER_H
#endif
