#pragma once

#include <strings.h>  // strcasecmp

#include <cstddef>

#include "config.h"

/**
 * Manufacturer classification of an aircraft from its ICAO DOC 8643 type
 * designator (the ADS-B "t" field, already trimmed into Aircraft::type).
 *
 * Pure domain logic over the curated code lists in config.h — kept out of the
 * services layer so `services::adsb` stays free of manufacturer knowledge. The
 * isBeluga/isAirbus predicates have signature `bool(*)(const char*)` and are
 * handed to `services::adsb::TypeFilter` via main.cpp, mirroring how the fetch
 * radius and Beluga codes are mediated today.
 */
namespace aircraft {

enum class Manufacturer { kOther, kAirbus, kBoeing, kBeluga };

namespace detail {

/** Case-insensitive exact match of an already-trimmed type against a code list. */
inline bool matchesAny(const char* type, const char* const* codes,
                       size_t count) {
  for (size_t i = 0; i < count; ++i) {
    if (strcasecmp(type, codes[i]) == 0) {
      return true;
    }
  }
  return false;
}

}  // namespace detail

/**
 * Beluga is checked before the generic Airbus list so it keeps its own colour;
 * an empty/unknown type is kOther (cannot be proven to belong to a fleet).
 */
inline Manufacturer classify(const char* type) {
  if (type == nullptr || type[0] == '\0') {
    return Manufacturer::kOther;
  }
  if (detail::matchesAny(type, config::kBelugaTypeCodes,
                         config::kBelugaTypeCodeCount)) {
    return Manufacturer::kBeluga;
  }
  if (detail::matchesAny(type, config::kAirbusTypeCodes,
                         config::kAirbusTypeCodeCount)) {
    return Manufacturer::kAirbus;
  }
  if (detail::matchesAny(type, config::kBoeingTypeCodes,
                         config::kBoeingTypeCodeCount)) {
    return Manufacturer::kBoeing;
  }
  return Manufacturer::kOther;
}

/** TypeFilter predicate: keep only Airbus Beluga types. */
inline bool isBeluga(const char* type) {
  return classify(type) == Manufacturer::kBeluga;
}

/**
 * TypeFilter predicate: keep any Airbus — including the Beluga, which is an
 * Airbus sub-family. (The Beluga still classifies as kBeluga for colouring;
 * only the filter lets it through here.)
 */
inline bool isAirbus(const char* type) {
  const Manufacturer m = classify(type);
  return m == Manufacturer::kAirbus || m == Manufacturer::kBeluga;
}

}  // namespace aircraft
