#pragma once
#include "LexerTypes.h"
#include <set>
#include <vector>
#include <string>

struct ConstTable {
    std::set<std::string> data;

    bool contains(const std::string& elem) const;
    int getIndex(const std::string& elem) const;
    void loadFromFile(const std::string& filePath);
};

struct VarTable {
    struct Entry {
        std::string lexeme;
        LexemeAttributes attributes;
        bool occupied = false;
        bool deleted = false;
    };

    std::size_t table_size;
    std::size_t elements = 0;
    std::vector<Entry> table;
    std::string filename;
    ConstTable reservedWords;

    VarTable(std::string file = "data/variables.txt", std::size_t initial_size = 7);

    std::size_t customHash(const std::string& key) const;
    std::size_t rehash(std::size_t hash, std::size_t attempt) const;
    void checkNewName(const std::string& lexeme) const;
    void checkExistingName(const std::string& lexeme) const;
    void resize();
    void insertInternal(const std::string& lexeme, const LexemeAttributes& attributes);
    void insert(const std::string& lexeme, const LexemeAttributes& attributes);
    void update(const std::string& lexeme, const LexemeAttributes& attributes);
    void remove(const std::string& lexeme);
    bool contains(const std::string& lexeme) const;
    std::size_t getIndex(const std::string& lexeme) const;
    LexemeAttributes getAttributes(const std::string& lexeme) const;
    void saveToFile() const;
    void loadFromFile();
    void print() const;
};
