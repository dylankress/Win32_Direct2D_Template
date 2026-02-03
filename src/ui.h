// ui.h
#pragma once
#include <stdint.h>

#define MAX_UI_RECTANGLES 256
#define MAX_UI_TEXTS 256
#define MAX_UI_TEXT_LENGTH 256

enum UI_Direction {
	UI_DIRECTION_ROW = 0,
	UI_DIRECTION_COLUMN = 1
};

enum UI_Align {
	UI_ALIGN_START = 0,
	UI_ALIGN_CENTER = 1,
	UI_ALIGN_END = 2
};

// Mouse button enums
enum UI_Mouse_Button {
	UI_MOUSE_LEFT = 0,
	UI_MOUSE_RIGHT = 1,
	UI_MOUSE_MIDDLE = 2,
	UI_MOUSE_BUTTON_COUNT = 3
};

// Virtual key codes
#define UI_KEY_COUNT 256

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

struct UI_Style {
    uint32_t color;
	int min_w, max_w;
    int min_h, max_h;
    int pref_w, pref_h;
    int pad_l, pad_t, pad_r, pad_b;
    float flex_grow;
    float flex_shrink;
    int flex_basis;
    int direction; // 0=row, 1=column
    int gap;
};

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
    UI_Panel panels[1024];
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
	char char_buffer[32];
	int char_count;
	char last_char;
	
	// Modifier keys
	int ctrl;
	int shift;
	int alt;
};

// Widget interaction state
struct UI_Interaction {
	UI_Id hot_widget;
	UI_Id hot_widget_prev;
	UI_Id active_widget;
	UI_Id active_widget_prev;
	UI_Id focused_widget;
};

struct UI_Context {
	int screen_w;
	int screen_h;
	UI_State state;
	
	// Immediate-mode API support
	int parent_stack[32];
	int parent_stack_count;
	UI_Text_Measure_Func measure_text;
	
	// ID deduplication
	UI_Id used_ids[1024];
	int used_id_counts[1024];
	int used_id_count;
	
	// Input state
	UI_Input input;
	UI_Input input_prev;
	UI_Interaction interaction;
	
	// Debug/diagnostic tracking
	int frame_number;
	float delta_time_ms;
	char last_button_clicked[MAX_UI_TEXT_LENGTH];
};

// Frame management
void UI_BeginFrame(UI_Context *ui, UI_Render_List *out_list, int w, int h);
void UI_BeginFrame_WithTime(UI_Context *ui, UI_Render_List *out_list, int w, int h, float delta_time_ms);

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
void UI_Panel_Set_Direction(UI_Context *ui, UI_Direction dir);
void UI_Panel_Set_Gap(UI_Context *ui, int gap);
void UI_Panel_Set_Grow(UI_Context *ui, float grow);

// Widgets
void UI_Label(UI_Context *ui, const char *text, uint32_t color);
int UI_Button(UI_Context *ui, const char *text);

// Input system
void UI_Input_Init(UI_Input *input);
void UI_Input_NewFrame(UI_Context *ui);
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

// Interaction update
void UI_Update_Interaction(UI_Context *ui);

// Debug overlay
void UI_Debug_Mouse_Overlay(UI_Context *ui);

// Demo
void UI_PanelDemo(UI_Context *ui);
