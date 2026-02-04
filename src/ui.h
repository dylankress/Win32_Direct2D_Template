// ui.h - Immediate-mode UI library for Direct2D
//
// This is a retained-layout immediate-mode UI system designed for Direct2D rendering.
//
// ARCHITECTURE:
// 1. Build Phase: Construct panel tree each frame using UI_Begin_Panel/UI_End_Panel
// 2. Layout Phase: Calculate positions using flexbox-inspired algorithm (UI_LayoutPanelTree)
// 3. Render Phase: Generate rectangles and text primitives (UI_EmitPanels)
// 4. Draw Phase: Render primitives with Direct2D/DirectWrite (application-specific)
//
// KEY CONCEPTS:
// - Panels: Rectangular containers with layout properties (row/column direction, padding, gaps)
// - IDs: String-based identification with automatic deduplication (e.g., "Save" -> "Save##0", "Save##1")
// - Parent Stack: Tracks current panel for automatic child parenting in immediate-mode API
// - Size Modes: 
//   * UI_SIZE_AUTO (-1): Auto-size based on content
//   * UI_SIZE_FLEX (-2): Flex-grow to fill available space
//   * Fixed pixels (>= 0): Explicit size with optional size overrides
// - Size Overrides: Persistent sizing across frame rebuilds (enables resizable dividers)
// - Flexbox Layout: Two-pass algorithm (fixed sizes first, then distribute remaining space to flex-grow)
//
// USAGE PATTERN:
//   UI_Begin_Frame(&ctx, &render_list, width, height);
//   UI_Begin_Panel(&ctx, "root"); 
//     UI_Panel_Set_Direction(&ctx, UI_DIRECTION_COLUMN);
//     UI_Label(&ctx, "Hello", 0xFFFFFFFF);
//     if (UI_Button(&ctx, "Click")) { /* handle click */ }
//   UI_End_Panel(&ctx);
//   UI_LayoutPanelTree(&ctx.state, 0);
//   UI_Update_Interaction(&ctx);
//   UI_EmitPanels(&ctx.state, 0);
//   // ... render render_list with Direct2D ...
//   UI_Input_EndFrame(&ctx);
//
#pragma once
#include <stdint.h>

// UI system capacity limits
#define UI_MAX_PANELS 1024
#define UI_MAX_PARENT_STACK_DEPTH 32
#define UI_MAX_RECTANGLES 256
#define UI_MAX_TEXTS 256
#define UI_MAX_TEXT_LENGTH 256
#define UI_MAX_USED_IDS 1024
#define UI_MAX_SIZE_OVERRIDES 32
#define UI_MAX_CHAR_BUFFER 32
#define UI_KEY_COUNT 256
#define UI_MOUSE_BUTTON_COUNT 3

// Size sentinel values (for UI_Panel_Set_Size and helper functions)
#define UI_SIZE_AUTO -1        // Auto-size based on content
#define UI_SIZE_FLEX -2        // Flex-grow to fill available space

// Text format cache capacity (application-specific)
#define APP_MAX_TEXT_FORMATS 16

// Legacy compatibility (keeping old names for now)
#define MAX_UI_RECTANGLES UI_MAX_RECTANGLES
#define MAX_UI_TEXTS UI_MAX_TEXTS
#define MAX_UI_TEXT_LENGTH UI_MAX_TEXT_LENGTH

enum UI_Direction {
	UI_DIRECTION_ROW = 0,
	UI_DIRECTION_COLUMN = 1
};

enum UI_Align {
	UI_ALIGN_START = 0,
	UI_ALIGN_CENTER = 1,
	UI_ALIGN_END = 2
};

enum UI_Divider_Orientation {
	UI_DIVIDER_VERTICAL = 0,      // Vertical line (resizes horizontally)
	UI_DIVIDER_HORIZONTAL = 1     // Horizontal line (resizes vertically)
};

// Mouse button enums
enum UI_Mouse_Button {
	UI_MOUSE_LEFT = 0,
	UI_MOUSE_RIGHT = 1,
	UI_MOUSE_MIDDLE = 2
};

// Helper key codes
#define UI_KEY_TAB       0x09
#define UI_KEY_ENTER     0x0D
#define UI_KEY_ESCAPE    0x1B
#define UI_KEY_SPACE     0x20
#define UI_KEY_LEFT      0x25
#define UI_KEY_UP        0x26
#define UI_KEY_RIGHT     0x27
#define UI_KEY_DOWN      0x28
#define UI_KEY_DELETE    0x2E
#define UI_KEY_BACKSPACE 0x08

struct UI_Rectangle {
	int left, top, right, bottom;
	uint32_t color;
};

struct UI_Text {
	int x, y, w, h;
	uint32_t color;
	char text[MAX_UI_TEXT_LENGTH];
	int font_size;
	int align_h;
	int align_v;
};

struct UI_Render_List {
	UI_Rectangle rectangles[MAX_UI_RECTANGLES];
	UI_Text texts[MAX_UI_TEXTS];
	int rect_count;
	int text_count;
};

typedef int32_t UI_Id;

struct UI_RectI { int x, y, w, h; };

typedef UI_RectI (*UI_Text_Measure_Func)(const char *text, int font_size);

// UI_Style - Layout and visual properties for a panel
// Flexbox-inspired layout system with explicit size constraints
struct UI_Style {
    uint32_t color;
	int min_w, max_w;
    int min_h, max_h;
    int pref_w, pref_h;         // Preferred size (-1=auto, -2=flex-grow, >=0=fixed pixels)
    int pad_l, pad_t, pad_r, pad_b;  // Padding (inner spacing)
    float flex_grow;            // Flex-grow factor (0=no grow, 1.0=grow proportionally)
    float flex_shrink;          // Flex-shrink factor (unused currently, reserved for future)
    int flex_basis;             // Flex-basis (unused currently, reserved for future)
    int direction;              // Layout direction: 0=row (horizontal), 1=column (vertical)
    int gap;                    // Space between child panels
	int resizable;              // 0 = not resizable, 1 = resizable
	int resize_hitbox_padding;  // Extra pixels around divider for hitbox
};

// UI_Panel - A rectangular container in the panel tree
// Tree structure: parent -> first_child -> next_sibling -> ...
// Layout is calculated top-down based on parent's direction and child constraints
struct UI_Panel {
    UI_Id id;
    UI_Style style;
    int parent;
    int first_child;
	int last_child;
    int next_sibling;
    UI_RectI rect;
	
	// Label metadata (if this panel is a label)
	char label_text[MAX_UI_TEXT_LENGTH];
	uint32_t label_color;
	int is_label;
};

struct UI_State {
    UI_Panel panels[UI_MAX_PANELS];
    int panel_count;
};

// Input state structure
struct UI_Input {
	// Mouse state
	int mouse_x, mouse_y;
	int mouse_x_prev, mouse_y_prev;
	int mouse_dx, mouse_dy;
	
	int mouse_down[UI_MOUSE_BUTTON_COUNT];
	int mouse_pressed[UI_MOUSE_BUTTON_COUNT];
	int mouse_released[UI_MOUSE_BUTTON_COUNT];
	
	float mouse_wheel_delta;
	
	// Keyboard state
	int key_down[UI_KEY_COUNT];
	int key_pressed[UI_KEY_COUNT];
	int key_released[UI_KEY_COUNT];
	
	// Character input
	char char_buffer[UI_MAX_CHAR_BUFFER];
	int char_count;
	char last_char;
	
	// Modifier keys
	int ctrl;
	int shift;
	int alt;
};

// Widget interaction state - Tracks hot/active/focused widgets
// Hot: Widget under mouse cursor (hover state)
// Active: Widget being clicked/dragged (pressed state)
// Focused: Widget with keyboard focus (for text input, etc.)
struct UI_Interaction {
	UI_Id hot_widget;           // Currently hovered widget
	UI_Id hot_widget_prev;      // Previous frame's hot widget (for transition detection)
	UI_Id active_widget;        // Currently active (pressed) widget
	UI_Id active_widget_prev;   // Previous frame's active widget
	UI_Id focused_widget;       // Widget with keyboard focus (unused currently)
	UI_Id dragging_divider;      // Panel ID being dragged (0 if not)
	int drag_start_pos;           // Initial mouse x or y
	int drag_start_size_left;     // Left panel's starting size
	int drag_start_size_right;    // Right panel's starting size
	UI_Id resize_target_left_id;  // Panel to left of divider (stable across frames)
	UI_Id resize_target_right_id; // Panel to right of divider (stable across frames)
};

// Size override for persistent panel sizing across frame rebuilds
// Needed because immediate-mode rebuilds the tree every frame, but we want
// resizable dividers to remember their size between frames
struct UI_Size_Override {
	UI_Id panel_id;
	int pref_w;
	int pref_h;
};

struct UI_Context {
	int screen_w;
	int screen_h;
	UI_State state;
	
	// Immediate-mode API support
	int parent_stack[UI_MAX_PARENT_STACK_DEPTH];
	int parent_stack_count;
	UI_Text_Measure_Func measure_text;
	
	// ID deduplication
	UI_Id used_ids[UI_MAX_USED_IDS];
	int used_id_counts[UI_MAX_USED_IDS];
	int used_id_count;
	
	// Input state
	UI_Input input;
	UI_Input input_prev;
	UI_Interaction interaction;
	
	// Size overrides (persist across frame rebuilds)
	UI_Size_Override size_overrides[UI_MAX_SIZE_OVERRIDES];
	int size_override_count;
	
	// Debug/diagnostic tracking
	int frame_number;
	float delta_time_ms;
	int current_fps;
	char last_button_clicked[MAX_UI_TEXT_LENGTH];
};

// Frame management
void UI_Begin_Frame(UI_Context *ui, UI_Render_List *out_list, int w, int h);
void UI_Begin_Frame_With_Time(UI_Context *ui, UI_Render_List *out_list, int w, int h, float delta_time_ms);

// Panel API
typedef UI_Style UI_Panel_Style;
UI_Panel_Style UI_Default_Panel_Style(void);
void UI_Begin_Panel(UI_Context *ui, const char *id);
void UI_Begin_Panel_With_Id(UI_Context *ui, UI_Id id, const char *debug_name);
void UI_Begin_Panel_Ex(UI_Context *ui, const char *id, UI_Panel_Style *style);
void UI_End_Panel(UI_Context *ui);

// Panel style setters (operate on current panel)
void UI_Panel_Set_Color(UI_Context *ui, uint32_t color);
void UI_Panel_Set_Size(UI_Context *ui, int width, int height);
void UI_Panel_Set_Padding(UI_Context *ui, int l, int t, int r, int b);
void UI_Panel_Set_Padding_Uniform(UI_Context *ui, int padding);
void UI_Panel_Set_Direction(UI_Context *ui, UI_Direction dir);
void UI_Panel_Set_Gap(UI_Context *ui, int gap);
void UI_Panel_Set_Grow(UI_Context *ui, float grow);
void UI_Panel_Set_Resizable(UI_Context *ui, int resizable, int hitbox_padding);

// Compact panel creation helpers
void UI_BeginPanel(UI_Context *ui, const char *id, int direction, int w, int h, 
                   int padding, int gap, uint32_t color);
void UI_Panel_Resizable(UI_Context *ui, const char *id, int direction, 
                        int default_w, int default_h, int padding, int gap, uint32_t color);

// Divider helpers
void UI_Divider(UI_Context *ui, const char *id, int orientation);
void UI_Divider_Ex(UI_Context *ui, const char *id, int orientation, uint32_t color, int hitbox_padding);

// Widgets
void UI_Label(UI_Context *ui, const char *text, uint32_t color);
int UI_Button(UI_Context *ui, const char *text);

// Input system
void UI_Input_Init(UI_Input *input);
void UI_Input_NewFrame(UI_Context *ui);
void UI_Input_EndFrame(UI_Context *ui);
void UI_Input_ProcessMouseMove(UI_Context *ui, int x, int y);
void UI_Input_ProcessMouseButton(UI_Context *ui, UI_Mouse_Button button, int down);
void UI_Input_ProcessMouseWheel(UI_Context *ui, float delta);
void UI_Input_ProcessKey(UI_Context *ui, int vk_code, int down);
void UI_Input_ProcessChar(UI_Context *ui, char c);

// Input queries
int UI_Is_Mouse_Down(UI_Context *ui, UI_Mouse_Button button);
int UI_Is_Mouse_Pressed(UI_Context *ui, UI_Mouse_Button button);
int UI_Is_Mouse_Released(UI_Context *ui, UI_Mouse_Button button);
int UI_Is_Key_Down(UI_Context *ui, int vk_code);
int UI_Is_Key_Pressed(UI_Context *ui, int vk_code);
int UI_Is_Key_Released(UI_Context *ui, int vk_code);

// Mouse helpers
void UI_Get_Mouse_Pos(UI_Context *ui, int *out_x, int *out_y);
void UI_Get_Mouse_Delta(UI_Context *ui, int *out_dx, int *out_dy);
float UI_Get_Mouse_Wheel(UI_Context *ui);

// Widget interaction helpers
int UI_Is_Point_In_Rect(int x, int y, UI_RectI rect);
int UI_Is_Hovered(UI_Context *ui, UI_RectI rect);
int UI_Is_Widget_Hot(UI_Context *ui, UI_Id id);
int UI_Is_Widget_Active(UI_Context *ui, UI_Id id);
void UI_Set_Hot_Widget(UI_Context *ui, UI_Id id);
void UI_Set_Active_Widget(UI_Context *ui, UI_Id id);
void UI_Clear_Active_Widget(UI_Context *ui);

// Size override helpers (for persistent panel sizing)
void UI_Set_Size_Override(UI_Context *ui, UI_Id panel_id, int pref_w, int pref_h);
int UI_Get_Size_Override_W(UI_Context *ui, UI_Id panel_id);
int UI_Get_Size_Override_H(UI_Context *ui, UI_Id panel_id);

// Interaction update
void UI_Update_Interaction(UI_Context *ui);

// Debug overlay
void UI_Debug_Mouse_Overlay(UI_Context *ui);
