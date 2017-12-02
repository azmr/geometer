#if !defined(WIN32_GFX_H)
typedef struct win32_image_buffer
{
    // NOTE: Pixels are always 32-bits wide, Memory Order BB GG RR AA
    void *Memory;
    int Width;
    int Height;
    int Pitch;
    BITMAPINFO Info;
} win32_image_buffer;

typedef struct win32_window
{
	HWND Handle;
	f32 TargetSecondsPerFrame;

	int Width;
	int Height;
	int OffsetX;
	int OffsetY;
} win32_window;
win32_window ZeroWin32Window = {0};

/// Gets DC for current monitor (GetDC gives the primary monitor)
internal inline HDC
Win32WindowDC(HWND WindowHandle)
{
	HMONITOR MonitorHandle = MonitorFromWindow(WindowHandle, MONITOR_DEFAULTTONEAREST);
	MONITORINFOEX MonitorInfo = {0};
	MonitorInfo.cbSize = sizeof(MONITORINFOEX);
	GetMonitorInfo(MonitorHandle, (LPMONITORINFO)&MonitorInfo);
	char *DeviceName = MonitorInfo.szDevice;
	HDC DeviceContext = CreateDC("DISPLAY", DeviceName, 0, 0);
	return DeviceContext;
}

internal inline void
Win32PrimaryScreenResolution(HWND Handle, int *Width, int *Height)
{
	HDC DeviceContext = GetDC(Handle);
	*Width = GetDeviceCaps(DeviceContext, HORZRES);
	*Height = GetDeviceCaps(DeviceContext, VERTRES);
	ReleaseDC(Handle, DeviceContext);
}

/// Gets resolution of window's current monitor
internal inline void
Win32ScreenResolution(HWND WindowHandle, int *Width, int *Height)
{
	HDC DeviceContext = Win32WindowDC(WindowHandle);
	*Width = GetDeviceCaps(DeviceContext, HORZRES);
	*Height = GetDeviceCaps(DeviceContext, VERTRES);
	ReleaseDC(WindowHandle, DeviceContext);
}

internal void
Win32SetIcon(HWND Window, u8 *BigImage, u32 BigSize, u8 *SmallImage, u32 SmallSize)
{
	HICON BigIcon = CreateIconFromResourceEx(BigImage, BigSize, TRUE, 0x00030000, 0, 0, LR_DEFAULTSIZE | LR_SHARED);
	HICON SmallIcon = CreateIconFromResourceEx(SmallImage, SmallSize, TRUE, 0x00030000, 0, 0, LR_DEFAULTSIZE | LR_SHARED);
	SendMessage(Window, WM_SETICON, ICON_BIG, (LPARAM)BigIcon);
	SendMessage(Window, WM_SETICON, ICON_SMALL, (LPARAM)SmallIcon);
}

internal void
Win32ResizeDIBSection(win32_image_buffer *Buffer, int Width, int Height)
{
	BEGIN_TIMED_BLOCK;
	// TODO: Bulletproof
	// maybe free after, then free first if that fails.

	if(Buffer->Memory)
	{
		VirtualFree(Buffer->Memory, 0, MEM_RELEASE);
		// Protects from use after free in stale memory page:
	}

	Buffer->Width = Width;
	Buffer->Height = Height;

	// NOTE: negative biHeight tells window to treat top as 0, not bottom
	Buffer->Info.bmiHeader.biSize = sizeof(Buffer->Info.bmiHeader);
	Buffer->Info.bmiHeader.biWidth = Width;
	Buffer->Info.bmiHeader.biHeight = Height;
	Buffer->Info.bmiHeader.biPlanes = 1;
	Buffer->Info.bmiHeader.biBitCount = 32;
	Buffer->Info.bmiHeader.biCompression = BI_RGB;

	int BitmapMemorySize = Width*Height*BytesPerPixel;
	Buffer->Memory = VirtualAlloc(0, BitmapMemorySize, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
	Buffer->Pitch = Width*BytesPerPixel;

	// TODO: Probably clear to black
	END_TIMED_BLOCK;
}


/// ensure window info is up to date before calling
internal b32
Win32DisplayBufferInWindow(win32_image_buffer *Buffer, win32_window Window)
{
	BEGIN_TIMED_BLOCK;
	HDC DeviceContext = GetDC(Window.Handle);
	b32 Fullscreen = 0;

#if 0
	if((Window.Width >= Buffer->Width*2) &&
	   (Window.Height >= Buffer->Height*2))
	{
		StretchDIBits(DeviceContext,
				0, 0, 2*Buffer->Width, 2*Buffer->Height,	// Source
				0, 0, Buffer->Width, Buffer->Height,	// Destination
				Buffer->Memory,
				&Buffer->Info,
				DIB_RGB_COLORS, SRCCOPY);
		
		Fullscreen = 1;
	}
	else
#endif
	{
		// TODO: this is still blitting black?
#define SCREEN_BG_COLOR WHITENESS
		PatBlt(DeviceContext, 0, 0, Window.Width, Window.OffsetY, SCREEN_BG_COLOR);
		PatBlt(DeviceContext, 0, Window.OffsetY + Buffer->Height, Window.Width, Window.Height, SCREEN_BG_COLOR);
		PatBlt(DeviceContext, 0, 0, Window.OffsetX, Window.Height, SCREEN_BG_COLOR);
		PatBlt(DeviceContext, Window.OffsetX + Buffer->Width, 0, Window.Width, Window.Height, SCREEN_BG_COLOR);

		// NOTE: For prototyping purposes, we're going to always blit
		// 1-to-1 pixels to make sure we don't introduce artifacts with
		// stretching while we are learning to code the renderer!
		StretchDIBits(DeviceContext,
				Window.OffsetX, Window.OffsetY, Buffer->Width, Buffer->Height,	// Source
				0, 0, Buffer->Width, Buffer->Height,	// Destination
				Buffer->Memory,
				&Buffer->Info,
				DIB_RGB_COLORS, SRCCOPY);
	}
	ReleaseDC(Window.Handle, DeviceContext);
	END_TIMED_BLOCK;
	return Fullscreen;
}

internal inline void
Win32GetWindowDimensionAndOffset(win32_window *Window, int BufferWidth, int BufferHeight)
{
	BEGIN_TIMED_BLOCK;
	RECT ClientRect;
	GetClientRect(Window->Handle, &ClientRect);
	Window->Width = ClientRect.right - ClientRect.left;
	Window->Height = ClientRect.bottom - ClientRect.top;
	Window->OffsetX = (Window->Width - BufferWidth) / 2;
	Window->OffsetY = (Window->Height - BufferHeight) / 2;
	END_TIMED_BLOCK;
}

internal void
Win32ToggleFullscreen(HWND Window)
{
	// NOTE: This follows Raymond Chen's prescription for fullscreen toggling:
	// https://blogs.msdn.microsoft.com/oldnewthing/20100412-00/?p=14353

	DWORD Style = GetWindowLong(Window, GWL_STYLE);
	if (Style & WS_OVERLAPPEDWINDOW) {
		MONITORINFO MonitorInfo = {sizeof(MonitorInfo)};
		if (GetWindowPlacement(Window, &GlobalWindowPosition) &&
			GetMonitorInfo(MonitorFromWindow(Window, MONITOR_DEFAULTTOPRIMARY), &MonitorInfo))
		{
			SetWindowLong(Window, GWL_STYLE, Style & ~WS_OVERLAPPEDWINDOW);
			SetWindowPos(Window, HWND_TOP,
						 MonitorInfo.rcMonitor.left, MonitorInfo.rcMonitor.top,
						 MonitorInfo.rcMonitor.right - MonitorInfo.rcMonitor.left,
						 MonitorInfo.rcMonitor.bottom - MonitorInfo.rcMonitor.top,
						 SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
		}
	}
	else
	{
		SetWindowLong(Window, GWL_STYLE, Style | WS_OVERLAPPEDWINDOW);
		SetWindowPlacement(Window, &GlobalWindowPosition);
		SetWindowPos(Window, NULL, 0, 0, 0, 0,
					 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
					 SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
	}
}

#if !defined(USING_INPUT)
internal void
Win32ProcessPendingMessages()
{
	BEGIN_TIMED_BLOCK;
	// b32 Success
	MSG Msg;
	while(PeekMessage(&Msg, 0, 0, 0, PM_REMOVE))
	{
		switch(Msg.message)
		{
            case WM_QUIT:
            {
            	// TODO: return 0 for caller to deal with
				OutputDebugStringA("Quit\n");
                GlobalRunning = 0;
            } break;

			default:
			{
				TranslateMessage(&Msg);
				DispatchMessageA(&Msg);
			} break;
		}
	}
	END_TIMED_BLOCK;
}
#endif

// TODO: Both of these???
internal LRESULT CALLBACK
Win32MainWindowCallback(HWND   Window,
						UINT   Msg,
						WPARAM WParam,
						LPARAM LParam)
{
	LRESULT Result = 0;

	switch(Msg)
	{
		case WM_CLOSE:
		case WM_DESTROY:
		{
			OutputDebugStringA("Close/Destroy\n");
			GlobalRunning = 0;
		} break;
		case WM_SIZE:
		 // Zilarrezko: Often times I've seen that when my window was resized that it didn't seem to update the window,
		 // so everytime WM_SIZE is called I do an UpdateAndRender, then I Blit the back buffer, and I make sure to call ValidateRect.
		{ } break;

		default:
		{
			// TODO: "It is more efficient to perform any move or size change processing
			// during the WM_WINDOWPOSCHANGED message without calling DefWindowProc" [MSDN]
			// NOTE: 88 byte leak here
			Result = DefWindowProcA(Window, Msg, WParam, LParam);
		} break;
	}

	return Result;
}

// TODO: not catastrophic if fails?
internal void
Win32SetWindowSize(HWND Handle, int W, int H, u32 Style)
{
	RECT Dimensions;
	Dimensions.left = 0;
	Dimensions.top = 0;
	Dimensions.right = W;
	Dimensions.bottom = H;
	AdjustWindowRect(&Dimensions, Style, 0);
	SetWindowPos(Handle, 0, 0, 0, Dimensions.right - Dimensions.left, Dimensions.bottom - Dimensions.top, SWP_NOMOVE);
}

// TODO: WindowClass might be useful at later date?
internal b32
Win32BasicWindow(HINSTANCE Instance, win32_window *Window, int W, int H,
		char *WindowName, char *Icon, char *IconSm)
{
	*Window = ZeroWin32Window;
	WNDCLASSEX WindowClass = {0};
	WindowClass.cbSize = sizeof(WNDCLASSEX);
    WindowClass.style = CS_HREDRAW|CS_VREDRAW;
	WindowClass.lpfnWndProc = Win32MainWindowCallback;
	WindowClass.hInstance = Instance;
	WindowClass.hCursor = LoadCursor(0, IDC_ARROW);
	WindowClass.hIcon   = LoadIcon(Instance, Icon);
	WindowClass.hIconSm = LoadIcon(Instance, IconSm);
	WindowClass.lpszClassName = "BasicWindowClass";
	if(!RegisterClassEx(&WindowClass))
	{ return 0; }
	u32 WindowStyle = WS_OVERLAPPEDWINDOW | WS_VISIBLE;
	Window->Handle =
		//CreateWindowExA(
		CreateWindowEx(
			0, //WS_EX_TOPMOST|WS_EX_LAYERED,
			WindowClass.lpszClassName,
			WindowName,
			WindowStyle,
			CW_USEDEFAULT,
			CW_USEDEFAULT,
			CW_USEDEFAULT,
			CW_USEDEFAULT,
			0,
			0,
			Instance,
			0);

	// TODO: How do we reliably query this on Windows?
	HDC RefreshDC = GetDC(Window->Handle);
	// NOTE: default 60Hz refreshrate if test fails
	int MonitorRefreshHz = 60;
	int Win32RefreshRate = GetDeviceCaps(RefreshDC, VREFRESH);
	ReleaseDC(Window->Handle, RefreshDC);

	if(Win32RefreshRate > 1)
	{
		MonitorRefreshHz = Win32RefreshRate;
	}
#if 0 // 30FPS
	f32 UpdateHz = (MonitorRefreshHz / 2.0f);
#else // 60FPS
	f32 UpdateHz = (f32)MonitorRefreshHz;
#endif
	Window->TargetSecondsPerFrame = (1.0f / (f32)UpdateHz);

	Win32SetWindowSize(Window->Handle, W, H, WindowStyle);

	return 1;
}

#define WIN32_GFX_H
#endif
