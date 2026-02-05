// profiling.h - Tracy profiler wrapper macros
//
// This header wraps Tracy profiling macros to allow easy enable/disable.
// When TRACY_ENABLE is defined, profiling is active.
// When undefined, all macros compile to nothing (zero overhead).
//
// USAGE:
//   #include "profiling.h"
//   
//   void MyFunction() {
//       PROFILE_ZONE;  // Auto-named from function name
//       // ... code
//   }
//
//   void ComplexFunction() {
//       {
//           PROFILE_ZONE_N("Phase 1");  // Custom name
//           // ... phase 1 code
//       }
//       {
//           PROFILE_ZONE_N("Phase 2");
//           // ... phase 2 code
//       }
//   }
//
#pragma once

#ifdef TRACY_ENABLE
    #include "tracy/Tracy.hpp"
    #define PROFILE_FRAME FrameMark
    #define PROFILE_ZONE ZoneScoped
    #define PROFILE_ZONE_N(name) ZoneScopedN(name)
    #define PROFILE_ZONE_C(color) ZoneScopedC(color)
    #define PROFILE_TEXT(text, size) ZoneText(text, size)
#else
    #define PROFILE_FRAME
    #define PROFILE_ZONE
    #define PROFILE_ZONE_N(name)
    #define PROFILE_ZONE_C(color)
    #define PROFILE_TEXT(text, size)
#endif
