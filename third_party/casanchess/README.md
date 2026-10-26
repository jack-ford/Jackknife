# Casanchess NNUE attribution

The network architecture, feature mapping, quantized inference, and activation
kernels are adapted from Carlos Sánchez Mayordomo's Casanchess, under the adjacent
MIT [LICENSE](LICENSE).

- Upstream: https://github.com/casanche/casanchess
- Inspected commit: `32d51984b304b12cb6f786054e02bd2cedbc0e31`
- Network: `data/network-20260910.nnue` from that commit
- SHA-256: `b17d5a73bb312345c442c72d7f87474cfa3b20e9c42889efc8184b17c211560f`

Retain the license with redistributions containing the adapted evaluator or net.
The network is pretrained by Casanchess; it was not trained by this project.

The port preserves the 26 king buckets, 128 channels per perspective, 48-neuron
hidden layer, SCReLU rounding, linear bypass, and signed draw residual head.
Its inputs explicitly map this engine's different piece/color enums. Special
moves use feature deltas; a king bucket change rebuilds only the affected view.
Undo restores saved accumulators, and null moves leave the feature sums unchanged.

To compare against upstream's unmodified scalar evaluator, check out the pinned
commit and configure with `-DCHESS_CASANCHESS_REFERENCE=/path/to/casanchess`.
This adds `nnue_casanchess_parity` to CTest, comparing both outputs and perspectives
through special-move fixtures and deterministic random playouts.
