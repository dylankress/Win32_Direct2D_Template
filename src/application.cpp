// application.cpp - Direct2D application entry point
//
// UNITY BUILD: Includes ui.cpp and app_ui.cpp as single translation unit for fast compilation
//
// ARCHITECTURE:
// - Continuous rendering loop at 120 FPS with precise frame pacing
// - Full mouse and keyboard input forwarded to UI system
// - Direct2D for hardware-accelerated 2D rendering
// - DirectWrite for text rendering with UTF-8 support
// - Text format caching for performance (16 font sizes max)
//
// FRAME LOOP:
// 1. Process Windows messages (non-blocking)
// 2. Render UI (build → layout → interaction → emit → draw)
// 3. Wait for target frame time (busy-wait for precision)
//
#include <windows.h>
#include <windowsx.h>
#include <d2d1.h>
#include <dwrite.h>
#include <stdio.h>
#include <stdint.h>
#include "profiling.h"
#include "ui.cpp"
#include "app_ui.h"

bool g_is_running;
bool g_is_resizing = false;

// Cursor management
HCURSOR g_cursor_arrow = NULL;
HCURSOR g_cursor_size_we = NULL;  // Horizontal resize ↔
HCURSOR g_cursor_size_ns = NULL;  // Vertical resize ↕
HCURSOR g_current_cursor = NULL;

ID2D1Factory *p_d2d_factory;
ID2D1HwndRenderTarget *p_render_target;
ID2D1SolidColorBrush *p_brush;

// DirectWrite resources
IDWriteFactory *p_dwrite_factory;
IDWriteTextFormat *p_text_format_default;
IDWriteTextFormat *p_text_format_monospace;  // 14pt Consolas/Courier New

// Text format cache (for performance)
struct Text_Format_Cache {
	IDWriteTextFormat *formats[APP_MAX_TEXT_FORMATS];
	int sizes[APP_MAX_TEXT_FORMATS];
	int styles[APP_MAX_TEXT_FORMATS];  // 0=Segoe UI, 1=monospace
	int count;
};
Text_Format_Cache g_text_format_cache;

// Global UI context (for window message handler access)
UI_Context g_ui_context;

// Frame timing system
struct Frame_Timer {
	LARGE_INTEGER frequency;
	LARGE_INTEGER frame_start;
	LARGE_INTEGER frame_end;
	double target_frame_time_ms;
	double actual_frame_time_ms;
	int target_fps;
	int actual_fps;
	double fps_update_timer;
	int frame_count_for_fps;
};

Frame_Timer g_frame_timer;


// .............................................................................................
IDWriteTextFormat*
Get_Text_Format(int font_size, int font_style)
{
	// Check cache (must match BOTH size and style)
	for (int i = 0; i < g_text_format_cache.count; i++) {
		if (g_text_format_cache.sizes[i] == font_size && 
		    g_text_format_cache.styles[i] == font_style) {
			return g_text_format_cache.formats[i];
		}
	}
	
	// Create new format if not cached
	if (g_text_format_cache.count < APP_MAX_TEXT_FORMATS) {
		IDWriteTextFormat *fmt;
		
		// Choose font family based on style
		const wchar_t *font_family = L"Segoe UI";
		if (font_style == 1) {
			// Try Consolas first (common on Windows), fall back to Courier New
			font_family = L"Consolas";
		}
		
		HRESULT hr = p_dwrite_factory->CreateTextFormat(
			font_family,
			NULL,
			DWRITE_FONT_WEIGHT_NORMAL,
			DWRITE_FONT_STYLE_NORMAL,
			DWRITE_FONT_STRETCH_NORMAL,
			(float)font_size,
			L"en-us",
			&fmt
		);
		
		// If Consolas failed, try Courier New
		if (FAILED(hr) && font_style == 1) {
			hr = p_dwrite_factory->CreateTextFormat(
				L"Courier New",
				NULL,
				DWRITE_FONT_WEIGHT_NORMAL,
				DWRITE_FONT_STYLE_NORMAL,
				DWRITE_FONT_STRETCH_NORMAL,
				(float)font_size,
				L"en-us",
				&fmt
			);
		}
		
		if (SUCCEEDED(hr)) {
			int idx = g_text_format_cache.count++;
			g_text_format_cache.formats[idx] = fmt;
			g_text_format_cache.sizes[idx] = font_size;
			g_text_format_cache.styles[idx] = font_style;
			return fmt;
		}
	}
	
	return p_text_format_default;
}


// .............................................................................................
UI_RectI
App_Measure_Text(const char *text, int font_size)
{
	if (!text || !p_dwrite_factory) {
		UI_RectI empty = {0, 0, 0, 0};
		return empty;
	}
	
	// Convert UTF-8 to UTF-16
	wchar_t wtext[MAX_UI_TEXT_LENGTH];
	MultiByteToWideChar(CP_UTF8, 0, text, -1, wtext, MAX_UI_TEXT_LENGTH);
	
	// Get text format (default proportional font)
	IDWriteTextFormat *fmt = Get_Text_Format(font_size, 0);
	
	// Create text layout for measurement
	IDWriteTextLayout *layout;
	HRESULT hr = p_dwrite_factory->CreateTextLayout(
		wtext,
		(UINT32)wcslen(wtext),
		fmt,
		10000.0f,  // Max width (large number for single-line)
		10000.0f,  // Max height
		&layout
	);
	
	if (FAILED(hr)) {
		UI_RectI fallback = {0, 0, 0, 20};
		return fallback;
	}
	
	// Get metrics
	DWRITE_TEXT_METRICS metrics;
	layout->GetMetrics(&metrics);
	layout->Release();
	
	UI_RectI result;
	result.x = 0;
	result.y = 0;
	result.w = (int)(metrics.width + 0.5f);
	result.h = (int)(metrics.height + 0.5f);
	return result;
}


// .............................................................................................
UI_RectI
App_Measure_Text_Monospace(const char *text, int font_size)
{
	if (!text || !p_dwrite_factory) {
		UI_RectI fallback;
		memset(&fallback, 0, sizeof(UI_RectI));
		fallback.w = 100;
		fallback.h = 20;
		return fallback;
	}
	
	// Convert UTF-8 to UTF-16
	wchar_t wtext[MAX_UI_TEXT_LENGTH];
	MultiByteToWideChar(CP_UTF8, 0, text, -1, wtext, MAX_UI_TEXT_LENGTH);
	
	// Get monospace text format (style=1)
	IDWriteTextFormat *fmt = Get_Text_Format(font_size, 1);
	if (!fmt) {
		UI_RectI fallback;
		memset(&fallback, 0, sizeof(UI_RectI));
		fallback.w = 100;
		fallback.h = 20;
		return fallback;
	}
	
	// Create text layout for measurement
	IDWriteTextLayout *layout;
	HRESULT hr = p_dwrite_factory->CreateTextLayout(
		wtext,
		(UINT32)wcslen(wtext),
		fmt,
		10000.0f,  // Max width (large number for single-line)
		10000.0f,  // Max height
		&layout
	);
	
	if (FAILED(hr)) {
		UI_RectI fallback;
		memset(&fallback, 0, sizeof(UI_RectI));
		fallback.w = 100;
		fallback.h = 20;
		return fallback;
	}
	
	// Get metrics
	DWRITE_TEXT_METRICS metrics;
	layout->GetMetrics(&metrics);
	layout->Release();
	
	UI_RectI result;
	result.x = 0;
	result.y = 0;
	result.w = (int)(metrics.width + 0.5f);
	result.h = (int)(metrics.height + 0.5f);
	return result;
}


// .............................................................................................
void
Render_UI(UI_Render_List *render_list)
{
	PROFILE_ZONE;  // Auto-named "Render_UI"
	
	for (int i = 0; i < render_list->rect_count; i++)
	{
		const UI_Rectangle *src = &render_list->rectangles[i];
		
		D2D1_RECT_F rect = D2D1::RectF(
			(float)src->left,
			(float)src->top,
			(float)src->right,
			(float)src->bottom
		);
		
		uint32_t c = src->color;
		
		float a = ((c >> 24) & 0xFF) / 255.0f;
		float r = ((c >> 16) & 0xFF) / 255.0f;
		float g = ((c >>  8) & 0xFF) / 255.0f;
		float b = ((c >>  0) & 0xFF) / 255.0f;
		
		// If your color is 0xAARRGGBB, use the byte-based ColorF:
		p_brush->SetColor(D2D1::ColorF(r, g, b, a));
		p_render_target->FillRectangle(rect, p_brush);
	}
}


// .............................................................................................
void
Render_UI_Text(UI_Render_List *render_list)
{
	PROFILE_ZONE;  // Auto-named "Render_UI_Text"
	
	for (int i = 0; i < render_list->text_count; i++)
	{
		const UI_Text *src = &render_list->texts[i];
		
		// Convert UTF-8 to UTF-16
		wchar_t wtext[MAX_UI_TEXT_LENGTH];
		MultiByteToWideChar(CP_UTF8, 0, src->text, -1, wtext, MAX_UI_TEXT_LENGTH);
		
		// Set color
		uint32_t c = src->color;
		float a = ((c >> 24) & 0xFF) / 255.0f;
		float r = ((c >> 16) & 0xFF) / 255.0f;
		float g = ((c >>  8) & 0xFF) / 255.0f;
		float b = ((c >>  0) & 0xFF) / 255.0f;
		p_brush->SetColor(D2D1::ColorF(r, g, b, a));
		
		// Get text format (use cached or default based on font style)
		IDWriteTextFormat *fmt;
		if (src->font_size > 0) {
			fmt = Get_Text_Format(src->font_size, src->font_style);
		} else {
			// Use default format based on style
			fmt = (src->font_style == 1) ? p_text_format_monospace : p_text_format_default;
		}
		
		// Set alignment
		DWRITE_TEXT_ALIGNMENT h_align;
		if (src->align_h == UI_ALIGN_CENTER) h_align = DWRITE_TEXT_ALIGNMENT_CENTER;
		else if (src->align_h == UI_ALIGN_END) h_align = DWRITE_TEXT_ALIGNMENT_TRAILING;
		else h_align = DWRITE_TEXT_ALIGNMENT_LEADING;
		
		DWRITE_PARAGRAPH_ALIGNMENT v_align;
		if (src->align_v == UI_ALIGN_CENTER) v_align = DWRITE_PARAGRAPH_ALIGNMENT_CENTER;
		else if (src->align_v == UI_ALIGN_END) v_align = DWRITE_PARAGRAPH_ALIGNMENT_FAR;
		else v_align = DWRITE_PARAGRAPH_ALIGNMENT_NEAR;
		
		fmt->SetTextAlignment(h_align);
		fmt->SetParagraphAlignment(v_align);
		
		// Draw text
		D2D1_RECT_F rect = D2D1::RectF(
			(float)src->x, 
			(float)src->y,
			(float)(src->x + src->w), 
			(float)(src->y + src->h)
		);
		
		p_render_target->DrawText(
			wtext, 
			(UINT32)wcslen(wtext),
			fmt,
			rect,
			p_brush
		);
	}
}


// .............................................................................................
void
UI_Set_Cursor(HCURSOR cursor)
{
	if (cursor != g_current_cursor) {
		SetCursor(cursor);
		g_current_cursor = cursor;
	}
}


// .............................................................................................
void
Wait_For_Target_Frame_Time()
{
	PROFILE_ZONE;  // Auto-named "Wait_For_Target_Frame_Time"
	
	// Calculate elapsed time this frame
	LARGE_INTEGER current_time;
	QueryPerformanceCounter(&current_time);
	
	double elapsed_ms = (double)(current_time.QuadPart - g_frame_timer.frame_start.QuadPart) 
	                    * 1000.0 / (double)g_frame_timer.frequency.QuadPart;
	
	// Busy-wait until we reach target frame time
	while (elapsed_ms < g_frame_timer.target_frame_time_ms) {
		QueryPerformanceCounter(&current_time);
		elapsed_ms = (double)(current_time.QuadPart - g_frame_timer.frame_start.QuadPart) 
		             * 1000.0 / (double)g_frame_timer.frequency.QuadPart;
		
		// Yield CPU if we're more than 1ms away
		if (elapsed_ms < g_frame_timer.target_frame_time_ms - 1.0) {
			Sleep(0);
		}
	}
	
	// Update frame timing
	g_frame_timer.frame_end = current_time;
	g_frame_timer.actual_frame_time_ms = elapsed_ms;
	
	// Update FPS counter (once per second)
	g_frame_timer.frame_count_for_fps++;
	g_frame_timer.fps_update_timer += elapsed_ms;
	
	if (g_frame_timer.fps_update_timer >= 1000.0) {
		g_frame_timer.actual_fps = g_frame_timer.frame_count_for_fps;
		g_frame_timer.frame_count_for_fps = 0;
		g_frame_timer.fps_update_timer -= 1000.0;
	}
	
	// Mark start of next frame
	g_frame_timer.frame_start = current_time;
}


// .............................................................................................
void
Render(HWND window)
{
    PROFILE_ZONE;  // Profile entire render function
    
    // Use frame timer's actual frame time
    float delta_time_ms = (float)g_frame_timer.actual_frame_time_ms;
    
    RECT cr; GetClientRect(window, &cr);
    int w = cr.right - cr.left;
    int h = cr.bottom - cr.top;

    p_render_target->BeginDraw();
    p_render_target->Clear(D2D1::ColorF(D2D1::ColorF::Black));

    // Use global context
    g_ui_context.measure_text = App_Measure_Text;
    UI_Render_List list;
    memset(&list, 0, sizeof(UI_Render_List));
    UI_Begin_Frame_With_Time(&g_ui_context, &list, w, h, delta_time_ms);
    
    // Update FPS for debug display
    g_ui_context.current_fps = g_frame_timer.actual_fps;
    
    // Build UI tree
    {
        PROFILE_ZONE_N("UI Build");
        App_UI_Build(&g_ui_context);
    }
    
    // Layout (calculates panel rects)
    if (g_ui_context.state.panel_count > 0) {
        PROFILE_ZONE_N("UI Layout");
        UI_Layout_Panel_Tree(&g_ui_context.state, 0);
    }
    
    // Update interaction (after layout, before render)
    {
        PROFILE_ZONE_N("UI Interaction");
        UI_Update_Interaction(&g_ui_context);
    }
    
    // Cursor selection based on hot widget and drag state
    {
        PROFILE_ZONE_N("Cursor Update");
        if (g_ui_context.interaction.dragging_divider != 0) {
            // During drag, keep resize cursor
            int divider_idx = -1;
            for (int i = 0; i < g_ui_context.state.panel_count; i++) {
                if (g_ui_context.state.panels[i].id == g_ui_context.interaction.dragging_divider) {
                    divider_idx = i;
                    break;
                }
            }
            
            if (divider_idx >= 0) {
                UI_Panel *divider = &g_ui_context.state.panels[divider_idx];
                if (divider->parent >= 0) {
                    UI_Panel *parent = &g_ui_context.state.panels[divider->parent];
                    if (parent->style.direction == UI_DIRECTION_ROW) {
                        UI_Set_Cursor(g_cursor_size_we);
                    } else {
                        UI_Set_Cursor(g_cursor_size_ns);
                    }
                }
            }
        } else if (g_ui_context.interaction.hot_widget != 0) {
            // Check if hot widget is a resizable divider
            int hot_idx = -1;
            for (int i = 0; i < g_ui_context.state.panel_count; i++) {
                if (g_ui_context.state.panels[i].id == g_ui_context.interaction.hot_widget) {
                    hot_idx = i;
                    break;
                }
            }
            
            if (hot_idx >= 0 && g_ui_context.state.panels[hot_idx].style.resizable) {
                UI_Panel *hot_panel = &g_ui_context.state.panels[hot_idx];
                if (hot_panel->parent >= 0) {
                    UI_Panel *parent = &g_ui_context.state.panels[hot_panel->parent];
                    if (parent->style.direction == UI_DIRECTION_ROW) {
                        UI_Set_Cursor(g_cursor_size_we);
                    } else {
                        UI_Set_Cursor(g_cursor_size_ns);
                    }
                }
            } else {
                UI_Set_Cursor(g_cursor_arrow);
            }
        } else {
            UI_Set_Cursor(g_cursor_arrow);
        }
    }
    
    // Render
    if (g_ui_context.state.panel_count > 0) {
        PROFILE_ZONE_N("UI Emit");
        UI_Emit_Panels(&g_ui_context.state, 0);
    }

    Render_UI(&list);
    Render_UI_Text(&list);

    p_render_target->EndDraw();
    
    // Copy input state for next frame's edge detection
    UI_Input_EndFrame(&g_ui_context);
}


// .............................................................................................
LRESULT CALLBACK
MainWindowCallback(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
	LRESULT result = 0;
	
	switch (message)
	{
		case WM_GETMINMAXINFO:
		{
			LPMINMAXINFO lpMMI = (LPMINMAXINFO)lparam;
			lpMMI->ptMinTrackSize.x = 1280;
			lpMMI->ptMinTrackSize.y = 720;
		} break;

		case WM_SIZE:
		{
			if (p_render_target)
			{
				UINT width = LOWORD(lparam);
				UINT height = HIWORD(lparam);
				p_render_target->Resize(D2D1::SizeU(width, height));
			}

		InvalidateRect(window, NULL, FALSE);

	} break;

	case WM_MOUSEMOVE:
	{
		int x = GET_X_LPARAM(lparam);
		int y = GET_Y_LPARAM(lparam);
		UI_Input_ProcessMouseMove(&g_ui_context, x, y);
	} break;

	case WM_SETCURSOR:
	{
		// Only handle cursor in client area
		if (LOWORD(lparam) == HTCLIENT) {
			// We manage cursor ourselves in Render(), tell Windows to not override
			return TRUE;
		}
		return DefWindowProcW(window, message, wparam, lparam);
	} break;

	case WM_LBUTTONDOWN:
	{
		UI_Input_ProcessMouseButton(&g_ui_context, UI_MOUSE_LEFT, 1);
		SetCapture(window);
	} break;

	case WM_LBUTTONUP:
	{
		UI_Input_ProcessMouseButton(&g_ui_context, UI_MOUSE_LEFT, 0);
		ReleaseCapture();
	} break;

	case WM_RBUTTONDOWN:
	{
		UI_Input_ProcessMouseButton(&g_ui_context, UI_MOUSE_RIGHT, 1);
	} break;

	case WM_RBUTTONUP:
	{
		UI_Input_ProcessMouseButton(&g_ui_context, UI_MOUSE_RIGHT, 0);
	} break;

	case WM_MBUTTONDOWN:
	{
		UI_Input_ProcessMouseButton(&g_ui_context, UI_MOUSE_MIDDLE, 1);
	} break;

	case WM_MBUTTONUP:
	{
		UI_Input_ProcessMouseButton(&g_ui_context, UI_MOUSE_MIDDLE, 0);
	} break;

	case WM_MOUSEWHEEL:
	{
		int delta = GET_WHEEL_DELTA_WPARAM(wparam);
		UI_Input_ProcessMouseWheel(&g_ui_context, (float)delta / (float)WHEEL_DELTA);
	} break;

	case WM_KEYDOWN:
	case WM_SYSKEYDOWN:
	{
		int vk = (int)wparam;
		int was_down = (lparam & (1 << 30)) != 0;
		if (!was_down) {
			UI_Input_ProcessKey(&g_ui_context, vk, 1);
		}
	} break;

	case WM_KEYUP:
	case WM_SYSKEYUP:
	{
		int vk = (int)wparam;
		UI_Input_ProcessKey(&g_ui_context, vk, 0);
	} break;

	case WM_CHAR:
	{
		char c = (char)wparam;
		if (c >= 32 && c < 127) {
			UI_Input_ProcessChar(&g_ui_context, c);
		}
	} break;

	case WM_PAINT:
		{
			PAINTSTRUCT paint_struct;
			BeginPaint(window, &paint_struct);
			
			// Render during resize (Windows blocks main loop)
			if (g_is_resizing) {
				Render(window);
			}
			// Otherwise, continuous loop handles rendering
			
			EndPaint(window, &paint_struct);

		} break;

	case WM_ERASEBKGND:
	{
		return 1;
	}
	
	case WM_ENTERSIZEMOVE:
	{
		g_is_resizing = true;
	} break;
	
	case WM_EXITSIZEMOVE:
	{
		g_is_resizing = false;
	} break;

	case WM_CLOSE:
		{
			DestroyWindow(window);
		} break;

        case WM_DESTROY:
		{
			// Release cached text formats
			for (int i = 0; i < g_text_format_cache.count; i++) {
				if (g_text_format_cache.formats[i]) {
					g_text_format_cache.formats[i]->Release();
				}
			}
			g_text_format_cache.count = 0;
			
			if (p_text_format_default) { p_text_format_default->Release(); p_text_format_default = 0; }
			if (p_text_format_monospace) { p_text_format_monospace->Release(); p_text_format_monospace = 0; }
			if (p_dwrite_factory) { p_dwrite_factory->Release(); p_dwrite_factory = 0; }
			if (p_render_target) { p_render_target->Release(); p_render_target = 0; }
			if (p_brush) { p_brush->Release(); p_brush = 0; }
			if (p_d2d_factory) { p_d2d_factory->Release(); p_d2d_factory = 0; }
			PostQuitMessage(0);
		} break;

		default:
		{
			return DefWindowProcW(window, message, wparam, lparam);
		}
	}
	return result;
}


// .............................................................................................
int WINAPI
WinMain(HINSTANCE instance, HINSTANCE previous_instance, LPSTR command_line, int show_code)
{
	WNDCLASSW window_class;
	memset(&window_class, 0, sizeof(WNDCLASSW));
	window_class.style = CS_HREDRAW|CS_VREDRAW;
	window_class.lpfnWndProc = MainWindowCallback;
	window_class.cbClsExtra = 0;
	window_class.cbWndExtra = 0;
	window_class.hInstance = instance;
	window_class.hIcon = 0;
	window_class.hCursor = LoadCursor(NULL, IDC_ARROW);
	window_class.hbrBackground = 0;
	window_class.lpszMenuName = 0;
	window_class.lpszClassName = L"AppTemplate";

	if (RegisterClassW(&window_class))
	{
		RECT window_rect = {0, 0, 1920, 1080};
		HWND window = CreateWindowExW(0, window_class.lpszClassName, L"AppTemplate",
										WS_OVERLAPPEDWINDOW,
										0, 0, window_rect.right - window_rect.left,
										window_rect.bottom - window_rect.top,
										0, 0, instance, 0);

		if (!window)
		{
			MessageBoxA(NULL, "Failed to create window", "Error", MB_OK | MB_ICONERROR);
			return 1;
		}
		
		g_is_running = true;

			if (!p_d2d_factory)
			{
				HRESULT result = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &p_d2d_factory);
				if (result != S_OK)
				{
					MessageBoxA(NULL, "Failed to create Direct2D factory", "Error", MB_OK | MB_ICONERROR);
					PostQuitMessage(1);
					return 1;
				}
			}

			RECT client_rect;
			GetClientRect(window, &client_rect);
			HRESULT result = p_d2d_factory->CreateHwndRenderTarget(D2D1::RenderTargetProperties(),
					D2D1::HwndRenderTargetProperties(window, D2D1::SizeU(client_rect.right - client_rect.left, client_rect.bottom - client_rect.top),
					D2D1_PRESENT_OPTIONS_IMMEDIATELY),
					&p_render_target);

		if (SUCCEEDED(result))
		{
			p_render_target->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), &p_brush);
		}
		else
		{
			MessageBoxA(NULL, "Failed to create Direct2D render target", "Error", MB_OK | MB_ICONERROR);
			PostQuitMessage(1);
			return 1;
		}
		
		// Initialize DirectWrite
		HRESULT dwrite_result = DWriteCreateFactory(
			DWRITE_FACTORY_TYPE_SHARED,
			__uuidof(IDWriteFactory),
			reinterpret_cast<IUnknown**>(&p_dwrite_factory)
		);
		
		if (FAILED(dwrite_result)) {
			MessageBoxA(NULL, "Failed to initialize DirectWrite", "Error", MB_OK | MB_ICONERROR);
			PostQuitMessage(1);
			return 1;
		}
		else {
			// Create default text format
			dwrite_result = p_dwrite_factory->CreateTextFormat(
				L"Segoe UI",
				NULL,
				DWRITE_FONT_WEIGHT_NORMAL,
				DWRITE_FONT_STYLE_NORMAL,
				DWRITE_FONT_STRETCH_NORMAL,
				14.0f,
				L"en-us",
				&p_text_format_default
			);
			
			if (FAILED(dwrite_result)) {
				MessageBoxA(NULL, "Failed to create text format", "Error", MB_OK | MB_ICONERROR);
				PostQuitMessage(1);
				return 1;
			}
		else {
			p_text_format_default->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
			p_text_format_default->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
		}
		
		// Create monospace text format (Consolas 14pt)
		HRESULT mono_result = p_dwrite_factory->CreateTextFormat(
			L"Consolas",
			NULL,
			DWRITE_FONT_WEIGHT_NORMAL,
			DWRITE_FONT_STYLE_NORMAL,
			DWRITE_FONT_STRETCH_NORMAL,
			14.0f,
			L"en-us",
			&p_text_format_monospace
		);
		
		// Fallback to Courier New if Consolas not available
		if (FAILED(mono_result)) {
			mono_result = p_dwrite_factory->CreateTextFormat(
				L"Courier New",
				NULL,
				DWRITE_FONT_WEIGHT_NORMAL,
				DWRITE_FONT_STYLE_NORMAL,
				DWRITE_FONT_STRETCH_NORMAL,
				14.0f,
				L"en-us",
				&p_text_format_monospace
			);
		}
		
		if (SUCCEEDED(mono_result)) {
			p_text_format_monospace->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
			p_text_format_monospace->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
		}
		
		// Initialize cache
		g_text_format_cache.count = 0;
	}
	
	// Initialize UI context
	memset(&g_ui_context, 0, sizeof(UI_Context));
	g_ui_context.frame_number = 0;
	g_ui_context.delta_time_ms = 0.0f;
	g_ui_context.last_button_clicked[0] = 0;  // Empty string
	
	// Initialize input system
	UI_Input_Init(&g_ui_context.input);
	UI_Input_Init(&g_ui_context.input_prev);
	
	// Initialize cursor handles
	g_cursor_arrow = LoadCursor(NULL, IDC_ARROW);
	g_cursor_size_we = LoadCursor(NULL, IDC_SIZEWE);
	g_cursor_size_ns = LoadCursor(NULL, IDC_SIZENS);
	g_current_cursor = g_cursor_arrow;
	SetCursor(g_cursor_arrow);  // Set initial cursor to prevent spinning wheel
	
	// Initialize frame timing system
	QueryPerformanceFrequency(&g_frame_timer.frequency);
	QueryPerformanceCounter(&g_frame_timer.frame_start);
	g_frame_timer.target_fps = 720;
	g_frame_timer.target_frame_time_ms = 1000.0 / g_frame_timer.target_fps;
	g_frame_timer.actual_fps = 0;
	g_frame_timer.fps_update_timer = 0.0;
	g_frame_timer.frame_count_for_fps = 0;

	ShowWindow(window, SW_MAXIMIZE);
			UpdateWindow(window);

		MSG message;
		while (g_is_running)
		{
			// Process all pending messages (non-blocking)
			while (PeekMessage(&message, 0, 0, 0, PM_REMOVE))
			{
				if(message.message == WM_QUIT)
				{
					g_is_running = false;
					break;
				}

				TranslateMessage(&message);
				DispatchMessageW(&message);
			}
			
		Render(window);
		
		// Wait for target frame time (precise pacing)
		Wait_For_Target_Frame_Time();
		
		PROFILE_FRAME;  // Mark end of frame for Tracy profiler
	}
	}
	
	return 0;
}
