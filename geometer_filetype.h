#ifndef GEOMETER_FILETYPE_H
// IMPORTANT NOTE: all enums are append-only, don't edit

#define NUM_SHAPE_POINTS 3

#define SHAPE_TYPES /********************************************************/\
	SHAPE_TYPE(SHAPE_Free,    0) \
	SHAPE_TYPE(SHAPE_Line,    1) \
	SHAPE_TYPE(SHAPE_Ray,     2) \
	SHAPE_TYPE(SHAPE_Segment, 3) \
	SHAPE_TYPE(SHAPE_Circle,  4) \
	SHAPE_TYPE(SHAPE_Arc,     5) \

#define SHAPE_TYPE(name, val) name = val,
typedef enum {
	SHAPE_ToggleActive = -1, // ensure negatives allowed
	SHAPE_TYPES
} shape_types;
#undef SHAPE_TYPE

#define SHAPE_TYPE(name, val) #name,
char *ShapeTypesStrings[] = { SHAPE_TYPES };
#undef SHAPE_TYPE
#undef SHAPE_TYPES ////////////////////////////////////////////////////////////

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

typedef union shape_union
{
	line Line;
	circle Circle;
	arc Arc;
	union {
		uint P[NUM_SHAPE_POINTS];
		struct { uint P[NUM_SHAPE_POINTS]; } AllPoints;
	};
} shape_union;

typedef struct shape_v1
{
	// TODO: does this guarantee a size?
	shape_types Kind;
	shape_union;
} shape_v1;


typedef struct compressed_basis
{
	v2 XAxis; // NOTE: Y-axis is the perp. Length is Zoom.
	v2 Offset;
} compressed_basis;
typedef compressed_basis basis_v1;

typedef struct basis_v2
{
	v2 XAxis; // NOTE: describes X-axis. Y-axis is the perp.
	v2 Offset;
	f32 Zoom;
} basis_v2;

// TODO (opt): define the statements to redo each action here?
#define ACTION_TYPES /*******************************************************/\
	ACTION_TYPE(ACTION_Reset   = 0,             "Reset"        ) \
	ACTION_TYPE(ACTION_Line    = SHAPE_Line,    "Add line"     ) \
	ACTION_TYPE(ACTION_Ray     = SHAPE_Ray,     "Add ray"      ) \
	ACTION_TYPE(ACTION_Segment = SHAPE_Segment, "Add segment"  ) \
	ACTION_TYPE(ACTION_Circle  = SHAPE_Circle,  "Add circle"   ) \
	ACTION_TYPE(ACTION_Arc     = SHAPE_Arc,     "Add arc"      ) \
	ACTION_TYPE(ACTION_Point,                   "Add point"    ) \
	ACTION_TYPE(ACTION_RemovePt,                "Remove point" ) \
	ACTION_TYPE(ACTION_RemoveShape,             "Remove shape" ) \
	ACTION_TYPE(ACTION_Basis,                   "Change basis" ) \
	ACTION_TYPE(ACTION_Move,                    "Move point"   ) \

#define ACTION_TYPE(a, b) a,
typedef enum action_types
{
	ACTION_TYPES
	ACTION_Count,
	ACTION_SHAPE_START = ACTION_Line,
	ACTION_SHAPE_END   = ACTION_Arc
} action_types;
#undef ACTION_TYPE

#define ACTION_TYPE(a, b) b,
char *ActionTypesStrings[] = { ACTION_TYPES };
#undef ACTION_TYPE
#undef ACTION_TYPES ///////////////////////////////////////////////////////////

#define IsUserAction(a) ((a) >= 0)
#define USERIFY_ACTION(a) (IsUserAction(a) ? (a) : -(a))
#define NONUSER_ACTION(a) (IsUserAction(a) ? -(a) : (a))

struct shape_action {
	u32 i;
	u32 iLayer;
	shape_union;
};
struct pt_action {
	u32 ipo;
	u32 iLayer;
	u32 Empty_Space_To_Fill; // need another v2 to be useful
	v2 po;
};
struct reset_action {
	u32 i;
	u32 cPoints; // maybe needed
	u32 cShapes; // maybe needed
	f32 Empty_Space_To_Fill; // not needed
};
// TODO: add text to action
// just waste the Kind each time (I think I want to be able to look at any individual action and see what it is
// struct text_action {
// 	u32 ipo;
// 	char Text[12];
// };
// struct more_text_action {
// 	char Text[16];
// };
// struct remove_action {
// 	u32 i[4]; // could be indices of shapes or points
// };

SAssert(sizeof(shape_union) == 3 * sizeof(u32));
SAssert(sizeof(compressed_basis) == 4 * sizeof(u32));
typedef struct action_v1
{
	i32 Kind;
	union {
		struct reset_action_v1 {
			u32 i;
			u32 cPoints; // maybe needed
			u32 cShapes; // maybe needed
			f32 Empty_Space_To_Fill; // not needed
		} Reset;
		struct shape_action_v1 {
			u32 i;
			shape_union;
		} Shape;
		struct move_action_v1 {
			v2 Dir;
			u32 ipo[2]; // could add any number more points...
		} Move;
		struct pt_action_v1 {
			u32 ipo;
			u32 Empty_Space_To_Fill; // need another v2 to be useful
			v2 po;
		} Point;
		compressed_basis Basis;   // 16 bytes - combines XAxis and Zoom
	};
} action_v1;

typedef struct action_v2
{
	// Is there any way of tagging an enum AND having consistent base type?
	i32 Kind; // 4 bytes  ->  smaller type?
	union {
		struct reset_action Reset;
		struct shape_action Shape;
		/* struct move_action Move; */
		struct pt_action Point;
		basis_v2 Basis;
	};
} action_v2;

typedef action_v2 action;
#define HEADER_SECTIONS /****************************************************/\
	HEADER_SECTION(HEAD_Points_v1 = 0,  sizeof(v2)) \
	HEADER_SECTION(HEAD_PointStatus_v1, sizeof(u8)) \
	HEADER_SECTION(HEAD_Shapes_v1,      sizeof(shape_v1)) \
	HEADER_SECTION(HEAD_Actions_v1,     sizeof(action_v1)) \
	HEADER_SECTION(HEAD_Basis_v1,       sizeof(basis_v1)) \
	HEADER_SECTION(HEAD_Lengths_v1,     sizeof(f32)) \
	HEADER_SECTION(HEAD_PointLayer_v1,  sizeof(uint)) \
	HEADER_SECTION(HEAD_Actions_v2,     sizeof(action_v2)) \
	HEADER_SECTION(HEAD_Basis_v2,       sizeof(basis_v2)) \

#define HEADER_SECTION(name, size) name,
typedef enum header_section
{
	//HEAD_Invalid = (u32)-1,
	HEADER_SECTIONS
	HEAD_Count,
	HEAD_Points      = HEAD_Points_v1,
	HEAD_PointStatus = HEAD_PointStatus_v1,
	HEAD_Shapes      = HEAD_Shapes_v1,
	HEAD_Actions     = HEAD_Actions_v2,
	HEAD_Basis       = HEAD_Basis_v2,
	HEAD_Lengths     = HEAD_Lengths_v1,
	HEAD_PointLayer  = HEAD_PointLayer_v1,
} header_section;
#undef HEADER_SECTION

#define HEADER_SECTION(name, size) size,
size_t HeaderElSizes[] = { HEADER_SECTIONS };
#undef HEADER_SECTION
#undef HEADER_SECTIONS ////////////////////////////////////////////////////////

typedef struct file_header
{
	char ID[8];        // unique(ish) text id: "Geometer"
	u16 FormatVersion; // file format version num
	u16 cArrays;       // for data section
	u32 CRC32;         // checksum of data
	u64 cBytes;        // bytes in data section (everything after this point)
	// Data following this point:
	//   cArrays * [
	//     - u32 ElementType; // array type tag (from which element size is known)
	//     - u32 cElements;   // size of array (could be 1 for individual data element)
	//     - Elements...      // array elements
	//   ]
} file_header;

#define GEOMETER_FILETYPE_H
#endif//GEOMETER_FILETYPE_H
