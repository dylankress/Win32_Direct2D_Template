@echo off
pushd %~dp0

if not exist build mkdir build
pushd build

REM Tracy profiling build with full optimization
REM /O2 - Maximum speed optimization (release performance)
REM /DTRACY_ENABLE - Enable Tracy instrumentation
REM /DTRACY_NO_SYSTEM_TRACING - Disable ETW (fixes compilation on newer Windows SDK)
REM /I - Include path for Tracy headers
REM /Zi - Debug info for profiler symbols
REM ws2_32.lib - Winsock library (Tracy networking)

cl /O2 /Zi /W4 /DTRACY_ENABLE /DTRACY_NO_SYSTEM_TRACING /I..\..\tracy\public ^
   ..\application.cpp ^
   ..\..\tracy\public\TracyClient.cpp ^
   d2d1.lib dwrite.lib user32.lib ws2_32.lib

popd
popd
