#ifndef HELPER_HPP
#define HELPER_HPP


#include <limits>
#include <cmath>


static bool isClose(float val, float target, float acceptedErr = std::numeric_limits<float>::epsilon()) {
	return std::abs(target - val) <= acceptedErr;
}


#endif
