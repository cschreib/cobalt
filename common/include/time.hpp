#ifndef TIME_HPP
#define TIME_HPP

#include <chrono>
#include <string>

// Return the current time [seconds]
double now();

// Return the current time of the day [seconds]
double time_of_the_day();

// Return the current time of day [hhmmss]
std::string time_of_day_str(const std::string& sep = "");

// Return the current date [yyyymmdd]
std::string today_str(const std::string& sep = "");

// Converts an ammount of time [seconds] to a formatted string hh:mm:ss
std::string time_str(double t);

// Converts an ammount of time [seconds] to a formatted string ss:ms:us:ns
std::string seconds_str(double t);

#endif
