// Copyright (c) 2017-2024, The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0
//
// Used in conformance tests.

#include "utilities/feature_availability.h"
#include "interaction_info.h"

namespace Conformance {

//# macro make_qualified_path_entry(user_path, component)
    InputSourcePathAvailData{
        /*{ (user_path + component.subpath) | quote_string }*/,
        /*{ component.action_type }*/,
        InteractionProfileAvailability::Avail_/*{- component.availability.as_normalized_symbol() }*/
        //# if component.system
        , true
        //# endif
    }
//# endmacro

const std::vector<InteractionProfileAvailMetadata>& GetAllInteractionProfiles() {
    //
    // Generated lists of component paths for interaction profiles, with metadata and availability expressions.
    //
//# for path, profile in interaction_profiles.items()

    static const InputSourcePathAvailCollection /*{'c' + (path | replace("/", "_")) }*/{
//# for component in profile.components.values()
//# for user_path in component.valid_user_paths
        /*{ make_qualified_path_entry(user_path, component) | collapse_whitespace }*/,
//# endfor
//# endfor
    };

//# endfor

    //
    // Generated list of all known interaction profiles and metadata, referring to paths defined in the preceding section.
    //

    static const std::vector<InteractionProfileAvailMetadata> cAllProfiles{
//# for path, profile in interaction_profiles.items()
        {
            /*{ profile.name | quote_string }*/,
            /*{ profile.name | replace("/interaction_profiles/", "") | quote_string }*/,
            {
                //# for user_path in profile.valid_user_paths
                /*{ user_path | quote_string }*/,
                //# endfor
            },
            InteractionProfileAvailability::Avail_/*{- profile.availability.as_normalized_symbol() -}*/,
            /*{'c' + (path |replace("/", "_")) }*/
        },
//# endfor
    };

    return cAllProfiles;
}

} // namespace Conformance
