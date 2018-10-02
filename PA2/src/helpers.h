#ifndef HELPERS_H
#define HELPERS_H

/**
 * Removes whitespace at the start and end of a string.
 *
 * See https://stackoverflow.com/questions/216823/
 */
 // trim from start (in place)
 static inline void ltrim(std::string &s) {
     s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](int ch) {
         return !std::isspace(ch);
     }));
 }

 // trim from end (in place)
 static inline void rtrim(std::string &s) {
     s.erase(std::find_if(s.rbegin(), s.rend(), [](int ch) {
         return !std::isspace(ch);
     }).base(), s.end());
 }

 // trim from both ends (in place)
 static inline void trim(std::string &s) {
     ltrim(s);
     rtrim(s);
 }


/**
 * Replaces all occurrences of `from` with `to` in `str`.
 */
static bool replace(std::string& str, const std::string& from, const std::string& to) {
    size_t start_pos = str.find(from);
    if(start_pos == std::string::npos)
        return false;
    str.replace(start_pos, from.length(), to);
    return true;
}


#endif
