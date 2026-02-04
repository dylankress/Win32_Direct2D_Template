// ui.cpp
#include "ui.h"
#include <string.h>


// internal per-frame pointer (not visible outside UI)
static UI_Render_List *g_render_list;


// .............................................................................................
static UI_Id
UI_HashString(const char *str)
{
	if (!str) return 0;
	
	UI_Id hash = 5381;
	int c;
	while ((c = *str++))
		hash = ((hash << 5) + hash) + c;
	
	return hash;
}


// .............................................................................................
static UI_Id
UI_Generate_Id(UI_Context *ctx, const char *str)
{
	UI_Id base_id = UI_HashString(str);
	
	// Check if this ID was already used this frame
	for (int i = 0; i < ctx->used_id_count; i++) {
		if (ctx->used_ids[i] == base_id) {
			// Found duplicate - increment counter and rehash
			int count = ctx->used_id_counts[i]++;
			
			// Create unique ID: hash(original + "##" + count)
			char unique_str[MAX_UI_TEXT_LENGTH];
			snprintf(unique_str, sizeof(unique_str), "%s##%d", str, count);
			return UI_HashString(unique_str);
		}
	}
	
	// First use - track it
	if (ctx->used_id_count < 1024) {
		ctx->used_ids[ctx->used_id_count] = base_id;
		ctx->used_id_counts[ctx->used_id_count] = 0;
		ctx->used_id_count++;
	}
	
	return base_id;
}


// .............................................................................................
static void
UI_AddRectangle(int l, int t, int r, int b, uint32_t color)
{
	if (!g_render_list) return;
	if (g_render_list->rect_count >= MAX_UI_RECTANGLES) return;

	UI_Rectangle *dst = &g_render_list->rectangles[g_render_list->rect_count++];
	dst->left = l;
	dst->top = t;
	dst->right = r;
	dst->bottom = b;
	dst->color = color;
}


// .............................................................................................
static void
UI_AddText(int x, int y, int w, int h, const char *text_str, 
           uint32_t color, int font_size, int align_h, int align_v)
{
	if (!g_render_list) return;
	if (g_render_list->text_count >= MAX_UI_TEXTS) return;
	if (!text_str) return;

	UI_Text *dst = &g_render_list->texts[g_render_list->text_count++];
	dst->x = x;
	dst->y = y;
	dst->w = w;
	dst->h = h;
	dst->color = color;
	
	// Safe string copy
	int len = 0;
	while (len < MAX_UI_TEXT_LENGTH - 1 && text_str[len]) {
		dst->text[len] = text_str[len];
		len++;
	}
	dst->text[len] = 0;
	
	dst->font_size = font_size;
	dst->align_h = align_h;
	dst->align_v = align_v;
}


// .............................................................................................
static int
UI_NewPanel(UI_State *s, UI_Id id)
{
	if (s->panel_count >= (int)(sizeof(s->panels)/sizeof(s->panels[0]))) return -1;

	int idx = s->panel_count++;
	UI_Panel *p = &s->panels[idx];
	memset(p, 0, sizeof(UI_Panel));

	p->id = id;
	p->parent = -1;
	p->first_child = -1;
	p->last_child = -1;
	p->next_sibling = -1;

	p->style.color = 0xFF222222;
	p->style.min_w = 200; p->style.max_w = INT32_MAX;
	p->style.min_h = 200; p->style.max_h = INT32_MAX;
	p->style.pref_w = -1; p->style.pref_h = -1;

	p->style.flex_grow = 0.0f;
	p->style.flex_shrink = 1.0f;
	p->style.flex_basis = -1;

	p->style.direction = 0; // row by default
	p->style.gap = 0;

	p->style.pad_l = p->style.pad_t = p->style.pad_r = p->style.pad_b = 0;
	
	p->style.resizable = 0;
	p->style.resize_hitbox_padding = 4;
	
	// Initialize label metadata
	p->is_label = 0;
	p->label_text[0] = 0;
	p->label_color = 0xFFFFFFFF;

	return idx;
}


// .............................................................................................
static void UI_AddChild(UI_State *s, int parent_idx, int child_idx)
{
    UI_Panel *parent = &s->panels[parent_idx];
    UI_Panel *child  = &s->panels[child_idx];

    child->parent = parent_idx;
    child->next_sibling = -1;

    if (parent->first_child == -1)
    {
        parent->first_child = child_idx;
        parent->last_child  = child_idx;
    }
    else
    {
        s->panels[parent->last_child].next_sibling = child_idx;
        parent->last_child = child_idx;
    }
}


// .............................................................................................
static void UI_LayoutRow(UI_State *s, int panel_idx)
{
    UI_Panel *p = &s->panels[panel_idx];

    // Content box inside padding
    int x0 = p->rect.x + p->style.pad_l;
    int y0 = p->rect.y + p->style.pad_t;
    int cw = p->rect.w - (p->style.pad_l + p->style.pad_r);
    int ch = p->rect.h - (p->style.pad_t + p->style.pad_b);
    if (cw < 0) cw = 0;
    if (ch < 0) ch = 0;

    // First pass: count children, sum fixed widths, sum flex grow
    int child_count = 0;
    int fixed_sum = 0;
    float grow_sum = 0.0f;

    for (int c = p->first_child; c != -1; c = s->panels[c].next_sibling)
    {
        child_count++;

        UI_Panel *child = &s->panels[c];
        int w = (child->style.pref_w >= 0) ? child->style.pref_w : 0;
        fixed_sum += w;

        if (child->style.flex_grow > 0.0f)
            grow_sum += child->style.flex_grow;
    }

    int gaps_total = (child_count > 1) ? (p->style.gap * (child_count - 1)) : 0;

    int remaining = cw - fixed_sum - gaps_total;
    if (remaining < 0) remaining = 0;

    // Second pass: assign child rects
    int cursor_x = x0;

    for (int c = p->first_child; c != -1; c = s->panels[c].next_sibling)
    {
        UI_Panel *child = &s->panels[c];

        int w = (child->style.pref_w >= 0) ? child->style.pref_w : 0;

        if (child->style.flex_grow > 0.0f && grow_sum > 0.0f)
        {
            // distribute remaining space proportionally
            float t = child->style.flex_grow / grow_sum;
            int flex_w = (int)(t * (float)remaining);
            w += flex_w;
        }

        // V1: child fills height
        child->rect.x = cursor_x;
        child->rect.y = y0;
        child->rect.w = w;
        child->rect.h = ch;

        cursor_x += w + p->style.gap;
    }
}


// .............................................................................................
static void UI_LayoutColumn(UI_State *s, int panel_idx)
{
    UI_Panel *p = &s->panels[panel_idx];

    // Content box inside padding
    int x0 = p->rect.x + p->style.pad_l;
    int y0 = p->rect.y + p->style.pad_t;
    int cw = p->rect.w - (p->style.pad_l + p->style.pad_r);
    int ch = p->rect.h - (p->style.pad_t + p->style.pad_b);
    if (cw < 0) cw = 0;
    if (ch < 0) ch = 0;

    // First pass: count children, sum fixed heights, sum flex grow
    int child_count = 0;
    int fixed_sum = 0;
    float grow_sum = 0.0f;

    for (int c = p->first_child; c != -1; c = s->panels[c].next_sibling)
    {
        child_count++;

        UI_Panel *child = &s->panels[c];
        int h = (child->style.pref_h >= 0) ? child->style.pref_h : 0;
        fixed_sum += h;

        if (child->style.flex_grow > 0.0f)
            grow_sum += child->style.flex_grow;
    }

    int gaps_total = (child_count > 1) ? (p->style.gap * (child_count - 1)) : 0;

    int remaining = ch - fixed_sum - gaps_total;
    if (remaining < 0) remaining = 0;

    // Second pass: assign child rects
    int cursor_y = y0;

    for (int c = p->first_child; c != -1; c = s->panels[c].next_sibling)
    {
        UI_Panel *child = &s->panels[c];

        int h = (child->style.pref_h >= 0) ? child->style.pref_h : 0;

        if (child->style.flex_grow > 0.0f && grow_sum > 0.0f)
        {
            float t = child->style.flex_grow / grow_sum;
            int flex_h = (int)(t * (float)remaining);
            h += flex_h;
        }

        // V1: child fills width
        child->rect.x = x0;
        child->rect.y = cursor_y;
        child->rect.w = cw;
        child->rect.h = h;

        cursor_y += h + p->style.gap;
    }
}


// .............................................................................................
static void UI_LayoutPanelTree(UI_State *s, int panel_idx)
{
    UI_Panel *p = &s->panels[panel_idx];

    // Layout this container’s children based on its direction
    if (p->first_child != -1)
    {
        if (p->style.direction == 0)      UI_LayoutRow(s, panel_idx);
		else if (p->style.direction == 1) UI_LayoutColumn(s, panel_idx);

        // Recurse into children
        for (int c = p->first_child; c != -1; c = s->panels[c].next_sibling)
            UI_LayoutPanelTree(s, c);
    }
}


// .............................................................................................
static void UI_EmitPanels(UI_State *s, int panel_idx)
{
    UI_Panel *p = &s->panels[panel_idx];

    // Emit this panel's rect (skip if transparent and is label)
    if (!(p->is_label && p->style.color == 0x00000000)) {
        UI_AddRectangle(
            p->rect.x,
            p->rect.y,
            p->rect.x + p->rect.w,
            p->rect.y + p->rect.h,
            p->style.color
        );
    }
    
    // Emit label text if this is a label panel
    if (p->is_label && p->label_text[0] != 0) {
        UI_AddText(
            p->rect.x,
            p->rect.y,
            p->rect.w,
            p->rect.h,
            p->label_text,
            p->label_color,
            14,
            UI_ALIGN_START,
            UI_ALIGN_CENTER
        );
    }

    // Emit children
    for (int c = p->first_child; c != -1; c = s->panels[c].next_sibling)
        UI_EmitPanels(s, c);
}


// .............................................................................................
void
UI_BeginFrame(UI_Context *ui, UI_Render_List *out_list, int w, int h)
{
	UI_BeginFrame_WithTime(ui, out_list, w, h, 0.0f);
}


// .............................................................................................
void
UI_BeginFrame_WithTime(UI_Context *ui, UI_Render_List *out_list, int w, int h, float delta_time_ms)
{
	// Store previous frame interaction state
	ui->interaction.hot_widget_prev = ui->interaction.hot_widget;
	ui->interaction.active_widget_prev = ui->interaction.active_widget;
	
	// Update frame tracking
	ui->frame_number++;
	ui->delta_time_ms = delta_time_ms;
	
	ui->screen_w = w;
	ui->screen_h = h;

	// reset UI state for this frame
    ui->state.panel_count = 0;

	// reset render list for this frame
	*out_list = {};
	g_render_list = out_list;
	
	// Reset immediate-mode state
	ui->parent_stack_count = 0;
	ui->used_id_count = 0;
	
	// Process input for new frame
	UI_Input_NewFrame(ui);
}


// .............................................................................................
UI_Panel_Style
UI_Default_Panel_Style(void)
{
	UI_Panel_Style s = {};
	s.color = 0xFF222222;
	s.min_w = 0;
	s.max_w = INT32_MAX;
	s.min_h = 0;
	s.max_h = INT32_MAX;
	s.pref_w = -1;
	s.pref_h = -1;
	s.flex_grow = 0.0f;
	s.flex_shrink = 1.0f;
	s.flex_basis = -1;
	s.direction = UI_DIRECTION_ROW;
	s.gap = 0;
	s.pad_l = s.pad_t = s.pad_r = s.pad_b = 0;
	return s;
}


// .............................................................................................
void
UI_Begin_Panel(UI_Context *ctx, const char *id_str)
{
	UI_Id id = UI_Generate_Id(ctx, id_str);
	int panel_idx = UI_NewPanel(&ctx->state, id);
	
	if (panel_idx < 0) return;
	
	int parent_idx = (ctx->parent_stack_count > 0) 
	                 ? ctx->parent_stack[ctx->parent_stack_count - 1] 
	                 : -1;
	
	if (parent_idx >= 0) {
		UI_AddChild(&ctx->state, parent_idx, panel_idx);
	} else {
		ctx->state.panels[panel_idx].rect.x = 0;
		ctx->state.panels[panel_idx].rect.y = 0;
		ctx->state.panels[panel_idx].rect.w = ctx->screen_w;
		ctx->state.panels[panel_idx].rect.h = ctx->screen_h;
	}
	
	if (ctx->parent_stack_count < 32) {
		ctx->parent_stack[ctx->parent_stack_count++] = panel_idx;
	}
}


// .............................................................................................
void
UI_Begin_Panel_With_Id(UI_Context *ctx, UI_Id id, const char *debug_name)
{
	int panel_idx = UI_NewPanel(&ctx->state, id);
	
	if (panel_idx < 0) return;
	
	int parent_idx = (ctx->parent_stack_count > 0) 
	                 ? ctx->parent_stack[ctx->parent_stack_count - 1] 
	                 : -1;
	
	if (parent_idx >= 0) {
		UI_AddChild(&ctx->state, parent_idx, panel_idx);
	} else {
		ctx->state.panels[panel_idx].rect.x = 0;
		ctx->state.panels[panel_idx].rect.y = 0;
		ctx->state.panels[panel_idx].rect.w = ctx->screen_w;
		ctx->state.panels[panel_idx].rect.h = ctx->screen_h;
	}
	
	if (ctx->parent_stack_count < 32) {
		ctx->parent_stack[ctx->parent_stack_count++] = panel_idx;
	}
}


// .............................................................................................
void
UI_Begin_Panel_Ex(UI_Context *ctx, const char *id_str, UI_Panel_Style *style)
{
	UI_Begin_Panel(ctx, id_str);
	
	if (ctx->parent_stack_count > 0) {
		int panel_idx = ctx->parent_stack[ctx->parent_stack_count - 1];
		ctx->state.panels[panel_idx].style = *style;
	}
}


// .............................................................................................
void
UI_End_Panel(UI_Context *ctx)
{
	if (ctx->parent_stack_count > 0) {
		ctx->parent_stack_count--;
	}
}


// .............................................................................................
void
UI_Panel_Set_Color(UI_Context *ui, uint32_t color)
{
	if (ui->parent_stack_count == 0) return;
	int idx = ui->parent_stack[ui->parent_stack_count - 1];
	ui->state.panels[idx].style.color = color;
}


// .............................................................................................
void
UI_Panel_Set_Size(UI_Context *ui, int width, int height)
{
	if (ui->parent_stack_count == 0) return;
	int idx = ui->parent_stack[ui->parent_stack_count - 1];
	ui->state.panels[idx].style.pref_w = width;
	ui->state.panels[idx].style.pref_h = height;
}


// .............................................................................................
void
UI_Panel_Set_Padding(UI_Context *ui, int l, int t, int r, int b)
{
	if (ui->parent_stack_count == 0) return;
	int idx = ui->parent_stack[ui->parent_stack_count - 1];
	UI_Panel *p = &ui->state.panels[idx];
	p->style.pad_l = l;
	p->style.pad_t = t;
	p->style.pad_r = r;
	p->style.pad_b = b;
}


// .............................................................................................
void
UI_Panel_Set_Direction(UI_Context *ui, UI_Direction dir)
{
	if (ui->parent_stack_count == 0) return;
	int idx = ui->parent_stack[ui->parent_stack_count - 1];
	ui->state.panels[idx].style.direction = dir;
}


// .............................................................................................
void
UI_Panel_Set_Gap(UI_Context *ui, int gap)
{
	if (ui->parent_stack_count == 0) return;
	int idx = ui->parent_stack[ui->parent_stack_count - 1];
	ui->state.panels[idx].style.gap = gap;
}


// .............................................................................................
void
UI_Panel_Set_Grow(UI_Context *ui, float grow)
{
	if (ui->parent_stack_count == 0) return;
	int idx = ui->parent_stack[ui->parent_stack_count - 1];
	ui->state.panels[idx].style.flex_grow = grow;
}


// .............................................................................................
void
UI_Panel_Set_Padding_Uniform(UI_Context *ui, int padding)
{
	UI_Panel_Set_Padding(ui, padding, padding, padding, padding);
}


// .............................................................................................
void
UI_Panel_Set_Resizable(UI_Context *ui, int resizable, int hitbox_padding)
{
	if (ui->parent_stack_count == 0) return;
	int idx = ui->parent_stack[ui->parent_stack_count - 1];
	ui->state.panels[idx].style.resizable = resizable;
	ui->state.panels[idx].style.resize_hitbox_padding = hitbox_padding;
}


// .............................................................................................
void
UI_BeginPanel(UI_Context *ui, const char *id, int direction, int w, int h, 
              int padding, int gap, uint32_t color)
{
	UI_Begin_Panel(ui, id);
	UI_Panel_Set_Direction(ui, (UI_Direction)direction);
	
	if (w >= 0 || h >= 0) {
		UI_Panel_Set_Size(ui, w, h);
	}
	
	if (padding > 0) {
		UI_Panel_Set_Padding_Uniform(ui, padding);
	}
	
	if (gap >= 0) {
		UI_Panel_Set_Gap(ui, gap);
	}
	
	if (color != 0) {
		UI_Panel_Set_Color(ui, color);
	}
}


// .............................................................................................
void
UI_Panel_Resizable(UI_Context *ui, const char *id, int direction, 
                   int default_w, int default_h, int padding, int gap, uint32_t color)
{
	UI_Begin_Panel(ui, id);
	UI_Panel_Set_Direction(ui, (UI_Direction)direction);
	
	// Check for size overrides (from user resizing via dividers)
	UI_Id panel_id = UI_HashString(id);
	int w = UI_Get_Size_Override_W(ui, panel_id);
	int h = UI_Get_Size_Override_H(ui, panel_id);
	
	// Use overrides if they exist, otherwise use defaults
	if (w < 0) w = default_w;
	if (h < 0) h = default_h;
	
	// Handle flex-grow vs fixed-size (-2 means flex-grow)
	int wants_flex_w = (default_w == -2);
	int wants_flex_h = (default_h == -2);
	
	if (w == -2 || (w < 0 && wants_flex_w)) {
		// Width wants flex-grow
		if (h >= 0 && h != -2) {
			// Fixed height, flex width
			UI_Panel_Set_Size(ui, -1, h);
		}
		UI_Panel_Set_Grow(ui, 1.0f);
	} else if (h == -2 || (h < 0 && wants_flex_h)) {
		// Height wants flex-grow
		if (w >= 0) {
			// Fixed width, flex height
			UI_Panel_Set_Size(ui, w, -1);
		}
		UI_Panel_Set_Grow(ui, 1.0f);
	} else if (w >= 0 || h >= 0) {
		// At least one dimension is fixed
		UI_Panel_Set_Size(ui, w, h);
	}
	
	if (padding > 0) {
		UI_Panel_Set_Padding_Uniform(ui, padding);
	}
	
	if (gap >= 0) {
		UI_Panel_Set_Gap(ui, gap);
	}
	
	if (color != 0) {
		UI_Panel_Set_Color(ui, color);
	}
}


// .............................................................................................
void
UI_Divider(UI_Context *ui, const char *id, int orientation)
{
	UI_Divider_Ex(ui, id, orientation, 0x33FFFFFF, 4);
}


// .............................................................................................
void
UI_Divider_Ex(UI_Context *ui, const char *id, int orientation, uint32_t color, int hitbox_padding)
{
	UI_Begin_Panel(ui, id);
	
	if (orientation == UI_DIVIDER_VERTICAL) {
		// Vertical line (full height, 1px wide)
		UI_Panel_Set_Size(ui, 1, -1);
	} else {
		// Horizontal line (full width, 1px tall)
		UI_Panel_Set_Size(ui, -1, 1);
	}
	
	UI_Panel_Set_Color(ui, color);
	UI_Panel_Set_Resizable(ui, 1, hitbox_padding);
	
	UI_End_Panel(ui);
}


// .............................................................................................
void
UI_Label(UI_Context *ui, const char *text, uint32_t color)
{
	if (!text || !ui->measure_text) return;
	
	UI_RectI text_size = ui->measure_text(text, 14);
	
	UI_Begin_Panel(ui, text);
	UI_Panel_Set_Size(ui, text_size.w + 2, text_size.h);
	
	if (ui->parent_stack_count > 0) {
		int panel_idx = ui->parent_stack[ui->parent_stack_count - 1];
		UI_Panel *panel = &ui->state.panels[panel_idx];
		
		panel->style.color = 0x00000000;
		panel->is_label = 1;
		panel->label_color = color;
		
		int len = 0;
		while (len < MAX_UI_TEXT_LENGTH - 1 && text[len]) {
			panel->label_text[len] = text[len];
			len++;
		}
		panel->label_text[len] = 0;
	}
	
	UI_End_Panel(ui);
}


// .............................................................................................
void
UI_Input_Init(UI_Input *input)
{
	*input = {};
	input->mouse_x = 0;
	input->mouse_y = 0;
	input->mouse_x_prev = 0;
	input->mouse_y_prev = 0;
	input->last_char = 0;
}


// .............................................................................................
void
UI_Input_NewFrame(UI_Context *ui)
{
	// Calculate mouse delta (input_prev set by UI_Input_EndFrame from last frame)
	ui->input.mouse_dx = ui->input.mouse_x - ui->input_prev.mouse_x;
	ui->input.mouse_dy = ui->input.mouse_y - ui->input_prev.mouse_y;
	
	// Update pressed/released flags
	for (int i = 0; i < UI_MOUSE_BUTTON_COUNT; i++) {
		ui->input.mouse_pressed[i] = ui->input.mouse_down[i] && !ui->input_prev.mouse_down[i];
		ui->input.mouse_released[i] = !ui->input.mouse_down[i] && ui->input_prev.mouse_down[i];
	}
	
	for (int i = 0; i < UI_KEY_COUNT; i++) {
		ui->input.key_pressed[i] = ui->input.key_down[i] && !ui->input_prev.key_down[i];
		ui->input.key_released[i] = !ui->input.key_down[i] && ui->input_prev.key_down[i];
	}
	
	// Clear per-frame data
	ui->input.char_count = 0;
	ui->input.mouse_wheel_delta = 0.0f;
	
	// Update modifier keys (VK_CONTROL, VK_SHIFT, VK_MENU are Windows constants)
	ui->input.ctrl = ui->input.key_down[0x11];   // VK_CONTROL
	ui->input.shift = ui->input.key_down[0x10];  // VK_SHIFT
	ui->input.alt = ui->input.key_down[0x12];    // VK_MENU (Alt)
}


// .............................................................................................
void
UI_Input_EndFrame(UI_Context *ui)
{
	// Copy current state to previous for next frame's edge detection
	ui->input_prev = ui->input;
}


// .............................................................................................
void
UI_Input_ProcessMouseMove(UI_Context *ui, int x, int y)
{
	ui->input.mouse_x = x;
	ui->input.mouse_y = y;
}


// .............................................................................................
void
UI_Input_ProcessMouseButton(UI_Context *ui, UI_Mouse_Button button, int down)
{
	if (button >= 0 && button < UI_MOUSE_BUTTON_COUNT) {
		ui->input.mouse_down[button] = down;
	}
}


// .............................................................................................
void
UI_Input_ProcessMouseWheel(UI_Context *ui, float delta)
{
	ui->input.mouse_wheel_delta += delta;
}


// .............................................................................................
void
UI_Input_ProcessKey(UI_Context *ui, int vk_code, int down)
{
	if (vk_code >= 0 && vk_code < UI_KEY_COUNT) {
		ui->input.key_down[vk_code] = down;
	}
}


// .............................................................................................
void
UI_Input_ProcessChar(UI_Context *ui, char c)
{
	if (ui->input.char_count < 32) {
		ui->input.char_buffer[ui->input.char_count++] = c;
	}
	ui->input.last_char = c;
}


// .............................................................................................
int
UI_Is_Mouse_Down(UI_Context *ui, UI_Mouse_Button button)
{
	return (button >= 0 && button < UI_MOUSE_BUTTON_COUNT) 
	       ? ui->input.mouse_down[button] 
	       : 0;
}


// .............................................................................................
int
UI_Is_Mouse_Pressed(UI_Context *ui, UI_Mouse_Button button)
{
	return (button >= 0 && button < UI_MOUSE_BUTTON_COUNT) 
	       ? ui->input.mouse_pressed[button] 
	       : 0;
}


// .............................................................................................
int
UI_Is_Mouse_Released(UI_Context *ui, UI_Mouse_Button button)
{
	return (button >= 0 && button < UI_MOUSE_BUTTON_COUNT) 
	       ? ui->input.mouse_released[button] 
	       : 0;
}


// .............................................................................................
int
UI_Is_Key_Down(UI_Context *ui, int vk_code)
{
	return (vk_code >= 0 && vk_code < UI_KEY_COUNT) 
	       ? ui->input.key_down[vk_code] 
	       : 0;
}


// .............................................................................................
int
UI_Is_Key_Pressed(UI_Context *ui, int vk_code)
{
	return (vk_code >= 0 && vk_code < UI_KEY_COUNT) 
	       ? ui->input.key_pressed[vk_code] 
	       : 0;
}


// .............................................................................................
int
UI_Is_Key_Released(UI_Context *ui, int vk_code)
{
	return (vk_code >= 0 && vk_code < UI_KEY_COUNT) 
	       ? ui->input.key_released[vk_code] 
	       : 0;
}


// .............................................................................................
void
UI_Get_Mouse_Pos(UI_Context *ui, int *out_x, int *out_y)
{
	if (out_x) *out_x = ui->input.mouse_x;
	if (out_y) *out_y = ui->input.mouse_y;
}


// .............................................................................................
void
UI_Get_Mouse_Delta(UI_Context *ui, int *out_dx, int *out_dy)
{
	if (out_dx) *out_dx = ui->input.mouse_dx;
	if (out_dy) *out_dy = ui->input.mouse_dy;
}


// .............................................................................................
float
UI_Get_Mouse_Wheel(UI_Context *ui)
{
	return ui->input.mouse_wheel_delta;
}


// .............................................................................................
int
UI_Is_Point_In_Rect(int x, int y, UI_RectI rect)
{
	return x >= rect.x && x < (rect.x + rect.w) &&
	       y >= rect.y && y < (rect.y + rect.h);
}


// .............................................................................................
int
UI_Is_Hovered(UI_Context *ui, UI_RectI rect)
{
	return UI_Is_Point_In_Rect(ui->input.mouse_x, ui->input.mouse_y, rect);
}


// .............................................................................................
int
UI_Is_Widget_Hot(UI_Context *ui, UI_Id id)
{
	return ui->interaction.hot_widget == id;
}


// .............................................................................................
int
UI_Is_Widget_Active(UI_Context *ui, UI_Id id)
{
	return ui->interaction.active_widget == id;
}


// .............................................................................................
void
UI_Set_Hot_Widget(UI_Context *ui, UI_Id id)
{
	ui->interaction.hot_widget = id;
}


// .............................................................................................
void
UI_Set_Active_Widget(UI_Context *ui, UI_Id id)
{
	ui->interaction.active_widget = id;
}


// .............................................................................................
void
UI_Clear_Active_Widget(UI_Context *ui)
{
	ui->interaction.active_widget = 0;
}


// .............................................................................................
void
UI_Set_Size_Override(UI_Context *ui, UI_Id panel_id, int pref_w, int pref_h)
{
	// Check if override already exists
	for (int i = 0; i < ui->size_override_count; i++) {
		if (ui->size_overrides[i].panel_id == panel_id) {
			// Update existing override
			if (pref_w >= 0) ui->size_overrides[i].pref_w = pref_w;
			if (pref_h >= 0) ui->size_overrides[i].pref_h = pref_h;
			return;
		}
	}
	
	// Add new override
	if (ui->size_override_count < 32) {
		ui->size_overrides[ui->size_override_count].panel_id = panel_id;
		ui->size_overrides[ui->size_override_count].pref_w = pref_w;
		ui->size_overrides[ui->size_override_count].pref_h = pref_h;
		ui->size_override_count++;
	}
}


// .............................................................................................
int
UI_Get_Size_Override_W(UI_Context *ui, UI_Id panel_id)
{
	for (int i = 0; i < ui->size_override_count; i++) {
		if (ui->size_overrides[i].panel_id == panel_id) {
			return ui->size_overrides[i].pref_w;
		}
	}
	return -1;  // No override found
}


// .............................................................................................
int
UI_Get_Size_Override_H(UI_Context *ui, UI_Id panel_id)
{
	for (int i = 0; i < ui->size_override_count; i++) {
		if (ui->size_overrides[i].panel_id == panel_id) {
			return ui->size_overrides[i].pref_h;
		}
	}
	return -1;  // No override found
}


// .............................................................................................
static int
UI_Find_Panel_By_Id(UI_State *s, UI_Id id)
{
	for (int i = 0; i < s->panel_count; i++) {
		if (s->panels[i].id == id) {
			return i;
		}
	}
	return -1;
}


// .............................................................................................
static int
UI_Get_Resize_Direction(UI_State *s, int panel_idx)
{
	UI_Panel *p = &s->panels[panel_idx];
	
	// If no parent, cannot resize
	if (p->parent < 0) return -1;
	
	UI_Panel *parent = &s->panels[p->parent];
	
	// Resize direction is perpendicular to parent's layout direction
	// If parent is ROW (horizontal), dividers resize horizontally (0)
	// If parent is COLUMN (vertical), dividers resize vertically (1)
	return parent->style.direction;
}


// .............................................................................................
static UI_RectI
UI_Get_Expanded_Rect(UI_RectI rect, int padding)
{
	return UI_RectI{
		rect.x - padding,
		rect.y - padding,
		rect.w + padding * 2,
		rect.h + padding * 2
	};
}


// .............................................................................................
static int
UI_Find_Adjacent_Panel(UI_State *s, int divider_idx)
{
	UI_Panel *divider = &s->panels[divider_idx];
	
	// Find the panel immediately before this divider in parent's child list
	if (divider->parent < 0) return -1;
	
	UI_Panel *parent = &s->panels[divider->parent];
	int prev_idx = -1;
	
	for (int c = parent->first_child; c != -1; c = s->panels[c].next_sibling) {
		if (c == divider_idx) {
			return prev_idx;  // Return previous sibling
		}
		prev_idx = c;
	}
	
	return -1;
}


// .............................................................................................
static int
UI_Find_Next_Panel(UI_State *s, int divider_idx)
{
	UI_Panel *divider = &s->panels[divider_idx];
	
	// Return the panel immediately AFTER this divider in sibling list
	return divider->next_sibling;
}


// .............................................................................................
static void
UI_Update_Divider_Resize(UI_Context *ui)
{
	if (ui->interaction.dragging_divider == 0) return;
	
	// Look up both panels by ID each frame (indices are not stable in immediate-mode)
	int left_idx = -1, right_idx = -1;
	
	if (ui->interaction.resize_target_left_id != 0) {
		left_idx = UI_Find_Panel_By_Id(&ui->state, ui->interaction.resize_target_left_id);
	}
	if (ui->interaction.resize_target_right_id != 0) {
		right_idx = UI_Find_Panel_By_Id(&ui->state, ui->interaction.resize_target_right_id);
	}
	
	// Need at least one panel to resize
	if (left_idx < 0 && right_idx < 0) return;
	
	int divider_idx = UI_Find_Panel_By_Id(&ui->state, ui->interaction.dragging_divider);
	if (divider_idx < 0) return;
	
	// Get resize direction (0=horizontal, 1=vertical)
	int resize_dir = UI_Get_Resize_Direction(&ui->state, divider_idx);
	if (resize_dir < 0) return;
	
	// Calculate delta from drag start
	int current_pos = (resize_dir == 0) ? ui->input.mouse_x : ui->input.mouse_y;
	int delta = current_pos - ui->interaction.drag_start_pos;
	
	// Calculate new sizes (zero-sum: left grows/shrinks by +delta, right by -delta)
	int new_left_size = ui->interaction.drag_start_size_left + delta;
	int new_right_size = ui->interaction.drag_start_size_right - delta;
	
	// Apply left panel constraints and recalculate delta if needed
	if (left_idx >= 0) {
		UI_Panel *left = &ui->state.panels[left_idx];
		int min = (resize_dir == 0) ? left->style.min_w : left->style.min_h;
		int max = (resize_dir == 0) ? left->style.max_w : left->style.max_h;
		
		if (new_left_size < min) {
			// Left panel hit minimum - recalculate delta
			delta = min - ui->interaction.drag_start_size_left;
			new_left_size = min;
			new_right_size = ui->interaction.drag_start_size_right - delta;
		} else if (new_left_size > max) {
			// Left panel hit maximum - recalculate delta
			delta = max - ui->interaction.drag_start_size_left;
			new_left_size = max;
			new_right_size = ui->interaction.drag_start_size_right - delta;
		}
	}
	
	// Apply right panel constraints and recalculate delta if needed
	if (right_idx >= 0) {
		UI_Panel *right = &ui->state.panels[right_idx];
		int min = (resize_dir == 0) ? right->style.min_w : right->style.min_h;
		int max = (resize_dir == 0) ? right->style.max_w : right->style.max_h;
		
		if (new_right_size < min) {
			// Right panel hit minimum - recalculate delta from right's perspective
			delta = ui->interaction.drag_start_size_right - min;
			new_right_size = min;
			new_left_size = ui->interaction.drag_start_size_left + delta;
		} else if (new_right_size > max) {
			// Right panel hit maximum - recalculate delta from right's perspective
			delta = ui->interaction.drag_start_size_right - max;
			new_right_size = max;
			new_left_size = ui->interaction.drag_start_size_left + delta;
		}
	}
	
	// Apply final sizes to both panels
	if (resize_dir == 0) {
		// Horizontal resize
		if (left_idx >= 0) {
			UI_Panel *left = &ui->state.panels[left_idx];
			UI_Set_Size_Override(ui, ui->interaction.resize_target_left_id, new_left_size, -1);
			left->style.pref_w = new_left_size;
			left->style.flex_grow = 0.0f;  // Convert from flex to fixed
		}
		if (right_idx >= 0) {
			UI_Panel *right = &ui->state.panels[right_idx];
			UI_Set_Size_Override(ui, ui->interaction.resize_target_right_id, new_right_size, -1);
			right->style.pref_w = new_right_size;
			right->style.flex_grow = 0.0f;  // Convert from flex to fixed
		}
	} else {
		// Vertical resize
		if (left_idx >= 0) {
			UI_Panel *left = &ui->state.panels[left_idx];
			UI_Set_Size_Override(ui, ui->interaction.resize_target_left_id, -1, new_left_size);
			left->style.pref_h = new_left_size;
			left->style.flex_grow = 0.0f;  // Convert from flex to fixed
		}
		if (right_idx >= 0) {
			UI_Panel *right = &ui->state.panels[right_idx];
			UI_Set_Size_Override(ui, ui->interaction.resize_target_right_id, -1, new_right_size);
			right->style.pref_h = new_right_size;
			right->style.flex_grow = 0.0f;  // Convert from flex to fixed
		}
	}
}


// .............................................................................................
static void
UI_Update_Panel_Interaction_NonResizable(UI_Context *ui, UI_State *s, int panel_idx)
{
	UI_Panel *p = &s->panels[panel_idx];
	
	// Check non-resizable panels only (skip labels and resizable dividers)
	if (!p->is_label && !p->style.resizable) {
		if (UI_Is_Hovered(ui, p->rect)) {
			UI_Set_Hot_Widget(ui, p->id);
		}
	}
	
	// Recurse into children
	for (int c = p->first_child; c != -1; c = s->panels[c].next_sibling) {
		UI_Update_Panel_Interaction_NonResizable(ui, s, c);
	}
}


// .............................................................................................
static void
UI_Update_Panel_Interaction_Resizable(UI_Context *ui, UI_State *s, int panel_idx)
{
	UI_Panel *p = &s->panels[panel_idx];
	
	// Check resizable panels with expanded hitbox (highest priority)
	if (!p->is_label && p->style.resizable) {
		UI_RectI check_rect = UI_Get_Expanded_Rect(p->rect, p->style.resize_hitbox_padding);
		
		if (UI_Is_Hovered(ui, check_rect)) {
			UI_Set_Hot_Widget(ui, p->id);
		}
	}
	
	// Recurse into children
	for (int c = p->first_child; c != -1; c = s->panels[c].next_sibling) {
		UI_Update_Panel_Interaction_Resizable(ui, s, c);
	}
}


// .............................................................................................
void
UI_Update_Interaction(UI_Context *ui)
{
	// Update divider resize if dragging
	UI_Update_Divider_Resize(ui);
	
	// Clear hot widget
	ui->interaction.hot_widget = 0;
	
	// Two-pass system: check all non-resizable panels first, then all resizable panels
	// This ensures resizable dividers always have highest priority when their expanded hitbox is hovered
	if (ui->state.panel_count > 0) {
		// PASS 1: Check all non-resizable panels (buttons, sidebars, content areas)
		UI_Update_Panel_Interaction_NonResizable(ui, &ui->state, 0);
		
		// PASS 2: Check all resizable panels with expanded hitbox (dividers win conflicts)
		UI_Update_Panel_Interaction_Resizable(ui, &ui->state, 0);
	}
	
	// Handle drag start
	if (UI_Is_Mouse_Pressed(ui, UI_MOUSE_LEFT)) {
		if (ui->interaction.hot_widget != 0) {
			// Check if hot widget is a resizable divider
			int hot_idx = UI_Find_Panel_By_Id(&ui->state, ui->interaction.hot_widget);
			if (hot_idx >= 0 && ui->state.panels[hot_idx].style.resizable) {
				// Start dragging divider
				ui->interaction.dragging_divider = ui->interaction.hot_widget;
				
				// Find BOTH adjacent panels (left and right of divider)
				int left_idx = UI_Find_Adjacent_Panel(&ui->state, hot_idx);
				int right_idx = UI_Find_Next_Panel(&ui->state, hot_idx);
				
				// Get resize direction (0=horizontal, 1=vertical)
				int resize_dir = UI_Get_Resize_Direction(&ui->state, hot_idx);
				
				// Store drag start mouse position
				ui->interaction.drag_start_pos = (resize_dir == 0) 
					? ui->input.mouse_x 
					: ui->input.mouse_y;
				
				// Store left panel info (if exists)
				if (left_idx >= 0) {
					UI_Panel *left = &ui->state.panels[left_idx];
					ui->interaction.resize_target_left_id = left->id;
					// Use actual computed size (not pref_w/h which might be -1 for flex)
					ui->interaction.drag_start_size_left = (resize_dir == 0) 
						? left->rect.w 
						: left->rect.h;
				} else {
					ui->interaction.resize_target_left_id = 0;
					ui->interaction.drag_start_size_left = 0;
				}
				
				// Store right panel info (if exists)
				if (right_idx >= 0) {
					UI_Panel *right = &ui->state.panels[right_idx];
					ui->interaction.resize_target_right_id = right->id;
					// Use actual computed size (not pref_w/h which might be -1 for flex)
					ui->interaction.drag_start_size_right = (resize_dir == 0) 
						? right->rect.w 
						: right->rect.h;
				} else {
					ui->interaction.resize_target_right_id = 0;
					ui->interaction.drag_start_size_right = 0;
				}
			} else {
				// Normal widget activation
				UI_Set_Active_Widget(ui, ui->interaction.hot_widget);
			}
		}
	}
	
	// Handle drag end
	if (UI_Is_Mouse_Released(ui, UI_MOUSE_LEFT)) {
		if (ui->interaction.dragging_divider != 0) {
			// Stop dragging
			ui->interaction.dragging_divider = 0;
			ui->interaction.resize_target_left_id = 0;
			ui->interaction.resize_target_right_id = 0;
			ui->interaction.drag_start_size_left = 0;
			ui->interaction.drag_start_size_right = 0;
			ui->interaction.drag_start_pos = 0;
		} else {
			// Normal widget deactivation
			UI_Clear_Active_Widget(ui);
		}
	}
}


// .............................................................................................
int
UI_Button(UI_Context *ui, const char *text)
{
	// Generate unique ID for button
	UI_Id id = UI_Generate_Id(ui, text);
	
	// Get button state from PREVIOUS frame
	int is_hot = UI_Is_Widget_Hot(ui, id);
	int is_active = (ui->interaction.active_widget_prev == id);
	
	// Determine if button was clicked
	int clicked = 0;
	if (is_active && UI_Is_Mouse_Released(ui, UI_MOUSE_LEFT)) {
		if (is_hot) {
			clicked = 1;
			
			// Track last button clicked
			int len = 0;
			while (len < MAX_UI_TEXT_LENGTH - 1 && text[len]) {
				ui->last_button_clicked[len] = text[len];
				len++;
			}
			ui->last_button_clicked[len] = 0;
		}
	}
	
	// Visual state
	uint32_t bg_color;
	uint32_t text_color;
	
	if (is_active) {
		bg_color = 0xFF1A5FB4;  // Pressed (darker blue)
		text_color = 0xFFFFFFFF;
	} else if (is_hot) {
		bg_color = 0xFF3584E4;  // Hovered (medium blue)
		text_color = 0xFFFFFFFF;
	} else {
		bg_color = 0xFF2A2A2E;  // Normal (dark gray)
		text_color = 0xFFAAAAAA;
	}
	
	// Create button panel with same ID
	UI_Begin_Panel_With_Id(ui, id, text);
	UI_Panel_Set_Size(ui, -1, 30);
	UI_Panel_Set_Color(ui, bg_color);
	UI_Panel_Set_Padding(ui, 8, 4, 8, 4);
	UI_Panel_Set_Direction(ui, UI_DIRECTION_ROW);
		UI_Label(ui, text, text_color);
	UI_End_Panel(ui);
	
	return clicked;
}


// .............................................................................................
void
UI_Debug_Mouse_Overlay(UI_Context *ui)
{
	// Build line 1 - input & timing state
	char line1[512];
	snprintf(line1, sizeof(line1), 
	         "Frame:%d %.2fms | %d FPS | Mouse:(%d,%d) | Down L:%d R:%d M:%d | Press L:%d R:%d M:%d | Release L:%d R:%d M:%d | Char:'%c'",
	         ui->frame_number,
	         ui->delta_time_ms,
	         ui->current_fps,
	         ui->input.mouse_x, ui->input.mouse_y,
	         ui->input.mouse_down[UI_MOUSE_LEFT],
	         ui->input.mouse_down[UI_MOUSE_RIGHT],
	         ui->input.mouse_down[UI_MOUSE_MIDDLE],
	         ui->input.mouse_pressed[UI_MOUSE_LEFT],
	         ui->input.mouse_pressed[UI_MOUSE_RIGHT],
	         ui->input.mouse_pressed[UI_MOUSE_MIDDLE],
	         ui->input.mouse_released[UI_MOUSE_LEFT],
	         ui->input.mouse_released[UI_MOUSE_RIGHT],
	         ui->input.mouse_released[UI_MOUSE_MIDDLE],
	         ui->input.last_char ? ui->input.last_char : ' '
	);
	
	// Build line 2 - widget interaction state
	char line2[512];
	snprintf(line2, sizeof(line2), 
	         "Hot:%d Active:%d | Drag:%d | L:%d(sz:%d) R:%d(sz:%d) Pos:%d | Last:\"%s\"",
	         ui->interaction.hot_widget,
	         ui->interaction.active_widget,
	         ui->interaction.dragging_divider,
	         ui->interaction.resize_target_left_id,
	         ui->interaction.drag_start_size_left,
	         ui->interaction.resize_target_right_id,
	         ui->interaction.drag_start_size_right,
	         ui->interaction.drag_start_pos,
	         ui->last_button_clicked[0] ? ui->last_button_clicked : "None"
	);
	
	// Measure both lines
	UI_RectI size1 = ui->measure_text ? ui->measure_text(line1, 14) : UI_RectI{0, 0, 800, 20};
	UI_RectI size2 = ui->measure_text ? ui->measure_text(line2, 14) : UI_RectI{0, 0, 800, 20};
	
	// Use max width for panel
	int max_width = (size1.w > size2.w) ? size1.w : size2.w;
	int total_height = size1.h + size2.h + 2;  // +2 for gap between lines
	
	// Create debug overlay panel (COLUMN direction for vertical stacking)
	UI_Begin_Panel(ui, "##debug_overlay");
	UI_Panel_Set_Size(ui, max_width + 16, total_height + 8);
	UI_Panel_Set_Color(ui, 0xEE000000);
	UI_Panel_Set_Padding(ui, 8, 4, 8, 4);
	UI_Panel_Set_Direction(ui, UI_DIRECTION_COLUMN);
	UI_Panel_Set_Gap(ui, 2);
		UI_Label(ui, line1, 0xFF00FF00);
		UI_Label(ui, line2, 0xFF00FF00);
	UI_End_Panel(ui);
}


// Include user UI for unity build
#include "app_ui.cpp"

