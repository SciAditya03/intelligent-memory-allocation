# Intelligent Memory Allocation

**CS4 Case Study | Dynamic Memory Allocation | C++17**

An experimental study investigating whether an adaptive memory allocator can reduce fragmentation and allocation failures compared with conventional dynamic memory allocation strategies.

The project implements four conventional placement strategies—First Fit, Best Fit, Worst Fit and Next Fit—and an adaptive strategy called **HAFS (Hybrid Adaptive Fit Selection)**. Experiments use three workload scales and record allocation outcomes, search cost, fragmentation, timing metrics and adaptive policy transitions.

## Research Question

Can an adaptive memory allocator reduce fragmentation and allocation failures compared with conventional fit strategies?

## Objectives

- Implement First Fit, Best Fit, Worst Fit and Next Fit.
- Generate and evaluate allocation traces with variable request sizes, arrival times and process lifetimes.
- Measure allocation success, failures, search cost, timing and external fragmentation.
- Design an adaptive policy-selection mechanism using memory statistics and allocation outcomes.
- Compare adaptive and conventional strategies across multiple workload scales.
- Preserve raw experimental outputs to support reproducibility and analysis.

## Allocation Strategies

| Strategy | Description |
|---|---|
| First Fit | Selects the first suitable free block encountered during the search. |
| Best Fit | Selects the smallest suitable free block. |
| Worst Fit | Selects the largest suitable free block. |
| Next Fit | Continues searching from the maintained search position. |
| HAFS | Adaptively selects a placement strategy using observed memory and allocation statistics. |

Refer to the source code for the exact implementation and policy-selection rules.

## Implementation

The simulator is implemented in C++17 and models a contiguous memory pool using memory blocks.

The implementation supports:

- Allocation from free memory blocks.
- Splitting free blocks when a request uses only part of a block.
- Merging adjacent free blocks after deallocation.
- Processing allocation and release events.
- Recording per-request allocation outcomes and memory-state snapshots.
- Exporting summary metrics and policy-transition logs.

The exact event ordering, failure classifications, timing boundaries and HAFS thresholds are defined by the source code.

## Experimental Setup

The recorded workload configurations are:

| Workload | Requests | Memory Capacity | Recorded Seed |
|---|---:|---:|---:|
| Small | 300 | 4,096 KB | 101 |
| Medium | 1,500 | 16,384 KB | 202 |
| Large | 5,000 | 65,536 KB | 303 |

**Recorded environment**

- Operating system: Ubuntu 24.04.4 LTS
- Compiler: GCC 13.3.0
- Language standard: C++17
- Execution environment: AWS EC2 Ubuntu instance

The workload CSV files contain request IDs, arrival times, request sizes and lifetimes. See `workloads/` for the recorded traces.

## Experimental Results

The project contains three separate experiment stages:

1. **Baseline:** First Fit, Best Fit, Worst Fit and Next Fit.
2. **Previous HAFS:** The initial adaptive controller with policy-transition logging.
3. **Tuned HAFS:** A subsequent controller experiment that changes policy-switching behavior.

The recorded results show that the previous HAFS version achieved fewer allocation failures than the fixed strategies on the medium and large workloads. However, its search cost was higher than Next Fit's.

The tuned controller reduced search cost on medium and large workloads but produced more allocation failures than the previous HAFS version. These results illustrate the trade-off between allocation success and search overhead.

The results are workload-specific and do not establish that HAFS is universally superior.

## Metrics

The simulator records metrics including:

- Successful and failed allocations.
- Allocation success rate.
- Failure classifications.
- Total, average and maximum search cost.
- Allocation timing measurements.
- Mean external fragmentation.
- Time-weighted external fragmentation, where reported.
- Final free and allocated memory.
- Free-block count and largest free block.
- HAFS policy-switch counts.

The implemented external-fragmentation metric is:

`External Fragmentation = 1 - (Largest Free Block / Total Free Memory)`

When total free memory is zero, the implementation returns zero. Timing and search-cost measurements should be interpreted according to the exact measurement boundaries implemented in the simulator.

## Repository Structure

The repository is intended to organize the source code, workload traces, experiment outputs and documentation as follows:

```text
intelligent-memory-allocation/
├── main.cpp
├── workloads/
│   ├── small.csv
│   ├── medium.csv
│   └── large.csv
├── results_before_hafs/
├── results_hafs_logged/
├── results_hafs_tuned/
├── scripts/
├── docs/
├── .gitignore
└── README.md
```

This is the intended organization. Update the tree to match the actual committed files and avoid including empty or nonexistent directories.

### Result Files

Depending on the experiment, output directories contain:

- `baseline_summary.csv` — aggregated strategy and workload metrics.
- `per_request_results.csv` — individual request outcomes.
- `memory_snapshots.csv` — recorded memory states.
- `policy_transitions.csv` — HAFS policy changes and associated measurements.

Keep the three experiment stages separate so that results from different HAFS versions are not accidentally combined.

## Building and Running

Compile the simulator using GCC with C++17 support:

```bash
g++ -std=c++17 -O2 -Wall -Wextra -pedantic main.cpp -o memory_allocator
```

Run the simulator with an output directory:

```bash
./memory_allocator results
```

**Important:** These commands assume that the committed `main.cpp` accepts an output-directory argument and that the expected `workloads/` CSV files are present. Verify the command-line interface and source-to-results correspondence before publishing this as a reproducibility guarantee.

## Validation and Limitations

The recorded baseline validation notes report checks for:

- Memory-accounting consistency.
- Final memory state and adjacent-block coalescing.
- Failure-classification consistency.
- Fragmentation-metric consistency.
- Agreement between summary and per-request metrics.

These are documented validation results; they should not be interpreted as proof that every possible edge case has been tested.

Known limitations include the timing measurement boundary, Next Fit pointer edge cases, zero-lifetime request behavior and the limited number of workload scales. The HAFS experiments also demonstrate that reducing search cost can come at the expense of allocation success.

## Reproducibility

The repository should include the source code, workload traces, relevant configuration details, raw CSV results and validation documentation.

To reproduce the study:

1. Inspect the workload traces and implementation configuration.
2. Compile the relevant source version using the documented compiler settings.
3. Run each strategy against the same workload trace.
4. Save results to separate output directories.
5. Compare allocation failures, search cost, fragmentation and timing metrics.
6. Verify memory accounting and final-state consistency.

Historical result files should be associated with their corresponding source versions wherever that relationship can be established.

## Academic Integrity and Contributions

This repository supports a college case study. Existing or shared baseline code, individual contributions and externally sourced algorithms must be attributed accurately.

**Individual contribution:** [Describe your actual implementation, integration, experiments, analysis and documentation work.]

**Collaborators:** [Add names and contributions, if applicable.]

## References

Add verified references for the memory-allocation algorithms, external-fragmentation concepts and any related work discussed in the case study. Do not include unverified or fabricated citations.

## License

[Choose an appropriate license after confirming that you have permission to publish all included source code and artifacts.]

---

**Project status:** Experimental implementation and comparative analysis.

**Repository URL:** [Add the GitHub repository URL after creation.]
