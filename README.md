# ChessEngineV2

The project builds the engine in `engine/` as a reusable library, a console executable, and a separate SDL3 desktop GUI.

## Windows setup

Install Visual Studio 2022 with the Desktop development with C++ workload, CMake, and Git. The setup script downloads vcpkg into the project, installs SDL3 from the manifest, and builds every target.

```powershell
.\scripts\setup.ps1
```

For later builds in a new terminal, set `$env:VCPKG_ROOT = "$PWD\.vcpkg"` and run `cmake --build --preset windows`.

The GUI executable is created at `build/windows/Debug/chess_gui.exe`. Drag any piece with the left mouse button and release it over another square. Moves are intentionally unrestricted while the engine's move rules are under development.

The console engine executable is created at `build/windows/Debug/chess_cli.exe`.

## Targets

- `chess_engine` contains only engine code and has no GUI dependency.
- `chess_cli` is the current console entry point.
- `chess_gui` contains the debugging interface and links the engine with SDL3.

To build the engine without installing SDL3, configure with `-DCHESS_BUILD_GUI=OFF`.
