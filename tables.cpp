#include "tables.h"
#include "utils.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <sstream>
#include <cctype>

using namespace std;

bool ConstTable::contains(const string& elem) const {
    return data.find(elem) != data.end();
}

int ConstTable::getIndex(const string& elem) const {
    auto it = data.find(elem);
    if (it != data.end()) {
        return distance(data.begin(), it);
    }
    return -1;
}

void ConstTable::loadFromFile(const string& filePath) {
    ifstream file(filePath);
    if (!file.is_open()) throw runtime_error("Can't open file " + filePath);
    string line;
    while (getline(file, line)) {
        if (!line.empty()) data.insert(line);
    }
}

VarTable::VarTable(string file, size_t initial_size)
    : table_size(NextPrime(initial_size)), table(table_size), filename(file) {
    reservedWords.loadFromFile("config/reserved_words.txt");
}

size_t VarTable::customHash(const string& key) const {
    size_t hash = 0;
    const size_t prime = 31;
    for (char ch : key) {
        hash = hash * prime + ch;
    }
    return hash % table_size;
}

size_t VarTable::rehash(size_t hash, size_t attempt) const {
    return (hash + attempt * attempt) % table_size;
}

void VarTable::checkNewName(const string& lexeme) const {
    bool is_unnamed_const = IsInt(lexeme) || IsFloat(lexeme);
    if (!is_unnamed_const) {
        if (!ValidName(lexeme)) {
            throw runtime_error("Invalid lexeme name format: " + lexeme);
        }
        if (reservedWords.contains(lexeme)) {
            throw runtime_error("Lexeme is a reserved word: " + lexeme);
        }
    }
    if (contains(lexeme)) {
        throw runtime_error("Lexeme already exists: " + lexeme);
    }
}

void VarTable::checkExistingName(const string& lexeme) const {
    if (!contains(lexeme)) {
        throw runtime_error("Lexeme not found: " + lexeme);
    }
}

void VarTable::resize() {
    size_t old_size = table_size;
    vector<Entry> old_table = table;

    table_size = NextPrime(old_size * 2);
    table = vector<Entry>(table_size);
    elements = 0;

    for (const auto& entry : old_table) {
        if (entry.occupied) {
            insertInternal(entry.lexeme, entry.attributes);
        }
    }
}

void VarTable::insertInternal(const string& lexeme, const LexemeAttributes& attributes) {
    size_t hash = customHash(lexeme);
    size_t attempt = 0;
    while (table[hash].occupied) {
        hash = rehash(customHash(lexeme), ++attempt);
    }
    LexemeAttributes attr = attributes;
    attr.updateArrayData();
    table[hash] = { lexeme, attr, true, false };
    elements++;
}

void VarTable::insert(const string& lexeme, const LexemeAttributes& attributes) {
    checkNewName(lexeme);
    if (!IsValidType(attributes.type)) {
        throw runtime_error("Invalid type: " + attributes.type);
    }
    if (!IsValidValue(attributes.type, attributes.value)) {
        throw runtime_error("Value '" + attributes.value + "' is not valid for type '" + attributes.type + "'");
    }

    if (elements + 1 > table_size / 2) {
        resize();
    }

    size_t hash = customHash(lexeme);
    size_t attempt = 0;
    int firstDeleted = -1;

    while (table[hash].occupied || table[hash].deleted) {
        if (table[hash].deleted && firstDeleted == -1) {
            firstDeleted = hash;
        }
        hash = rehash(customHash(lexeme), ++attempt);
    }

    size_t insertionIndex = (firstDeleted != -1) ? (size_t)firstDeleted : hash;
    LexemeAttributes attr = attributes;
    attr.updateArrayData();
    table[insertionIndex] = { lexeme, attr, true, false };
    elements++;
    saveToFile();
}

void VarTable::update(const string& lexeme, const LexemeAttributes& attributes) {
    checkExistingName(lexeme);
    if (!IsValidType(attributes.type)) {
        throw runtime_error("Invalid type: " + attributes.type);
    }
    if (!IsValidValue(attributes.type, attributes.value)) {
        throw runtime_error("Value '" + attributes.value + "' is not valid for type '" + attributes.type + "'");
    }
    size_t hash = customHash(lexeme);
    size_t attempt = 0;
    while (table[hash].occupied || table[hash].deleted) {
        if (table[hash].occupied && table[hash].lexeme == lexeme) {
            LexemeAttributes attr = attributes;
            attr.updateArrayData();
            table[hash].attributes = attr;
            saveToFile();
            return;
        }
        hash = rehash(customHash(lexeme), ++attempt);
    }
}

void VarTable::remove(const string& lexeme) {
    checkExistingName(lexeme);
    size_t hash = customHash(lexeme);
    size_t attempt = 0;
    while (table[hash].occupied || table[hash].deleted) {
        if (table[hash].occupied && table[hash].lexeme == lexeme) {
            table[hash].occupied = false;
            table[hash].deleted = true;
            elements--;
            saveToFile();
            return;
        }
        hash = rehash(customHash(lexeme), ++attempt);
    }
}

bool VarTable::contains(const string& lexeme) const {
    size_t hash = customHash(lexeme);
    size_t attempt = 0;
    while (table[hash].occupied || table[hash].deleted) {
        if (table[hash].occupied && table[hash].lexeme == lexeme) return true;
        hash = rehash(customHash(lexeme), ++attempt);
    }
    return false;
}

size_t VarTable::getIndex(const string& lexeme) const {
    size_t hash = customHash(lexeme);
    size_t attempt = 0;
    while (table[hash].occupied || table[hash].deleted) {
        if (table[hash].occupied && table[hash].lexeme == lexeme) return hash;
        hash = rehash(customHash(lexeme), ++attempt);
    }
    throw runtime_error("Lexeme not found: " + lexeme);
}

LexemeAttributes VarTable::getAttributes(const string& lexeme) const {
    size_t hash = customHash(lexeme);
    size_t attempt = 0;
    while (table[hash].occupied || table[hash].deleted) {
        if (table[hash].occupied && table[hash].lexeme == lexeme) return table[hash].attributes;
        hash = rehash(customHash(lexeme), ++attempt);
    }
    throw runtime_error("Lexeme not found: " + lexeme);
}

void VarTable::saveToFile() const {
    ofstream file(filename);
    if (!file.is_open()) throw runtime_error("Can't open file " + filename);
    for (const auto& entry : table) {
        if (entry.occupied) {
            file << entry.lexeme << " " << entry.attributes.type << " " << entry.attributes.value << "\n";
        }
    }
}

void VarTable::loadFromFile() {
    ifstream file(filename);
    if (!file.is_open()) return;
    string line;
    while (getline(file, line)) {
        istringstream iss(line);
        string lexeme, type, value;
        iss >> lexeme;
        string temp;
        type = "";
        while (iss >> temp) {
            if (value.empty() && (temp == "int" || temp == "float" || temp.find("array") == 0)) {
                type = temp;
            }
            else {
                value += (value.empty() ? "" : " ") + temp;
            }
        }
        if (!lexeme.empty()) {
            LexemeAttributes attr = { type, value };
            if (elements + 1 > table_size / 2) {
                resize();
            }
            insertInternal(lexeme, attr);
        }
    }
}

void VarTable::print() const {
    for (const auto& entry : table) {
        if (entry.occupied) {
            cout << "Lexeme: " << entry.lexeme << ", Type: " << entry.attributes.type << ", Value: " << entry.attributes.value << endl;
        }
    }
}
