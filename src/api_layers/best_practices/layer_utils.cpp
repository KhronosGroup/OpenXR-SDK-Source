// Copyright (c) 2025 The Khronos Group Inc.
// Copyright (c) 2025 Collabora, Ltd.
//
// SPDX-License-Identifier: Apache-2.0

#include "layer_utils.h"

void LockedDispatchTable::Reset(std::unique_ptr<XrGeneratedDispatchTable> &&newTable) {
    std::unique_lock<std::mutex> lock{m_mutex};
    m_dispatch = std::move(newTable);
}
