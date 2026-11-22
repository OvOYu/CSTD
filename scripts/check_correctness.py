#!/usr/bin/env python3
"""Check every reported truss number against an exhaustive reference."""

from itertools import combinations
from pathlib import Path
import random
import re
import subprocess
from tempfile import mkdtemp

ROOT = Path(__file__).resolve().parents[1]
BINARY = ROOT / "build/cstd"


def reference(edges):
    edges = [set(edge) for edge in edges]
    neighbors = [set() for _ in edges]
    for a, b in combinations(range(len(edges)), 2):
        if edges[a] & edges[b]:
            neighbors[a].add(b)
            neighbors[b].add(a)

    active = set(range(len(edges)))
    result = [2] * len(edges)
    threshold = 1
    while active:
        support = dict.fromkeys(active, 0)
        for a, b, c in combinations(sorted(active), 3):
            if b in neighbors[a] and c in neighbors[a] and c in neighbors[b]:
                support[a] += 1
                support[b] += 1
                support[c] += 1
        threshold = max(threshold, min(support.values()) + 1)
        removed = {edge for edge in active if support[edge] < threshold}
        for edge in removed:
            result[edge] = threshold + 1
        active -= removed

    return result


def run(input_path):
    completed = subprocess.run(
        [str(BINARY), str(input_path)],
        stdin=subprocess.DEVNULL,
        capture_output=True,
        text=True,
        timeout=20,
        check=True,
    )
    values = []
    for index, line in enumerate(completed.stdout.splitlines()):
        match = re.fullmatch(r"Hyperedge ID: (\d+), Trussness: (\d+)", line)
        if not match or int(match[1]) != index:
            raise AssertionError(f"unexpected output row {index + 1}: {line!r}")
        values.append(int(match[2]))
    return values


def check(path, edges):
    expected = reference(edges)
    actual = run(path)
    if actual != expected:
        raise AssertionError(f"{path.name}: expected {expected}, got {actual}")
    return actual


def main():
    tiny = ROOT / "data/tiny.txt"
    tiny_edges = [[int(v) for v in line.split(",")] for line in tiny.read_text().splitlines()]
    checked = check(tiny, tiny_edges)
    expected = [int(value) for value in (ROOT / "data/tiny.expected").read_text().split()]
    if checked != expected:
        raise AssertionError(f"tiny fixture changed: expected {expected}, got {checked}")

    fixtures = [
        [],
        [[0]],
        [[0], [1], [2]],
        [[0, 1], [1, 2], [0, 2]],
        [list(range(75)) + [80 + i] for i in range(4)],
        [[0, i] for i in range(1, 71)] + [[71]],
    ]
    rng = random.Random(49017)
    for _ in range(80):
        n = rng.randint(1, 13)
        edges = []
        for _ in range(rng.randint(1, 13)):
            edge = sorted(rng.sample(range(n), rng.randint(1, min(n, 7))))
            if edge not in edges:
                edges.append(edge)
        fixtures.append(edges)

    total = len(tiny_edges)
    work = Path(mkdtemp(prefix="cstd-check-", dir=ROOT / "build"))
    for index, edges in enumerate(fixtures):
        path = work / f"{index}.txt"
        path.write_text("".join(",".join(map(str, edge)) + "\n" for edge in edges))
        check(path, edges)
        total += len(edges)
    print(f"CORRECTNESS_PASS fixtures={len(fixtures) + 1} output_entries={total}")


if __name__ == "__main__":
    main()
