#pragma once
#include "LexerTypes.h"
#include "Tables.h"
#include <string>
#include <vector>

std::vector<Token> tokenizeFile(const std::string& filepath, const ConstTable& alphabet, const ConstTable& reservedWords, const ConstTable& delimiters, VarTable& varTable);
