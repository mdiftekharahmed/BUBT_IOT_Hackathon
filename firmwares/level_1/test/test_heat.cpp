#include <cassert>
#include <cmath>

#include "../src/core/heat.h"

int main() {
  using fastrobox::heatIndexC;
  assert(std::fabs(heatIndexC(25.0f, 60.0f) - 25.0f) < 0.01f);
  assert(std::fabs(heatIndexC(32.2222f, 70.0f) - 41.07f) < 0.1f);
  assert(heatIndexC(40.0f, 90.0f) > 40.0f);
  return 0;
}
