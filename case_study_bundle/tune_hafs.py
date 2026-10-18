from pathlib import Path

p = Path("main.cpp")
s = p.read_text()

old_state = """    long long hafsWindowsSinceSwitch = 1000;
    long long hafsPolicySwitches = 0;
"""

new_state = """    long long hafsWindowsSinceSwitch = 1000;
    long long hafsPolicySwitches = 0;
    long long hafsHighPressureWindows = 0;
"""

old_logic = """        Strategy desired = hafsPolicy;
        if (hafsWindowsSinceSwitch >= 2) {
            if (frag >= 0.60 || fragFailureRate >= 0.05) {
                desired = Strategy::BestFit;
            } else if (hafsPolicy == Strategy::BestFit &&
                       frag <= 0.40 && fragFailureRate <= 0.01) {
                desired = Strategy::FirstFit;
            } else if (hafsPolicy == Strategy::NextFit &&
                       (frag >= 0.40 || fragFailureRate >= 0.01)) {
                desired = Strategy::FirstFit;
            } else if (hafsPolicy == Strategy::FirstFit &&
                       frag < 0.40 && fragFailureRate < 0.01 &&
                       searchFraction >= 0.60) {
                desired = Strategy::NextFit;
            }
        }
"""

new_logic = """        Strategy desired = hafsPolicy;

        // Confirm high pressure across consecutive windows before
        // switching into Best Fit.
        const bool highPressure = (frag >= 0.60 ||
                                   fragFailureRate >= 0.05);

        if (highPressure) {
            ++hafsHighPressureWindows;
        } else {
            hafsHighPressureWindows = 0;
        }

        if (hafsWindowsSinceSwitch >= 2) {
            if (hafsPolicy != Strategy::BestFit &&
                hafsHighPressureWindows >= 2) {
                desired = Strategy::BestFit;
            } else if (hafsPolicy == Strategy::BestFit &&
                       frag <= 0.35 && fragFailureRate <= 0.01) {
                // Lower exit thresholds provide hysteresis.
                desired = Strategy::FirstFit;
            } else if (hafsPolicy == Strategy::NextFit &&
                       (frag >= 0.40 || fragFailureRate >= 0.01)) {
                desired = Strategy::FirstFit;
            } else if (hafsPolicy == Strategy::FirstFit &&
                       frag < 0.40 && fragFailureRate < 0.01 &&
                       searchFraction >= 0.60) {
                desired = Strategy::NextFit;
            }
        }
"""

if s.count(old_state) != 1:
    raise SystemExit(
        f"Expected one state-variable block; found {s.count(old_state)}. No changes saved."
    )

if s.count(old_logic) != 1:
    raise SystemExit(
        f"Expected one policy block; found {s.count(old_logic)}. No changes saved."
    )

s = s.replace(old_state, new_state, 1)
s = s.replace(old_logic, new_logic, 1)

p.write_text(s)
print("HAFS controller patch applied.")
