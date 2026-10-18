from pathlib import Path

p = Path("main.cpp")
s = p.read_text()

def replace_once(old, new, label):
    global s
    count = s.count(old)
    if count != 1:
        raise SystemExit(
            f"ABORT: expected exactly one match for {label}; found {count}. "
            "main.cpp was not written."
        )
    s = s.replace(old, new, 1)

replace_once(
    "enum class Strategy { FirstFit, BestFit, WorstFit, NextFit };",
    "enum class Strategy { FirstFit, BestFit, WorstFit, NextFit, HAFS };",
    "strategy enum"
)

replace_once(
    "    Strategy::FirstFit, Strategy::BestFit, Strategy::WorstFit, Strategy::NextFit\n};",
    "    Strategy::FirstFit, Strategy::BestFit, Strategy::WorstFit, "
    "Strategy::NextFit, Strategy::HAFS\n};",
    "strategy list"
)

replace_once(
    '        case Strategy::NextFit: return "NextFit";',
    '        case Strategy::NextFit: return "NextFit";\n'
    '        case Strategy::HAFS: return "HAFS";',
    "strategy name"
)

replace_once(
    "    long long requests = 0;\n    long long successes = 0;",
    "    long long requests = 0;\n"
    "    long long policySwitches = 0;\n"
    "    long long successes = 0;",
    "summary switch counter"
)

replace_once(
    "    string workloadName;\n    string strategyNameValue;",
    "    string workloadName;\n"
    "    string strategyNameValue;\n\n"
    "    static constexpr long long HAFS_WINDOW_SIZE = 50;\n"
    "    Strategy hafsPolicy = Strategy::FirstFit;\n"
    "    long long hafsWindowAllocations = 0;\n"
    "    long long hafsWindowFragFailures = 0;\n"
    "    long long hafsWindowSearchCost = 0;\n"
    "    long long hafsWindowsSinceSwitch = 1000;\n"
    "    long long hafsPolicySwitches = 0;",
    "HAFS controller state"
)

replace_once(
    "    double externalFragmentation() const {\n"
    "        long long free = totalFreeMemory();\n"
    "        if (free <= 0) return 0.0;\n"
    "        return 1.0 - static_cast<double>(largestFreeBlock()) / free;\n"
    "    }",
    "    double externalFragmentation() const {\n"
    "        long long free = totalFreeMemory();\n"
    "        if (free <= 0) return 0.0;\n"
    "        return 1.0 - static_cast<double>(largestFreeBlock()) / free;\n"
    "    }\n\n"
    "    void updateHAFSPolicy() {\n"
    "        if (activeStrategy != Strategy::HAFS ||\n"
    "            hafsWindowAllocations < HAFS_WINDOW_SIZE) return;\n\n"
    "        const double frag = externalFragmentation();\n"
    "        const double fragFailureRate =\n"
    "            static_cast<double>(hafsWindowFragFailures) / hafsWindowAllocations;\n"
    "        const double avgSearch =\n"
    "            static_cast<double>(hafsWindowSearchCost) / hafsWindowAllocations;\n"
    "        const double searchFraction =\n"
    "            avgSearch / static_cast<double>(max(1LL, freeBlockCount()));\n\n"
    "        Strategy desired = hafsPolicy;\n"
    "        if (hafsWindowsSinceSwitch >= 2) {\n"
    "            if (frag >= 0.60 || fragFailureRate >= 0.05) {\n"
    "                desired = Strategy::BestFit;\n"
    "            } else if (hafsPolicy == Strategy::BestFit &&\n"
    "                       frag <= 0.40 && fragFailureRate <= 0.01) {\n"
    "                desired = Strategy::FirstFit;\n"
    "            } else if (hafsPolicy == Strategy::NextFit &&\n"
    "                       (frag >= 0.40 || fragFailureRate >= 0.01)) {\n"
    "                desired = Strategy::FirstFit;\n"
    "            } else if (hafsPolicy == Strategy::FirstFit &&\n"
    "                       frag < 0.40 && fragFailureRate < 0.01 &&\n"
    "                       searchFraction >= 0.60) {\n"
    "                desired = Strategy::NextFit;\n"
    "            }\n"
    "        }\n\n"
    "        if (desired != hafsPolicy) {\n"
    "            hafsPolicy = desired;\n"
    "            ++hafsPolicySwitches;\n"
    "            hafsWindowsSinceSwitch = 0;\n"
    "        } else {\n"
    "            ++hafsWindowsSinceSwitch;\n"
    "        }\n\n"
    "        hafsWindowAllocations = 0;\n"
    "        hafsWindowFragFailures = 0;\n"
    "        hafsWindowSearchCost = 0;\n"
    "    }\n\n"
    "    void recordHAFSOutcome(const AllocationOutcome& o) {\n"
    "        if (activeStrategy != Strategy::HAFS) return;\n"
    "        ++hafsWindowAllocations;\n"
    "        hafsWindowSearchCost += o.examined;\n"
    "        if (!o.success && o.failureCause == \"external_fragmentation\")\n"
    "            ++hafsWindowFragFailures;\n"
    "    }",
    "HAFS controller methods"
)

replace_once(
    "    AllocationOutcome allocate(const Request& r) {\n"
    "        AllocationOutcome out;",
    "    AllocationOutcome allocate(const Request& r) {\n"
    "        updateHAFSPolicy();\n"
    "        const Strategy placementStrategy =\n"
    "            activeStrategy == Strategy::HAFS ? hafsPolicy : activeStrategy;\n"
    "        AllocationOutcome out;",
    "allocation policy selection"
)

replace_once(
    "if (activeStrategy == Strategy::FirstFit) {",
    "if (placementStrategy == Strategy::FirstFit) {",
    "First Fit branch"
)
replace_once(
    "} else if (activeStrategy == Strategy::BestFit) {",
    "} else if (placementStrategy == Strategy::BestFit) {",
    "Best Fit branch"
)
replace_once(
    "} else if (activeStrategy == Strategy::WorstFit) {",
    "} else if (placementStrategy == Strategy::WorstFit) {",
    "Worst Fit branch"
)
replace_once(
    "        if (activeStrategy == Strategy::NextFit) {",
    "        if (placementStrategy == Strategy::NextFit) {",
    "Next Fit pointer update"
)

replace_once(
    "                AllocationOutcome o = allocate(r);\n"
    "                totalSearchCost += o.examined;",
    "                AllocationOutcome o = allocate(r);\n"
    "                recordHAFSOutcome(o);\n"
    "                totalSearchCost += o.examined;",
    "HAFS window metrics"
)

replace_once(
    "        s.finalAverageFreeBlockKB = averageFreeBlockSize();\n"
    "        return s;",
    "        s.finalAverageFreeBlockKB = averageFreeBlockSize();\n"
    "        s.policySwitches = hafsPolicySwitches;\n"
    "        return s;",
    "summary policy switch value"
)

replace_once(
    '                  "final_average_free_block_kb\\n";',
    '                  "final_average_free_block_kb,policy_switches\\n";',
    "summary CSV header"
)

replace_once(
    "                   << s.finalLargestFreeKB << ',' << s.finalAverageFreeBlockKB << '\\n';",
    "                   << s.finalLargestFreeKB << ',' << s.finalAverageFreeBlockKB "
    "<< ',' << s.policySwitches << '\\n';",
    "summary CSV row"
)

p.write_text(s)
print("HAFS changes applied successfully.")
