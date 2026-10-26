# ChessEngineV2

The project builds the engine in `engine/` as a reusable library, a console executable, and a separate SDL3 desktop GUI.

## Windows setup

Install Visual Studio 2022 with the Desktop development with C++ workload, CMake, and Git. The setup script downloads vcpkg into the project, installs SDL3 from the manifest, and builds every target.

```powershell
.\scripts\setup.ps1
```

For later builds in a new terminal, run `cmake --preset windows` and then `cmake --build --preset windows`. The preset uses the repository's `.vcpkg` directory directly; no `VCPKG_ROOT` environment variable is required.

The GUI executable is created at `build/windows/Debug/chess_gui.exe`. Click or drag a piece to make a legal move. Choose a promotion piece from the panel beside the board. You play White against the engine by default; the engine searches in the background with a one-second move budget.

The left panel has clickable controls and keyboard shortcuts:

- `W` / `B`: start a new game as White / Black.
- `L`: start a local two-player game.
- `N`: restart the current game mode.
- `U`: cancel thinking and undo to your previous turn (one ply in local mode).
- `Space`: stop thinking and play the best move from the last completed iteration.
- `-` / `+`: adjust the next move's thinking time from 250 ms to 10 seconds.
- `M`: cycle between one, two, and three principal variations for subsequent searches.

The panel displays completed search depth, nodes, elapsed time, scores, and PV moves. Scores are in centipawns from the searching side's perspective; `M` denotes mate distance in moves. Checkmate, stalemate, repetition, insufficient material, and the fifty-move rule end the game. Restart, undo, and closing the window cancel outstanding work before replacing its position.

The console engine executable is created at `build/windows/Debug/chess_cli.exe`.

## Targets

- `chess_engine` contains only engine code and has no GUI dependency.
- `chess_cli` is the current console entry point.
- `chess_gui` contains the debugging interface and links the engine with SDL3.

To build the engine without installing SDL3, configure with `-DCHESS_BUILD_GUI=OFF`.

## Search

`search::Search::iterative_deepening` takes a board copy, `search::Limits`, an optional `std::stop_token`, and an optional completed-iteration callback. Use one search per instance at a time. The callback runs on the calling/search thread. `clear()` resets move ordering between games and must be called while idle.

```cpp
search::Search engine;
search::Limits limits;
limits.depth = 8;
limits.multiPv = 3;
limits.time = std::chrono::milliseconds(1000);
search::Result result = engine.iterative_deepening(board, limits);
if (result.bestMove != 0)
    board.make_move(result.bestMove);
```

A zero time or node budget disables that limit; depth must be between 1 and 127. MultiPV is clamped to the number of legal moves. Results contain only complete iterations, with a legal fallback move when stopped before depth one. Terminal roots return no move. Cancellation and node limits are checked at each node; the clock is checked every 256 nodes. PVs include quiescence moves and can exceed the reported nominal depth.

The fail-soft search implements iterative deepening, root search, negamax, quiescence, asymmetric aspiration windows, PVS, verified null-move pruning, reverse futility pruning, move-quality futility pruning, check and recapture extensions, IIR, LMR, history, killers, and MultiPV. Aspiration and PVS have separate implementations. SEE uses local bitboards, checks pinned attackers and king captures, and handles en passant and promotions. Capture ordering separates winning and losing exchanges; quiescence supports SEE and delta pruning with checks and promotions protected. LMR uses history or exchange quality, static evaluation, improving status, and PV provenance.

The clustered transposition table stores exact, lower, and upper bounds, static evaluations, depth, moves, generation, and PV provenance. Mate scores are normalized by ply; context keys isolate repetition history, the fifty-move clock, and extension/null-verification state. The evaluator uses Casanchess's pretrained NNUE network. Piece values (pawn 100, knight 320, bishop 330, rook 500, queen 900) remain available for SEE and move ordering. Selective pruning is heuristic; this is not an exhaustive proof search.

## NNUE evaluation

The bundled `data/network-20260910.nnue` is embedded in the engine library at build
time, with its SHA-256 checked during configuration. No runtime network path or
download is needed. Attribution, source commit, and license are in
`third_party/casanchess/`.

`eval::evaluate(board)` returns the objective NNUE score in centipawns from the
side to move. Both perspectives are maintained incrementally through normal
moves, captures, all promotions, en passant, and castling. Only a king moving to
a different feature bucket rebuilds that king's perspective. Board history
restores accumulators on undo; null moves change perspective without changing
piece features. Board copies own independent accumulators and history, while
weights are immutable and shared across threads.

The implementation is split into feature/layout definitions under
`engine/headers/eval/nnue/`, move updates and inference in
`engine/src/eval/nnue.cpp`, and SIMD kernels/network storage under
`engine/src/eval/nnue/`. `eval::nnue::evaluate_outputs` additionally exposes the
network's signed drawishness residual; search uses the objective score without
Casanchess's optional Ambition adjustment.

AVX2 kernels are enabled by default and require an AVX2-capable CPU. Configure
with `-DCHESS_NNUE_AVX2=OFF` for the portable scalar kernels; both paths use the
same incremental updates and exact integer arithmetic. Build Release for play.

NNUE tests compare incremental accumulators with full refreshes after make/undo,
exercise special/null moves and copies, and compare SIMD arithmetic with scalar
references. For an additional comparison against Casanchess's actual source,
set `CHESS_CASANCHESS_REFERENCE` to its pinned checkout as described in
`third_party/casanchess/README.md`.

`Limits::softTime` optionally stops after a completed iteration, while `time` remains the hard budget. Clock allocation belongs to the calling application. Results expose selective depth and a ponder move from the completed PV; UCI formatting and pondering lifecycle also belong to the caller. `set_integrations` installs an optional tablebase callback while the search is idle and clears cached bounds. The callback returns `std::nullopt` for unsupported positions, or a side-to-move `TablebaseOutcome` that accounts for the fifty-move rule and repetition. Search uses wins/losses as bounds at non-PV nodes and excludes null-move subtrees. Syzygy file loading and probing are intentionally left to an external adapter; no tablebase backend is bundled.

## Tests

The normal CMake build includes the board, move-generation, and search regression suites. Run:

```powershell
cmake --build --preset windows
ctest --test-dir build/windows -C Debug --output-on-failure
```

Use `-DBUILD_TESTING=OFF` to omit test executables. For faster play, build with `cmake --build build/windows --config Release` and run `build/windows/Release/chess_gui.exe`.
