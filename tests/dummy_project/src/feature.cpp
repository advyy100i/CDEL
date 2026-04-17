#include "feature.h"

int feature_value(bool runtime_toggle) {
#ifdef EXPERIMENTAL_FEATURE
  if (runtime_toggle) {
    return 42;
  }
#endif

#ifdef LEGACY_MODE
  return -7;
#else
  return 7;
#endif
}

const char* feature_name() {
#ifdef EXPERIMENTAL_FEATURE
  return "experimental";
#else
  return "stable";
#endif
}
