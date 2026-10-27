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

`Limits::softTime` stops after a completed iteration; `time` remains the hard budget.
`search::Controller` adds allocation from a remaining clock, increment, moves to
go, and overhead. It scales the soft budget with PV-prefix stability and score
drops, caps spending at available time, and searches forced moves to depth one.
Cancellation is forwarded through the controller. Clock checks occur every 256
nodes, so the hard budget is a polling deadline, not a real-time guarantee.

```cpp
#include "search/controller.hpp"
using namespace std::chrono_literals;
search::Controller controller(32);
search::Clock clock {60s, 1s, 20ms, 0}; // remaining, increment, overhead, moves to go
search::Result result = controller.search(board, clock);
```

Move picking tries a validated hash move, non-losing tactical moves, quiet moves,
then losing captures. Generation and SEE are delayed until needed; sparse
positions reuse one legal-move list. Quiescence generates captures plus quiet
promotions outside check, and all evasions in check. Stalemate detection remains
active. Root moves retain scores and node effort between iterations/MultiPV
passes; aspiration widths follow recent score volatility.

TT entries are 16 bytes, with four entries in an asserted 64-byte cluster.
Position and search-context keys use separate 32-bit fingerprints; as with other
compact hash tables, collisions are possible. Legal hash moves are validated.
`allocated_bytes()` reports the full requested MiB allocation, including sizes
such as 3 MiB. Replacement generation uses a wrapping five-bit age.

Continuation, countermove, capture, and pawn histories have individual opt-in
option switches. Singular extensions, ProbCut, razoring, and correction history are
implemented as opt-in `search::Options` switches. Regression tests cover their
execution and legal PVs; these tests do not establish an Elo gain. Use benchmark
and match testing to tune them for your workload.

### Syzygy

`search::Syzygy` bundles the MIT-licensed Fathom loader/prober at the revision in
`third_party/fathom/README.md`. Supply your own `.rtbw` and `.rtbz` files:

```cpp
#include "search/syzygy.hpp"
search::Syzygy tables("C:/chess/syzygy");
controller.set_integrations(tables.integrations());
```

Check `max_pieces()` to see whether files were found (zero means none). Search
WDL probes require no castling rights and a zero half-move clock. Root DTZ
probing handles nonzero clocks, maps cursed wins/blessed losses to draws, and
returns a legal WDL-preserving move. Roots with repetition history use normal
search because DTZ does not encode that history. Missing files/unsupported
positions fall back to search. Root probing is used for single-PV searches;
MultiPV uses normal search with WDL probes. Mappings are shared and serialized;
a different path requires releasing all clients/integration callbacks. The last
mapping stays cached until replacement or process exit.

### Optional assets

Call `eval::nnue::load_network(path)` from `eval/nnue/network.hpp` before creating
any board or accessing the network to use another network with the same binary
architecture. Wrong-size/unreadable files are rejected transactionally. Once
weights are first used they are frozen; replacing them would invalidate existing
board accumulators. The embedded network remains the default.

`search::OpeningBook` loads a native text book, with records of the form
`full FEN | coordinate move | positive weight`. Blank lines and `#` comments are
accepted. This is not a Polyglot binary reader. Loading validates every move and
preserves the previous book on failure. `probe(board, choice)` selects by integer
weight (supply random entropy as `choice` for randomized play). Repeated or
terminal positions return no book move. Install it through
`Integrations::probeBook`; callback ownership is the caller's responsibility:

```cpp
#include "search/opening_book.hpp"
auto book = std::make_shared<search::OpeningBook>();
book->load("opening-book.txt");
auto integrations = tables.integrations();
integrations.probeBook = [book](const chess::Board& position) {
    return book->probe(position);
};
controller.set_integrations(std::move(integrations));
```

Book/tablebase hits return a one-move variation at depth zero; book scores are
static evaluations, not searched scores. UCI, protocol parsing, and pondering
lifecycle remain outside the engine library.

## Tests

The normal CMake build includes the board, move-generation, and search regression suites. Run:

```powershell
cmake --build --preset windows
ctest --test-dir build/windows -C Debug --output-on-failure
```

Use `-DBUILD_TESTING=OFF` to omit test executables. For faster play, build with `cmake --build build/windows --config Release` and run `build/windows/Release/chess_gui.exe`.

### Benchmarks and differential checks

`chess_bench` reports CSV rows with depth, nodes, seconds, NPS, score, best move,
and tactical pass/fail, followed by an aggregate row. It clears search state
before each position. Optional suite files contain `full FEN | acceptable moves`
(space-separated coordinate moves); omit `|` for positions without assertions.
A failed tactical expectation returns a nonzero exit status.

```powershell
build/windows/Release/chess_bench.exe 7
build/windows/Release/chess_bench.exe 7 tests/data/tactics.txt
build/windows/Release/chess_bench.exe 7 - experimental
```

The dedicated core benchmark suite in `benchmarking/` separately measures legal
move generation, validated perft, NNUE evaluation, fixed-depth search, and every
search-option ablation. Its PowerShell runner creates a GUI-free Release build
and saves the results:

```powershell
benchmarking/run.ps1 -Mode all -Depth 7 -Repetitions 3
```

Tests include start-position perft through depth five (4,865,609 nodes), seeded
legal-game walks comparing staged/tactical moves with full generation, SEE
against a separate legal-board recapture reference, FEN and accumulator undo
round trips, TT replacement/context/size checks, clock allocation/cancellation,
real KQvK WDL/DTZ fixtures, opening-book reloads, and startup NNUE loading.

A local Windows/MSVC 19.44 Release AVX2 comparison against commit `1c20d88`
used the six built-in positions at depth 7 with a cleared 16 MiB hash, alternating
three runs per version. Median elapsed time was 0.780 s before and 0.736 s after;
aggregate NPS was 795,451 before and 844,404 after (about 6% higher). Total nodes
were 620,494 and 621,482. These are local throughput measurements, not an Elo
estimate or a guarantee for other positions/hardware. New history/pruning
options were disabled for the default comparison.
