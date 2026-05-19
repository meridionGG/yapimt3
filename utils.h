#pragma once
#include <string>
#include <cstddef>

bool ValidName(const std::string& str);
bool IsNamedConstant(const std::string& str);
bool IsInt(const std::string& s);
bool IsFloat(const std::string& s);
bool IsValidValue(const std::string& type, const std::string& value);
bool IsValidType(const std::string& type);
bool IsPrime(std::size_t n);
std::size_t NextPrime(std::size_t n);
