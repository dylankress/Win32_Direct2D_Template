//application.cpp
#include <windows.h>
#include <windowsx.h>
#include <d2d1.h>
#include <dwrite.h>
#include <stdio.h>
#include <stdint.h>
#include "ui.cpp"
#include "app_ui.h"

bool is_running;
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

// Text format cache (for performance)
struct Text_Format_Cache {
	IDWriteTextFormat *formats[16];
	int sizes[16];
	int count;
};
Text_Format_Cache g_text_format_cache;

// Global UI context (for window message handler access)
UI_Context g_ui_context = {};

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

Frame_Timer g_frame_timer = {};


// .............................................................................................
IDWriteTextFormat*
Get_Text_Format(int font_size)
{
	// Check cache
	for (int i = 0; i < g_text_format_cache.count; i++) {
		if (g_text_format_cache.sizes[i] == font_size) {
			return g_text_format_cache.formats[i];
		}
	}
	
	// Create new format if not cached
	if (g_text_format_cache.count < 16) {
		IDWriteTextFormat *fmt;
		HRESULT hr = p_dwrite_factory->CreateTextFormat(
			L"Segoe UI",
			NULL,
			DWRITE_FONT_WEIGHT_NORMAL,
			DWRITE_FONT_STYLE_NORMAL,
			DWRITE_FONT_STRETCH_NORMAL,
			(float)font_size,
			L"en-us",
			&fmt
		);
		
		if (SUCCEEDED(hr)) {
			int idx = g_text_format_cache.count++;
			g_text_format_cache.formats[idx] = fmt;
			g_text_format_cache.sizes[idx] = font_size;
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
		return UI_RectI{ 0, 0, 0, 0 };
	}
	
	// Convert UTF-8 to UTF-16
	wchar_t wtext[MAX_UI_TEXT_LENGTH];
	MultiByteToWideChar(CP_UTF8, 0, text, -1, wtext, MAX_UI_TEXT_LENGTH);
	
	// Get text format
	IDWriteTextFormat *fmt = Get_Text_Format(font_size);
	
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
		return UI_RectI{ 0, 0, 0, 20 }; // Fallback height
	}
	
	// Get metrics
	DWRITE_TEXT_METRICS metrics;
	layout->GetMetrics(&metrics);
	layout->Release();
	
	return UI_RectI{ 
		0, 0, 
		(int)(metrics.width + 0.5f), 
		(int)(metrics.height + 0.5f) 
	};
}


// .............................................................................................
void
Render_UI(UI_Render_List *render_list)
{
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
		
		// Get text format (use cached or default)
		IDWriteTextFormat *fmt = (src->font_size > 0) 
			? Get_Text_Format(src->font_size)
			: p_text_format_default;
		
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
    // Use frame timer's actual frame time
    float delta_time_ms = (float)g_frame_timer.actual_frame_time_ms;
    
    RECT cr; GetClientRect(window, &cr);
    int w = cr.right - cr.left;
    int h = cr.bottom - cr.top;

    p_render_target->BeginDraw();
    p_render_target->Clear(D2D1::ColorF(D2D1::ColorF::Black));

    // Use global context
    g_ui_context.measure_text = App_Measure_Text;
    UI_Render_List list = {};
    UI_BeginFrame_WithTime(&g_ui_context, &list, w, h, delta_time_ms);
    
    // Update FPS for debug display
    g_ui_context.current_fps = g_frame_timer.actual_fps;
    
    // Build UI tree
    App_UI_Build(&g_ui_context);
    
    // Layout (calculates panel rects)
    if (g_ui_context.state.panel_count > 0) {
        UI_LayoutPanelTree(&g_ui_context.state, 0);
    }
    
    // Update interaction (after layout, before render)
    UI_Update_Interaction(&g_ui_context);
    
    // Cursor selection based on hot widget and drag state
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
    
    // Render
    if (g_ui_context.state.panel_count > 0) {
        UI_EmitPanels(&g_ui_context.state, 0);
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
	WNDCLASSW window_class = {};
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

		if (window)
		{
			is_running = true;

			if (!p_d2d_factory)
			{
				HRESULT result = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &p_d2d_factory);
				if (result != S_OK)
				{
					PostQuitMessage(1);
				}
			}

			RECT client_rect;
			GetClientRect(window, &client_rect);
			HRESULT result = p_d2d_factory->CreateHwndRenderTarget(D2D1::RenderTargetProperties(),
					D2D1::HwndRenderTargetProperties(window, D2D1::SizeU(client_rect.right - client_rect.left, client_rect.bottom - client_rect.top)),
					&p_render_target);

		if (SUCCEEDED(result))
		{
			p_render_target->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), &p_brush);
		}
		else
		{
			PostQuitMessage(1);
		}
		
		// Initialize DirectWrite
		HRESULT dwrite_result = DWriteCreateFactory(
			DWRITE_FACTORY_TYPE_SHARED,
			__uuidof(IDWriteFactory),
			reinterpret_cast<IUnknown**>(&p_dwrite_factory)
		);
		
		if (FAILED(dwrite_result)) {
			MessageBoxA(NULL, "Failed to initialize DirectWrite", "Error", MB_OK);
			PostQuitMessage(1);
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
				MessageBoxA(NULL, "Failed to create text format", "Error", MB_OK);
				PostQuitMessage(1);
			}
			else {
				p_text_format_default->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
				p_text_format_default->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
			}
			
		// Initialize cache
		g_text_format_cache.count = 0;
	}
	
	// Initialize UI context
	g_ui_context = {};  // Zero-initialize everything
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
	g_frame_timer.target_fps = 120;
	g_frame_timer.target_frame_time_ms = 1000.0 / 120.0;  // ~8.33ms
	g_frame_timer.actual_fps = 120;
	g_frame_timer.fps_update_timer = 0.0;
	g_frame_timer.frame_count_for_fps = 0;

	ShowWindow(window, SW_MAXIMIZE);
			UpdateWindow(window);

		MSG message;
		while (is_running)
		{
			// Process all pending messages (non-blocking)
			while (PeekMessage(&message, 0, 0, 0, PM_REMOVE))
			{
				if(message.message == WM_QUIT)
				{
					is_running = false;
					break;
				}

				TranslateMessage(&message);
				DispatchMessageW(&message);
			}
			
			// Render frame at 120 FPS
			Render(window);
			
			// Wait for target frame time (precise pacing)
			Wait_For_Target_Frame_Time();
		}
		}
	}
	
	return 0;
}
