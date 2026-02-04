# Direct2D UI Template

A minimal Windows Direct2D application template with a custom immediate-mode UI system. Perfect for quickly prototyping tools, editors, and desktop applications with hardware-accelerated 2D graphics.

## Features

- ✨ **Immediate-Mode UI** - Rebuild UI tree every frame with simple, declarative API
- 📐 **Flexbox-Inspired Layout** - Row/column directions with flex-grow support
- 🔀 **Resizable Dividers** - Zero-sum constraint-aware resizing with persistent sizing
- 🖱️ **Full Input System** - Mouse, keyboard, and character input with edge detection
- 🎨 **Hardware Accelerated** - Direct2D rendering at 120 FPS
- 🔤 **UTF-8 Text Support** - DirectWrite text rendering with automatic sizing
- 📦 **Zero Dependencies** - Only Windows SDK required (no external libraries)
- 🚀 **~2600 Lines** - Small, readable codebase perfect for learning

## Screenshots

*(UI shows a 3-column resizable layout with buttons, labels, and a debug overlay)*

```
┌─────────────────────────────────────────────────────────────┐
│  [Left Sidebar]  │  [Main Content Area]  │  [Right Sidebar] │
│    Actions       │    Top Section        │    Properties    │
│   [Save]         │   [Click Me!]         │   Width: 1920    │
│   [Load]         │                       │   Height: 1080   │
│   [Reset]        │  ─────────────        │   FPS: 120       │
│                  │    Bottom Section     │                  │
│    Settings      │                       │                  │
│    Graphics      │                       │                  │
│    Audio         │                       │                  │
└─────────────────────────────────────────────────────────────┘
```

## Quick Start

### Prerequisites

- **Windows 10 or later**
- **Visual Studio 2022** (Community Edition or higher)
- **Windows SDK** (included with Visual Studio)

### Build & Run

```bash
# Clone the repository
git clone <your-repo-url>
cd Direct2D_ProjectTemplate

# Build the application
cd src
build.bat

# Run the executable
cd build
application.exe
```

The application window will open maximized with a resizable 3-column layout.

## Project Structure

```
Direct2D_ProjectTemplate/
├── README.md              # This file
├── AGENTS.md             # Developer guide (code style, architecture)
├── LICENSE               # License file
└── src/
    ├── ui.h              # UI library interface (~270 lines)
    ├── ui.cpp            # UI library implementation (~1450 lines)
    ├── app_ui.h          # User UI definition header
    ├── app_ui.cpp        # User UI definition (~130 lines)
    ├── application.cpp   # Application entry point (~700 lines)
    ├── build.bat         # Build script
    └── build/            # Build output (not in git)
```

## API Examples

### Simple Panel Layout

```c
// Create a column layout with buttons
UI_Begin_Panel(ui, "sidebar");
UI_Panel_Set_Direction(ui, UI_DIRECTION_COLUMN);
UI_Panel_Set_Padding(ui, 12, 12, 12, 12);
UI_Panel_Set_Gap(ui, 8);
UI_Panel_Set_Color(ui, 0xFF22222A);
{
    UI_Label(ui, "Actions", 0xFFFFFFFF);
    
    if (UI_Button(ui, "Save")) {
        // Handle save button click
    }
    
    if (UI_Button(ui, "Load")) {
        // Handle load button click
    }
}
UI_End_Panel(ui);
```

### Compact Helper API

```c
// Same layout with helper function (one line!)
UI_BeginPanel(ui, "sidebar", UI_DIRECTION_COLUMN, 240, -1, 12, 8, 0xFF22222A);
{
    UI_Label(ui, "Actions", 0xFFFFFFFF);
    if (UI_Button(ui, "Save")) { /* ... */ }
    if (UI_Button(ui, "Load")) { /* ... */ }
}
UI_End_Panel(ui);
```

### Resizable Panels

```c
// Panel with persistent resizing
UI_Panel_Resizable(ui, "left", UI_DIRECTION_COLUMN, 240, -1, 12, 8, 0xFF22222A);
{
    // Content automatically adjusts when user drags divider
}
UI_End_Panel(ui);

// Divider between panels
UI_Divider(ui, "divider", UI_DIVIDER_VERTICAL);
```

## Customization

### Modify the Demo UI

Edit `src/app_ui.cpp` to create your own UI layout. The file is well-commented and shows examples of all features.

### Add New Widgets

Widgets are implemented in `src/ui.cpp`. See `UI_Button()` and `UI_Label()` for examples. The pattern:
1. Generate unique ID
2. Check interaction state (hot/active)
3. Build UI panels
4. Return interaction result

### Change Rendering

The rendering loop is in `src/application.cpp`. Modify `Render_UI()` and `Render_UI_Text()` to customize how rectangles and text are drawn.

## Architecture

### Immediate-Mode UI

The UI rebuilds its entire tree every frame:

```c
// Every frame:
UI_BeginFrame(&ctx, &list, width, height);  // 1. Start frame
App_UI_Build(&ctx);                         // 2. Build UI tree
UI_LayoutPanelTree(&ctx.state, 0);          // 3. Calculate layout
UI_Update_Interaction(&ctx);                // 4. Process input
UI_EmitPanels(&ctx.state, 0);               // 5. Generate render list
// ... render list with Direct2D ...        // 6. Draw to screen
UI_Input_EndFrame(&ctx);                    // 7. End frame
```

### Flexbox Layout

Panels use a flexbox-inspired two-pass layout algorithm:

1. **Pass 1:** Calculate fixed sizes and sum flex-grow factors
2. **Pass 2:** Distribute remaining space proportionally

Children can be:
- **Fixed size:** `UI_Panel_Set_Size(ui, 240, -1)` → 240px wide, auto height
- **Flex-grow:** `UI_Panel_Set_Grow(ui, 1.0f)` → Fill available space
- **Auto:** Default behavior, size based on content

### Frame Pacing

Runs at 120 FPS with precise timing using busy-wait:

```c
while (g_is_running) {
    Render(window);                  // Render frame
    Wait_For_Target_Frame_Time();    // Wait for ~8.33ms (120 FPS)
}
```

## Performance

- **Rendering:** 120 FPS continuous rendering (capped)
- **Panel Capacity:** Up to 1024 panels per frame
- **Render Primitives:** 256 rectangles + 256 text elements per frame
- **Lookup Complexity:** O(n) panel lookups (acceptable for <100 panels)

For applications with >100 panels or tight performance requirements, see the optimization section in `AGENTS.md`.

## Development

### Build Configuration

Debug build only (see `src/build.bat`):

```batch
cl /Zi /Od /W4 ..\application.cpp d2d1.lib dwrite.lib user32.lib
```

Flags:
- `/Zi` - Generate debug info (PDB files)
- `/Od` - Disable optimizations
- `/W4` - Warning level 4 (all warnings)

### Unity Build

The project uses a unity build pattern for fast compilation:
- `application.cpp` includes `ui.cpp`
- `ui.cpp` includes `app_ui.cpp`
- Only `application.cpp` is compiled

### Code Style

- **Naming:** `UI_Verb_Noun` for functions, `snake_case` for variables, `g_` prefix for globals
- **Indentation:** Tabs (not spaces)
- **Braces:** K&R style (opening brace on same line for control structures, new line for functions)
- **Comments:** Explain "why", not "what" (code should be self-documenting)

See `AGENTS.md` for complete code style guidelines.

## Roadmap

Potential future enhancements:

- [ ] Text input widget (text box with cursor, selection)
- [ ] Scroll containers (vertical/horizontal scrolling)
- [ ] Checkbox and radio button widgets
- [ ] Dropdown/combo box widget
- [ ] Context menus (right-click menus)
- [ ] Tooltips (hover text)
- [ ] Drag & drop support
- [ ] Custom styling/theming system
- [ ] Performance optimizations (panel lookup caching)
- [ ] Release build configuration

## Known Limitations

- **Windows Only:** Uses Win32 API and Direct2D (not portable)
- **Debug Build Only:** No release configuration yet
- **No Clipping:** Panels can render outside bounds (use nested layouts)
- **Limited Widgets:** Only buttons and labels (extend as needed)
- **No Accessibility:** No screen reader support

## Contributing

This is a template project. Feel free to:
- Fork and customize for your own projects
- Submit issues for bugs or suggestions
- Share improvements via pull requests

## License

See `LICENSE` file for details.

## Acknowledgments

- **Direct2D/DirectWrite:** Microsoft's hardware-accelerated 2D graphics API
- **Immediate-Mode GUI:** Inspired by Dear ImGui and similar libraries
- **Flexbox Layout:** Layout algorithm inspired by CSS Flexbox

---

**Questions? Issues?** Check `AGENTS.md` for detailed documentation or open an issue on GitHub.
