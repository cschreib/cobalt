#ifndef TIME_HPP
#define TIME_HPP

#include <chrono>
#include "string.hpp"

// Return the current time [seconds]
double now();

// Return the current date [yymmdd]
std::string today();

// Converts an ammount of time [seconds] to a formatted string hh:mm:ss
std::string time_str(double t);

// Converts an ammount of time [seconds] to a formatted string ss:ms:us:ns
std::string seconds_str(double t);

#endif
