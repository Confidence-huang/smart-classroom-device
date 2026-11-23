#!/usr/bin/env bash
# Compile the existing firmware host tests and run the device contract suites.
set -euo pipefail

# Run from ecovision regardless of the caller's working directory.
cd "$(dirname "$0")/../../.."
tests=firmware/stm32/tests
app=firmware/stm32/EcoVision_F413ZH/App
build=$(mktemp -d "${TMPDIR:-/tmp}/ecovision-host.XXXXXXXX")

# Store executables outside the checkout so CI and local runs leave no changes.
trap 'rm -f -- "$build"/*; rmdir -- "$build"' EXIT

run_c_test() {
    local name=$1
    shift
    printf '\n=== %s ===\n' "$name"
    # Keep GCC warnings visible while gating on compilation and test failures.
    cc -std=c11 -Wall -Wextra -O2 "$@" -I "$app/Inc" -o "$build/$name"
    "$build/$name"
}

run_c_test vision_protocol "$tests/test_vision_protocol.c" "$app/Src/vision_protocol.c"
run_c_test net_codec "$tests/test_net_codec.c" "$app/Src/net_codec.c"
run_c_test net_recovery "$tests/test_net_recovery.c" "$app/Src/net_recovery.c"
run_c_test net_publish "$tests/test_net_publish.c" "$app/Src/net_publish.c"

# D01 main predates the D02 control adapter and K230 runtime.
if [[ -f "$app/Src/app_control.c" ]]; then
    run_c_test control_policy "$tests/test_control_policy.c" "$app/Src/control_policy.c"
    run_c_test control_adapter -I "$tests/control_host" \
        "$tests/test_control_adapter.c" "$app/Src/control_policy.c" "$app/Src/app_control.c"
fi

python3 -B -m unittest discover -s tests/integration -p 'test_*.py' -v
if [[ -f firmware/k230/figurine_uart.py ]]; then
    python3 -B -m unittest discover -s tests/vision -p 'test_*.py' -v
fi

run_c_test ir_decode "$tests/test_ir_pulse_decode.c" "$app/Src/ir_pulse_codec.c"
run_c_test ir_encode "$tests/test_ir_pulse_encode.c" "$app/Src/ir_pulse_codec.c"
run_c_test ir_driver -I "$tests/ir_pulse_host" "$tests/test_ir_pulse_driver.c" "$app/Src/app_ir_pulse.c" "$app/Src/ir_pulse_codec.c"
