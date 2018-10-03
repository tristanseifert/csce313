#ifndef HELPERS_H
#define HELPERS_H

/**
 * Helper functions to remove whitespace from strings
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

// trim from start (copying)
static inline std::string ltrim_copy(std::string s) {
    ltrim(s);
    return s;
}

// trim from end (copying)
static inline std::string rtrim_copy(std::string s) {
    rtrim(s);
    return s;
}

// trim from both ends (copying)
static inline std::string trim_copy(std::string s) {
    trim(s);
    return s;
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

static void replaceAll(std::string& str, const std::string& from, const std::string& to) {
  while(replace(str, from, to) == true) {}
}


#endif
