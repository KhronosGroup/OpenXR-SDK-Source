
//              Copyright Catch2 Authors
// Distributed under the Boost Software License, Version 1.0.
//   (See accompanying file LICENSE.txt or copy at
//        https://www.boost.org/LICENSE_1_0.txt)

// SPDX-License-Identifier: BSL-1.0

#include <catch2/catch_test_macros.hpp>

TEST_CASE("Simple REQUIRE - 10M") {
    for (size_t i = 0; i < 10'000'000; ++i) {
        REQUIRE(true);
    }
}

TEST_CASE("Simple NOTHROW - 10M") {
    for (size_t i = 0; i < 10'000'000; ++i) {
        REQUIRE_NOTHROW([](){}());
    }
}

TEST_CASE("Simple THROWS - 10M") {
    for (size_t i = 0; i < 10'000'000; ++i) {
        REQUIRE_THROWS([]() { throw 1; }());
    }
}
