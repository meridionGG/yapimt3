#pragma once
#include <cstddef>
#include <string>
#include <vector>

enum class TokenType {
    SYMBOL = 1,
    OPERATION = 2,
    KEYWORD = 3,
    DELIMITER = 4,
    IDENTIFIER = 5,
    NAMED_CONSTANT = 6,
    UNNAMED_CONSTANT = 7
};

struct Token {
    TokenType type;
    std::size_t index;
    std::size_t line;
    std::size_t col;
};

enum class State {
    START,
    IN_ID,
    IN_NUMBER,
    IN_OPERATOR,
    IN_COMMENT,
    PANIC
};

struct LexemeAttributes {
    std::string type;
    std::string value;
    bool is_constant = false;
    bool is_named = false;
    std::size_t array_size = 0;
    std::vector<std::string> array_values;

    void updateArrayData();
};
