# Core engine benchmarks

This suite measures engine internals only. It does not exercise the CLI, GUI, UCI parsing, opening book, or tablebases.

Build and run the complete Release/AVX2 suite from PowerShell:

```powershell
benchmarking/run.ps1 -Mode all -Depth 7 -Repetitions 1
```

The runner configures a GUI-free Release build, writes the CSV-compatible output to `benchmarking/results/`, and prints it to the terminal. Individual modes are `movegen`, `evaluation`, `search`, and `features`.

```powershell
benchmarking/run.ps1 -Mode movegen -Repetitions 3
benchmarking/run.ps1 -Mode evaluation -Repetitions 5
benchmarking/run.ps1 -Mode search -Depth 8 -Repetitions 3
benchmarking/run.ps1 -Mode features -Depth 7 -Repetitions 3
```

`movegen` reports fixed-position generation throughput and validated perft NPS over six standard positions. Perft includes make/unmake and incremental NNUE maintenance. `evaluation` measures hot-accumulator NNUE inference. `search` reports fixed-depth time, nodes, NPS, best move, and key heuristic counters with a cleared 16 MiB transposition table for every position. `features` performs one-factor-at-a-time ablations against the default options and reports tree size, elapsed-time, NPS, and best-move agreement.

Use at least three repetitions, close other CPU-heavy applications, keep the machine on a fixed power plan, and compare medians from alternating builds when evaluating small changes. A feature's elapsed-time and node ratios matter more than its raw NPS: pruning can lower NPS while still reaching a depth sooner by searching fewer nodes. Best-move agreement is a smoke test, not a strength or Elo measurement; use self-play with confidence intervals before changing playing defaults.
