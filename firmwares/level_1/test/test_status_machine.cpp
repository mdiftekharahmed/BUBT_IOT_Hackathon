#include <cassert>
#include <cstdint>
#include <cmath>

#include "../src/core/status_machine.h"

using fastrobox::HeatStatus;
using fastrobox::Readings;
using fastrobox::StatusMachine;
using fastrobox::Trend;
using fastrobox::TrendWindow;

int main() {
  StatusMachine machine;
  assert(machine.update({32.0f, 60.0f, true}, 0) == HeatStatus::Safe);
  assert(machine.update({32.0f, 60.0f, true}, 2999) == HeatStatus::Safe);
  assert(machine.update({32.0f, 60.0f, true}, 3000) == HeatStatus::Warning);

  assert(machine.update({40.0f, 60.0f, true}, 4000) == HeatStatus::Warning);
  assert(machine.update({31.0f, 60.0f, true}, 4500) == HeatStatus::Warning);
  assert(machine.update({40.0f, 60.0f, true}, 5000) == HeatStatus::Warning);
  assert(machine.update({40.0f, 60.0f, true}, 7999) == HeatStatus::Warning);
  assert(machine.update({40.0f, 60.0f, true}, 8000) == HeatStatus::Danger);

  assert(machine.update({38.0f, 60.0f, true}, 9000) == HeatStatus::Danger);
  assert(machine.update({38.0f, 60.0f, true}, 12000) == HeatStatus::Warning);
  assert(machine.update({30.0f, 80.0f, true}, 13000) == HeatStatus::Warning);
  assert(machine.update({30.0f, 80.0f, true}, 16000) == HeatStatus::Safe);

  assert(machine.update({40.0f, 60.0f, true}, 17000) == HeatStatus::Safe);
  assert(machine.update({40.0f, 60.0f, false}, 20000) == HeatStatus::Safe);
  assert(machine.update({51.0f, 60.0f, true}, 21000) == HeatStatus::Safe);
  assert(machine.update({NAN, 60.0f, true}, 22000) == HeatStatus::Safe);
  assert(machine.update({29.0f, 60.0f, true}, 22000) == HeatStatus::Safe);

  StatusMachine wraparound;
  const uint32_t start = UINT32_MAX - 1000U;
  assert(wraparound.update({29.0f, 85.0f, true}, start) == HeatStatus::Safe);
  assert(wraparound.update({29.0f, 85.0f, true}, start + 2999U) == HeatStatus::Safe);
  assert(wraparound.update({29.0f, 85.0f, true}, start + 3000U) == HeatStatus::Warning);

  TrendWindow window;
  for (int i = 0; i < 10; ++i) window.add(25.0f + i * 0.1f);
  assert(window.trend() == Trend::Rising);
  for (int i = 0; i < 10; ++i) window.add(26.0f - i * 0.1f);
  assert(window.trend() == Trend::Falling);
  for (int i = 0; i < 10; ++i) window.add(28.0f + (i % 2) * 0.02f);
  assert(window.trend() == Trend::Stable);
  return 0;
}
