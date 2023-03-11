//
// Created by marce on 07/03/2023.
//

#pragma once


namespace Math {
    int ScaleRange(int x, int srcFrom, int srcTo, int destFrom, int destTo);

    template<typename A, typename B>
    constexpr auto Min(A a, B b) -> decltype(a < b ? a : b) {
        return a < b ? a : b;
    }

    template<typename A, typename B>
    constexpr auto Max(A a, B b) -> decltype(a > b ? a : b) {
        return a > b ? a : b;
    }
}