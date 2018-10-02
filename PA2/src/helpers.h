#ifndef HELPERS_H
#define HELPERS_H

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
