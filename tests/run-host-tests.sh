#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
build_dir=$(mktemp -d)
trap 'rm -rf "$build_dir"' EXIT
${CXX:-c++} -std=c++17 -O2 -Wall -Wextra -Werror \
  -ISource/GemaLight_ESP_LED_clock_3.8 \
  tests/constraint_scan_test.cpp \
  Source/GemaLight_ESP_LED_clock_3.8/ConstraintScan.cpp \
  -o "$build_dir/constraint-scan-test"
"$build_dir/constraint-scan-test"

for grouped in 0 1; do
  ${CXX:-c++} -std=c++17 -O2 -Wall -Wextra -Werror \
    -DCOC_ENABLE_GROUPING=$grouped -Itests/stubs \
    -ISource/GemaLight_ESP_LED_clock_3.8 \
    tests/display_driver_test.cpp \
    Source/GemaLight_ESP_LED_clock_3.8/ConstraintScan.cpp \
    Source/GemaLight_ESP_LED_clock_3.8/CharlieDisplay.cpp \
    -o "$build_dir/display-driver-$grouped"
  "$build_dir/display-driver-$grouped"
done
