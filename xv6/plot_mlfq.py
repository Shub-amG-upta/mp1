#!/usr/bin/env python3
"""
plot_mlfq.py -- turn an MLFQ trace into the queue-vs-time figure for the report.

Usage:
    python3 plot_mlfq.py run.log            # writes mlfq_timeline.png
    python3 plot_mlfq.py run.log out.png

The kernel emits one line per scheduling decision when built with LOG=1:

    MLFQ <tick> <pid> <queue>

Everything else in the log (build output, xv6 boot messages, schedulertest's
own table) is ignored.

X axis is elapsed ticks, Y axis is the queue the process was in (0 at the top,
since 0 is the highest priority).  One colour per pid.  Dashed vertical lines
mark the 48-tick priority boosts.
"""

import re
import sys
from collections import defaultdict

try:
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
except ImportError:
    sys.exit("matplotlib not installed.  Run:  pip3 install matplotlib")

BOOST_INTERVAL = 48

# Long-lived system processes we don't want cluttering the figure.
# init is pid 1, sh is pid 2; the schedulertest parent is usually pid 3.
HIDE_PIDS = {1, 2}

LINE = re.compile(r"^MLFQ\s+(\d+)\s+(\d+)\s+(\d+)\s*$")


def parse(path):
    """Return {pid: ([ticks], [queues])} from the trace lines in path."""
    series = defaultdict(lambda: ([], []))
    matched = 0

    with open(path, errors="replace") as fh:
        for raw in fh:
            m = LINE.match(raw.strip())
            if not m:
                continue
            tick, pid, queue = (int(g) for g in m.groups())
            matched += 1
            if pid in HIDE_PIDS:
                continue
            xs, ys = series[pid]
            xs.append(tick)
            ys.append(queue)

    if matched == 0:
        sys.exit(
            f"no 'MLFQ <tick> <pid> <queue>' lines found in {path}.\n"
            "Did you build with:  make qemu CPUS=1 SCHEDULER=MLFQ LOG=1 ?"
        )
    return series


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)

    logfile = sys.argv[1]
    outfile = sys.argv[2] if len(sys.argv) > 2 else "mlfq_timeline.png"

    series = parse(logfile)
    if not series:
        sys.exit("trace contained only hidden pids -- nothing to plot")

    # Rebase time so the figure starts at 0 rather than at whatever tick the
    # workload happened to be launched.
    t0 = min(min(xs) for xs, _ in series.values())
    tmax = max(max(xs) for xs, _ in series.values()) - t0

    fig, ax = plt.subplots(figsize=(11, 5))

    # Boost lines first, so the data draws on top of them.
    boost = BOOST_INTERVAL - (t0 % BOOST_INTERVAL)
    if boost == BOOST_INTERVAL:
        boost = 0
    first = True
    while boost <= tmax:
        ax.axvline(
            boost,
            color="0.6",
            linestyle="--",
            linewidth=1,
            zorder=1,
            label="priority boost" if first else None,
        )
        first = False
        boost += BOOST_INTERVAL

    for pid in sorted(series):
        xs, ys = series[pid]
        xs = [x - t0 for x in xs]
        off = 0.06 * (sorted(series).index(pid) - len(series) / 2)
        ys = [y + off for y in ys]
        ax.step(xs, ys, where="post", linewidth=1.2, alpha=0.75, zorder=2)
        ax.scatter(xs, ys, s=12, zorder=3, label=f"pid {pid}")

    ax.set_xlabel("time elapsed (ticks)")
    ax.set_ylabel("MLFQ queue")
    ax.set_title("MLFQ: queue occupancy over time, one colour per process")
    ax.set_yticks([0, 1, 2, 3])
    ax.set_ylim(3.5, -0.5)          # queue 0 (highest priority) at the top
    ax.grid(axis="y", alpha=0.3)
    ax.legend(loc="center left", bbox_to_anchor=(1.01, 0.5), frameon=False)

    fig.tight_layout()
    fig.savefig(outfile, dpi=160, bbox_inches="tight")
    print(f"wrote {outfile}")
    print(f"processes plotted: {sorted(series)}")
    print(f"span: {tmax} ticks")


if __name__ == "__main__":
    main()