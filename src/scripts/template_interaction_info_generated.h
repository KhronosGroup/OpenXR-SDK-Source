// Copyright (c) 2017-2024, The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0
//
// Used in conformance tests.

#include "utilities/feature_availability.h"
#include <openxr/openxr.h>

#include <array>

namespace Conformance {

//# macro make_feature_set(strings)
FeatureSet{
          //# for s in strings
          FeatureBitIndex::BIT_/*{ s }*/,
          //# endfor
}
//# endmacro

//# macro make_avail(conjunctions)
     Availability{
          //#- if conjunctions | length == 1
               /*{ make_feature_set(conjunctions[0]) }*/
          //#- else
               //#- for conj in conjunctions
                    /*{ make_feature_set(conj) }*/,
               //#- endfor
          //#- endif
     }
//# endmacro

//# macro make_qualified_path_entry(user_path, component)
     {
          /*{- (user_path + component.subpath) | quote_string }*/,
          /*{- component.action_type -}*/,
          Avail_/*{- component.availability.as_normalized_symbol() }*/
          //#- if component.system
          , true
          //#- endif
     }
//# endmacro

/// This is a generated list naming combinations of availability expressions used by interaction profiles
enum class InteractionProfileAvailability {
//# for sym, conjunctions in availabilities
     Avail_/*{ sym }*/,
//# endfor
};

/// These are the actual generated availability expressions used by interaction profiles,
/// indexed by InteractionProfileAvailability, so that repeated work identifying satisfied availability expressions
/// can be minimized.
static const std::array<Availability, /*{ availabilities | length }*/> kInteractionAvailabilities{
//# for sym, conjunctions in availabilities
     /*{ make_avail(conjunctions) | collapse_whitespace }*/,
//# endfor
};

/// This is a generated list of all interaction profiles in the order returned by GetAllInteractionProfiles.
enum class InteractionProfileIndex {
//# for path, profile in interaction_profiles.items()
     Profile_/*{ (path | replace("/interaction_profiles/", "") | replace("/", "_") | replace("-", "_")) }*/,
//# endfor
};

}  // namespace Conformance
