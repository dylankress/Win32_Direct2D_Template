@echo off
pushd %~dp0

if not exist build mkdir build
pushd build

cl /Zi /Od /W4 ..\application.cpp d2d1.lib dwrite.lib user32.lib

popd
popd
