#include "utils.h"
#include <cctype>
#include <sstream>

using namespace std;

bool ValidName(const string& str) {
    if (str.empty()) return false;
    if (!isalpha((unsigned char)str[0])) return false;
    for (char ch : str) {
        if (!isalnum((unsigned char)ch) && ch != '_') return false;
    }
    return true;
}

bool IsNamedConstant(const string& str) {
    if (str.empty()) return false;
    for (char ch : str) {
        if (!isupper((unsigned char)ch) && ch != '_') return false;
    }
    return true;
}

bool IsInt(const string& s) {
    if (s.empty()) return false;
    size_t i = 0;
    if (s[0] == '-' || s[0] == '+') {
        if (s.size() == 1) return false;
        i = 1;
    }
    for (; i < s.size(); ++i) {
        if (!isdigit((unsigned char)s[i])) return false;
    }
    return true;
}

bool IsFloat(const string& s) {
    if (s.empty()) return false;
    istringstream iss(s);
    float f;
    iss >> noskipws >> f;
    return iss.eof() && !iss.fail();
}

bool IsValidValue(const string& type, const string& value) {
    if (type == "" || type == "-") return true;
    if (type == "int") return IsInt(value);
    if (type == "float") return IsFloat(value);

    if (type == "array<int>" || type == "array<float>") {
        if (value.empty() || value.front() != '[' || value.back() != ']') return false;
        string inner = value.substr(1, value.size() - 2);
        if (inner.empty()) return true;

        stringstream ss(inner);
        string item;
        while (getline(ss, item, ',')) {
            size_t first = item.find_first_not_of(" \t");
            if (first == string::npos) return false;
            size_t last = item.find_last_not_of(" \t");
            string trimmed = item.substr(first, last - first + 1);

            if (type == "array<int>") {
                if (!IsInt(trimmed)) return false;
            }
            else {
                if (!IsFloat(trimmed)) return false;
            }
        }
        return true;
    }
    return false;
}

bool IsValidType(const string& type) {
    return type == "int" || type == "float" || type == "array<int>" || type == "array<float>" || type == "" || type == "-";
}

bool IsPrime(size_t n) {
    if (n < 2) return false;
    if (n == 2 || n == 3) return true;
    if (n % 2 == 0 || n % 3 == 0) return false;
    for (size_t i = 5; i * i <= n; i += 6) {
        if (n % i == 0 || n % (i + 2) == 0) return false;
    }
    return true;
}

size_t NextPrime(size_t n) {
    if (n <= 2) return 2;
    size_t p = n;
    if (p % 2 == 0) p++;
    while (!IsPrime(p)) p += 2;
    return p;
}
