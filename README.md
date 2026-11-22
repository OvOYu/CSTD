# CSTD

Common-Subset Truss Decomposition computes exact hyper-truss numbers using common-subset counting and adaptive peeling.

Manuscript: **Efficient Hyper-Truss Decomposition via Common-Subset Counting and Adaptive Peeling**.

## Build and verify

Requirements: Linux or WSL, an x86-64 CPU with POPCNT, Bash, GCC 11 or newer, and Python 3.9 or newer.

```bash
bash scripts/build.sh
python3 scripts/check_correctness.py
```

The checker recomputes triangle supports by exhaustive enumeration and compares every hyperedge's truss number.

## Run

Place one hyperedge per line in a text file. Separate nonnegative integer vertex IDs with commas, spaces or tabs. The program writes one truss number per hyperedge and reports the number of hyperedges, the truss-number range and elapsed wall time on standard error.

```bash
build/cstd data/tiny.txt
```

The six expected truss numbers in input order are `5, 5, 5, 3, 2, 5`.

Download [email-Enron.tar.gz](https://drive.google.com/uc?export=download&id=1tTVZkdpgRW47WWmsrdUCukHz0x2M6N77) from the [Cornell dataset page](https://www.cs.cornell.edu/~arb/data/email-Enron/) and place it in the repository root. Convert the archive's simplex representation into one unique hyperedge per line and run CSTD with this command.

```bash
python3 -c 'import sys, tarfile; t=tarfile.open(sys.argv[1], "r:gz"); read=lambda suffix: map(int,t.extractfile(next(n for n in t.getnames() if n.endswith(suffix))).read().split()); vertices=read("-simplices.txt"); edges=dict.fromkeys(tuple(sorted({next(vertices) for _ in range(n)})) for n in read("-nverts.txt") if n); ids={v:i for i,v in enumerate(sorted({v for edge in edges for v in edge}))}; print("\n".join(",".join(str(ids[v]) for v in edge) for edge in edges))' email-Enron.tar.gz > email-Enron.txt && build/cstd email-Enron.txt
```

This repository accompanies the manuscript under review and contains the implementation of the proposed method with a correctness check. The complete experimental scripts will be released after publication.
