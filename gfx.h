#if !defined(GFX_H)
#if !defined(BEGIN_TIMED_BLOCK)
#define BEGIN_TIMED_BLOCK
#define END_TIMED_BLOCK
#endif // !defined(BEGIN_TIMED_BLOCK)

#define BytesPerPixel 4

/* #ifdef STB_IMAGE_WRITE_IMPLEMENTATION */
/* #include <stb_png_write.h> */
/* #endif //PNG_IMPLEMENTATION */

// TODO: remove
#include <intrin.h>
typedef struct image_buffer
{
	// NOTE: Pixels are always 32 bits wide, memory order BB GG RR xx
	void *Memory;
	int Width;
	int Height;
	int Pitch;
} image_buffer;

typedef struct alpha_map
{
	u32 Width;
	u32 Height;
	u8 *Memory;
} alpha_map;

typedef struct loaded_bitmap
{
	u32 Width;
	u32 Height;
	u32 *Pixels;
} loaded_bitmap;

typedef struct colour
{
	f32 R;
	f32 G;
	f32 B;
	f32 A;
} colour;

// tables? for light/dark/normal version of colour?
colour P_RED        = {1.0f, 0.0f, 0.0f, 1.0f};
colour P_GREEN      = {0.0f, 1.0f, 0.0f, 1.0f};
colour P_BLUE       = {0.0f, 0.0f, 1.0f, 1.0f};
colour P_YELLOW     = {1.0f, 1.0f, 0.0f, 1.0f};
colour P_MAGENTA    = {1.0f, 0.0f, 1.0f, 1.0f};
colour P_CYAN       = {0.0f, 1.0f, 1.0f, 1.0f};
colour RED          = {0.8f, 0.2f, 0.2f, 1.0f};
colour GREEN        = {0.2f, 0.8f, 0.2f, 1.0f};
colour BLUE         = {0.4f, 0.4f, 0.8f, 1.0f};
colour YELLOW       = {1.0f, 0.8f, 0.0f, 1.0f};
colour MAGENTA      = {0.8f, 0.0f, 0.8f, 1.0f};
colour CYAN         = {0.0f, 0.8f, 0.8f, 1.0f};
colour ORANGE       = {0.9f, 0.6f, 0.2f, 1.0f};
colour BLACK        = {0.0f, 0.0f, 0.0f, 1.0f};
colour WHITE        = {1.0f, 1.0f, 1.0f, 1.0f};
colour GREY         = {0.5f, 0.5f, 0.5f, 1.0f};
colour LIGHT_RED    = {1.0f, 0.65f, 0.65f, 1.0f};
colour LIGHT_GREEN  = {0.65f, 1.0f, 0.65f, 1.0f};
colour LIGHT_BLUE   = {0.65f, 0.65f, 1.0f, 1.0f};
colour LIGHT_GREY   = {0.75f, 0.75f, 0.75f, 1.0f};
colour LIGHT_YELLOW = {1.0f, 0.95f, 0.7f, 1.0f};
colour DARK_RED     = {0.5f, 0.15f, 0.15f, 1.0f};
colour DARK_GREEN   = {0.1f, 0.5f, 0.1f, 1.0f};
colour DARK_BLUE    = {0.1f, 0.1f, 0.5f, 1.0f};
colour DARK_YELLOW  = {0.5f, 0.4f, 0.1f, 1.0f};
colour V_LIGHT_GREY = {0.90f, 0.90f, 0.90f, 1.0f};
colour BLANK_COLOUR = {0.0f, 0.0f, 0.0f, 0.0f};

internal colour
Colour(f32 R, f32 G, f32 B, f32 A)
{
	colour Result;
	Result.R = R;
	Result.G = G;
	Result.B = B;
	Result.A = A;
	return Result;
}

typedef struct aabb_collision_sides
{
	b32 R;
	b32 T;
	b32 L;
	b32 B;
} aabb_collision_sides;
aabb_collision_sides ZeroAABBCollision = {0};

typedef struct line_intermediary
{
	f32 LargestAbsDimension;
	// for v2 largest dimension is always +/- 1, with other dimension the appropriate ratio
	v2 SignedDirFraction;
} line_intermediary;

// NOTE: useful for testing inner loops -> break at each point and check screen state
#ifdef INCLUDE_STB_IMAGE_WRITE_H
int
WritePNG(const char *Filename, image_buffer *ImgBuffer)
{
		return stbi_write_png(Filename, ImgBuffer->Width, ImgBuffer->Height, BytesPerPixel, ImgBuffer->Memory, ImgBuffer->Pitch);
}
#endif

internal inline u32 *
PixelLocationInBuffer(image_buffer *Buffer, size_t X, size_t Y)
{
	u32 *Result = (u32 *)Buffer->Memory + Y*(Buffer->Pitch/BytesPerPixel) + X;
	return Result;
}

internal inline u32
PixelColour(colour Colour)
{
	/* u32 PixelColour = (u32)((RoundF32ToU32(Colour.A * 255.0f) << 24) | */
	/*                         (RoundF32ToU32(Colour.R * 255.0f) << 16) | */
	/* 						(RoundF32ToU32(Colour.G * 255.0f) << 8)  | */
	/* 						(RoundF32ToU32(Colour.B * 255.0f) << 0)); */
	u32 PixelColour = (u32)(Colour.A * 255.0f) << 24 |
	                  (u32)(Colour.R * 255.0f) << 16 |
					  (u32)(Colour.G * 255.0f) <<  8 |
					  (u32)(Colour.B * 255.0f) <<  0 ;
	return PixelColour;
}

internal inline colour
PixelAsColour(u32 Pixel)
{
	// TODO: This may change with different pixel orders
	colour Colour;
		Colour.A = (u8)(Pixel >> 24) / 255.0f;
		Colour.R = (u8)(Pixel >> 16) / 255.0f;
		Colour.G = (u8)(Pixel >>  8) / 255.0f;
		Colour.B = (u8)(Pixel >>  0) / 255.0f;
	return Colour;
}

// TODO: SIMD
internal colour
PreMultiplyAlpha(colour Pixel)
{
	colour Result;
	Result.A = Pixel.A;
	Result.R = Pixel.R * Pixel.A;
	Result.G = Pixel.G * Pixel.A;
	Result.B = Pixel.B * Pixel.A;
	return Result;
}

/// Primarily for stock colours
/// Ignores Col's alpha
internal inline colour
PreMultiplyColour(colour Col, f32 Alpha)
{
	colour Result;
	Result.A = Alpha;
	Result.R = Col.R * Alpha;
	Result.G = Col.G * Alpha;
	Result.B = Col.B * Alpha;
	return Result;
}

internal inline colour
BlendAlphaColourSIMD(colour Dst, colour Src)
{
	if(Src.A == 1)
	{
		return Src;
	}
	else if(Src.A == 0)
	{
		return Dst;
	}

	__m128 SrcWide = _mm_set_ps(1, Src.R, Src.G, Src.B);
	__m128 DstWide = _mm_set_ps(1, Dst.R, Dst.G, Dst.B);
	__m128 SrcA_4x = _mm_set_ps1(Src.A);
	__m128 iSrcA_4x = _mm_set_ps1(1-Src.A);
	// Src * SrcA + Src * (Dest * (1-SrcA))
	__m128 ResultWide = _mm_add_ps(_mm_mul_ps(SrcWide, SrcA_4x), // +
							_mm_mul_ps(SrcWide, // *
								_mm_mul_ps(DstWide, // *
									iSrcA_4x))); 

	colour Result;
	Result.R = ((f32 *)&ResultWide)[2];
	Result.G = ((f32 *)&ResultWide)[1];
	Result.B = ((f32 *)&ResultWide)[0];
	Result.A = ((f32 *)&ResultWide)[3];
	return Result;
}

internal inline colour
BlendPMAColour(colour Dst, colour Src)
{
	f32 iSrcA = 1-Clamp01(Src.A);
	colour Result;

	Result.A = Clamp01(Src.A + Dst.A * iSrcA);
	Result.R = Clamp01(Src.R + Dst.R * iSrcA);
	Result.G = Clamp01(Src.G + Dst.G * iSrcA);
	Result.B = Clamp01(Src.B + Dst.B * iSrcA);

	return Result;
}

internal inline colour
BlendPMAColourSIMD(colour Dst, colour Src)
{
	// TODO: worse prediction with conditional?
	/* if(Src.A == 1) */
	/* { */
	/* 	return Src; */
	/* } */
	/* else if(Src.A == 0) */
	/* { */
	/* 	return Dst; */
	/* } */

	__m128 SrcWide = _mm_set_ps(Src.A, Src.R, Src.G, Src.B);
	__m128 DstWide = _mm_set_ps(Dst.A, Dst.R, Dst.G, Dst.B);
	__m128 iSrcA_4x = _mm_set_ps1(1-Src.A);
	// Src + Dst * (1-SrcA)
	__m128 ResultWide = _mm_add_ps(SrcWide, // +
							_mm_mul_ps(DstWide, // *
								iSrcA_4x)); 

	colour Result;
	Result.R = ((f32 *)&ResultWide)[2];
	Result.G = ((f32 *)&ResultWide)[1];
	Result.B = ((f32 *)&ResultWide)[0];
	Result.A = ((f32 *)&ResultWide)[3];
	return Result;
}

internal inline colour
BlendAlphaColour(colour Dst, colour Src)
{
	if(Src.A == 1)
	{
		return Src;
	}
	else if(Src.A == 0)
	{
		return Dst;
	}

	f32 iSrcA = 1-Src.A;
	colour Result;
	Result.A = 		  Src.A + 		Dst.A*iSrcA;
	Result.R = (Src.R*Src.A + Dst.R*Dst.A*iSrcA) / Result.A;
	Result.G = (Src.G*Src.A + Dst.G*Dst.A*iSrcA) / Result.A;
	Result.B = (Src.B*Src.A + Dst.B*Dst.A*iSrcA) / Result.A;

	return Result;
}

internal inline u32
BlendAlphaPxColour(u32 Dst, colour Src)
{
	BEGIN_TIMED_BLOCK;
	colour DstCol = PixelAsColour(Dst);
	/* for(int i = 0; i < 1000; ++i) */
	/* { */
	/* 	 DstCol = PixelAsColour(Dst); */
	/* } */
	colour ResultCol = BlendPMAColourSIMD(DstCol, Src);
	/* for(int i = 0; i < 1000; ++i) */
	/* { */
	/* 	 ResultCol = BlendPMAColour(DstCol, Src); */
	/* } */
	u32 Result = PixelColour(ResultCol);
	/* for(int i = 0; i < 1000; ++i) */
	/* { */
	/* 	 Result = PixelColour(ResultCol); */
	/* } */
	END_TIMED_BLOCK;
	return Result;
}

#if 1
#pragma pack(push, 1)
typedef struct bitmap_header
{
	u16 FileType;
	u32 FileSize;
	u16 Reserved1;
	u16 Reserved2;
	u32 BitmapOffset;
	u32 Size;
	i32 Width;
	i32 Height;
	u16 Planes;
	u16 BitsPerPixel;
	u32 Compression;
	u32 SizeOfBitmap;
	i32 HorzResolution;
	i32 VertResolution;
	u32 ColoursUsed;
	u32 ColoursImportant;

	// NOTE: Only when Compression == 3
	u32 RedMask;
	u32 GreenMask;
	u32 BlueMask;
} bitmap_header;
#pragma pack(pop)

internal loaded_bitmap
DEBUGLoadBMP(thread_context *Thread, debug_platform_read_entire_file *ReadEntireFile, char *Filename)
{
	loaded_bitmap Result = {0};

	debug_read_file_result ReadResult = ReadEntireFile(Thread, Filename);
	if(ReadResult.ContentsSize != 0)
	{
		bitmap_header *Header = (bitmap_header *)ReadResult.Contents;
		// NOTE: Bitmap scan lines are 4-byte aligned (not a problem for u32)
		u32 *Pixels = (u32 *)((u8 *)ReadResult.Contents + Header->BitmapOffset);
		Result.Pixels  = Pixels;
		Result.Width   = Header->Width;
		Result.Height  = Header->Height;

		Assert(Header->Compression == 3);

		// NOTE: If using generically, BMP files CAN GO IN EITHER DIRECTION
		// and the height will be negative for top-down.
		// (Also, there can be compression, etc... etc...)

		// NOTE: Byte order in memory determined by the Header, so
		// we have to read out the masks and convert the pixels ourselves.
		u32 RedMask   = Header->RedMask;
		u32 GreenMask = Header->GreenMask;
		u32 BlueMask  = Header->BlueMask;
		u32 AlphaMask = ~(RedMask | GreenMask | BlueMask);

		bit_scan_result RedScan   = FindLeastSignificantSetBit(RedMask);
		bit_scan_result GreenScan = FindLeastSignificantSetBit(GreenMask);
		bit_scan_result BlueScan  = FindLeastSignificantSetBit(BlueMask);
		bit_scan_result AlphaScan = FindLeastSignificantSetBit(AlphaMask);

		Assert(RedScan.Found);
		Assert(GreenScan.Found);
		Assert(BlueScan.Found);
		Assert(AlphaScan.Found);

		i32 RedShift   = 16 - (i32)RedScan.Index;
		i32 GreenShift = 8 - (i32)GreenScan.Index;
		i32 BlueShift  = 0 - (i32)BlueScan.Index;
		i32 AlphaShift = 24 - (i32)AlphaScan.Index;

		u32 *SourceDest = Pixels;
		for(i32 Y = 0;
			Y < Header->Height;
			++Y)
		{
			for(i32 X = 0;
				X < Header->Width;
				++X)
			{
				u32 C = *SourceDest;
				// TODO: Is microcoded rotate faster than 2 shifts?
#if 0
				*SourceDest++ = ((((C >> AlphaShift) & 0xFF) << 24)    |
								 (((C >> RedShift)   & 0xFF) << 16)    |
								 (((C >> GreenShift) & 0xFF) <<  8)    |
								 (((C >> BlueShift)  & 0xFF) <<  0));
#else
				*SourceDest++ = (RotateLeft(C & RedMask, RedShift)     |
								 RotateLeft(C & GreenMask, GreenShift) |
								 RotateLeft(C & BlueMask, BlueShift)   |
								 RotateLeft(C & AlphaMask, AlphaShift));
#endif
			}
		}
	}

	return Result;
}
#endif

internal void
DrawAlphaMapAligned(image_buffer *Buffer, alpha_map *Map,
					v2 Pos,
					i32 AlignX, i32 AlignY,
					colour Colour)
{
	BEGIN_TIMED_BLOCK;
	Pos.X -= (f32)AlignX;
	Pos.Y -= (f32)AlignY;

	i32 MinX = RoundF32ToI32(Pos.X);
	i32 MinY = RoundF32ToI32(Pos.Y);
	i32 MaxX = RoundF32ToI32(Pos.X + (f32)Map->Width);
	i32 MaxY = RoundF32ToI32(Pos.Y + (f32)Map->Height);

	i32 SourceOffsetX = 0;
	if(MinX < 0)
	{
		SourceOffsetX = -MinX;
		MinX = 0;
	}

	i32 SourceOffsetY = 0;
	if(MinY < 0)
	{
		SourceOffsetY = -MinY;
		MinY = 0;
	}

	if(MaxX > Buffer->Width)  { MaxX = Buffer->Width; }
	if(MaxY > Buffer->Height) { MaxY = Buffer->Height; }

	// TODO: SourceRow needs to be changed based on clipping.
	// NOTE: Width*(Height - 1) brings to first pixel of last row.
#define SCREEN_Y_DIRECTION 1
#if SCREEN_Y_DIRECTION == 1
	u8 *SourceRow = Map->Memory;
#else
	u8 *SourceRow = Map->Memory + Map->Width*(Map->Height - 1);
#endif
	SourceRow += -(i32)Map->Width*SourceOffsetY + SourceOffsetX;
	u8 *DestRow = ((u8 *)Buffer->Memory +
			MinX*BytesPerPixel +
			MinY*Buffer->Pitch);

	for(int Y = MinY;
			Y < MaxY;
			++Y)
	{
		u32 *Dest = (u32 *)DestRow;
		u8 *Source = SourceRow;
		for(int X = MinX;
				X < MaxX;
				++X)
		{
			f32 SA = ((f32)(*Source) / 255.0f) * Colour.A;
			colour SourceCol = PreMultiplyColour(PixelAsColour(*Source), SA);

			*Dest = BlendAlphaPxColour(*Dest, SourceCol);
			++Dest;
			++Source;
		}

		DestRow += Buffer->Pitch;
#if SCREEN_Y_DIRECTION == 1
		SourceRow += Map->Width;
#else
		SourceRow -= Bitmap->Width;
#endif
	}
	END_TIMED_BLOCK;
}

internal void
DrawBitmapAligned(image_buffer *Buffer, loaded_bitmap *Bitmap,
				  v2 Pos,
				  i32 AlignX, i32 AlignY)
{
	BEGIN_TIMED_BLOCK;
	Pos.X -= (f32)AlignX;
	Pos.Y -= (f32)AlignY;

	i32 MinX = RoundF32ToI32(Pos.X);
	i32 MinY = RoundF32ToI32(Pos.Y);
	i32 MaxX = RoundF32ToI32(Pos.X + (f32)Bitmap->Width);
	i32 MaxY = RoundF32ToI32(Pos.Y + (f32)Bitmap->Height);

	i32 SourceOffsetX = 0;
	if(MinX < 0)
	{
		SourceOffsetX = -MinX;
		MinX = 0;
	}

	i32 SourceOffsetY = 0;
	if(MinY < 0)
	{
		SourceOffsetY = -MinY;
		MinY = 0;
	}

	if(MaxX > Buffer->Width)
	{
		MaxX = Buffer->Width;
	}

	if(MaxY > Buffer->Height)
	{
		MaxY = Buffer->Height;
	}

	// TODO: SourceRow needs to be changed based on clipping.
	// NOTE: Width*(Height - 1) brings to first pixel of last row.
#define SCREEN_Y_DIRECTION 1
#if SCREEN_Y_DIRECTION == 1
	u32 *SourceRow = Bitmap->Pixels;
#else
	u32 *SourceRow = Bitmap->Pixels + Bitmap->Width*(Bitmap->Height - 1);
#endif
	SourceRow += -(i32)Bitmap->Width*SourceOffsetY + SourceOffsetX;
	u8 *DestRow = ((u8 *)Buffer->Memory +
			MinX*BytesPerPixel +
			MinY*Buffer->Pitch);

	for(int Y = MinY;
			Y < MaxY;
			++Y)
	{
		u32 *Dest = (u32 *)DestRow;
		u32 *Source = SourceRow;
		for(int X = MinX;
				X < MaxX;
				++X)
		{
			f32 SA = (f32)((*Source >> 24) & 0xFF) / 255.0f;
			colour SourceCol = PreMultiplyColour(PixelAsColour(*Source), SA);

			*Dest = BlendAlphaPxColour(*Dest, SourceCol);
			/* f32 SR = (f32)((*Source >> 16) & 0xFF); */
			/* f32 SG = (f32)((*Source >> 8)  & 0xFF); */
			/* f32 SB = (f32)((*Source >> 0)  & 0xFF); */

			/* f32 DR = (f32)((*Dest >> 16)   & 0xFF); */
			/* f32 DG = (f32)((*Dest >> 8)    & 0xFF); */
			/* f32 DB = (f32)((*Dest >> 0)    & 0xFF); */

			/* // TODO: premultiplied alpha (this is not that) */
			/* f32 R = (1.0f - A)*DR + A*SR; */
			/* f32 G = (1.0f - A)*DG + A*SG; */
			/* f32 B = (1.0f - A)*DB + A*SB; */

			/* *Dest = (((u32)(R + 0.5f) << 16) | */
			/* 		 ((u32)(G + 0.5f) << 8) | */
			/* 		 ((u32)(B + 0.5f) << 0)); */

			++Dest;
			++Source;
		}

		DestRow += Buffer->Pitch;
#if SCREEN_Y_DIRECTION == 1
		SourceRow += Bitmap->Width;
#else
		SourceRow -= Bitmap->Width;
#endif
	}
	END_TIMED_BLOCK;
}

internal void
DrawBitmap(image_buffer *Buffer, loaded_bitmap *Bitmap, v2 Pos)
{
	DrawBitmapAligned(Buffer, Bitmap, Pos, 0, 0);
}

internal void
RenderWeirdGradient(image_buffer *Buffer, int BlueOffset, int GreenOffset)
{
	// TODO: check what optimizer does:

	u8 *Row = (u8 *)Buffer->Memory;
    for(int Y = 0;
        Y < Buffer->Height;
        ++Y)
	{
		u32 *Pixel = (u32 *)Row;
		for(int X = 0;
				X < Buffer->Width;
				++X)
		{
			u8 Blue = (u8)(X + BlueOffset);
			u8 Green = (u8)(Y + GreenOffset);

            *Pixel++ = ((Green << 16) | Blue);
		}

		Row += Buffer->Pitch;
	}
}

internal inline b32
IsInScreenBounds(image_buffer *Buffer, v2 PixelPos)
{
	b32 Result = (PixelPos.X > 0 && PixelPos.X <= Buffer->Width  - 1 &&
				  PixelPos.Y > 0 && PixelPos.Y <= Buffer->Height - 1);
	return Result;
}

internal inline u32
GetLinearizedPosition(u32 Width, v2 PixelPos)
{
	u32 Result = RoundF32ToU32(PixelPos.Y) * Width + RoundF32ToU32(PixelPos.X);
	return Result;
}

internal inline void
DEBUGDrawCheckedPixel(image_buffer *Buffer, v2 PixelPos, colour Colour)
{
	u32 LinearizedPosition = GetLinearizedPosition(Buffer->Width, PixelPos);
	u32 *Pixel = (u32 *)Buffer->Memory + LinearizedPosition;
		
	if(IsInScreenBounds(Buffer, PixelPos))
	{
		*Pixel = BlendAlphaPxColour(*Pixel, Colour);
	}
}

internal inline void
DEBUGDrawPixel(image_buffer *Buffer, v2 PixelPos, colour Colour)
{
	BEGIN_TIMED_BLOCK;
	u32 LinearizedPosition = GetLinearizedPosition(Buffer->Width, PixelPos);
	u32 *Pixel = (u32 *)Buffer->Memory + LinearizedPosition;
		
	if(IsInScreenBounds(Buffer, PixelPos))
	{
		*Pixel = PixelColour(Colour);
	}
	END_TIMED_BLOCK;
}

internal inline u32
DEBUGGetPixel(image_buffer *Buffer, v2 PixelPos)
{
	u32 LinearizedPosition = GetLinearizedPosition(Buffer->Width, PixelPos);
	u32 Pixel = 0;
		
	if(IsInScreenBounds(Buffer, PixelPos))
	{
		Pixel = ((u32 *)Buffer->Memory)[LinearizedPosition];
	}

	return Pixel;
}

internal inline colour
GetPixelColour(image_buffer *Buffer, v2 PixelPos)
{
	return PixelAsColour(DEBUGGetPixel(Buffer, PixelPos));
}

internal inline colour
U32ToColour(u32 U32)
{
	colour Colour;

	Colour.R = ((0x00FF0000 & U32) >> 16) / 255.0f;
	Colour.G = ((0x0000FF00 & U32) >>  8) / 255.0f;
	Colour.B = ((0x000000FF & U32) >>  0) / 255.0f;
	Colour.A = ((0xFF000000 & U32) >> 24) / 255.0f;

	return Colour;
}

internal inline line_intermediary
LineIntermediary(v2 Point1, v2 Point2)
{
	line_intermediary Result = {0};

	v2 Diff = V2Sub(Point2, Point1);
	v2 AbsDiff;
	AbsDiff.X = Abs(Diff.X);
	AbsDiff.Y = Abs(Diff.Y);
	do {
		Result.LargestAbsDimension = AbsDiff.X;
		if(AbsDiff.Y > AbsDiff.X)
		{
			Result.LargestAbsDimension = AbsDiff.Y;
		}

		if(Result.LargestAbsDimension != 0)
		{
			Result.SignedDirFraction.X = Diff.X / Result.LargestAbsDimension;
			Result.SignedDirFraction.Y = Diff.Y / Result.LargestAbsDimension;
		}
		// NOTE: needed for decimals?
		AbsDiff.X = Abs(Result.SignedDirFraction.X);
		AbsDiff.Y = Abs(Result.SignedDirFraction.Y);
	} while((Result.SignedDirFraction.X > 1) || (Result.SignedDirFraction.Y > 1));

	return Result;
}

internal void
DEBUGDrawLine(image_buffer *Buffer, v2 Point1, v2 Point2, colour Colour)
{
	BEGIN_TIMED_BLOCK;
	// TODO: SIMD Comparison
	f32 Width = (f32)Buffer->Width;
	f32 Height = (f32)Buffer->Height;

	b32 P1OffLeft   = Point1.X < 0.f;
	b32 P1OffBottom = Point1.Y < 0.f;
	b32 P1OffRight  = Point1.X > Width;
	b32 P1OffTop    = Point1.Y > Height;

	b32 P2OffLeft   = Point2.X < 0.f;
	b32 P2OffBottom = Point2.Y < 0.f;
	b32 P2OffRight  = Point2.X > Width;
	b32 P2OffTop    = Point2.Y > Height;

	b32 OffSameSide = (P1OffLeft   && P2OffLeft)   ||
					  (P1OffBottom && P2OffBottom) ||
					  (P1OffRight  && P2OffRight)  ||
					  (P1OffTop    && P2OffTop);
	b32 BothOff = (P1OffLeft || P1OffBottom || P1OffRight || P1OffTop) &&
				  (P2OffLeft || P2OffBottom || P2OffRight || P2OffTop);

	v2 BottomLeft  = V2(0.f, 0.f);
	v2 BottomRight = V2(Width, 0.f);
	v2 TopLeft     = V2(0.f, Height);
	v2 TopRight    = V2(Width, Height);

	b32 P1Intersected = 0;
	b32 P2Intersected = 0;

	if(!OffSameSide)
	{
		// TODO: Stop line's angle from bunching when hitting screen edge
		// possibly best through rotating 1 horizontal line with given basis.
		if(P1OffLeft  )
		{ P1Intersected = IntersectSegmentsWinding(Point1, Point2, BottomLeft, TopLeft, &Point1); }

		if(P1OffBottom && !P1Intersected)
		{ P1Intersected = IntersectSegmentsWinding(Point1, Point2, BottomLeft, BottomRight, &Point1); }

		if(P1OffRight  && !P1Intersected)
		{ P1Intersected = IntersectSegmentsWinding(Point1, Point2, BottomRight, TopRight, &Point1); }

		if(P1OffTop    && !P1Intersected)
		{ P1Intersected = IntersectSegmentsWinding(Point1, Point2, TopLeft, TopRight, &Point1); }

		if(P2OffLeft  )
		{ P2Intersected = IntersectSegmentsWinding(Point1, Point2, BottomLeft, TopLeft, &Point2); }

		if(P2OffBottom && !P2Intersected)
		{ P2Intersected = IntersectSegmentsWinding(Point1, Point2, BottomLeft, BottomRight, &Point2); }

		if(P2OffRight  && !P2Intersected)
		{ P2Intersected = IntersectSegmentsWinding(Point1, Point2, BottomRight, TopRight, &Point2); }

		if(P2OffTop    && !P2Intersected)
		{ P2Intersected = IntersectSegmentsWinding(Point1, Point2, TopLeft, TopRight, &Point2); }

		if(!BothOff || (P1Intersected && P2Intersected))
		{
			line_intermediary Line = LineIntermediary(Point1, Point2);

			v2 DrawingPixel = Point1;
			// NOTE: Necessarily same round as in DrawPixel
			u32 LoopCounterEnd = RoundF32ToU32(Line.LargestAbsDimension);
			for(u32 LoopCounter = 0;
					LoopCounter < LoopCounterEnd;
					++LoopCounter)
			{
#if 1 // Checked 
				DEBUGDrawCheckedPixel(Buffer, DrawingPixel, Colour);
#else
				u32 *Pixel = (u32 *)Buffer->Memory + GetLinearizedPosition(Buffer->Width, DrawingPixel);
				*Pixel = BlendAlphaPxColour(*Pixel, Colour);
#endif

				DrawingPixel = V2Add(DrawingPixel, Line.SignedDirFraction);
			}
		}
	}
	END_TIMED_BLOCK;
}

internal inline void
DrawFullScreenLine(image_buffer *Buffer, v2 Point1, v2 Dir, colour Colour)
{
	Assert( ! V2Equals(Dir, ZeroV2));

	f32 W = (f32) Buffer->Width;
	f32 H = (f32) Buffer->Height;
	v2 FarCorner = V2(W, H);
	v2 BottomIntersect, LeftIntersect, TopIntersect, RightIntersect;
	f32 tBottom, tLeft, tTop, tRight;
#define INTERSECT_SIDE(side, start, dir) \
	b32 side = IntersectLineSegmentAndT(Point1, Dir, start, dir, &side##Intersect, &t##side)
	INTERSECT_SIDE(Bottom, ZeroV2,    V2(W,  0));
	INTERSECT_SIDE(Left,   ZeroV2,    V2(0,  H));
	INTERSECT_SIDE(Top,    FarCorner, V2(-W, 0));
	INTERSECT_SIDE(Right,  FarCorner, V2(0, -H));
#undef INTERSECT_SIDE
	v2 LineStart = ZeroV2, LineEnd = ZeroV2;
	b32 StartSet = 0, EndSet = 0;
	uint cSides = Bottom + Left + Top + Right;
	if(cSides == 2)
	{
#define SET_SIDE(side) \
		if(side) \
		{ \
			if(!StartSet) \
			{ \
				LineStart = side##Intersect; \
				StartSet = 1; \
			} \
			else if(!EndSet) \
			{ \
				LineEnd = side##Intersect; \
				EndSet = 1; \
			} \
		}
		SET_SIDE(Bottom)
		SET_SIDE(Left)
		SET_SIDE(Top)
		SET_SIDE(Right)

		DEBUGDrawLine(Buffer, LineStart, LineEnd, Colour);
	}
	else if(cSides > 2)
	{
		// TODO: probably exactly in corner?
		Assert(0);
	}
	else
	{ // should always cross at least 2
		Assert(0);
	}
}

internal alpha_map
RasterCircleLine(memory_arena *Arena, f32 Radius)
{
	BEGIN_TIMED_BLOCK;
	// NOTE: Only exact pixel widths (for now)
	u32 URadius = (u32)(Radius + 0.5f);
	Radius = (f32)URadius;
	alpha_map Result;
	// TODO is this +1 ok?
	Result.Width = 2*URadius + 1;
	Result.Height = Result.Width;
	/* Result.Memory = (u8 *) calloc(Result.Width * Result.Height, sizeof(u8)); */
	Result.Memory = GrowSize(Arena, Result.Width * Result.Height);

	v2 Centre = V2(Radius, Radius);
	v2 PixelPos = Centre;
	PixelPos.X += Radius;
	// NOTE: this prevents duplication on cardinal directions
	/* PixelPos.Y -= 1; */

	v2 RelPos = V2Sub(PixelPos, Centre);

	// NOTE: from inverted image?
	/* DEBUGDrawLine(Buffer, V2(Centre.X - Radius, Centre.Y), V2(Centre.X + Radius, Centre.Y), Colour); */
	int LoopCounter = 0;
	u8 *Pixel;
	while(RelPos.X >= -RelPos.Y)
	{
		Assert(Abs(RelPos.X) <= Radius);
		Assert(Abs(RelPos.Y) <= Radius);
		++LoopCounter;

		// TODO: anti-aliased lines etc
#define DRAW_PIXEL(x, y) Pixel = Result.Memory + GetLinearizedPosition(Result.Width, V2Add(Centre, V2(x, y)));\
		*Pixel = 255;

		DRAW_PIXEL( RelPos.X,  RelPos.Y);
		DRAW_PIXEL(-RelPos.X,  RelPos.Y);
		DRAW_PIXEL( RelPos.X, -RelPos.Y);
		DRAW_PIXEL(-RelPos.X, -RelPos.Y);

		DRAW_PIXEL( RelPos.Y,  RelPos.X);
		DRAW_PIXEL(-RelPos.Y,  RelPos.X);
		DRAW_PIXEL( RelPos.Y, -RelPos.X);
		DRAW_PIXEL(-RelPos.Y, -RelPos.X);

		v2 CandidateH = PixelPos, CandidateV = PixelPos;
		if(CandidateH.X == CandidateH.X - 1.f || CandidateH.Y == CandidateH.Y - 1.f)
		{ break; } // out of precision, will infinite loop otherwise
		CandidateH.X -= 1.f;
		CandidateV.Y -= 1.f;
		f32 DistSqH = V2LenSq(V2Sub(CandidateH, Centre));
		f32 DistSqV = V2LenSq(V2Sub(CandidateV, Centre));
		f32 RadiusSq = Radius * Radius;
		if(Abs(DistSqH - RadiusSq) >= Abs(DistSqV - RadiusSq))
			// NOTE: CandidateV is closer to radius length, thus the better choice
		{
			PixelPos = CandidateV;
			// NOTE: done here to avoid duplication when line moves up as well as in
			/* DEBUGDrawLine(Buffer, V2Add(Centre, V2(-RelPos.X,  RelPos.Y)), V2Add(Centre, V2(RelPos.X,  RelPos.Y)), Colour); */
			/* DEBUGDrawLine(Buffer, V2Add(Centre, V2(-RelPos.X, -RelPos.Y)), V2Add(Centre, V2(RelPos.X, -RelPos.Y)), Colour); */

			/* WritePNG("gfx.png", Buffer); */
			PixelPos;//PLACEHOLDER
		}
		else
		{
			PixelPos = CandidateH;

			// NOTE: separate from
			/* DEBUGDrawLine(Buffer, V2Add(Centre, V2(-RelPos.Y,  RelPos.X)), V2Add(Centre, V2(RelPos.Y,  RelPos.X)), Colour); */
			/* DEBUGDrawLine(Buffer, V2Add(Centre, V2(-RelPos.Y, -RelPos.X)), V2Add(Centre, V2(RelPos.Y, -RelPos.X)), Colour); */
		}
		RelPos = V2Sub(PixelPos, Centre);
	}
	int end;
	end;
	/* // NOTE: fills in missed orthogonal points */
	DRAW_PIXEL( Radius,  0);
	DRAW_PIXEL(-Radius,  0);
	DRAW_PIXEL( 0,  Radius);
	DRAW_PIXEL( 0, -Radius);
	/* // NOTE: fills in missed diagonal points */
	DRAW_PIXEL( RelPos.X,  RelPos.Y);
	DRAW_PIXEL(-RelPos.X,  RelPos.Y);
	DRAW_PIXEL( RelPos.X, -RelPos.Y);
	DRAW_PIXEL(-RelPos.X, -RelPos.Y);

	DRAW_PIXEL( RelPos.Y,  RelPos.X);
	DRAW_PIXEL(-RelPos.Y,  RelPos.X);
	DRAW_PIXEL( RelPos.Y, -RelPos.X);
	DRAW_PIXEL(-RelPos.Y, -RelPos.X);
#undef DRAW_PIXEL
	END_TIMED_BLOCK;
	return Result;
}

internal void
DrawCircleLine(memory_arena *Arena, image_buffer *Buffer, v2 Centre, f32 Radius, colour Colour)
{
	BEGIN_TIMED_BLOCK;
	alpha_map AlphaMap = RasterCircleLine(Arena, Radius);
	v2 BottomLeft;
	BottomLeft.X = Centre.X - 0.5f * (f32)AlphaMap.Width;
	BottomLeft.Y = Centre.Y - 0.5f * (f32)AlphaMap.Height;
	DrawAlphaMapAligned(Buffer, &AlphaMap, BottomLeft, 0, 0, Colour);
	END_TIMED_BLOCK;
}

internal void
DrawSuperSlowCircleLine(image_buffer *Buffer, v2 Centre, f32 Radius, colour Colour)
{
	BEGIN_TIMED_BLOCK;
	f32 Stroke = 2.f;
	f32 StrokeRadius = Radius + Stroke/2.f;
	for(f32 Y = Centre.Y-StrokeRadius; Y <= Centre.Y+StrokeRadius; Y+=1.f)
	{
		for(f32 X = Centre.X-StrokeRadius; X <= Centre.X+StrokeRadius; X+=1.f)
		{
			v2 PixelPos = {X, Y};
			f32 CentreDistance = Dist(PixelPos, Centre);
			f32 LineDistance = Stroke/2.f+CentreDistance-Radius;
			/* DebugAdd("LineDistance: %f", LineDistance); */
			f32 A = Stroke-LineDistance;
			if(A < 0.f)  A = 0.f;
			/* if(WithinEpsilon(Distance, Radius, 0.45f)) */ 
			{
				DEBUGDrawCheckedPixel(Buffer, PixelPos, Colour);
			}
		}
	}
	END_TIMED_BLOCK;
}

// TODO: ArcFill. May use WithinVectors && Dist<Radius
internal void
ArcLine(image_buffer *Buffer, v2 Centre, f32 Radius, v2 A, v2 B, colour Colour)
{
	BEGIN_TIMED_BLOCK;
	v2 PixelPos = Centre;
	PixelPos.X += Radius;

	v2 RelPos = V2Sub(PixelPos, Centre);
	while(RelPos.X > 0)
	{
		v2 TestP_000 = RelPos;
		v2 TestP_090 = Perp(TestP_000);
		v2 TestP_180 = Perp(TestP_090);
		v2 TestP_270 = Perp(TestP_180);
		// NOTE: v slightly slower (2%) using another 4 at 45°
		/* v2 TestP_045 = V2Mult(Radius, Norm(V2Add(TestP_000, TestP_090))); */

#define DRAW_WITHIN_BOUNDARIES(TestP, col) if(V2WithinBoundaries(TestP, A, B)) DEBUGDrawCheckedPixel(Buffer, V2Add(Centre, TestP), col)
		DRAW_WITHIN_BOUNDARIES(TestP_000, Colour);
		DRAW_WITHIN_BOUNDARIES(TestP_090, Colour);
		DRAW_WITHIN_BOUNDARIES(TestP_180, Colour);
		DRAW_WITHIN_BOUNDARIES(TestP_270, Colour);
#undef  DRAW_WITHIN_BOUNDARIES

		v2 CandidateH = PixelPos, CandidateV = PixelPos;
		if(CandidateH.X == CandidateH.X - 1.f || CandidateH.Y == CandidateH.Y + 1.f)
		{ break; } // out of precision, will infinite loop otherwise
		CandidateH.X -= 1.f;
		CandidateV.Y += 1.f;
		f32 DistSqH = DistSq(CandidateH, Centre);
		f32 DistSqV = DistSq(CandidateV, Centre);
		f32 RadiusSq = Radius * Radius;
		f32 AbsDiffH = Abs(DistSqH - RadiusSq);
		f32 AbsDiffV = Abs(DistSqV - RadiusSq);

		if(AbsDiffV < AbsDiffH) // CandidateV is closer to radius length -> the better choice
		{ PixelPos = CandidateV; }
		else
		{ PixelPos = CandidateH; }
		RelPos = V2Sub(PixelPos, Centre);
	}
	END_TIMED_BLOCK;
}

internal void
CircleLine(image_buffer *Buffer, v2 Centre, f32 Radius, colour Colour)
{
	BEGIN_TIMED_BLOCK;
	// NOTE: Only exact pixel widths (for now)
	Radius = (f32)(i32)(Radius + 0.5f);
	v2 PixelPos = Centre;
	PixelPos.X += Radius;
	// NOTE: this prevents duplication on cardinal directions
	/* PixelPos.Y -= 1; */

	v2 RelPos = V2Sub(PixelPos, Centre);

	// NOTE: from inverted image?
	/* DEBUGDrawLine(Buffer, V2(Centre.X - Radius, Centre.Y), V2(Centre.X + Radius, Centre.Y), Colour); */
	int LoopCounter = 0;
	while(RelPos.X >= -RelPos.Y)
	{
		// TODO: Try SDF for 1/8th section
		++LoopCounter;
		DEBUGDrawCheckedPixel(Buffer, V2Add(Centre, V2( RelPos.X,  RelPos.Y)), Colour);
		DEBUGDrawCheckedPixel(Buffer, V2Add(Centre, V2(-RelPos.X,  RelPos.Y)), Colour);
		DEBUGDrawCheckedPixel(Buffer, V2Add(Centre, V2( RelPos.X, -RelPos.Y)), Colour);
		DEBUGDrawCheckedPixel(Buffer, V2Add(Centre, V2(-RelPos.X, -RelPos.Y)), Colour);

		DEBUGDrawCheckedPixel(Buffer, V2Add(Centre, V2( RelPos.Y,  RelPos.X)), Colour);
		DEBUGDrawCheckedPixel(Buffer, V2Add(Centre, V2(-RelPos.Y,  RelPos.X)), Colour);
		DEBUGDrawCheckedPixel(Buffer, V2Add(Centre, V2( RelPos.Y, -RelPos.X)), Colour);
		DEBUGDrawCheckedPixel(Buffer, V2Add(Centre, V2(-RelPos.Y, -RelPos.X)), Colour);
		v2 CandidateH = PixelPos, CandidateV = PixelPos;
		CandidateH.X -= 1.f;
		CandidateV.Y -= 1.f;
		f32 DistSqH = V2LenSq(V2Sub(CandidateH, Centre));
		f32 DistSqV = V2LenSq(V2Sub(CandidateV, Centre));
		f32 RadiusSq = Radius * Radius;
		if(Abs(DistSqH - RadiusSq) >= Abs(DistSqV - RadiusSq))
			// NOTE: CandidateV is closer to radius length, thus the better choice
		{
			PixelPos = CandidateV;
			// NOTE: done here to avoid duplication when line moves up as well as in
			/* DEBUGDrawLine(Buffer, V2Add(Centre, V2(-RelPos.X,  RelPos.Y)), V2Add(Centre, V2(RelPos.X,  RelPos.Y)), Colour); */
			/* DEBUGDrawLine(Buffer, V2Add(Centre, V2(-RelPos.X, -RelPos.Y)), V2Add(Centre, V2(RelPos.X, -RelPos.Y)), Colour); */

			/* WritePNG("gfx.png", Buffer); */
			PixelPos;//PLACEHOLDER
		}
		else
		{
			PixelPos = CandidateH;

			// NOTE: separate from
			/* DEBUGDrawLine(Buffer, V2Add(Centre, V2(-RelPos.Y,  RelPos.X)), V2Add(Centre, V2(RelPos.Y,  RelPos.X)), Colour); */
			/* DEBUGDrawLine(Buffer, V2Add(Centre, V2(-RelPos.Y, -RelPos.X)), V2Add(Centre, V2(RelPos.Y, -RelPos.X)), Colour); */
		}
		RelPos = V2Sub(PixelPos, Centre);
	}
	/* // NOTE: fills in missed orthogonal points */
	DEBUGDrawCheckedPixel(Buffer, V2Add(Centre, V2( Radius,  0)), Colour);
	DEBUGDrawCheckedPixel(Buffer, V2Add(Centre, V2(-Radius,  0)), Colour);
	DEBUGDrawCheckedPixel(Buffer, V2Add(Centre, V2( 0, Radius)), Colour);
	DEBUGDrawCheckedPixel(Buffer, V2Add(Centre, V2( 0, -Radius)), Colour);
	/* // NOTE: fills in missed diagonal points */
	DEBUGDrawCheckedPixel(Buffer, V2Add(Centre, V2( RelPos.X,  RelPos.Y)), Colour);
	DEBUGDrawCheckedPixel(Buffer, V2Add(Centre, V2(-RelPos.X,  RelPos.Y)), Colour);
	DEBUGDrawCheckedPixel(Buffer, V2Add(Centre, V2( RelPos.X, -RelPos.Y)), Colour);
	DEBUGDrawCheckedPixel(Buffer, V2Add(Centre, V2(-RelPos.X, -RelPos.Y)), Colour);

	DEBUGDrawCheckedPixel(Buffer, V2Add(Centre, V2( RelPos.Y,  RelPos.X)), Colour);
	DEBUGDrawCheckedPixel(Buffer, V2Add(Centre, V2(-RelPos.Y,  RelPos.X)), Colour);
	DEBUGDrawCheckedPixel(Buffer, V2Add(Centre, V2( RelPos.Y, -RelPos.X)), Colour);
	DEBUGDrawCheckedPixel(Buffer, V2Add(Centre, V2(-RelPos.Y, -RelPos.X)), Colour);
	END_TIMED_BLOCK;
}

internal void
DrawCircleFill(image_buffer *Buffer, v2 Centre, f32 Radius, colour Colour)
{
	BEGIN_TIMED_BLOCK;
	// TODO: SubPixel
	// NOTE: Only exact pixel widths
	Radius = (f32)(i32)(Radius + 0.5f);
	v2 PixelPos = Centre;
	PixelPos.X += Radius;
	// NOTE: this prevents duplication on cardinal directions (seen with partial transparency)
	PixelPos.Y -= 1;

	DEBUGDrawLine(Buffer, V2(Centre.X - Radius, Centre.Y), V2(Centre.X + Radius + 1, Centre.Y), Colour);
	int LoopCounter = 0;
	for(v2 RelPos = V2Sub(PixelPos, Centre); RelPos.X >= -RelPos.Y; RelPos = V2Sub(PixelPos, Centre))
	/* DEBUG_for(v2, RelPos = V2Sub(PixelPos, Centre), RelPos.X >= -RelPos.Y, RelPos = V2Sub(PixelPos, Centre)) */

	/* static v2 RelPos; */
	/* static v2 RelPosInit; */
	/* int RelPos_IsDebug = 1; */
	/* { */
	/* 	static int DebugFor_IsInit; */
	/* 	if(! DEBUG_ATOMIC_EXCHANGE(&DebugFor_IsInit, 1)) { */
	/* 		v2 RelPosTemp = V2Sub(PixelPos, Centre); */
	/* 		RelPos = RelPosInit = RelPosTemp; */
	/* 	} */
	/* 	else */
	/* 	{ */
	/* 		RelPos = V2Sub(PixelPos, Centre); */
	/* 	} */
	/* 	/1* if(RelPos_IsDebug) { goto DEBUG_FOR_CAT1(debug_for_label_, __LINE__); } *1/ */
	/* } */
	/* for(RelPos = RelPosInit; !RelPos_IsDebug && (RelPos.X >= -RelPos.Y); RelPos = V2Sub(PixelPos, Centre)) */
	/* DEBUG_FOR_CAT1(debug_for_label_, __LINE__): */

	{
		++LoopCounter;
#if 1 // Overdraws - only good for Alpha = 1.0
		// TODO: Optimise ordering of line calls to keep branching predictable and minimise internal statements
		// Top L, R
		DEBUGDrawLine(Buffer, V2(Centre.X, Centre.Y + RelPos.X), V2Add(Centre, V2( RelPos.Y,  RelPos.X)), Colour);
		DEBUGDrawLine(Buffer, V2(Centre.X, Centre.Y + RelPos.X), V2Add(Centre, V2(-RelPos.Y,  RelPos.X)), Colour);

		// Middle TL, TR, BL, BR
		DEBUGDrawLine(Buffer, V2(Centre.X, Centre.Y - RelPos.Y), V2Add(Centre, V2(-RelPos.X, -RelPos.Y)), Colour);
		DEBUGDrawLine(Buffer, V2(Centre.X, Centre.Y - RelPos.Y), V2Add(Centre, V2( RelPos.X, -RelPos.Y)), Colour);
		DEBUGDrawLine(Buffer, V2(Centre.X, Centre.Y + RelPos.Y), V2Add(Centre, V2(-RelPos.X,  RelPos.Y)), Colour);
		DEBUGDrawLine(Buffer, V2(Centre.X, Centre.Y + RelPos.Y), V2Add(Centre, V2( RelPos.X,  RelPos.Y)), Colour);

		// Bottom L, R
		DEBUGDrawLine(Buffer, V2(Centre.X, Centre.Y - RelPos.X), V2Add(Centre, V2( RelPos.Y, -RelPos.X)), Colour);
		DEBUGDrawLine(Buffer, V2(Centre.X, Centre.Y - RelPos.X), V2Add(Centre, V2(-RelPos.Y, -RelPos.X)), Colour);
#endif
		v2 CandidateH = PixelPos, CandidateV = PixelPos;
		CandidateH.X -= 1;
		CandidateV.Y -= 1;
		f32 DistSqH = V2LenSq(V2Sub(CandidateH, Centre));
		f32 DistSqV = V2LenSq(V2Sub(CandidateV, Centre));
		f32 RadiusSq = Radius * Radius;
		if(Abs(DistSqH - RadiusSq) >= Abs(DistSqV - RadiusSq))
			// NOTE: CandidateV is closer to radius length, thus the better choice
		{
			// NOTE: done here to avoid duplication when line moves up as well as in
			PixelPos = CandidateV;
			/* DEBUGDrawLine(Buffer, V2Add(Centre, V2(-RelPos.X,  RelPos.Y)), V2Add(Centre, V2(RelPos.X + 1,  RelPos.Y)), Colour); */
			/* DEBUGDrawLine(Buffer, V2Add(Centre, V2(-RelPos.X, -RelPos.Y)), V2Add(Centre, V2(RelPos.X + 1, -RelPos.Y)), Colour); */

			/* WritePNG("gfx.png", Buffer); */
			PixelPos;//PLACEHOLDER
		}
		else
		{
			PixelPos = CandidateH;

			// NOTE: separate from
			/* DEBUGDrawLine(Buffer, V2Add(Centre, V2(-RelPos.Y,  RelPos.X)), V2Add(Centre, V2(RelPos.Y + 1,  RelPos.X)), Colour); */
			/* DEBUGDrawLine(Buffer, V2Add(Centre, V2(-RelPos.Y, -RelPos.X)), V2Add(Centre, V2(RelPos.Y + 1, -RelPos.X)), Colour); */
		}
	}
	/* // NOTE: fills in missed orthogonal points */
	/* DEBUGDrawBlendedPixel(Buffer, V2Add(Centre, V2( Radius,  0)), Colour); */
	/* DEBUGDrawBlendedPixel(Buffer, V2Add(Centre, V2(-Radius,  0)), Colour); */
	/* DEBUGDrawBlendedPixel(Buffer, V2Add(Centre, V2( 0, Radius)), Colour); */
	/* DEBUGDrawBlendedPixel(Buffer, V2Add(Centre, V2( 0, -Radius)), Colour); */
	/* /1* // NOTE: fills in missed diagonal points *1/ */
	/* DEBUGDrawBlendedPixel(Buffer, V2Add(Centre, V2( RelPos.X,  RelPos.Y)), Colour); */
	/* DEBUGDrawBlendedPixel(Buffer, V2Add(Centre, V2(-RelPos.X,  RelPos.Y)), Colour); */
	/* DEBUGDrawBlendedPixel(Buffer, V2Add(Centre, V2( RelPos.X, -RelPos.Y)), Colour); */
	/* DEBUGDrawBlendedPixel(Buffer, V2Add(Centre, V2(-RelPos.X, -RelPos.Y)), Colour); */


	/* DEBUGDrawBlendedPixel(Buffer, V2Add(Centre, V2( RelPos.Y,  RelPos.X)), Colour); */
	/* DEBUGDrawBlendedPixel(Buffer, V2Add(Centre, V2(-RelPos.Y,  RelPos.X)), Colour); */
	/* DEBUGDrawBlendedPixel(Buffer, V2Add(Centre, V2( RelPos.Y, -RelPos.X)), Colour); */
	/* DEBUGDrawBlendedPixel(Buffer, V2Add(Centre, V2(-RelPos.Y, -RelPos.X)), Colour); */
	END_TIMED_BLOCK;
}
#if 0 // NOTE: Draws outline only
	DEBUGDrawBlendedPixel(Buffer, V2Add(Centre, V2( RelPos.X,  RelPos.Y)), Colour);
	DEBUGDrawBlendedPixel(Buffer, V2Add(Centre, V2(-RelPos.X,  RelPos.Y)), Colour);
	DEBUGDrawBlendedPixel(Buffer, V2Add(Centre, V2( RelPos.X, -RelPos.Y)), Colour);
	DEBUGDrawBlendedPixel(Buffer, V2Add(Centre, V2(-RelPos.X, -RelPos.Y)), Colour);

	DEBUGDrawBlendedPixel(Buffer, V2Add(Centre, V2( RelPos.Y,  RelPos.X)), Colour);
	DEBUGDrawBlendedPixel(Buffer, V2Add(Centre, V2(-RelPos.Y,  RelPos.X)), Colour);
	DEBUGDrawBlendedPixel(Buffer, V2Add(Centre, V2( RelPos.Y, -RelPos.X)), Colour);
	DEBUGDrawBlendedPixel(Buffer, V2Add(Centre, V2(-RelPos.Y, -RelPos.X)), Colour);
 // NOTE: Draws radial lines
		DEBUGDrawLine(Buffer, Centre, V2Add(Centre, V2( RelPos.X,  RelPos.Y)), Colour);
		DEBUGDrawLine(Buffer, Centre, V2Add(Centre, V2(-RelPos.X,  RelPos.Y)), Colour);
		DEBUGDrawLine(Buffer, Centre, V2Add(Centre, V2( RelPos.X, -RelPos.Y)), Colour);
		DEBUGDrawLine(Buffer, Centre, V2Add(Centre, V2(-RelPos.X, -RelPos.Y)), Colour);

		DEBUGDrawLine(Buffer, Centre, V2Add(Centre, V2( RelPos.Y,  RelPos.X)), Colour);
		DEBUGDrawLine(Buffer, Centre, V2Add(Centre, V2(-RelPos.Y,  RelPos.X)), Colour);
		DEBUGDrawLine(Buffer, Centre, V2Add(Centre, V2( RelPos.Y, -RelPos.X)), Colour);
		DEBUGDrawLine(Buffer, Centre, V2Add(Centre, V2(-RelPos.Y, -RelPos.X)), Colour);
#endif
// TODO: draw arc

internal void
DrawRectangleLines(image_buffer *Buffer, v2 A, v2 B, colour Colour)
{
	BEGIN_TIMED_BLOCK;
	v2 vMin = A, vMax = B;
	v2 MinXMaxY, MaxXMinY;
#if 1
	if(A.X < B.X) { vMin.X = A.X; vMax.X = B.X; }
	else		  { vMin.X = B.X; vMax.X = A.X; }
	if(A.Y < B.Y) { vMin.Y = A.Y; vMax.Y = B.Y; }
	else		  { vMin.Y = B.Y; vMax.Y = A.Y; }
#endif
	MinXMaxY.X = vMin.X;
	MinXMaxY.Y = vMax.Y;
	MaxXMinY.X = vMax.X;
	MaxXMinY.Y = vMin.Y;

	DEBUGDrawLine(Buffer, vMin, MaxXMinY, Colour);
	DEBUGDrawLine(Buffer, MaxXMinY, vMax, Colour);
	DEBUGDrawLine(Buffer, vMax, MinXMaxY, Colour);
	DEBUGDrawLine(Buffer, MinXMaxY, vMin, Colour);
	END_TIMED_BLOCK;
}

internal void
DrawLinearGradient(image_buffer *Buffer,
					v2 vMin, v2 vMax,
					colour C1, colour C2)
{
	BEGIN_TIMED_BLOCK;
	// TODO: Dithering and/or noise
	colour C;
	for(f32 Y = vMin.Y; Y < vMax.Y; Y += 1.f)
	{
		f32 t = (Y-vMin.Y)/(vMax.Y-vMin.Y);
		C.A = Lerp(C1.A, t, C2.A);
		C.R = Lerp(C1.R, t, C2.R);
		C.G = Lerp(C1.G, t, C2.G);
		C.B = Lerp(C1.B, t, C2.B);
		DEBUGDrawLine(Buffer, V2(vMin.X, Y), V2(vMax.X, Y), C);
	}

	END_TIMED_BLOCK;
}

/// Corners
internal void
DrawRectangleFilled(image_buffer *Buffer,
					v2 vMin, v2 vMax,
					colour Colour)
{
	BEGIN_TIMED_BLOCK;
	i32 MinX = RoundF32ToI32(vMin.X);
	i32 MinY = RoundF32ToI32(vMin.Y);
	i32 MaxX = RoundF32ToI32(vMax.X);
	i32 MaxY = RoundF32ToI32(vMax.Y);

	// NOTE: clips to screen edges
	if(MinX < 0)
	{
		MinX = 0;
	}

	if(MinY < 0)
	{
		MinY = 0;
	}

	// TODO: -1?
	if(MaxX > Buffer->Width)
	{
		MaxX = Buffer->Width;
	}

	if(MaxY > Buffer->Height)
	{
		MaxY = Buffer->Height;
	}

	__m128 ColourWide = _mm_set_ps(Colour.A, Colour.R, Colour.G, Colour.B);
	__m128 Col255_4x = _mm_set_ps1(255.0f);
	__m128 iSrcA_4x = _mm_set_ps1(1-Colour.A);

	// BIT PATTERN: Ox AA RR GG BB
	/* u32 BitColour = PixelColour(Colour); */

	// NOTE: Preadvance to top-left corner
	u8 *Row = ((u8 *)Buffer->Memory +
			MinX*BytesPerPixel +
			MinY*Buffer->Pitch);
	for(int Y = MinY;
			/* Y < MinY + 1; */
			Y < MaxY;
			++Y)
	{
		u32 *Pixel = (u32 *)Row;
		for(int X = MinX;
				/* X < MinX + 1; */
				X < MaxX;
				++X)
		{
			__m128 U8Wide = _mm_set_ps((f32)(u8)(*Pixel >> 24), (f32)(u8)(*Pixel >> 16), (f32)(u8)(*Pixel >>  8), (f32)(u8)(*Pixel >>  0));
			__m128 PixelWide = _mm_add_ps(_mm_mul_ps(ColourWide, Col255_4x),
										  _mm_mul_ps(U8Wide, iSrcA_4x));

			/* u32 NewColour = RoundF32ToU32(((f32 *)&PixelWide)[3]) << 24 | */
			/* 				RoundF32ToU32(((f32 *)&PixelWide)[2]) << 16 | */
			/* 				RoundF32ToU32(((f32 *)&PixelWide)[1]) <<  8 | */
			/* 				RoundF32ToU32(((f32 *)&PixelWide)[0]) <<  0 ; */
			// TODO: Does rounding ever have to be better than this? math.h's roundf followed by cast gives ~20X slowdown
			// NOTE: SIMD ~1.5X faster than normal functions
			u32 NewColour = (u32)(((f32 *)&PixelWide)[3]) << 24 |
							(u32)(((f32 *)&PixelWide)[2]) << 16 |
							(u32)(((f32 *)&PixelWide)[1]) <<  8 |
							(u32)(((f32 *)&PixelWide)[0]) <<  0 ;
			*Pixel++ = NewColour;
		}

		Row += Buffer->Pitch;
	}
	END_TIMED_BLOCK;
}

internal inline void
DrawRectDimsFilled(image_buffer *Buffer,
					v2 vMin, f32 Width, f32 Height,
					colour Colour)
{
	BEGIN_TIMED_BLOCK;
	v2 vMax = vMin;
	vMax.X += Width;
	vMax.Y += Height;
	DrawRectangleFilled(Buffer, vMin, vMax, Colour);
	END_TIMED_BLOCK;
}

internal void
OLDDrawFilledRectangle(image_buffer *Buffer,
					v2 vMin, v2 vMax,
					colour Colour)
{
	BEGIN_TIMED_BLOCK;
	i32 MinX = RoundF32ToI32(vMin.X);
	i32 MinY = RoundF32ToI32(vMin.Y);
	i32 MaxX = RoundF32ToI32(vMax.X);
	i32 MaxY = RoundF32ToI32(vMax.Y);

	if(MinX < 0)
	{
		MinX = 0;
	}

	if(MinY < 0)
	{
		MinY = 0;
	}

	// TODO: -1?
	if(MaxX > Buffer->Width)
	{
		MaxX = Buffer->Width;
	}

	if(MaxY > Buffer->Height)
	{
		MaxY = Buffer->Height;
	}

	// BIT PATTERN: Ox AA RR GG BB
	/* u32 BitColour = PixelColour(Colour); */

	// NOTE: Preadvance to top-left corner
	u8 *Row = ((u8 *)Buffer->Memory +
			MinX*BytesPerPixel +
			MinY*Buffer->Pitch);
	if(Colour.A == 1.0f)
	{
		// NOTE: We don't need to check what we're on top of, so we can just stomp it
		u32 PixelCol = PixelColour(Colour);
		for(int Y = MinY;
				/* Y < MinY + 1; */
				Y < MaxY;
				++Y)
		{
			u32 *Pixel = (u32 *)Row;
			for(int X = MinX;
					/* X < MinX + 1; */
					X < MaxX;
					++X)
			{
				*Pixel++ = PixelCol;
			}

			Row += Buffer->Pitch;
		}
	}
	else
	{
		for(int Y = MinY;
				/* Y < MinY + 1; */
				Y < MaxY;
				++Y)
		{
			u32 *Pixel = (u32 *)Row;
			for(int X = MinX;
					/* X < MinX + 1; */
					X < MaxX;
					++X)
			{
				*Pixel++ = BlendAlphaPxColour(*Pixel, Colour);
			}

			Row += Buffer->Pitch;
		}
	}
	END_TIMED_BLOCK;
}

internal void
DrawX(image_buffer *Buffer, v2 CentrePos, f32 Radius, colour Colour)
{
	BEGIN_TIMED_BLOCK;
	v2 L1_1, L1_2, L2_1, L2_2;

	L1_1.X = CentrePos.X - Radius;
	L1_1.Y = CentrePos.Y - Radius;
	L1_2.X = CentrePos.X + Radius;
	L1_2.Y = CentrePos.Y + Radius;

	L2_1.X = CentrePos.X + Radius;
	L2_1.Y = CentrePos.Y - Radius;
	L2_2.X = CentrePos.X - Radius;
	L2_2.Y = CentrePos.Y + Radius;

	DEBUGDrawLine(Buffer, L1_1, L1_2, Colour);
	DEBUGDrawLine(Buffer, L2_1, L2_2, Colour);
	END_TIMED_BLOCK;
}

internal void
DrawCrosshair(image_buffer *Buffer, v2 CentrePos, f32 Radius, colour Colour)
{
	BEGIN_TIMED_BLOCK;
	v2 L1_1, L1_2, L2_1, L2_2;

	L1_1.X = CentrePos.X;
	L1_1.Y = CentrePos.Y - Radius;
	L1_2.X = CentrePos.X;
	L1_2.Y = CentrePos.Y + Radius;

	L2_1.X = CentrePos.X - Radius;
	L2_1.Y = CentrePos.Y;
	L2_2.X = CentrePos.X + Radius;
	L2_2.Y = CentrePos.Y;

	DEBUGDrawLine(Buffer, L1_1, V2(CentrePos.X, CentrePos.Y - 1), Colour);
	DEBUGDrawLine(Buffer, L1_2, V2(CentrePos.X, CentrePos.Y + 1), Colour);
	DEBUGDrawLine(Buffer, L2_1, V2(CentrePos.X - 1, CentrePos.Y), Colour);
	DEBUGDrawLine(Buffer, L2_2, V2(CentrePos.X + 1, CentrePos.Y), Colour);
	END_TIMED_BLOCK;
}

internal inline aabb_collision_sides
RectInRectEdgeCheck(v2 InnerCentre, f32 InnerXRadius, f32 InnerYRadius, v2 OuterSize)
{
	BEGIN_TIMED_BLOCK;
	aabb_collision_sides Result = {0};
	if(InnerCentre.X < InnerXRadius)
	{
		Result.L = 1;
	}
	// TODO: -1?
	if(InnerCentre.X > OuterSize.X - InnerXRadius)
	{
		Result.R = 1;
	}
	if(InnerCentre.Y < InnerYRadius)
	{
		Result.B = 1;
	}
	if(InnerCentre.Y > OuterSize.Y - InnerYRadius)
	{
		Result.T = 1;
	}
	return Result;
	END_TIMED_BLOCK;
}

internal inline b32
RectInRectCollisionCheck(v2 InnerCentre, f32 InnerXRadius, f32 InnerYRadius, v2 OuterSize)
{
	BEGIN_TIMED_BLOCK;
	aabb_collision_sides Collision = RectInRectEdgeCheck(InnerCentre, InnerXRadius, InnerYRadius, OuterSize);
	b32 Result = (Collision.R || Collision.T || Collision.L || Collision.B);
	return Result;
	END_TIMED_BLOCK;
}

#define GFX_H
#endif
