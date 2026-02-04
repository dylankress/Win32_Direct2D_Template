// app_ui.cpp
#include "app_ui.h"

// Forward declarations
static void App_Sidebar_Left(UI_Context *ui);
static void App_Content_Area(UI_Context *ui);
static void App_Sidebar_Right(UI_Context *ui);
static void App_Content_Top(UI_Context *ui);
static void App_Content_Bottom(UI_Context *ui);


// .............................................................................................
// Main UI builder - called every frame
void
App_UI_Build(UI_Context *ui)
{
	// Root panel
	UI_BeginPanel(ui, "root", UI_DIRECTION_COLUMN, -1, -1, 8, 0, 0xFF111115);
	{
		// Main content area (3-column layout)
		UI_BeginPanel(ui, "main", UI_DIRECTION_ROW, -1, -1, 0, 0, 0);
		UI_Panel_Set_Grow(ui, 1.0f);
		{
			App_Sidebar_Left(ui);
			UI_Divider(ui, "left_div", UI_DIVIDER_VERTICAL);
			App_Content_Area(ui);
			UI_Divider(ui, "right_div", UI_DIVIDER_VERTICAL);
			App_Sidebar_Right(ui);
		}
		UI_End_Panel(ui);
		
		// Debug overlay
		UI_Debug_Mouse_Overlay(ui);
	}
	UI_End_Panel(ui);
}


// .............................................................................................
static void
App_Sidebar_Left(UI_Context *ui)
{
	UI_Panel_Resizable(ui, "left", UI_DIRECTION_COLUMN, 240, -1, 12, 8, 0xFF22222A);
	{
		UI_Label(ui, "Actions", 0xFFFFFFFF);
		
		if (UI_Button(ui, "Save")) {
			// Save button clicked
		}
		
		if (UI_Button(ui, "Load")) {
			// Load button clicked
		}
		
		if (UI_Button(ui, "Reset")) {
			// Reset button clicked
		}
		
		UI_Label(ui, "", 0xFF000000);  // Spacer
		UI_Label(ui, "Settings", 0xFFFFFFFF);
		UI_Label(ui, "Graphics", 0xFFAAAAAA);
		UI_Label(ui, "Audio", 0xFFAAAAAA);
	}
	UI_End_Panel(ui);
}


// .............................................................................................
static void
App_Content_Area(UI_Context *ui)
{
	UI_Panel_Resizable(ui, "mid", UI_DIRECTION_COLUMN, -2, -1, 8, 8, 0xFF1A1A20);
	{
		UI_Label(ui, "Main Content Area", 0xFFFFFFFF);
		
		App_Content_Top(ui);
		UI_Divider(ui, "mid_div", UI_DIVIDER_HORIZONTAL);
		App_Content_Bottom(ui);
	}
	UI_End_Panel(ui);
}


// .............................................................................................
static void
App_Content_Top(UI_Context *ui)
{
	UI_Panel_Resizable(ui, "mid_top", UI_DIRECTION_COLUMN, -2, -2, 12, 8, 0x44FFFFFF);
	{
		UI_Label(ui, "Top Section", 0xFFFFFFFF);
		
		if (UI_Button(ui, "Click Me!")) {
			// Button clicked
		}
		
		UI_Label(ui, "Hover over buttons to see effects", 0xFFAAAAAA);
	}
	UI_End_Panel(ui);
}


// .............................................................................................
static void
App_Content_Bottom(UI_Context *ui)
{
	UI_Panel_Resizable(ui, "mid_bottom", UI_DIRECTION_COLUMN, -2, -2, 12, 4, 0x44FFFFFF);
	{
		UI_Label(ui, "Bottom Section", 0xFFFFFFFF);
		UI_Label(ui, "Press mouse buttons and watch the debug overlay", 0xFFAAAAAA);
	}
	UI_End_Panel(ui);
}


// .............................................................................................
static void
App_Sidebar_Right(UI_Context *ui)
{
	UI_Panel_Resizable(ui, "right", UI_DIRECTION_COLUMN, 320, -1, 12, 4, 0xFF22222A);
	{
		UI_Label(ui, "Properties", 0xFFFFFFFF);
		UI_Label(ui, "Width: 1920", 0xFFAAAAAA);
		UI_Label(ui, "Height: 1080", 0xFFAAAAAA);
		UI_Label(ui, "FPS: 60", 0xFFAAAAAA);
	}
	UI_End_Panel(ui);
}
