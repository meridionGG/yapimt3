#include "LexerTypes.h"
#include <sstream>

void LexemeAttributes::updateArrayData() {
    array_size = 0;
    array_values.clear();
    if ((type == "array<int>" || type == "array<float>") && !value.empty() && value.front() == '[' && value.back() == ']') {
        std::string inner = value.substr(1, value.size() - 2);
        if (!inner.empty()) {
            std::stringstream ss(inner);
            std::string item;
            while (std::getline(ss, item, ',')) {
                std::size_t first = item.find_first_not_of(" \t");
                if (first != std::string::npos) {
                    std::size_t last = item.find_last_not_of(" \t");
                    array_values.push_back(item.substr(first, last - first + 1));
                }
            }
        }
        array_size = array_values.size();
    }
}
