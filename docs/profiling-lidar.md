# Profiling the lidar

Measured before deciding whether to optimise anything. The conclusion is that
no optimisation was warranted, and the headroom is recorded here instead.

## Method

The sweep is wrapped in a cycle counter (`stat RoboSim`), and each
configuration was driven for roughly ten seconds in PIE while recording:

- **Lidar Sweep, inclusive** — the whole sweep including the line traces and,
  where enabled, the debug line drawing.
- **Lidar Sweep, exclusive** — only the sweep's own code: loop, angles, noise.
- **Game, Draw, Prims** from `stat unit`.
- **Real-time factor** from the simulation clock, which reports how much
  simulated time was achieved per real second.

Machine: 16 physical cores / 22 logical, 32 GB RAM, editor PIE at 1560x772.
The test level is near-empty, so the physics broadphase has little to reject;
a dense scene would cost more per ray.

## Results

| Beams | Rate | Rays/s | Debug draw | Sweep incl. | Sweep excl. | Game | Draw | Prims | RTF |
|---|---|---|---|---|---|---|---|---|---|
| 180 | 10 Hz | 1.8k | on | — | — | 8.0 | — | — | 1.00 |
| 500 | 10 Hz | 5k | on | — | — | 10.2 | — | — | 1.00 |
| 1,000 | 10 Hz | 10k | on | — | — | 10.2 | — | — | 1.00 |
| 2,000 | 10 Hz | 20k | on | — | — | 11.0 | — | — | 1.00 |
| 2,000 | 40 Hz | 80k | on | — | — | 11.5 | — | — | 1.00 |
| 20,000 | 10 Hz | 200k | on | 4.67 | 0.77 | 20.3 | 24.8 | 121K | 1.00 |
| 20,000 | 20 Hz | 400k | on | 8.90 | 1.42 | 25.9 | 28.9 | 228K | 1.00 |
| 30,000 | 30 Hz | 900k | on | 31.39 | 4.21 | 48.6 | 33.6 | 279K | 0.99 |
| 30,000 | 30 Hz | 900k | **off** | **11.48** | **1.38** | **23.1** | **6.7** | **19K** | 0.99 |

All timings in milliseconds.

## Findings

**The sweep's own code is about 12% of its cost.** Exclusive time stays near
1–4 ms while inclusive time reaches 31 ms. The time is in the engine's scene
queries, not in the loop around them, so rewriting the loop would buy nothing.

**The visualisation cost roughly twice the sensor.** At 30,000 beams and 30 Hz,
turning off debug drawing took the sweep from 31.4 ms to 11.5 ms, the draw
thread from 33.6 ms to 6.7 ms, and rendered primitives from 279,000 to 19,000.
The picture of the data was more expensive than producing it.

**Cost is linear in ray count until it is not.** 200k to 400k rays per second
doubled the cost exactly. 400k to 900k (2.25x) cost 3.5x. At 30,000 beams a
single sweep takes longer than a frame, so the work arrives in spikes rather
than spread evenly: inclusive average 11.5 ms against an inclusive maximum of
27 ms. A sensor whose average fits the budget can still miss a frame deadline.

**Measured ceiling is about 900,000 ray casts per second** before the real-time
factor leaves 1.00, with visualisation disabled. For comparison, a Velodyne
VLP-16 produces roughly 288,000 points per second, so there is about 3x the
headroom required for a real 3D lidar on this machine.

## Decision

No optimisation was applied. The sensor is not the bottleneck at any
configuration this project needs, and the obvious change — parallelising the
beams — would add complexity and risk for no measured benefit.

Recorded here so the decision can be revisited with evidence if a future
scenario needs denser sensing, a dense scene, or several robots at once.

## If it does need optimising later

In rough order of expected benefit:

1. Run the beams through `ParallelFor`. Scene queries are read-only against the
   physics scene, so they parallelise well, and the work is trivially divisible.
2. Spread one sweep across several frames rather than taking it in one burst,
   which fixes the spike without reducing total work.
3. Narrow the trace channel so the broadphase rejects more candidates earlier.

Debug drawing should stay off whenever cost is being measured, and probably
whenever more than a few hundred beams are in use.
