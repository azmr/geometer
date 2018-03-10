#if !SINGLE_EXECUTABLE
internal void PrintDebugHierarchy(image_buffer ScreenBuffer, input Input);

#if !defined(GEOMETER_DEBUG_H) && defined(GEOMETER_DEBUG_IMPLEMENTATION)
#include "blit32.h"
#include "blit16.h"

typedef struct debug_print_props
{
	uint *Buffer;
	int RowStride;
	uint X, Y;
	int Scale;
	uint Col;
} debug_print_props;

internal inline void
PrintIndented(debug_print_props *Props, int Indent, char *String)
{
	debug_print_props LProps = *Props;
	LProps.X += Indent * 4 * (blit16_WIDTH + 1) * LProps.Scale;

#if 1
	blit16_String(LProps.Buffer, LProps.RowStride, String, LProps.X, LProps.Y, LProps.Scale, LProps.Col);
#else
	blit16_StringLine(LProps.Buffer, LProps.RowStride, -1, String, LProps.X, LProps.Y, LProps.Scale, LProps.Col);
#endif
	Props->Y -= LProps.Scale * blit16_ROW_ADVANCE;
}

u32 DebugRecordsCount;
// anything need returning?
internal void
PrintDebugHierarchy(image_buffer ScreenBuffer, input Input)
{
	keyboard_state Keyboard = Input.New->Keyboard;
	mouse_state Mouse = Input.New->Mouse;
	for(uint i = 1; i <= DebugTweakCount; ++i) {
		debug_variable *Var = &DebugTweakVariables[i];
		DebugHierarchy_InitElement(Var->Name, Var, DebugHierarchyKind_Live, 0);
	}
	for(uint i = 1; i <= DebugWatchCount; ++i) {
		debug_variable *Var = &DebugWatchVariables[i];
		DebugHierarchy_InitElement(Var->Name, Var, DebugHierarchyKind_Observed, 0);
	}
	// TODO: make manual hierarchy parents safe
	debug_hierarchy_el *Profiling = DebugHierarchy_InitElement("Profiling_CycleCounters", 0, DebugHierarchyGroup, 0);
	DEBUG_LIVE_if(Debug_ShowProfilingInHierarchy)
	{
		for(uint i = 0; i <= DebugRecordsCount; ++i) {
			debug_record *Var = &DEBUG_RECORDS[i];
			if(Var->FunctionName)
			{
				DebugHierarchy_InitElement(*Var->Name ? Var->FullName : Var->FunctionName,
				                           Var, DebugHierarchyKind_Profiling, Profiling);
			}
		}
	}

	char DebugVarBuffer[4096] = {0};
	int Indent = 0;
	b32 RequestRewrite = 0;
	int iNum = 0;
	debug_print_props Props = { ScreenBuffer.Memory, -ScreenBuffer.Width, 5, ScreenBuffer.Height, 2, PixelColour(BLACK) };
#define DEBUG_PRINT(...) ssnprintf(DebugVarBuffer, sizeof(DebugVarBuffer), __VA_ARGS__); PrintIndented(&Props, Indent, DebugVarBuffer);
	DEBUG_PRINT(DebugLiveVar_IsCompiling ? "COMPILING "COMPILE_OPT_LEVEL :"");

	for(debug_hierarchy_el *HVar = DebugHierarchy_Next(DebugHierarchy);
		HVar;
		HVar = DebugHierarchy_Next(HVar))
	{ // print and interact with variables in hierarchy
		Assert(HVar->Parent != HVar->NextSibling);
		Indent = DebugHierarchy_Level(*HVar)-1;

		switch(HVar->Kind)
		{
			case DebugHierarchyGroup:
			{
				if(DEBUGPress(Keyboard.Num[iNum]))
				{ HVar->IsClosed = ! HVar->IsClosed; }
				DEBUG_PRINT("%2d)%c%.*s", iNum++, HVar->IsClosed?'>':' ', HVar->NameLength, HVar->Name); 
			} break;

			case DebugHierarchyKind_Observed:
			case DebugHierarchyKind_Live:
			{
				debug_variable *Var = (debug_variable *)HVar->Data;
				switch(Var->Type)
				{
					case DebugVarType_b32:
					case DebugVarType_debug_if:
					{
						b32 *Val = Var->Data;
						if(DEBUGPress(Keyboard.Num[iNum]))
						{
							*Val = ! *Val;
							RequestRewrite = 1;
						}
						DEBUG_PRINT("%2d) %s %u", iNum, HVar->Name, *Val);
					} break;

					case DebugVarType_uint:
					{
						uint *Val = Var->Data;
						if(DEBUGPress(Keyboard.Num[iNum]))
						{
							++*Val;
							RequestRewrite = 1;
						}
						DEBUG_PRINT("%2d) %s %u", iNum, HVar->Name, *Val);
					} break;

					case DebugVarType_f32:
					{
						f32 *Val = Var->Data;
						if(Keyboard.Num[iNum].EndedDown)
						{
							if(Mouse.LMB.EndedDown)
							{
								v2 dMouse = DEBUGdMouse();
								*Val += dMouse.X;
							}
						}
						else if(DEBUGRelease(Keyboard.Num[iNum]))
						{ RequestRewrite = 1; }
						DEBUG_PRINT("%2d) %s %f", iNum, HVar->Name, *Val);
					} break;

					case DebugVarType_char:
					{
						char *Val = Var->Data;
						if(Keyboard.Num[iNum].EndedDown)
						{
						}
						else if(DEBUGRelease(Keyboard.Num[iNum]))
						{ RequestRewrite = 1; }
						DEBUG_PRINT("%2d) %s - %s", iNum, HVar->Name, Val);
					} break;

					case DebugVarType_v2:
					{
						v2 *Val = Var->Data;
						if(Keyboard.Num[iNum].EndedDown)
						{
							if(Mouse.LMB.EndedDown)
							{
								v2 dMouse = DEBUGdMouse();
								*Val = V2Add(*Val, dMouse);
							}
						}
						else if(DEBUGRelease(Keyboard.Num[iNum]))
						{ RequestRewrite = 1; }
						DEBUG_PRINT("%2d) %s { %f, %f }", iNum, HVar->Name, Val->X, Val->Y);
					} break;

					default:
					{
						switch(Var->Type)
						{ // print Var value in appropriate format for type
#define DEBUG_TYPE_MEMBER(struct, member) (((struct *)(Var->Data))->member)
#define DEBUG_TYPE(type, format) DEBUG_TYPE_STRUCT(type, format, *(type *)Var->Data)
#define DEBUG_TYPE_STRUCT(type, format, ...) \
							/* case DebugVarType_## type: { DEBUG_PRINT("%2d) %s "format, iNum, HVar->Name, __VA_ARGS__); } break; */
							DEBUG_TWEAK_TYPES
						}
#undef  DEBUG_TYPE_STRUCT
#undef  DEBUG_TYPE
#undef  DEBUG_TYPE_MEMBER
					}
				}
				++iNum;
			} break;

			case DebugHierarchyKind_Profiling:
			{
				debug_record *Counter = HVar->Data;
				debug_record_hits_cycles HC = DebugRecord_HitCycleCount(Counter);
				if(HC.HitCount) { DEBUG_PRINT("%24s(%4d): %'12ucy %'8uh %'10ucy/h\n",
								  HVar->Name,
								  Counter->LineNumber,
								  HC.CycleCount,
								  HC.HitCount,
								  HC.CycleCount / HC.HitCount); }
			} break;
		}
	}

	if(RequestRewrite)
	{ DebugLiveVar_RewriteConfig("E:\\Documents\\Coding\\C\\geometer\\geometer_live.h"); }
	//RecompileOnRequest(/*start /MIN*/ "cmd /K \"cd E:\\Documents\\Coding\\C\\geometer && build.bat "COMPILE_OPT_LEVEL "\"");
}
#undef DEBUG_PRINT

global_variable char DebugTextBuffer[Megabytes(8)];

DECLARE_DEBUG_RECORDS;
u32 DebugRecordsCount = DEBUG_RECORDS_ENUM;
DECLARE_DEBUG_FUNCTION
{
	DebugTextBuffer[0] = '\0';
	/* debug_state *DebugState = Memory->DebugStorage; */
	int Offset = 0;
	if(1)//DebugState)
	{
		/* u32 HitI = 0; */
		for(u32 i = 0;
			/* i < cCounters; */
			/* i < ArrayCount(DEBUG_RECORDS); */
			i < DEBUG_RECORDS_ENUM;
			++i)
		{
			debug_record *Counter = DEBUG_RECORD(i);

			u64 HitCount_CycleCount = AtomicExchangeU64(&Counter->HitCount_CycleCount, 0);
			u32 HitCount = (u32)(HitCount_CycleCount >> 32);
			u32 CycleCount = (u32)(HitCount_CycleCount & 0xFFFFFFFF);

			if(HitCount)
			{
				Offset +=
					ssnprintf(DebugTextBuffer, Megabytes(8)-1,
								   /* AltFormat ? AltFormat :*/ "%s%24s%8s(%4d): %'12ucy %'8uh %'10ucy/h\n", 
								   DebugTextBuffer,
								   Counter->FunctionName,
								   Counter->Name,
								   Counter->LineNumber,
								   CycleCount,
								   HitCount,
								   CycleCount / HitCount);
				/* TextBuffer[Offset] = '\n'; */
				/* ++HitI; */
			}
		}
		DEBUG_WATCHED_INIT(uint, Debug, Scale, 3);
		switch(Scale % 2)
		{
			case 0: blit16_String(Buffer->Memory, -Buffer->Width, DebugTextBuffer, 300, Buffer->Height - Scale * blit16_HEIGHT, Scale/2, PixelColour(GREY)); break;
			case 1: blit32_String(Buffer->Memory, -Buffer->Width, DebugTextBuffer, 300, Buffer->Height - Scale * blit32_HEIGHT, Scale/2, PixelColour(GREY)); break;
		}
	}
}
DEBUG_HIERARCHY_DECLARATION;
DEBUG_WATCH_DECLARATION;
#define GEOMETER_DEBUG_H
#endif

#endif //!SINGLE_EXECUTABLE
