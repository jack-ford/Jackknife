<div align="center">

<picture>
  <source media="(prefers-color-scheme: dark)"
          srcset="https://github.com/jack-ford/Jackknife/blob/v2/data/assets/banner.png">
  <img src="https://github.com/jack-ford/Jackknife/blob/v2/data/assets/banner.png"
       height="400" alt="Jackknife Logo">
</picture>

</div>


Jackknife is built as a reusable engine library with two front ends: a standard UCI executable for use in any chess GUI, and its own SDL3 desktop GUI for playing and analysing against the engine.

## Features

**Board and move generation**
* Bitboard representation with magic bitboards for sliding pieces
* Zobrist hashing and full legal move generation, verified with perft

**Evaluation**
* NNUE with 26 king buckets, a 2×128 accumulator, a 48-neuron hidden layer and a separate draw head
* Embedded network, so the binary runs without external files
* AVX2 SIMD kernels, with a scalar fallback for older CPUs
* Static exchange evaluation (SEE) for captures

**Search**
* Principal variation search with aspiration windows and iterative deepening
* Transposition table with 4-entry, cache-line-aligned clusters
* Null move pruning, reverse futility pruning, futility pruning, razoring and SEE pruning
* Late move reductions and singular extensions
* Killer moves, continuation history and correction history
* Quiescence search with delta pruning
* Syzygy endgame tablebases (via [Fathom](https://github.com/jdart1/Fathom)) and PGN opening book support
* MultiPV, pondering and analysis mode

## Strength

Jackknife's estimated rating is about **3100 Elo (±57)**.

To get this number, the engine played a 1000-game gauntlet against five engines rated on the [CCRL Blitz](https://computerchess.org.uk/ccrl/404/) list. Each match gives an Elo difference between Jackknife and that opponent. Adding the difference to the opponent's CCRL rating gives one estimate of Jackknife's rating, and the five estimates are then combined.

The games were run with [fastchess](https://github.com/Disservin/fastchess) on a single machine at 10+0.1, using openings from the bundled `8moves_v3` book (`data/books`).

| Opponent | CCRL Blitz | Games | Jackknife score | Draws | Elo difference | Implied Elo |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Casanchess 2.0 | 3194 | 200 | 35.2% | 33.0% | −105.6 ± 33.5 | 3088 |
| Bit-Genie 9 | 3088 | 200 | 61.5% | 30.0% | +81.4 ± 43.5 | 3169 |
| Qapla 0.4.0 | 3015 | 200 | 55.8% | 34.0% | +40.1 ± 38.5 | 3055 |
| Cheese 3.3 | 2962 | 200 | 75.5% | 28.0% | +195.5 ± 47.9 | 3158 |
| Daydreamer 2.0.0-pre2 | 2898 | 200 | 71.3% | 33.0% | +157.7 ± 43.7 | 3056 |

* Average of the estimates: **3105**
* Weighted by each match's error margin: **3099**
* Uncertainty: **±57**

Over the whole gauntlet Jackknife scored 59.9%, which works out to +69.4 ± 19.1 Elo against this group of opponents.

## Benchmarks

Measured on an AMD Ryzen AI 9 HX 370, single thread, AVX2 build:

| Test | Result |
| --- | --- |
| `perft 6` (start position) | 119,060,324 nodes in 6.95 s (~17.1M nps) |
| `bench 14` (start position) | 5,957,607 nodes in 2.96 s (~2.0M nps) |
| `bench 12` (Kiwipete) | 5,607,737 nodes in 2.86 s (~2.0M nps) |

## Getting started

Build with CMake and a C++20 compiler. The GUI depends on SDL3, SDL3_image and SDL3_ttf, which are installed through vcpkg:

```
git clone https://github.com/jack-ford/ChessEngineV2.git
cd ChessEngineV2

cmake --preset windows
cmake --build --preset windows
```

To build only the UCI engine without the GUI or vcpkg:

```
cmake -B build -DCHESS_BUILD_GUI=OFF
cmake --build build --config Release --parallel
```

Pass `-DCHESS_AVX2=OFF` for CPUs without AVX2 support.

## Non-standard UCI commands

* `d`: Prints the current board.
* `eval`: Prints the static NNUE evaluation of the current position.
* `perft <depth>`: Counts leaf nodes at the given depth, split by root move.
* `bench [depth]`: Searches the current position to a fixed depth (default 6) and reports nodes and time.

Most search features can also be switched on and off through UCI options (e.g. `Null Move Pruning`, `Late Move Reductions`, `Singular Extensions`), which is handy for testing.

## Acknowledgements

Thanks to the [Chess Programming Wiki](https://www.chessprogramming.org/) and the wider chess programming community for the knowledge they share openly, and to the authors of [Fathom](https://github.com/jdart1/Fathom) for the Syzygy probing code.
