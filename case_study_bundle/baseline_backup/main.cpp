#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <numeric>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

using namespace std;
namespace fs = std::filesystem;

enum class Strategy { FirstFit, BestFit, WorstFit, NextFit };
static const vector<Strategy> STRATEGIES = {
    Strategy::FirstFit, Strategy::BestFit, Strategy::WorstFit, Strategy::NextFit
};

string strategyName(Strategy s) {
    switch (s) {
        case Strategy::FirstFit: return "FirstFit";
        case Strategy::BestFit: return "BestFit";
        case Strategy::WorstFit: return "WorstFit";
        case Strategy::NextFit: return "NextFit";
    }
    return "Unknown";
}

struct Request {
    string id;
    long long arrival;
    long long sizeKB;
    long long lifetime;
};

struct Workload {
    string name;
    long long memoryKB;
    vector<Request> requests;
};

struct Block {
    long long start;
    long long size;
    bool free;
    string owner; // Request ID if allocated; empty if free
};

struct RequestResult {
    string workload;
    string strategy;
    string requestId;
    long long arrival;
    long long sizeKB;
    long long lifetime;
    bool success = false;
    string failureCause;
    long long blocksExamined = 0;
    double latencyMicroseconds = 0.0;
    double externalFragmentationAtAttempt = 0.0;
    long long totalFreeAtAttempt = 0;
    long long largestFreeAtAttempt = 0;
};

struct Snapshot {
    string workload;
    string strategy;
    long long time;
    string event;
    long long totalFree;
    long long totalAllocated;
    long long freeBlockCount;
    long long largestFree;
    double averageFreeBlock;
    double externalFragmentation;
};

struct RunSummary {
    string workload;
    string strategy;
    long long requests = 0;
    long long successes = 0;
    long long failures = 0;
    long long insufficientTotalFailures = 0;
    long long fragmentationFailures = 0;
    long long totalSearchCost = 0;
    long long maxSearchCost = 0;
    double totalLatencyUs = 0.0;
    double averageExternalFragmentation = 0.0;
    double timeWeightedExternalFragmentation = 0.0;
    long long finalFreeKB = 0;
    long long finalAllocatedKB = 0;
    long long finalFreeBlocks = 0;
    long long finalLargestFreeKB = 0;
    double finalAverageFreeBlockKB = 0.0;
};

class MemorySimulator {
private:
    long long capacityKB;
    vector<Block> blocks;
    size_t nextFitStart = 0;
    long long totalSearchCost = 0;
    long long maxSearchCost = 0;
    double totalLatencyUs = 0.0;
    vector<RequestResult> requestResults;
    vector<Snapshot> snapshots;
    string workloadName;
    string strategyNameValue;

    struct AllocationOutcome {
        bool success = false;
        long long examined = 0;
        double latencyUs = 0.0;
        string failureCause;
        long long totalFree = 0;
        long long largestFree = 0;
        double fragmentation = 0.0;
    };

    long long totalFreeMemory() const {
        long long sum = 0;
        for (const auto& b : blocks) if (b.free) sum += b.size;
        return sum;
    }

    long long totalAllocatedMemory() const {
        long long sum = 0;
        for (const auto& b : blocks) if (!b.free) sum += b.size;
        return sum;
    }

    long long freeBlockCount() const {
        long long n = 0;
        for (const auto& b : blocks) if (b.free) ++n;
        return n;
    }

    long long largestFreeBlock() const {
        long long largest = 0;
        for (const auto& b : blocks) if (b.free) largest = max(largest, b.size);
        return largest;
    }

    double averageFreeBlockSize() const {
        long long n = freeBlockCount();
        return n == 0 ? 0.0 : static_cast<double>(totalFreeMemory()) / n;
    }

    double externalFragmentation() const {
        long long free = totalFreeMemory();
        if (free <= 0) return 0.0;
        return 1.0 - static_cast<double>(largestFreeBlock()) / free;
    }

    void mergeAdjacentFreeBlocks() {
        vector<Block> merged;
        merged.reserve(blocks.size());
        for (const auto& b : blocks) {
            if (!merged.empty() && merged.back().free && b.free) {
                merged.back().size += b.size;
            } else {
                merged.push_back(b);
            }
        }
        blocks.swap(merged);
        if (blocks.empty()) nextFitStart = 0;
        else if (nextFitStart >= blocks.size()) nextFitStart = 0;
    }

    void recordSnapshot(long long time, const string& event) {
        snapshots.push_back({
            workloadName, strategyNameValue, time, event,
            totalFreeMemory(), totalAllocatedMemory(), freeBlockCount(),
            largestFreeBlock(), averageFreeBlockSize(), externalFragmentation()
        });
    }

    AllocationOutcome allocate(const Request& r) {
        AllocationOutcome out;
        out.totalFree = totalFreeMemory();
        out.largestFree = largestFreeBlock();
        out.fragmentation = externalFragmentation();

        auto startTime = chrono::steady_clock::now();
        const size_t n = blocks.size();
        long long chosen = -1;

        if (r.sizeKB <= 0) {
            out.failureCause = "invalid_size";
            auto endTime = chrono::steady_clock::now();
            out.latencyUs = chrono::duration<double, micro>(endTime - startTime).count();
            return out;
        }

        if (STRATEGIES.size() == 0) { // Defensive; never reached.
            out.failureCause = "no_strategy";
            return out;
        }

        // The caller sets activeStrategy immediately before this method.
        if (activeStrategy == Strategy::FirstFit) {
            for (size_t i = 0; i < n; ++i) {
                if (!blocks[i].free) continue;
                ++out.examined;
                if (blocks[i].size >= r.sizeKB) { chosen = static_cast<long long>(i); break; }
            }
        } else if (activeStrategy == Strategy::BestFit) {
            long long bestWaste = numeric_limits<long long>::max();
            for (size_t i = 0; i < n; ++i) {
                if (!blocks[i].free) continue;
                ++out.examined;
                if (blocks[i].size >= r.sizeKB && blocks[i].size - r.sizeKB < bestWaste) {
                    bestWaste = blocks[i].size - r.sizeKB;
                    chosen = static_cast<long long>(i);
                }
            }
        } else if (activeStrategy == Strategy::WorstFit) {
            long long largest = -1;
            for (size_t i = 0; i < n; ++i) {
                if (!blocks[i].free) continue;
                ++out.examined;
                if (blocks[i].size >= r.sizeKB && blocks[i].size > largest) {
                    largest = blocks[i].size;
                    chosen = static_cast<long long>(i);
                }
            }
        } else { // Next Fit
            if (n > 0) {
                size_t start = min(nextFitStart, n - 1);
                for (size_t step = 0; step < n; ++step) {
                    size_t i = (start + step) % n;
                    if (!blocks[i].free) continue;
                    ++out.examined;
                    if (blocks[i].size >= r.sizeKB) {
                        chosen = static_cast<long long>(i);
                        break;
                    }
                }
            }
        }

        auto endTime = chrono::steady_clock::now();
        out.latencyUs = chrono::duration<double, micro>(endTime - startTime).count();

        if (chosen < 0) {
            if (out.totalFree >= r.sizeKB && out.largestFree < r.sizeKB)
                out.failureCause = "external_fragmentation";
            else
                out.failureCause = "insufficient_total_memory";
            return out;
        }

        size_t idx = static_cast<size_t>(chosen);
        Block original = blocks[idx];
        Block allocated{original.start, r.sizeKB, false, r.id};
        if (original.size == r.sizeKB) {
            blocks[idx] = allocated;
        } else {
            Block remainder{original.start + r.sizeKB, original.size - r.sizeKB, true, ""};
            blocks[idx] = allocated;
            blocks.insert(blocks.begin() + static_cast<long long>(idx + 1), remainder);
        }

        if (activeStrategy == Strategy::NextFit) {
            // Continue at the remainder if it exists; otherwise at the next list position.
            size_t newN = blocks.size();
            nextFitStart = newN == 0 ? 0 : min(idx + 1, newN - 1);
        } else if (nextFitStart >= blocks.size()) {
            nextFitStart = 0;
        }

        out.success = true;
        return out;
    }

    void deallocate(const string& id, long long time) {
        bool found = false;
        for (auto& b : blocks) {
            if (!b.free && b.owner == id) {
                b.free = true;
                b.owner.clear();
                found = true;
                break;
            }
        }
        if (found) {
            mergeAdjacentFreeBlocks();
            recordSnapshot(time, "FREE:" + id);
        }
    }

public:
    Strategy activeStrategy = Strategy::FirstFit;

    MemorySimulator(long long memoryKB, string workload, Strategy strategy)
        : capacityKB(memoryKB), workloadName(std::move(workload)),
          strategyNameValue(strategyName(strategy)) {
        if (capacityKB <= 0) throw invalid_argument("Memory capacity must be positive.");
        blocks.push_back({0, capacityKB, true, ""});
        activeStrategy = strategy;
        recordSnapshot(0, "INITIAL");
    }

    RunSummary run(const Workload& workload) {
        struct Event {
            long long time;
            int type; // 0=release, 1=arrival (release processed first)
            size_t requestIndex;
        };
        vector<Event> events;
        for (size_t i = 0; i < workload.requests.size(); ++i) {
            const auto& r = workload.requests[i];
            if (r.arrival < 0 || r.lifetime < 0)
                throw invalid_argument("Arrival and lifetime must be non-negative.");
            events.push_back({r.arrival, 1, i});
            if (r.lifetime > 0)
                events.push_back({r.arrival + r.lifetime, 0, i});
        }
        sort(events.begin(), events.end(), [&](const Event& a, const Event& b) {
            if (a.time != b.time) return a.time < b.time;
            if (a.type != b.type) return a.type < b.type;
            const auto& ra = workload.requests[a.requestIndex];
            const auto& rb = workload.requests[b.requestIndex];
            if (ra.id != rb.id) return ra.id < rb.id;
            return a.requestIndex < b.requestIndex;
        });

        unordered_map<string, bool> allocatedSuccessfully;
        unordered_map<string, long long> endTimeById;
        for (const auto& r : workload.requests) {
            if (r.id.empty()) throw invalid_argument("Request IDs cannot be empty.");
            if (endTimeById.count(r.id)) throw invalid_argument("Duplicate request ID: " + r.id);
            endTimeById[r.id] = r.arrival + r.lifetime;
        }

        // Stable event semantics: releases first at a timestamp, then arrivals.
        // Failed allocations never get release events in practice because release checks ownership.
        for (const auto& e : events) {
            const auto& r = workload.requests[e.requestIndex];
            if (e.type == 0) {
                if (allocatedSuccessfully[r.id]) deallocate(r.id, e.time);
            } else {
                AllocationOutcome o = allocate(r);
                totalSearchCost += o.examined;
                maxSearchCost = max(maxSearchCost, o.examined);
                totalLatencyUs += o.latencyUs;
                requestResults.push_back({
                    workloadName, strategyNameValue, r.id, r.arrival, r.sizeKB, r.lifetime,
                    o.success, o.failureCause, o.examined, o.latencyUs,
                    o.fragmentation, o.totalFree, o.largestFree
                });
                if (o.success) allocatedSuccessfully[r.id] = true;
                recordSnapshot(e.time, o.success ? "ALLOC_SUCCESS:" + r.id : "ALLOC_FAIL:" + r.id);
            }
        }

        RunSummary s;
        s.workload = workloadName;
        s.strategy = strategyNameValue;
        s.requests = static_cast<long long>(workload.requests.size());
        s.totalSearchCost = totalSearchCost;
        s.maxSearchCost = maxSearchCost;
        s.totalLatencyUs = totalLatencyUs;

        long long fragFailures = 0, insuffFailures = 0, successes = 0;
        double fragSum = 0.0;
        for (const auto& rr : requestResults) {
            if (rr.success) ++successes;
            else if (rr.failureCause == "external_fragmentation") ++fragFailures;
            else ++insuffFailures;
            fragSum += rr.externalFragmentationAtAttempt;
        }
        s.successes = successes;
        s.failures = s.requests - successes;
        s.fragmentationFailures = fragFailures;
        s.insufficientTotalFailures = insuffFailures;
        s.averageExternalFragmentation = s.requests == 0 ? 0.0 : fragSum / s.requests;

        // Time-weighted fragmentation across recorded state intervals.
        // Snapshot events at the same timestamp have zero duration.
        if (snapshots.size() >= 2) {
            long long weightedDuration = 0;
            double weightedFrag = 0.0;
            for (size_t i = 0; i + 1 < snapshots.size(); ++i) {
                long long dt = max(0LL, snapshots[i + 1].time - snapshots[i].time);
                weightedDuration += dt;
                weightedFrag += snapshots[i].externalFragmentation * static_cast<double>(dt);
            }
            s.timeWeightedExternalFragmentation =
                weightedDuration == 0 ? s.averageExternalFragmentation : weightedFrag / weightedDuration;
        } else s.timeWeightedExternalFragmentation = s.averageExternalFragmentation;

        s.finalFreeKB = totalFreeMemory();
        s.finalAllocatedKB = totalAllocatedMemory();
        s.finalFreeBlocks = freeBlockCount();
        s.finalLargestFreeKB = largestFreeBlock();
        s.finalAverageFreeBlockKB = averageFreeBlockSize();
        return s;
    }

    const vector<RequestResult>& getRequestResults() const { return requestResults; }
    const vector<Snapshot>& getSnapshots() const { return snapshots; }
};

Workload generateWorkload(const string& name, long long memoryKB, int count,
                          uint32_t seed, double meanInterarrival = 1.0) {
    if (count < 0) throw invalid_argument("Request count cannot be negative.");
    mt19937 rng(seed);
    // Mix small, medium and occasional large requests.
    uniform_real_distribution<double> category(0.0, 1.0);
    uniform_int_distribution<long long> smallSize(max(1LL, memoryKB / 100), max(2LL, memoryKB / 20));
    uniform_int_distribution<long long> mediumSize(max(2LL, memoryKB / 30), max(3LL, memoryKB / 8));
    uniform_int_distribution<long long> largeSize(max(4LL, memoryKB / 12), max(5LL, memoryKB / 3));
    uniform_int_distribution<long long> lifetimeDist(2, 30);
    exponential_distribution<double> interarrival(1.0 / max(0.1, meanInterarrival));

    Workload w{name, memoryKB, {}};
    long long arrival = 0;
    for (int i = 0; i < count; ++i) {
        double c = category(rng);
        long long size;
        if (c < 0.60) size = smallSize(rng);
        else if (c < 0.90) size = mediumSize(rng);
        else size = largeSize(rng);
        size = min(size, memoryKB); // no request exceeds total physical memory
        long long lifetime = lifetimeDist(rng);
        if (i > 0) arrival += max(1LL, static_cast<long long>(round(interarrival(rng))));
        w.requests.push_back({"P" + to_string(i + 1), arrival, size, lifetime});
    }
    return w;
}

void writeWorkloadCSV(const Workload& w, const string& path) {
    ofstream out(path);
    if (!out) throw runtime_error("Cannot write " + path);
    out << "request_id,arrival_time,size_kb,lifetime\n";
    for (const auto& r : w.requests)
        out << r.id << ',' << r.arrival << ',' << r.sizeKB << ',' << r.lifetime << '\n';
}

Workload readWorkloadCSV(const string& path, long long memoryKB, const string& name) {
    ifstream in(path);
    if (!in) throw runtime_error("Cannot open workload file: " + path);
    string line;
    getline(in, line); // header
    Workload w{name, memoryKB, {}};
    while (getline(in, line)) {
        if (line.empty()) continue;
        stringstream ss(line);
        string id, arrivalStr, sizeStr, lifeStr;
        if (!getline(ss, id, ',') || !getline(ss, arrivalStr, ',') ||
            !getline(ss, sizeStr, ',') || !getline(ss, lifeStr, ','))
            throw runtime_error("Malformed workload CSV line: " + line);
        w.requests.push_back({id, stoll(arrivalStr), stoll(sizeStr), stoll(lifeStr)});
    }
    return w;
}

void writeResults(const string& outDir, const vector<RunSummary>& summaries,
                  const vector<RequestResult>& requests, const vector<Snapshot>& snapshots) {
    fs::create_directories(outDir);
    ofstream summaryOut(outDir + "/baseline_summary.csv");
    summaryOut << "workload,strategy,requests,successes,failures,success_rate_percent,"
                  "insufficient_total_failures,external_fragmentation_failures,"
                  "total_search_cost,average_search_cost,max_search_cost,"
                  "total_allocation_latency_us,average_allocation_latency_us,"
                  "mean_external_fragmentation_at_attempt,time_weighted_external_fragmentation,"
                  "final_free_kb,final_allocated_kb,final_free_blocks,final_largest_free_kb,"
                  "final_average_free_block_kb\n";
    summaryOut << fixed << setprecision(6);
    for (const auto& s : summaries) {
        double successRate = s.requests == 0 ? 0.0 : 100.0 * s.successes / s.requests;
        double avgSearch = s.requests == 0 ? 0.0 : static_cast<double>(s.totalSearchCost) / s.requests;
        double avgLatency = s.requests == 0 ? 0.0 : s.totalLatencyUs / s.requests;
        summaryOut << s.workload << ',' << s.strategy << ',' << s.requests << ',' << s.successes << ','
                   << s.failures << ',' << successRate << ',' << s.insufficientTotalFailures << ','
                   << s.fragmentationFailures << ',' << s.totalSearchCost << ',' << avgSearch << ','
                   << s.maxSearchCost << ',' << s.totalLatencyUs << ',' << avgLatency << ','
                   << s.averageExternalFragmentation << ',' << s.timeWeightedExternalFragmentation << ','
                   << s.finalFreeKB << ',' << s.finalAllocatedKB << ',' << s.finalFreeBlocks << ','
                   << s.finalLargestFreeKB << ',' << s.finalAverageFreeBlockKB << '\n';
    }

    ofstream reqOut(outDir + "/per_request_results.csv");
    reqOut << "workload,strategy,request_id,arrival_time,size_kb,lifetime,success,failure_cause,"
              "blocks_examined,latency_us,external_fragmentation_at_attempt,total_free_at_attempt,"
              "largest_free_at_attempt\n";
    reqOut << fixed << setprecision(6);
    for (const auto& r : requests)
        reqOut << r.workload << ',' << r.strategy << ',' << r.requestId << ',' << r.arrival << ','
               << r.sizeKB << ',' << r.lifetime << ',' << (r.success ? 1 : 0) << ','
               << r.failureCause << ',' << r.blocksExamined << ',' << r.latencyMicroseconds << ','
               << r.externalFragmentationAtAttempt << ',' << r.totalFreeAtAttempt << ','
               << r.largestFreeAtAttempt << '\n';

    ofstream snapOut(outDir + "/memory_snapshots.csv");
    snapOut << "workload,strategy,time,event,total_free_kb,total_allocated_kb,free_block_count,"
               "largest_free_kb,average_free_block_kb,external_fragmentation\n";
    snapOut << fixed << setprecision(6);
    for (const auto& s : snapshots)
        snapOut << s.workload << ',' << s.strategy << ',' << s.time << ',' << s.event << ','
                << s.totalFree << ',' << s.totalAllocated << ',' << s.freeBlockCount << ','
                << s.largestFree << ',' << s.averageFreeBlock << ',' << s.externalFragmentation << '\n';
}

int main(int argc, char* argv[]) {
    try {
        string outDir = "results";
        if (argc >= 2) outDir = argv[1];
        fs::create_directories("workloads");
        fs::create_directories(outDir);

        struct Config { string name; long long memoryKB; int requests; uint32_t seed; double interarrival; };
        vector<Config> configs = {
            {"small", 4096, 300, 101, 1.8},
            {"medium", 16384, 1500, 202, 1.2},
            {"large", 65536, 5000, 303, 0.8}
        };

        vector<RunSummary> summaries;
        vector<RequestResult> allRequestResults;
        vector<Snapshot> allSnapshots;

        for (const auto& cfg : configs) {
            Workload w = generateWorkload(cfg.name, cfg.memoryKB, cfg.requests, cfg.seed, cfg.interarrival);
            string workloadPath = "workloads/" + cfg.name + ".csv";
            writeWorkloadCSV(w, workloadPath);
            cout << "Generated " << workloadPath << " (" << w.requests.size()
                 << " requests, memory=" << cfg.memoryKB << " KB)\n";

            for (Strategy strategy : STRATEGIES) {
                MemorySimulator sim(cfg.memoryKB, cfg.name, strategy);
                RunSummary s = sim.run(w);
                summaries.push_back(s);
                allRequestResults.insert(allRequestResults.end(),
                    sim.getRequestResults().begin(), sim.getRequestResults().end());
                allSnapshots.insert(allSnapshots.end(),
                    sim.getSnapshots().begin(), sim.getSnapshots().end());
                cout << left << setw(8) << cfg.name << setw(10) << strategyName(strategy)
                     << " success=" << s.successes << '/' << s.requests
                     << " failures=" << s.failures
                     << " search=" << s.totalSearchCost
                     << " avg_frag=" << fixed << setprecision(4)
                     << s.averageExternalFragmentation << '\n';
            }
        }

        writeResults(outDir, summaries, allRequestResults, allSnapshots);
        cout << "\nFinished. Results written to " << outDir << "/\n"
             << "  baseline_summary.csv\n"
             << "  per_request_results.csv\n"
             << "  memory_snapshots.csv\n"
             << "Workloads are in workloads/.\n";
    } catch (const exception& e) {
        cerr << "ERROR: " << e.what() << '\n';
        return 1;
    }
    return 0;
}
