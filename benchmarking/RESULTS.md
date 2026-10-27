# Baseline review results

Measured on October 27, 2026 with MSVC 19.44, a 64-bit Release build, AVX2 NNUE kernels, one search thread, a cleared 16 MiB transposition table per position, and no book or tablebases. These are local engineering baselines, not portable ratings.

## Component throughput

| Component | Measured range |
| --- | ---: |
| Cached fixed-position legal generation | 3.7-8.6 million positions/s |
| Validated standard perft | 36.7-77.1 million leaf nodes/s |
| Hot-accumulator NNUE evaluation | 1.16-1.20 million evaluations/s |
| Fixed-depth search | 0.55 million nodes/s aggregate |

All six canonical perft totals passed. At depth 7, the six-position search set visited 401,387 nodes in 0.73 seconds and chose the same best moves in every three-repeat baseline run.

## Search feature ablation

Three runs per configuration at depth 7 produced the following elapsed-time ratios. Values above 1.00 mean the tested configuration is slower than the current default. Each row changes only the named option.

| Configuration | Node ratio | Time ratio | Best-move agreement |
| --- | ---: | ---: | ---: |
| No principal variation search | 7.295 | 6.491 | 6/6 |
| No late move reductions | 2.009 | 2.065 | 6/6 |
| No reverse futility pruning | 1.556 | 1.737 | 6/6 |
| No transposition table | 1.417 | 1.591 | 5/6 |
| No null-move pruning | 1.334 | 1.324 | 6/6 |
| No futility pruning | 1.402 | 1.225 | 6/6 |
| No SEE pruning | 1.161 | 1.157 | 6/6 |
| No history heuristic | 1.098 | 1.107 | 6/6 |
| No recapture extensions | 1.108 | 1.101 | 6/6 |
| No delta pruning | 1.047 | 1.024 | 6/6 |
| No aspiration windows | 1.011 | 1.012 | 6/6 |
| No internal iterative reduction | 1.013 | 1.006 | 6/6 |
| No killer moves | 0.980 | 0.977 | 6/6 |

The default PVS, LMR, reverse futility, transposition table, null-move, futility, SEE, and history options are clear throughput wins on this sample. Killer moves are a candidate for disabling or retuning: removing them reduced both nodes and time by about 2%. Aspiration windows and IIR are positive but marginal.

Disabling check extensions reduced the node count to 0.767 and time to 0.746 of baseline. That is an expected cost of extending tactical lines, not evidence that the feature should be disabled. Recapture extensions likewise cost about 10%. Judge both with tactical coverage and self-play, not fixed-depth throughput.

Opt-in options measured as follows:

| Enabled option | Node ratio | Time ratio | Best-move agreement |
| --- | ---: | ---: | ---: |
| Continuation history | 1.106 | 1.116 | 6/6 |
| Counter moves | 0.999 | 0.992 | 6/6 |
| Capture history | 1.016 | 1.000 | 6/6 |
| Pawn history | 0.996 | 1.000 | 6/6 |
| Singular extensions | 1.700 | 1.725 | 6/6 |
| ProbCut | 0.990 | 0.992 | 6/6 |
| Razoring | 0.956 | 1.073 | 6/6 |
| Correction history | 1.000 | 0.991 | 6/6 |

Keep continuation history, singular extensions, and razoring disabled pending redesign or tuning. Counter moves, ProbCut, and correction history are small positive candidates for self-play. Capture and pawn history are neutral at this depth. Fixed-depth speed and six best moves cannot establish playing strength; any default change still needs tactical regression and statistically significant self-play.

## Optimization priorities

1. Remove full NNUE accumulator snapshots from every `HistoryEntry`. Make/unmake currently transfers an aligned accumulator in both directions on every edge. Store reversible feature deltas and refresh only the affected king perspective when a bucket changes.
2. Profile and optimize NNUE inference. Its 1.2 million evaluations/s ceiling is close enough to search throughput to make it a likely dominant cost. Count actual eval calls, then test fused activation/dot-product kernels, aligned loads, whole-program optimization, and profile-guided optimization.
3. Remove `std::function` from `MovePicker`'s per-node scoring callback. A templated scorer or non-owning inline delegate avoids type erasure and exposes history/capture scoring to inlining.
4. Retune killer ordering. It slightly enlarges the tree in this suite; test lower priorities, one killer slot, and duplicate suppression against counter/history scores.
5. Only after the above, flatten the per-square magic attack vectors into contiguous aligned tables and optimize repetition scanning. Raw move generation is already much faster than evaluation and end-to-end search, so these are lower-impact targets.
