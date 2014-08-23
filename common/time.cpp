#include "time.hpp"
#include "string.hpp"

double now() {
    return std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch()
    ).count()*1e-6;
}

double time_of_the_day() {
    std::time_t t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm tm = *std::localtime(&t);
    return tm.tm_sec + tm.tm_min*60 + tm.tm_hour*3600;
}

std::string time_of_day_str(const std::string& sep) {
    std::time_t t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm tm = *std::localtime(&t);
    return string::convert(tm.tm_hour,2)+sep+string::convert(tm.tm_min,2)+sep
        +string::convert(tm.tm_sec,2);
}

std::string today_str(const std::string& sep) {
    std::time_t t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm tm = *std::localtime(&t);
    return string::convert(tm.tm_year+1900,4)+sep+string::convert(tm.tm_mon+1, 2)+sep
       +string::convert(tm.tm_mday,2);
}

std::string time_str(double t) {
    std::string date;

    if (t < 1.0) {
        int mag = floor(log10(t));
        if (mag >= -3) {
            return string::convert(round(t*1e3))+"ms";
        } else if (mag >= -6) {
            return string::convert(round(t*1e6))+"us";
        } else {
            return string::convert(round(t*1e9))+"ns";
        }
    } else {
        std::size_t day  = floor(t/(24*60*60));
        std::size_t hour = floor(t/(60*60)) - day*24;
        std::size_t min  = floor(t/60) - day*24*60 - hour*60;
        std::size_t sec  = floor(t) - day*24*60*60 - hour*60*60 - min*60;

        if (day  != 0) date += string::convert(day)+'d';
        if (hour != 0) date += string::convert(hour,2)+'h';
        if (min  != 0) date += string::convert(min,2)+'m';
        date += string::convert(sec,2)+'s';

        if (date[0] == '0' && date.size() != 2) {
            date.erase(0,1);
        }
    }

    return date;
}

std::string seconds_str(double t) {
    std::string date;

    std::size_t sec = floor(t);
    std::size_t ms  = floor((t - sec)*1000);
    std::size_t us  = floor(((t - sec)*1000 - ms)*1000);
    std::size_t ns  = floor((((t - sec)*1000 - ms)*1000 - us)*1000);

    if (sec != 0)                  date += string::convert(sec)+"s";
    if (ms  != 0 || !date.empty()) date += string::convert(ms,3)+"ms";
    if (us  != 0 || !date.empty()) date += string::convert(us,3)+"us";
    date += string::convert(ns,3)+"ns";

    while (date[0] == '0' && date.size() != 3) {
        date.erase(0,1);
    }

    return date;
}
