# SEP Entropy Pool LT Model

This model implements the firmware-visible entropy-pool aperture at
`0x1095_0000`. It models the 32-to-64-bit packer, 32-entry destructive-read
FIFO, live low/stall/error interrupt causes, reset scrubbing, and an
event-driven EDN endpoint.

The model deliberately abstracts cycle-level CDC and random-data generation.
Its default 20.48 us fill-stall interval represents the RTL's 4096 cycles at
200 MHz while preserving loosely timed execution.
