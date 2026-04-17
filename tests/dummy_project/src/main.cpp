#include <iostream>

#include "feature.h"

int main() {
  bool runtime_toggle = false;
  std::cout << "Feature name: " << feature_name() << "\n";
  std::cout << "Feature value: " << feature_value(runtime_toggle) << "\n";
  return 0;
}
