# Agent Guidelines for Direct2D Project Template

**IMPORTANT:** This file must be kept synchronized with code changes. Update line counts, API surface, and examples whenever refactoring.

This document provides coding agents with essential information about this codebase's build system, code style, and conventions.

## Project Overview

This is a minimal Windows Direct2D application template with a custom immediate-mode UI system. The codebase is intentionally simple (~2874 lines total) with no external dependencies beyond Windows SDK.

**Current Line Counts (as of last update):**
- `ui.h`: 331 lines (UI library interface)
- `ui.cpp`: 1539 lines (UI library implementation)
- `app_ui.h`: 10 lines (User UI header)
- `app_ui.cpp`: 147 lines (User UI implementation - demo)
- `application.cpp`: 837 lines (Application entry point)
- `build.bat`: 10 lines (Build script)
- **Total**: 2874 lines

**Key Technologies:**
- Language: C++ (Windows-specific)
- Graphics: Direct2D (hardware-accelerated 2D rendering)
- Platform: Windows (Win32 API)
- Compiler: MSVC (`cl.exe`)

## Build Commands

### Building the Project
```bash
cd src
build.bat
```

This runs: `cl /Zi /Od /W4 ..\application.cpp d2d1.lib dwrite.lib user32.lib`

**Compiler Flags:**
- `/Zi` - Generate debug information (PDB files)
- `/Od` - Disable optimizations (debug build)
- `/W4` - Warning level 4 (high)

**Linked Libraries:**
- `d2d1.lib` - Direct2D graphics
- `dwrite.lib` - DirectWrite text rendering
- `user32.lib` - Windows User32 API

**Output Location:** `src/build/application.exe`

### Running the Application
```bash
cd src/build
./application.exe
```

### Testing
**No test framework exists.** Tests would need to be added from scratch.

### Linting
**No linter configured.** Consider adding `clang-format` or `clang-tidy` if needed.

## Code Style Guidelines

### Naming Conventions

**Functions:**
- `PascalCase_With_Underscores`: `UI_BeginFrame()`, `UI_NewPanel()`, `Render_UI()`
- Full PascalCase for callbacks: `MainWindowCallback()`
- Use descriptive names that indicate purpose

**Variables:**
- `snake_case`: `is_running`, `render_list`, `window_class`, `panel_count`
- Pointer prefix `p_`: `p_d2d_factory`, `p_render_target`, `p_brush`
- Internal globals prefix `g_`: `g_render_list`
- Loop counters: single letter (`i`, `c`)

**Types/Structs:**
- `PascalCase_With_Underscores`: `UI_Rectangle`, `UI_Render_List`, `UI_Context`
- Typedef example: `UI_Id` (for `int32_t`)

**Constants/Defines:**
- `SCREAMING_SNAKE_CASE`: `MAX_UI_RECTANGLES`

### Formatting

**Indentation:**
- Use **TABS** (not spaces) for indentation throughout
- Keep consistent tab usage across all files

**Braces:**
- Functions: Opening brace on next line
  ```cpp
  void
  FunctionName()
  {
      // body
  }
  ```
- Control structures: Opening brace on same line
  ```cpp
  if (condition) {
      // body
  }
  ```

**Spacing:**
- Spaces around operators: `int w = cr.right - cr.left;`
- No space before pointer: `UI_Context *ui` (asterisk with type)

**Section Separators:**
Use dotted lines between function definitions:
```cpp
// .............................................................................................
void NextFunction()
```

### File Organization

**Header Comments:**
Start each file with a simple comment:
```cpp
// application.cpp
```

**Include Order:**
1. Windows/system headers: `<windows.h>`, `<d2d1.h>`, `<stdio.h>`, `<stdint.h>`
2. Local headers: `"ui.h"`

**Note:** This project uses a **unity build pattern** - `application.cpp` directly includes `ui.cpp` for single translation unit compilation.

### Types and Type Safety

**Use Fixed-Width Types:**
- `int32_t`, `uint32_t` instead of `int`, `unsigned int`
- `float` for floating-point (Direct2D uses float, not double)

**Struct Initialization:**
- Zero-initialize: `UI_Panel p = {};` or `UI_Panel{}`
- Use designated initializers where clear: `{ 0, 0, 1920, 1080 }`

**Color Format:**
- Use hex ARGB: `0xAARRGGBB` (e.g., `0xFF222222` for opaque dark gray)
- Extract channels via bit manipulation: `(color >> 24) & 0xFF`

### Error Handling

**HRESULT Checking:**
Direct2D functions return `HRESULT`. Check for failures:
```cpp
HRESULT hr = p_d2d_factory->CreateHwndRenderTarget(/* ... */);
if (FAILED(hr)) {
    // handle error
}
```

**COM Lifetime Management:**
- Manually call `Release()` on COM interfaces
- No smart pointers used in this codebase
- Ensure all acquired resources are released

### Comments

**Keep Comments Minimal:**
- Code should be self-documenting through clear naming
- Add comments only for non-obvious logic or algorithms
- No function documentation headers currently used

**Example of Good Comment:**
```cpp
// Calculate remaining space after fixed-size children
```

## Architecture Guidelines

### UI System Design

The UI system is **immediate-mode style**:
1. **Build Phase:** Construct panel tree each frame via `UI_Begin_Panel()` / `UI_End_Panel()`
2. **Layout Phase:** Calculate sizes and positions via `UI_LayoutPanelTree()`
3. **Render Phase:** Generate rectangles and text to render list via `UI_EmitPanels()`
4. **Draw Phase:** Render all primitives with Direct2D and DirectWrite

**Key Concepts:**
- Panels are laid out in rows or columns (flexbox-inspired)
- Children can have fixed sizes or grow to fill space
- Layout properties: direction, gap, padding, grow factor
- Text rendering with UTF-8 support via DirectWrite
- Auto-layout labels with measured text height

### Immediate-Mode API

**Stack-Based Parent Tracking:**
```c
UI_Begin_Panel(ui, "parent");
UI_Panel_Set_Color(ui, 0xFF222222);
UI_Panel_Set_Direction(ui, UI_DIRECTION_COLUMN);
{
    UI_Label(ui, "Hello", 0xFFFFFFFF);
    
    UI_Begin_Panel(ui, "child");
    UI_Panel_Set_Grow(ui, 1.0f);
        // Automatically parented
    UI_End_Panel(ui);
}
UI_End_Panel(ui);
```

**Style Struct Approach:**
```c
UI_Panel_Style sidebar = UI_Default_Panel_Style();
sidebar.pref_w = 240;
sidebar.color = 0xFF22222A;
sidebar.direction = UI_DIRECTION_COLUMN;
sidebar.gap = 4;

UI_Begin_Panel_Ex(ui, "sidebar", &sidebar);
    UI_Label(ui, "Menu", 0xFFFFFFFF);
UI_End_Panel(ui);
```

**Enums:**
- `UI_DIRECTION_ROW` - Horizontal layout (default)
- `UI_DIRECTION_COLUMN` - Vertical layout
- `UI_ALIGN_START` - Left/top alignment
- `UI_ALIGN_CENTER` - Center alignment
- `UI_ALIGN_END` - Right/bottom alignment

**Labels:**
- Automatically create child panels with measured text height
- Support UTF-8 characters: `UI_Label(ui, "Café ☕", color);`
- Auto-incrementing IDs for duplicates (e.g., three "Save" labels get unique IDs)

### Adding New Features

**New UI Components:**
1. Add properties to `UI_Panel` struct in `ui.h`
2. Update `UI_NewPanel()` to accept new parameters
3. Modify layout logic in `UI_Layout()` if needed
4. Add rendering in `UI_GenerateRenderList()`

**New Rendering Primitives:**
1. Define new struct in `ui.h` (e.g., `UI_Circle`)
2. Add to render list array
3. Implement rendering in `Render_UI()` using Direct2D

**Direct2D Integration:**
- All rendering must occur between `BeginDraw()` and `EndDraw()`
- Use the global `p_brush` for solid colors (set via `SetColor()`)
- Coordinate system: origin at top-left, y increases downward

## Development Workflow

### Recommended Tools
- **Visual Studio 2022** (full IDE) or **Visual Studio Code** with C++ extension
- **Windows SDK** must be installed for Direct2D headers
- **MSVC compiler** (`cl.exe`) must be in PATH

### Debugging
- PDB files are generated automatically (`/Zi` flag)
- Use Visual Studio debugger or VS Code with MSVC debugger
- Debug build only (no release configuration in `build.bat`)

### Before Committing Changes
Since there's no automated tooling:
1. Ensure code compiles without warnings (`/W4` is enabled)
2. Manually test the application runs correctly
3. Verify tab indentation is consistent
4. Check naming conventions match existing code

## Helper Functions API

The library provides two API levels:

**Low-Level API** (verbose, full control):
```c
UI_Begin_Panel(ui, "sidebar");
UI_Panel_Set_Direction(ui, UI_DIRECTION_COLUMN);
UI_Panel_Set_Size(ui, 240, -1);
UI_Panel_Set_Padding(ui, 12, 12, 12, 12);
UI_Panel_Set_Gap(ui, 8);
UI_Panel_Set_Color(ui, 0xFF22222A);
  // ... children ...
UI_End_Panel(ui);
```

**Helper API** (compact, recommended for most use):
```c
UI_BeginPanel(ui, "sidebar", UI_DIRECTION_COLUMN, 240, -1, 12, 8, 0xFF22222A);
  // ... children ...
UI_End_Panel(ui);
```

**Helper Functions:**
- `UI_BeginPanel()` - Compact panel initialization (direction, size, padding, gap, color in one call)
- `UI_Panel_Resizable()` - Auto-handles size overrides for persistent resizing
- `UI_Divider()` - One-liner for resizable divider with standard style
- `UI_Divider_Ex()` - Customizable divider (color, hitbox padding)
- `UI_Panel_Set_Padding_Uniform()` - Single value for all padding sides

**Size Parameter Conventions:**
- `w or h >= 0`: Fixed size in pixels
- `w or h == -1` (UI_SIZE_AUTO): Auto-size based on content
- `w or h == -2` (UI_SIZE_FLEX): Flex-grow to fill space
- `padding == 0`: Skip setting padding
- `gap == -1`: Skip setting gap
- `color == 0`: Skip setting color

## Monospace Text Support

The UI library supports monospace fonts (Consolas/Courier New) for displaying code, logs, or debug information where character alignment is important.

**API:**
```c
UI_Label_Monospace(ui, "Frame: 1234 | FPS: 120", 0xFFFFFFFF);
```

**Font Selection Strategy:**
1. Attempts Consolas (default Windows monospace font)
2. Falls back to Courier New if Consolas unavailable
3. Falls back to Segoe UI if neither available

**Use Cases:**
- Debug overlays (frame timers, FPS counters, mouse coordinates)
- Log windows and console output
- Code editors or syntax-highlighted text
- Tabular data where column alignment matters

**Implementation Details:**
- Font style tracked via `UI_Text.font_style` field (0=proportional, 1=monospace)
- Text format cache stores formats by both size AND style
- Monospace text measurement available via `App_Measure_Text_Monospace()` (application.cpp)
- Width approximation: ~9px per character for Consolas 14pt

**Example - Debug Panel:**
```c
void UI_Debug_Mouse_Overlay(UI_Context *ui) {
    char debug_info[512];
    snprintf(debug_info, sizeof(debug_info), 
             "Frame:%6d | FPS:%3d | Mouse:(%4d,%4d)",
             frame, fps, mouse_x, mouse_y);
    
    UI_Label_Monospace(ui, debug_info, 0xFF00FF00);
}
```

## Resizable Dividers

Dividers are 1px panels with `resizable` flag and expanded hitbox for easy grabbing.

**Key Features:**
- Zero-sum resizing (left grows +10px, right shrinks -10px)
- Constraint-aware (respects min/max sizes, stops at boundaries)
- Bidirectional (affects both adjacent panels)
- Persistent sizing (size overrides saved across frame rebuilds)
- Cursor feedback (IDC_SIZEWE for horizontal, IDC_SIZENS for vertical)

**Default Constraints:**
- All panels have 200px min width/height by default
- Prevents panels from collapsing to zero size

**Example:**
```c
UI_Panel_Resizable(ui, "left", UI_DIRECTION_COLUMN, 240, -1, 12, 8, 0xFF22222A);
UI_Divider(ui, "div", UI_DIVIDER_VERTICAL);  // Resizes left and right panels
UI_Panel_Resizable(ui, "right", UI_DIRECTION_COLUMN, 320, -1, 12, 8, 0xFF22222A);
```

## Performance Considerations

**Current Performance Characteristics:**
- Panel lookup: O(n) linear search (~1000 lookups per frame during resize)
- Size override lookup: O(n) linear search (32 max, checked per resizable panel)
- ID deduplication: O(n) search per widget creation
- Rendering: 120 FPS continuous (capped)

**Optimization Opportunities (if needed):**
- Panel lookup cache: Simple index array for O(1) lookups (reduces resize lag)
- Size override hash map: O(1) average case instead of O(n)
- ID hash table: O(1) deduplication instead of O(n)

**Current Performance:**
Acceptable for typical UIs (<100 panels). Optimize only if profiling shows bottlenecks.

### VSync Configuration

**Location:** `application.cpp:699` - `CreateHwndRenderTarget` call

**Development (uncapped FPS):**
```c
D2D1_PRESENT_OPTIONS_IMMEDIATELY  // No VSync, shows true performance
```

**Production (smooth rendering):**
```c
D2D1_PRESENT_OPTIONS_NONE  // Default VSync, no tearing
```

**Trade-offs:**
- VSync OFF: Higher FPS, visible tearing, useful for profiling
- VSync ON: Monitor-capped FPS, smooth visuals, lower power usage

## Tracy Profiler Integration

**Status:** Fully integrated for real-time performance profiling in development builds

Tracy is a real-time, nanosecond resolution frame profiler that provides deep insights into application performance. The integration includes 10 instrumented zones covering the entire render pipeline.

**Tracy Version:** v0.13.1+ (protocol 77)  
**Location:** `/tracy/` (git submodule/clone, excluded from project git via .gitignore)

### Initial Setup - Building Tracy Profiler

**IMPORTANT:** The Tracy profiler executable must match the protocol version of the Tracy library compiled into your application. Version mismatches will cause connection failures.

**Prerequisites:**
- CMake 3.20+ (download from https://cmake.org/download/)
- Visual Studio 2022 (for building)
- Tracy repository cloned to project root

**Build Tracy Profiler from Source:**

```bash
# Navigate to Tracy profiler directory
cd tracy/profiler

# Create build directory
mkdir build
cd build

# Generate build files with CMake
cmake .. -G "Visual Studio 17 2022" -A x64

# Build release version (takes 5-10 minutes first time)
cmake --build . --config Release

# Copy to convenient location
copy Release\Tracy.exe C:\Dev\Tracy\tracy-profiler.exe
```

**Why build from source?** The Tracy repository is frequently updated, and protocol versions change between releases. Building the profiler from the same source tree as the library guarantees compatibility.

**Alternative - Download Prebuilt:**
If you prefer not to build from source, download from https://github.com/wolfpld/tracy/releases, but ensure the version matches your cloned Tracy repository. Check protocol version compatibility if connection fails.

### Building Application with Profiling

Use the dedicated profile build script:
```bash
cd src
build_profile.bat
```

**Build configuration details:**
- **Optimization:** `/O2` - Full release-mode optimization for realistic performance measurement
- **Debug symbols:** `/Zi` - Enables Tracy symbol resolution and function name display
- **Tracy enabled:** `/DTRACY_ENABLE` - Activates all profiling macros
- **ETW disabled:** `/DTRACY_NO_SYSTEM_TRACING` - Prevents Windows SDK compatibility issues with Tracy's ETW integration
- **Tracy source:** `..\..\tracy\public\TracyClient.cpp` - Compiled directly into application
- **Networking:** `ws2_32.lib` - Required for Tracy's network protocol

**Build output:**
- Fully optimized executable with embedded Tracy client
- ~30-60 second compile time (Tracy compilation adds overhead)
- Zero profiling overhead when built without `/DTRACY_ENABLE`

**Note:** Must be run from **Visual Studio Developer Command Prompt** to have `cl.exe` in PATH.

### Running with Tracy

**1. Build profiled version:**
```bash
cd src
build_profile.bat
```

**2. Launch application:**
```bash
cd build
application.exe
```

**Expected behavior:**
- Application runs normally with full optimization
- Waits on port 8086 for Tracy profiler connection
- UI renders at target FPS (240 Hz by default)

**3. Launch Tracy profiler:**
```bash
C:\Dev\Tracy\tracy-profiler.exe
```

**Expected:**
- Tracy GUI window opens
- Shows "Connect to..." dialog or list of available clients

**4. Connect to application:**
- Click **"Connect"** button in Tracy
- Application should appear in discovery list (if on same machine)
- Or manually enter: `localhost` or `127.0.0.1`
- Click application entry to establish connection

**Connection success indicators:**
- Status changes to "Connected"
- Frame timeline starts populating at top of window
- Profiling zones appear in hierarchical timeline view
- Statistics panel shows live frame timing data
- FPS counter updates in real-time

### Profiling Zones (Level 1)

**Currently instrumented zones (10 total):**
- `FrameMark` - End of each frame (line 852, application.cpp)
- `Render` - Main render function (line 387, application.cpp)
- `UI Build` - Panel tree construction (line 410, application.cpp)
- `UI Layout` - Layout calculation (line 416, application.cpp)
- `UI Interaction` - Input processing (line 422, application.cpp)
- `Cursor Update` - Cursor selection logic (line 427, application.cpp)
- `UI Emit` - Render list generation (line 481, application.cpp)
- `Render_UI` - Rectangle rendering (line 243, application.cpp)
- `Render_UI_Text` - Text rendering (line 274, application.cpp)
- `Wait_For_Target_Frame_Time` - Frame pacing (line 349, application.cpp)

**Total zones:** 10 (Level 1 instrumentation)

### Profiling Macro Wrappers

Tracy macros are wrapped in `profiling.h` for easy disable:

| Wrapper Macro | Tracy Macro | Usage |
|---------------|-------------|-------|
| `PROFILE_FRAME` | `FrameMark` | End of frame loop |
| `PROFILE_ZONE` | `ZoneScoped` | Auto-named function zone |
| `PROFILE_ZONE_N(name)` | `ZoneScopedN(name)` | Custom-named zone |
| `PROFILE_ZONE_C(color)` | `ZoneScopedC(color)` | Colored zone |
| `PROFILE_TEXT(text, size)` | `ZoneText(text, size)` | Add text to zone |

**Example - Adding new zone:**
```c
void My_Function() {
    PROFILE_ZONE;  // Auto-named "My_Function"
    // ... code
}

// Or with custom name:
void Complex_Function() {
    {
        PROFILE_ZONE_N("Phase 1: Setup");
        // ... setup code
    }
    {
        PROFILE_ZONE_N("Phase 2: Processing");
        // ... processing code
    }
}
```

### Performance Impact of Tracy

**Tracy overhead per zone:** ~20-50ns (negligible)  
**Network overhead:** ~0.1ms per frame (sends data to profiler)  
**Memory overhead:** ~16MB buffer for frame history

**Note:** Tracy overhead is only present when compiled with `/DTRACY_ENABLE`. Regular builds (build.bat) have zero overhead.

### Maintaining Tracy Version

**Updating Tracy library:**
```bash
cd tracy
git pull origin master
cd ../src
build_profile.bat  # Rebuild application with updated Tracy
```

**Important:** After updating Tracy, rebuild the profiler to maintain protocol compatibility:
```bash
cd tracy/profiler/build
cmake --build . --config Release
copy Release\Tracy.exe C:\Dev\Tracy\tracy-profiler.exe
```

**Version checking:**
- Tracy library version is embedded in compiled application
- Profiler shows its protocol version on connection attempt
- Protocol mismatch errors indicate version incompatibility

**Best practice:** Keep Tracy library and profiler in sync by rebuilding both when updating.

### Expanding Instrumentation

To add more profiling zones (Level 2+):

**High-value zones to add:**
- `UI_Layout_Row()` and `UI_Layout_Column()` in ui.cpp
- `UI_New_Panel()` - panel creation cost
- `UI_Update_Divider_Resize()` - resize logic
- DirectWrite text measurement loops
- Size override lookups

**Add with:**
```c
PROFILE_ZONE;  // At start of function
```

### Interpreting Tracy Data

**Note:** For accurate profiling, ensure VSync is disabled (`D2D1_PRESENT_OPTIONS_IMMEDIATELY` in application.cpp line 699). With VSync enabled, frame times will be capped at monitor refresh rate regardless of actual rendering performance.

**Key metrics to check (240 FPS target = 4.16ms budget):**

| Zone | Expected Time |
|------|---------------|
| **Render** (total) | ~3.0-3.5ms |
| UI Build | ~0.1-0.3ms |
| UI Layout | ~0.05-0.15ms |
| UI Interaction | ~0.05-0.1ms |
| UI Emit | ~0.05-0.1ms |
| Render_UI | ~0.1-0.3ms |
| Render_UI_Text | ~0.2-0.5ms |
| Cursor Update | ~0.01-0.02ms |
| Wait_For_Target_Frame_Time | Variable (fills remaining time) |

**Red flags:**
- ⚠️ Any single zone taking >50% of frame time
- ⚠️ Frame times spiking irregularly
- ⚠️ Text rendering taking >0.5ms consistently

### Troubleshooting

**Protocol version mismatch (most common issue):**
- **Error:** "Incompatible protocol - application uses X, profiler requires Y"
- **Cause:** Tracy profiler executable doesn't match Tracy library version
- **Solution:** Rebuild Tracy profiler from your cloned Tracy repository:
  ```bash
  cd tracy/profiler/build
  cmake --build . --config Release
  copy Release\Tracy.exe C:\Dev\Tracy\tracy-profiler.exe
  ```
- **Prevention:** Always rebuild profiler after updating Tracy repository (`git pull`)

**Application doesn't appear in Tracy:**
- Check firewall isn't blocking port 8086
- Verify application was built with `build_profile.bat`
- Ensure Tracy profiler is running before or shortly after application launch

**Build errors:**
- **Missing `cl.exe`:** Run from Visual Studio Developer Command Prompt
- **Tracy ETW errors:** Ensure `/DTRACY_NO_SYSTEM_TRACING` flag is present in `build_profile.bat`
- **Linker errors about winsock:** Verify `ws2_32.lib` in link line
- **Static assertion failed in TracyETW.cpp:** Already fixed with `/DTRACY_NO_SYSTEM_TRACING` flag

**High overhead / slow performance:**
- Normal - profiling adds ~5-10% overhead
- Disconnect profiler to remove network overhead
- Rebuild with regular `build.bat` for production performance

**Missing zones:**
- Check `#include "profiling.h"` is present in application.cpp
- Verify `/DTRACY_ENABLE` in build_profile.bat
- Ensure zone is inside a called code path (not dead code)

**CMake not found when building profiler:**
- Install CMake from https://cmake.org/download/
- Ensure "Add CMake to PATH" is checked during installation
- Restart terminal/command prompt after installation

## Known Limitations

- **No tests:** Consider adding a test framework if project grows
- **Debug only:** Add release build configuration (`/O2` instead of `/Od`)
- **No CI/CD:** Consider adding GitHub Actions for automated builds
- **Windows-only:** Code is not portable to other platforms
- **Linear search:** Panel/override lookups are O(n) (optimize if >100 panels cause lag)

## File Structure

```
Direct2D_ProjectTemplate/
├── README.md           (user-facing documentation)
├── AGENTS.md           (this file - developer guide)
├── LICENSE
├── .gitignore
└── src/
    ├── ui.h            (UI library interface, 331 lines)
    ├── ui.cpp          (UI library implementation, 1539 lines)
    ├── app_ui.h        (User UI header, 10 lines)
    ├── app_ui.cpp      (User UI implementation, 147 lines)
    ├── application.cpp (Application entry point, 837 lines)
    ├── build.bat       (Build script, 10 lines)
    └── build/          (Build artifacts - gitignored)
        └── application.exe
```

---

*This document should be updated as the project evolves and new conventions are established.*
