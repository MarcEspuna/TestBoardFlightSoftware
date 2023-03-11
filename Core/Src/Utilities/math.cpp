//
// Created by marce on 07/03/2023.
//


#include "Utilities/math.h"


namespace {

    int ScaleRange(int x, int srcFrom, int srcTo, int destFrom, int destTo)
    {
        long int a = ((long int) destTo - (long int) destFrom) * ((long int) x - (long int) srcFrom);
        long int b = (long int) srcTo - (long int) srcFrom;
        return (a / b) + destFrom;
    }




}


