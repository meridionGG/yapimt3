#include <iostream>
#include <fstream>
#include <string>
#include <set>
#include <vector>
#include <stdexcept>
#include <sstream>
#include <cctype>
#include <iterator>
#include <algorithm>
#include <climits>

using namespace std;

// Типы лексем (определяют номер словаря для токена)
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
    size_t index;
    size_t line;
    size_t col;
};

// Состояния лексического автомата (DFA)
enum class State {
    START,
    IN_ID,
    IN_NUMBER,
    IN_OPERATOR,
    IN_COMMENT,
    PANIC
};

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

// Структура для постоянных таблиц (алфавит, ключевые слова, разделители)
struct ConstTable {
    set<string> data;

    bool contains(const string& elem) const {
        return data.find(elem) != data.end();
    }

    int getIndex(const string& elem) const {
        auto it = data.find(elem);
        if (it != data.end()) {
            return distance(data.begin(), it);
        }
        return -1;
    }

    void loadFromFile(const string& filePath) {
        ifstream file(filePath);
        if (!file.is_open()) throw runtime_error("Can't open file " + filePath);
        string line;
        while (getline(file, line)) {
            if (!line.empty()) data.insert(line);
        }
    }
};

// Структура для атрибутов лексемы (тип: int/float/array<int>/array<float>; значение: string)
struct LexemeAttributes {
    string type; // int, float, array<int>, array<float>
    string value; // Значение или для массивов "[val1,val2]"
    bool is_constant = false;
    bool is_named = false;
    size_t array_size = 0;
    vector<string> array_values;

    void updateArrayData() {
        array_size = 0;
        array_values.clear();
        if ((type == "array<int>" || type == "array<float>") && !value.empty() && value.front() == '[' && value.back() == ']') {
            string inner = value.substr(1, value.size() - 2);
            if (!inner.empty()) {
                stringstream ss(inner);
                string item;
                while (getline(ss, item, ',')) {
                    size_t first = item.find_first_not_of(" \t");
                    if (first != string::npos) {
                        size_t last = item.find_last_not_of(" \t");
                        array_values.push_back(item.substr(first, last - first + 1));
                    }
                }
            }
            array_size = array_values.size();
        }
    }
};

// Структура для переменных таблиц (идентификаторы и константы)
struct VarTable {
    struct Entry {
        string lexeme;
        LexemeAttributes attributes;
        bool occupied = false;
        bool deleted = false;
    };

    size_t table_size;
    size_t elements = 0;
    vector<Entry> table;
    string filename;
    ConstTable reservedWords;

    VarTable(string file = "variables.txt", size_t initial_size = 7)
        : table_size(NextPrime(initial_size)), filename(file), table(table_size) {
        reservedWords.loadFromFile("reserved_words.txt");
    }

    // Хэш-функция для строк (алгоритм Горнера с простым числом 31)
    size_t customHash(const string& key) const {
        size_t hash = 0;
        const size_t prime = 31;
        for (char ch : key) {
            hash = hash * prime + ch;
        }
        return hash % table_size;
    }

    // Квадратичное пробирование для разрешения коллизий
    size_t rehash(size_t hash, size_t attempt) const {
        return (hash + attempt * attempt) % table_size;
    }

    void checkNewName(const string& lexeme) const {
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

    void checkExistingName(const string& lexeme) const {
        if (!contains(lexeme)) {
            throw runtime_error("Lexeme not found: " + lexeme);
        }
    }

    // Динамическое увеличение и перехеширование таблицы при заполнении > 50%
    void resize() {
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

    void insertInternal(const string& lexeme, const LexemeAttributes& attributes) {
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

    void insert(const string& lexeme, const LexemeAttributes& attributes) {
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

    void update(const string& lexeme, const LexemeAttributes& attributes) {
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

    void remove(const string& lexeme) {
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

    bool contains(const string& lexeme) const {
        size_t hash = customHash(lexeme);
        size_t attempt = 0;
        while (table[hash].occupied || table[hash].deleted) {
            if (table[hash].occupied && table[hash].lexeme == lexeme) return true;
            hash = rehash(customHash(lexeme), ++attempt);
        }
        return false;
    }

    size_t getIndex(const string& lexeme) const {
        size_t hash = customHash(lexeme);
        size_t attempt = 0;
        while (table[hash].occupied || table[hash].deleted) {
            if (table[hash].occupied && table[hash].lexeme == lexeme) return hash;
            hash = rehash(customHash(lexeme), ++attempt);
        }
        throw runtime_error("Lexeme not found: " + lexeme);
    }

    LexemeAttributes getAttributes(const string& lexeme) const {
        size_t hash = customHash(lexeme);
        size_t attempt = 0;
        while (table[hash].occupied || table[hash].deleted) {
            if (table[hash].occupied && table[hash].lexeme == lexeme) return table[hash].attributes;
            hash = rehash(customHash(lexeme), ++attempt);
        }
        throw runtime_error("Lexeme not found: " + lexeme);
    }

    void saveToFile() const {
        ofstream file(filename);
        if (!file.is_open()) throw runtime_error("Can't open file " + filename);
        for (const auto& entry : table) {
            if (entry.occupied) {
                file << entry.lexeme << " " << entry.attributes.type << " " << entry.attributes.value << "\n";
            }
        }
    }

    void loadFromFile() {
        ifstream file(filename);
        if (!file.is_open()) return; // Файл может не существовать
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

    void print() const {
        for (const auto& entry : table) {
            if (entry.occupied) {
                cout << "Lexeme: " << entry.lexeme << ", Type: " << entry.attributes.type << ", Value: " << entry.attributes.value << endl;
            }
        }
    }
};

// Главная функция лексического анализа: читает файл посимвольно и реализует конечный автомат
vector<Token> tokenizeFile(const string& filepath, const ConstTable& alphabet, const ConstTable& reservedWords, const ConstTable& delimiters, VarTable& varTable) {
    ifstream file(filepath);
    if (!file.is_open()) {
        cerr << "Cannot open file " << filepath << endl;
        return {};
    }

    ofstream tokensOut("tokens.txt");
    ofstream errorsOut("errors.txt");
    vector<Token> tokens;

    State state = State::START;
    string buffer = "";
    size_t current_line = 1;
    size_t current_col = 0;
    size_t start_col = 0;

    // Проверка, является ли символ "безопасным разделителем" (конец идентификатора или числа)
    auto isSafeBreak = [&](char ch) {
        return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n' ||
            delimiters.contains(string(1, ch)) ||
            ch == '=' || ch == '!' || ch == '<' || ch == '>' ||
            ch == '+' || ch == '-' || ch == '*' || ch == '/';
        };

    auto processBuffer = [&]() {
        if (state == State::IN_ID) {
            bool ok = true;
            for (char b : buffer) {
                if (isalpha((unsigned char)b)) {
                    if (!alphabet.contains(string(1, b)) && !alphabet.contains(string(1, (char)tolower((unsigned char)b)))) {
                        ok = false;
                    }
                }
            }
            if (!ok) {
                errorsOut << "[Line " << current_line << ", Col " << start_col << "] Lexical Error: Invalid alphabet characters in identifier '" << buffer << "'\n";
            }
            else {
                TokenType t_type = TokenType::IDENTIFIER;
                size_t table_index = 0;
                if (reservedWords.contains(buffer)) {
                    t_type = TokenType::KEYWORD;
                    int idx = reservedWords.getIndex(buffer);
                    table_index = (size_t)(idx < 0 ? 0 : idx);
                }
                else {
                    if (IsNamedConstant(buffer)) t_type = TokenType::NAMED_CONSTANT;
                    try {
                        if (!varTable.contains(buffer)) {
                            LexemeAttributes attr;
                            attr.is_named = (t_type == TokenType::NAMED_CONSTANT);
                            attr.is_constant = attr.is_named;
                            attr.type = "-"; // unknown type at this stage
                            attr.value = "-"; // unknown value
                            varTable.insert(buffer, attr);
                        }
                        table_index = varTable.getIndex(buffer);
                    }
                    catch (exception& e) {
                        errorsOut << "[Line " << current_line << ", Col " << start_col << "] Lexical Error: " << e.what() << " in '" << buffer << "'\n";
                    }
                }
                tokens.push_back({ t_type, table_index, current_line, start_col });
            }
        }
        else if (state == State::IN_NUMBER) {
            size_t table_index = 0;
            try {
                if (!varTable.contains(buffer)) {
                    LexemeAttributes attr;
                    attr.type = (buffer.find('.') != string::npos) ? "float" : "int";
                    attr.value = buffer;
                    attr.is_constant = true;
                    varTable.insert(buffer, attr);
                }
                table_index = varTable.getIndex(buffer);
            }
            catch (exception& e) {
                errorsOut << "[Line " << current_line << ", Col " << start_col << "] Lexical Error: " << e.what() << " in '" << buffer << "'\n";
            }
            tokens.push_back({ TokenType::UNNAMED_CONSTANT, table_index, current_line, start_col });
        }
        else if (state == State::IN_OPERATOR) {
            if (buffer == "!") {
                errorsOut << "[Line " << current_line << ", Col " << start_col << "] Lexical Error: Unknown operator '!'\n";
            }
            else {
                int idx = delimiters.getIndex(buffer);
                size_t table_index = (size_t)(idx < 0 ? 0 : idx);
                tokens.push_back({ TokenType::OPERATION, table_index, current_line, start_col });
            }
        }
        };

    char c;
    while (file.get(c)) {
        current_col++;
        bool process_char = true;

        while (process_char) {
            process_char = false;

            switch (state) {
            case State::START: {
                if (c == ' ' || c == '\t' || c == '\r') {
                    // ignore
                }
                else if (c == '\n') {
                    current_line++;
                    current_col = 0;
                }
                else if (c == '/') {
                    char next_c;
                    if (file.get(next_c)) {
                        if (next_c == '/') {
                            state = State::IN_COMMENT;
                            buffer = "//";
                        }
                        else if (next_c == '*') {
                            state = State::IN_COMMENT;
                            buffer = "/*";
                        }
                        else {
                            file.unget();
                            buffer = c;
                            state = State::IN_OPERATOR;
                            start_col = current_col;
                        }
                    }
                    else {
                        buffer = c;
                        state = State::IN_OPERATOR;
                        start_col = current_col;
                    }
                }
                else if (isalpha((unsigned char)c)) {
                    buffer = c;
                    start_col = current_col;
                    state = State::IN_ID;
                }
                else if (isdigit((unsigned char)c)) {
                    buffer = c;
                    start_col = current_col;
                    state = State::IN_NUMBER;
                }
                else if (c == '=' || c == '!' || c == '<' || c == '>' || c == '+' || c == '-' || c == '*') {
                    buffer = c;
                    start_col = current_col;
                    state = State::IN_OPERATOR;
                }
                else if (delimiters.contains(string(1, c))) {
                    int index = delimiters.getIndex(string(1, c));
                    tokens.push_back({ TokenType::DELIMITER, (size_t)(index < 0 ? 0 : index), current_line, current_col });
                }
                else {
                    errorsOut << "[Line " << current_line << ", Col " << current_col << "] Lexical Error: Unknown symbol '" << c << "'. Switching to PANIC mode.\n";
                    state = State::PANIC;
                    buffer = c;
                }
                break;
            }
            case State::IN_COMMENT: {
                if (buffer == "//") {
                    if (c == '\n') {
                        current_line++;
                        current_col = 0;
                        state = State::START;
                        buffer = "";
                    }
                }
                else if (buffer == "/*") {
                    if (c == '*') {
                        char next_c;
                        if (file.get(next_c)) {
                            if (next_c == '/') {
                                state = State::START;
                                buffer = "";
                            }
                            else {
                                file.unget();
                            }
                        }
                    }
                    else if (c == '\n') {
                        current_line++;
                        current_col = 0;
                    }
                }
                break;
            }
            case State::IN_ID: {
                if (isalnum((unsigned char)c) || c == '_' || c == '$') {
                    buffer += c;
                }
                else if (isSafeBreak(c)) {
                    processBuffer();
                    state = State::START;
                    buffer = "";
                    process_char = true;
                }
                else {
                    buffer += c;
                    errorsOut << "[Line " << current_line << ", Col " << start_col << "] Lexical Error: Invalid character '" << c << "' in identifier '" << buffer << "'. Switching to PANIC mode.\n";
                    state = State::PANIC;
                }
                break;
            }
            case State::IN_NUMBER: {
                if (isdigit((unsigned char)c) || c == '.') {
                    buffer += c;
                }
                else if (isSafeBreak(c)) {
                    processBuffer();
                    state = State::START;
                    buffer = "";
                    process_char = true;
                }
                else {
                    buffer += c;
                    errorsOut << "[Line " << current_line << ", Col " << start_col << "] Lexical Error: Invalid character '" << c << "' in number '" << buffer << "'. Switching to PANIC mode.\n";
                    state = State::PANIC;
                }
                break;
            }
            case State::IN_OPERATOR: {
                if ((buffer == "=" || buffer == "!") && c == '=') {
                    buffer += c;
                    int idx = delimiters.getIndex(buffer);
                    size_t table_index = (size_t)(idx < 0 ? 0 : idx);
                    tokens.push_back({ TokenType::OPERATION, table_index, current_line, start_col });
                    state = State::START;
                    buffer = "";
                }
                else {
                    processBuffer();
                    state = State::START;
                    buffer = "";
                    process_char = true;
                }
                break;
            }
            case State::PANIC: {
                if (isSafeBreak(c)) {
                    state = State::START;
                    buffer = "";
                    process_char = true; // Let START handle the space or delimiter
                }
                // otherwise keep skipping
                break;
            }
            }
        }
    }

    if (state == State::IN_ID || state == State::IN_NUMBER || state == State::IN_OPERATOR) {
        processBuffer();
    }

    if (!tokens.empty()) {
        size_t current_print_line = tokens[0].line;
        tokensOut << current_print_line << ":";
        for (const auto& t : tokens) {
            if (t.line != current_print_line) {
                current_print_line = t.line;
                tokensOut << "\n" << current_print_line << ":";
            }
            tokensOut << " (" << (int)t.type << ", " << t.index << ")";
        }
        tokensOut << "\n";
    }

    cout << "Tokenization complete. Check tokens.txt and errors.txt.\n";
    return tokens;
}


// ---------------------------------------------------------
// Класс синтаксического анализатора (LL(1))
// ---------------------------------------------------------
#include <stack>
#include <map>

enum class Symbol {
    // Non-terminals
    Program, StmtList, Stmt, Type, DeclTail, Expr, RelOpTail, RelOp, 
    SimpleExpr, AddOpTail, AddOp, Term, MulOpTail, Factor, ArrayElems, ArrayElemsTail,
    // Terminals
    T_INT, T_FLOAT, T_ID, T_CONST, T_ASSIGN, T_SEMI, T_LBRACKET, T_RBRACKET, 
    T_EQ, T_NEQ, T_LESS, T_GREATER, T_PLUS, T_MINUS, T_MUL, T_COMMA, T_EOF,
    // Epsilon
    EPS
};

enum class Action { NONE, DO_ASSIGN, DO_ADD, DO_MUL, DO_REL, DO_AEM };

struct StateRow {
    vector<Symbol> terminals;
    int jump;
    bool accept;
    bool stack;
    bool return_flag;
    bool error;
    Action action;
};

class SyntaxAnalyzer {
private:
    vector<Token> tokens;
    const ConstTable& reservedWords;
    const ConstTable& delimiters;
    const VarTable& varTable;
    
    vector<StateRow> parseTable;
    stack<int> stateStack;
    stack<string> opStack;
    vector<string> poliz;
    
    string getTokenString(const Token& t) {
        if (t.type == TokenType::KEYWORD) {
            auto it = reservedWords.data.begin(); advance(it, t.index); return *it;
        }
        if (t.type == TokenType::DELIMITER || t.type == TokenType::OPERATION) {
            auto it = delimiters.data.begin(); advance(it, t.index); return *it;
        }
        if (t.type == TokenType::IDENTIFIER || t.type == TokenType::NAMED_CONSTANT || t.type == TokenType::UNNAMED_CONSTANT) {
            return varTable.table[t.index].lexeme;
        }
        return "";
    }

    Symbol tokenToSymbol(const Token& t) {
        if (t.type == TokenType::KEYWORD) {
            string w = getTokenString(t);
            if (w == "int") return Symbol::T_INT;
            if (w == "float") return Symbol::T_FLOAT;
        }
        if (t.type == TokenType::IDENTIFIER || t.type == TokenType::NAMED_CONSTANT) return Symbol::T_ID;
        if (t.type == TokenType::UNNAMED_CONSTANT) return Symbol::T_CONST;
        if (t.type == TokenType::DELIMITER || t.type == TokenType::OPERATION) {
            string w = getTokenString(t);
            if (w == "=") return Symbol::T_ASSIGN;
            if (w == ";") return Symbol::T_SEMI;
            if (w == "[") return Symbol::T_LBRACKET;
            if (w == "]") return Symbol::T_RBRACKET;
            if (w == "==") return Symbol::T_EQ;
            if (w == "!=") return Symbol::T_NEQ;
            if (w == "<") return Symbol::T_LESS;
            if (w == ">") return Symbol::T_GREATER;
            if (w == "+") return Symbol::T_PLUS;
            if (w == "-") return Symbol::T_MINUS;
            if (w == "*") return Symbol::T_MUL;
            if (w == ",") return Symbol::T_COMMA;
        }
        return Symbol::EPS;
    }

    string symToStr(Symbol s) {
        switch (s) {
            case Symbol::T_INT: return "int"; case Symbol::T_FLOAT: return "float";
            case Symbol::T_ID: return "id"; case Symbol::T_CONST: return "const";
            case Symbol::T_ASSIGN: return "="; case Symbol::T_SEMI: return ";";
            case Symbol::T_LBRACKET: return "["; case Symbol::T_RBRACKET: return "]";
            case Symbol::T_EQ: return "=="; case Symbol::T_NEQ: return "!=";
            case Symbol::T_LESS: return "<"; case Symbol::T_GREATER: return ">";
            case Symbol::T_PLUS: return "+"; case Symbol::T_MINUS: return "-";
            case Symbol::T_MUL: return "*"; case Symbol::T_COMMA: return ",";
            case Symbol::T_EOF: return "EOF";
            default: return "Unknown";
        }
    }

public:
    SyntaxAnalyzer(const vector<Token>& tks, const ConstTable& rw, const ConstTable& delims, const VarTable& vt)
        : tokens(tks), reservedWords(rw), delimiters(delims), varTable(vt) {
        
        parseTable.resize(93);

        auto addState = [&](int idx, vector<Symbol> terms, int jump, bool accept, bool stack, bool ret, bool err, Action act = Action::NONE) {
            parseTable[idx] = {terms, jump, accept, stack, ret, err, act};
        };

        int S_StmtList = 3, S_Stmt = 8, S_Type = 19, S_DeclTail = 23;
        int S_Expr = 33, S_RelOpTail = 36, S_RelOp = 42, S_SimpleExpr = 50;
        int S_AddOpTail = 53, S_AddOp = 60, S_Term = 64, S_MulOpTail = 67;
        int S_Factor = 74, S_ArrayElems = 82, S_ArrayElemsTail = 87;

        // Program
        addState(1, {Symbol::T_INT, Symbol::T_FLOAT, Symbol::T_ID, Symbol::T_EOF}, 2, false, false, false, true);
        addState(2, {}, S_StmtList, false, false, true, true);

        // StmtList
        addState(3, {Symbol::T_INT, Symbol::T_FLOAT, Symbol::T_ID}, 5, false, false, false, false);
        addState(4, {Symbol::T_EOF}, 7, false, false, false, true);
        addState(5, {}, S_Stmt, false, true, false, true);
        addState(6, {}, S_StmtList, false, false, true, true);
        addState(7, {Symbol::T_EOF}, 0, false, false, true, true);

        // Stmt
        addState(8, {Symbol::T_INT, Symbol::T_FLOAT}, 10, false, false, false, false);
        addState(9, {Symbol::T_ID}, 14, false, false, false, true);
        addState(10, {}, S_Type, false, true, false, true);
        addState(11, {Symbol::T_ID}, 12, true, false, false, true);
        addState(12, {}, S_DeclTail, false, true, false, true);
        addState(13, {Symbol::T_SEMI}, 0, true, false, true, true);
        addState(14, {Symbol::T_ID}, 15, true, false, false, true);
        addState(15, {Symbol::T_ASSIGN}, 16, true, false, false, true);
        addState(16, {}, S_Expr, false, true, false, true);
        addState(17, {}, 18, false, false, false, true, Action::DO_ASSIGN);
        addState(18, {Symbol::T_SEMI}, 0, true, false, true, true);

        // Type
        addState(19, {Symbol::T_INT}, 21, false, false, false, false);
        addState(20, {Symbol::T_FLOAT}, 22, false, false, false, true);
        addState(21, {Symbol::T_INT}, 0, true, false, true, true);
        addState(22, {Symbol::T_FLOAT}, 0, true, false, true, true);

        // DeclTail
        addState(23, {Symbol::T_ASSIGN}, 26, false, false, false, false);
        addState(24, {Symbol::T_LBRACKET}, 29, false, false, false, false);
        addState(25, {Symbol::T_SEMI}, 32, false, false, false, true);
        addState(26, {Symbol::T_ASSIGN}, 27, true, false, false, true);
        addState(27, {}, S_Expr, false, true, false, true);
        addState(28, {}, 0, false, false, true, true, Action::DO_ASSIGN);
        addState(29, {Symbol::T_LBRACKET}, 30, true, false, false, true);
        addState(30, {Symbol::T_CONST}, 31, true, false, false, true);
        addState(31, {Symbol::T_RBRACKET}, 0, true, false, true, true);
        addState(32, {Symbol::T_SEMI}, 0, false, false, true, true);

        // Expr
        addState(33, {Symbol::T_ID, Symbol::T_CONST, Symbol::T_LBRACKET}, 34, false, false, false, true);
        addState(34, {}, S_SimpleExpr, false, true, false, true);
        addState(35, {}, S_RelOpTail, false, false, true, true);

        // RelOpTail
        addState(36, {Symbol::T_EQ, Symbol::T_NEQ, Symbol::T_LESS, Symbol::T_GREATER}, 38, false, false, false, false);
        addState(37, {Symbol::T_SEMI, Symbol::T_RBRACKET, Symbol::T_COMMA}, 41, false, false, false, true);
        addState(38, {}, S_RelOp, false, true, false, true);
        addState(39, {}, S_SimpleExpr, false, true, false, true);
        addState(40, {}, 0, false, false, true, true, Action::DO_REL);
        addState(41, {Symbol::T_SEMI, Symbol::T_RBRACKET, Symbol::T_COMMA}, 0, false, false, true, true);

        // RelOp
        addState(42, {Symbol::T_EQ}, 46, false, false, false, false);
        addState(43, {Symbol::T_NEQ}, 47, false, false, false, false);
        addState(44, {Symbol::T_LESS}, 48, false, false, false, false);
        addState(45, {Symbol::T_GREATER}, 49, false, false, false, true);
        addState(46, {Symbol::T_EQ}, 0, true, false, true, true);
        addState(47, {Symbol::T_NEQ}, 0, true, false, true, true);
        addState(48, {Symbol::T_LESS}, 0, true, false, true, true);
        addState(49, {Symbol::T_GREATER}, 0, true, false, true, true);

        // SimpleExpr
        addState(50, {Symbol::T_ID, Symbol::T_CONST, Symbol::T_LBRACKET}, 51, false, false, false, true);
        addState(51, {}, S_Term, false, true, false, true);
        addState(52, {}, S_AddOpTail, false, false, true, true);

        // AddOpTail
        addState(53, {Symbol::T_PLUS, Symbol::T_MINUS}, 55, false, false, false, false);
        addState(54, {Symbol::T_SEMI, Symbol::T_RBRACKET, Symbol::T_COMMA, Symbol::T_EQ, Symbol::T_NEQ, Symbol::T_LESS, Symbol::T_GREATER}, 59, false, false, false, true);
        addState(55, {}, S_AddOp, false, true, false, true);
        addState(56, {}, S_Term, false, true, false, true);
        addState(57, {}, 58, false, false, false, true, Action::DO_ADD);
        addState(58, {}, S_AddOpTail, false, false, true, true);
        addState(59, {Symbol::T_SEMI, Symbol::T_RBRACKET, Symbol::T_COMMA, Symbol::T_EQ, Symbol::T_NEQ, Symbol::T_LESS, Symbol::T_GREATER}, 0, false, false, true, true);

        // AddOp
        addState(60, {Symbol::T_PLUS}, 62, false, false, false, false);
        addState(61, {Symbol::T_MINUS}, 63, false, false, false, true);
        addState(62, {Symbol::T_PLUS}, 0, true, false, true, true);
        addState(63, {Symbol::T_MINUS}, 0, true, false, true, true);

        // Term
        addState(64, {Symbol::T_ID, Symbol::T_CONST, Symbol::T_LBRACKET}, 65, false, false, false, true);
        addState(65, {}, S_Factor, false, true, false, true);
        addState(66, {}, S_MulOpTail, false, false, true, true);

        // MulOpTail
        addState(67, {Symbol::T_MUL}, 69, false, false, false, false);
        addState(68, {Symbol::T_PLUS, Symbol::T_MINUS, Symbol::T_SEMI, Symbol::T_RBRACKET, Symbol::T_COMMA, Symbol::T_EQ, Symbol::T_NEQ, Symbol::T_LESS, Symbol::T_GREATER}, 73, false, false, false, true);
        addState(69, {Symbol::T_MUL}, 70, true, false, false, true);
        addState(70, {}, S_Factor, false, true, false, true);
        addState(71, {}, 72, false, false, false, true, Action::DO_MUL);
        addState(72, {}, S_MulOpTail, false, false, true, true);
        addState(73, {Symbol::T_PLUS, Symbol::T_MINUS, Symbol::T_SEMI, Symbol::T_RBRACKET, Symbol::T_COMMA, Symbol::T_EQ, Symbol::T_NEQ, Symbol::T_LESS, Symbol::T_GREATER}, 0, false, false, true, true);

        // Factor
        addState(74, {Symbol::T_ID}, 77, false, false, false, false);
        addState(75, {Symbol::T_CONST}, 78, false, false, false, false);
        addState(76, {Symbol::T_LBRACKET}, 79, false, false, false, true);
        addState(77, {Symbol::T_ID}, 0, true, false, true, true);
        addState(78, {Symbol::T_CONST}, 0, true, false, true, true);
        addState(79, {Symbol::T_LBRACKET}, 80, true, false, false, true);
        addState(80, {}, S_ArrayElems, false, true, false, true);
        addState(81, {Symbol::T_RBRACKET}, 0, true, false, true, true, Action::DO_AEM);

        // ArrayElems
        addState(82, {Symbol::T_ID, Symbol::T_CONST, Symbol::T_LBRACKET}, 84, false, false, false, false);
        addState(83, {Symbol::T_RBRACKET}, 86, false, false, false, true);
        addState(84, {}, S_Expr, false, true, false, true);
        addState(85, {}, S_ArrayElemsTail, false, false, true, true);
        addState(86, {Symbol::T_RBRACKET}, 0, false, false, true, true);

        // ArrayElemsTail
        addState(87, {Symbol::T_COMMA}, 89, false, false, false, false);
        addState(88, {Symbol::T_RBRACKET}, 92, false, false, false, true);
        addState(89, {Symbol::T_COMMA}, 90, true, false, false, true);
        addState(90, {}, S_Expr, false, true, false, true);
        addState(91, {}, S_ArrayElemsTail, false, false, true, true);
        addState(92, {Symbol::T_RBRACKET}, 0, false, false, true, true);
    }

    vector<string> parse() {
        int currentState = 1;
        size_t ip = 0;
        bool error = false;
        ofstream errOut("errors.txt", ios::app);
        
        auto getSym = [&](size_t i) { return (i < tokens.size()) ? tokenToSymbol(tokens[i]) : Symbol::T_EOF; };

        while (true) {
            if (currentState <= 0 || currentState >= parseTable.size()) break;

            StateRow row = parseTable[currentState];
            Symbol token = getSym(ip);

            // Step 1
            bool match = row.terminals.empty() || (std::find(row.terminals.begin(), row.terminals.end(), token) != row.terminals.end());

            if (!match) {
                // Step 2
                if (row.error) {
                    error = true;
                    string lineCol = (ip < tokens.size()) ? ("[Line " + to_string(tokens[ip].line) + ", Col " + to_string(tokens[ip].col) + "] ") : "[EOF] ";
                    errOut << lineCol << "Syntax Error: Unexpected token " << symToStr(token) << "\n";
                    
                    // Panic mode
                    while (ip < tokens.size() && token != Symbol::T_SEMI && token != Symbol::T_EOF) {
                        ip++;
                        token = getSym(ip);
                    }
                    if (token == Symbol::T_SEMI) {
                        ip++;
                        while (!stateStack.empty()) stateStack.pop();
                        currentState = 3; // return to StmtList
                        continue;
                    } else {
                        break;
                    }
                } else {
                    currentState++;
                    continue;
                }
            }

            // Step 3
            if (row.accept) {
                string lexeme = (ip < tokens.size()) ? getTokenString(tokens[ip]) : "";
                if (token == Symbol::T_ID || token == Symbol::T_CONST) {
                    poliz.push_back(lexeme);
                } else if (token == Symbol::T_PLUS || token == Symbol::T_MINUS || token == Symbol::T_MUL || 
                           token == Symbol::T_EQ || token == Symbol::T_NEQ || token == Symbol::T_LESS || token == Symbol::T_GREATER) {
                    opStack.push(lexeme);
                }
                ip++;
            }

            // Execute Actions
            if (row.action == Action::DO_ASSIGN) {
                poliz.push_back("=");
            } else if (row.action == Action::DO_ADD || row.action == Action::DO_MUL || row.action == Action::DO_REL) {
                if (!opStack.empty()) {
                    poliz.push_back(opStack.top());
                    opStack.pop();
                }
            } else if (row.action == Action::DO_AEM) {
                poliz.push_back("АЭМ");
            }

            // Step 4
            if (row.stack) {
                stateStack.push(currentState + 1);
            }

            // Step 5
            if (row.jump > 0) {
                currentState = row.jump;
                continue;
            }

            // Step 6
            if (row.return_flag) {
                if (stateStack.empty()) break;
                currentState = stateStack.top();
                stateStack.pop();
                continue;
            }

            // Step 7
            if (row.jump == 0 && !row.return_flag) {
                currentState++;
            }
        }

        cout << "\n=== Syntax Analysis (State Machine LL(1)) ===\n";
        if (error) cout << "Syntax errors found. Check errors.txt\n";
        else cout << "Syntax analysis successful.\n";
        
        cout << "POLIZ: ";
        ofstream pOut("poliz.txt");
        for (const auto& p : poliz) { cout << p << " "; pOut << p << " "; }
        cout << "\n"; pOut << "\n";
        return poliz;
    }
};

// Виртуальная машина (Интерпретатор ПОЛИЗа)
void executePoliz(const vector<string>& poliz, VarTable& varTable) {
    stack<string> runtimeStack;
    
    auto isNumeric = [](const string& s) {
        if (s.empty()) return false;
        size_t start = (s[0] == '-' || s[0] == '+') ? 1 : 0;
        if (start == 1 && s.size() == 1) return false;
        for (size_t i = start; i < s.size(); ++i) {
            if (!isdigit((unsigned char)s[i]) && s[i] != '.') return false;
        }
        return true;
    };

    for (const string& token : poliz) {
        if (token == "АЭМ") {
            if (runtimeStack.size() < 2) throw runtime_error("Runtime Error: Stack underflow for АЭМ");
            string indexStr = runtimeStack.top(); runtimeStack.pop();
            string arrayName = runtimeStack.top(); runtimeStack.pop();
            
            int idx = 0;
            if (isNumeric(indexStr)) {
                idx = stoi(indexStr);
            } else {
                auto attr = varTable.getAttributes(indexStr);
                if (attr.value == "-") throw runtime_error("Runtime Error: Uninitialized array index '" + indexStr + "'");
                idx = stoi(attr.value);
            }
            
            auto arrAttr = varTable.getAttributes(arrayName);
            if (idx < 0 || (size_t)idx >= arrAttr.array_size) {
                throw runtime_error("Runtime Error: Array index out of bounds (" + to_string(idx) + ") for array '" + arrayName + "'");
            }
            
            string val = arrAttr.array_values[idx];
            runtimeStack.push(val);
        }
        else if (token == "+" || token == "-" || token == "*") {
            if (runtimeStack.size() < 2) throw runtime_error("Runtime Error: Stack underflow for operation " + token);
            string rightStr = runtimeStack.top(); runtimeStack.pop();
            string leftStr = runtimeStack.top(); runtimeStack.pop();
            
            auto resolveVal = [&](const string& s) -> double {
                if (isNumeric(s)) return stod(s);
                auto attr = varTable.getAttributes(s);
                if (attr.value == "-") throw runtime_error("Runtime Error: Uninitialized variable '" + s + "'");
                return stod(attr.value);
            };
            
            double r = resolveVal(rightStr);
            double l = resolveVal(leftStr);
            
            // Check for integer overflow
            bool isInt = (leftStr.find('.') == string::npos && rightStr.find('.') == string::npos);
            if (!isNumeric(leftStr)) {
                if (varTable.getAttributes(leftStr).type == "int") isInt = true;
            }
            if (!isNumeric(rightStr)) {
                if (varTable.getAttributes(rightStr).type == "int") isInt = true;
            }

            if (isInt) {
                long long rl = (long long)r;
                long long ll = (long long)l;
                if (token == "+") {
                    if ((rl > 0 && ll > INT_MAX - rl) || (rl < 0 && ll < INT_MIN - rl))
                        throw runtime_error("Runtime Error: Integer Overflow (+)");
                } else if (token == "*") {
                    if (rl != 0 && ll != 0 && (ll > INT_MAX / rl || ll < INT_MIN / rl))
                        throw runtime_error("Runtime Error: Integer Overflow (*)");
                } else if (token == "-") {
                    if ((rl < 0 && ll > INT_MAX + rl) || (rl > 0 && ll < INT_MIN + rl))
                        throw runtime_error("Runtime Error: Integer Overflow (-)");
                }
            }

            double res = 0;
            if (token == "+") res = l + r;
            else if (token == "-") res = l - r;
            else if (token == "*") res = l * r;
            
            if (isInt) runtimeStack.push(to_string((int)res));
            else runtimeStack.push(to_string(res));
        }
        else if (token == "<" || token == ">" || token == "==" || token == "!=") {
            if (runtimeStack.size() < 2) throw runtime_error("Runtime Error: Stack underflow for operation " + token);
            string rightStr = runtimeStack.top(); runtimeStack.pop();
            string leftStr = runtimeStack.top(); runtimeStack.pop();
            
            auto resolveVal = [&](const string& s) -> double {
                if (isNumeric(s)) return stod(s);
                auto attr = varTable.getAttributes(s);
                if (attr.value == "-") throw runtime_error("Runtime Error: Uninitialized variable '" + s + "'");
                return stod(attr.value);
            };
            
            double r = resolveVal(rightStr);
            double l = resolveVal(leftStr);
            
            bool res = false;
            if (token == "<") res = l < r;
            else if (token == ">") res = l > r;
            else if (token == "==") res = l == r;
            else if (token == "!=") res = l != r;
            
            runtimeStack.push(res ? "1" : "0");
        }
        else if (token == "=") {
            if (runtimeStack.size() < 2) throw runtime_error("Runtime Error: Stack underflow for operation =");
            string valStr = runtimeStack.top(); runtimeStack.pop();
            string varName = runtimeStack.top(); runtimeStack.pop();
            
            string resolvedVal = valStr;
            if (!isNumeric(valStr) && varTable.contains(valStr)) {
                resolvedVal = varTable.getAttributes(valStr).value;
            }
            
            auto attr = varTable.getAttributes(varName);
            attr.value = resolvedVal;
            varTable.update(varName, attr);
        }
        else {
            runtimeStack.push(token);
        }
    }
    
    cout << "\n=== VM Execution Finished ===\n";
    if (!runtimeStack.empty()) {
        cout << "Stack top: " << runtimeStack.top() << "\n";
    }
}

// Меню
void showMenu(ConstTable& alphabet, ConstTable& reservedWords, ConstTable& delimiters, VarTable& varTable) {
    while (true) {
        cout << "\n=== Menu ===\n";
        cout << "1. Check element in alphabet\n";
        cout << "2. Check element in reserved words\n";
        cout << "3. Check element in delimiters/operations\n";
        cout << "4. Add lexeme (identifier/constant)\n";
        cout << "5. Check if lexeme exists\n";
        cout << "6. Get lexeme attributes\n";
        cout << "7. Print all lexemes\n";
        cout << "8. Delete lexeme\n";
        cout << "9. Update lexeme\n";
        cout << "10. Exit\n";
        cout << "11. Run Scanner on file\n";
        cout << "12. Print tokens.txt\n";
        cout << "13. Print errors.txt\n";
        cout << "14. Run Syntax Analyzer (LL(1))\n";
        cout << "15. Execute Program (Run VM)\n";
        cout << "Choose: ";
        string input;
        getline(cin, input);
        try {
            int choice = stoi(input);
            switch (choice) {
            case 1: cout << "Enter: "; getline(cin, input); cout << (alphabet.contains(input) ? "Yes" : "No") << endl; break;
            case 2: cout << "Enter: "; getline(cin, input); cout << (reservedWords.contains(input) ? "Yes" : "No") << endl; break;
            case 3: cout << "Enter: "; getline(cin, input); cout << (delimiters.contains(input) ? "Yes" : "No") << endl; break;
            case 4: {
                string lexeme, type, value;
                cout << "Lexeme name: "; getline(cin, lexeme);
                varTable.checkNewName(lexeme);

                cout << "Type (int/float/array<int>/array<float>): "; getline(cin, type);
                if (!IsValidType(type)) throw runtime_error("Invalid type: " + type);

                cout << "Value: "; getline(cin, value);
                if (!IsValidValue(type, value)) throw runtime_error("Invalid value for type " + type);

                varTable.insert(lexeme, { type, value });
                cout << "Added.\n";
                break;
            }
            case 5: cout << "Enter: "; getline(cin, input); cout << (varTable.contains(input) ? "Yes" : "No") << endl; break;
            case 6: {
                cout << "Enter: "; getline(cin, input);
                auto attr = varTable.getAttributes(input);
                cout << "Type: " << attr.type << ", Value: " << attr.value << endl;
                break;
            }
            case 7: varTable.print(); break;
            case 8: {
                cout << "Enter lexeme to delete: "; getline(cin, input);
                varTable.remove(input);
                cout << "Deleted.\n";
                break;
            }
            case 9: {
                string lexeme, type, value;
                cout << "Lexeme name to update: "; getline(cin, lexeme);
                varTable.checkExistingName(lexeme);

                cout << "New Type (int/float/array<int>/array<float>): "; getline(cin, type);
                if (!IsValidType(type)) throw runtime_error("Invalid type: " + type);

                cout << "New Value: "; getline(cin, value);
                if (!IsValidValue(type, value)) throw runtime_error("Invalid value for type " + type);

                varTable.update(lexeme, { type, value });
                cout << "Updated.\n";
                break;
            }
            case 10: return;
            case 11: {
                cout << "Enter file path to scan: ";
                getline(cin, input);
                tokenizeFile(input, alphabet, reservedWords, delimiters, varTable);
                break;
            }
            case 12: {
                ifstream f("tokens.txt");
                if (!f) cout << "Cannot open tokens.txt\n";
                else { cout << "\n--- tokens.txt ---\n"; string l; while (getline(f, l)) cout << l << "\n"; cout << "------------------\n"; }
                break;
            }
            case 13: {
                ifstream f("errors.txt");
                if (!f) cout << "Cannot open errors.txt\n";
                else { cout << "\n--- errors.txt ---\n"; string l; while (getline(f, l)) cout << l << "\n"; cout << "------------------\n"; }
                break;
            }
            case 14: {
                cout << "Enter file path to parse: ";
                getline(cin, input);
                vector<Token> tks = tokenizeFile(input, alphabet, reservedWords, delimiters, varTable);
                SyntaxAnalyzer analyzer(tks, reservedWords, delimiters, varTable);
                analyzer.parse();
                break;
            }
            case 15: {
                cout << "Enter file path to execute: ";
                getline(cin, input);
                vector<Token> tks = tokenizeFile(input, alphabet, reservedWords, delimiters, varTable);
                SyntaxAnalyzer analyzer(tks, reservedWords, delimiters, varTable);
                vector<string> poliz = analyzer.parse();
                try {
                    executePoliz(poliz, varTable);
                } catch (const exception& e) {
                    cout << "\033[1;31m[RUNTIME ERROR] " << e.what() << "\033[0m\n";
                }
                break;
            }
            default: cout << "Invalid.\n";
            }
        }
        catch (const exception& e) { cerr << "Error: " << e.what() << endl; }
    }
}

int main() {
    try {
        ConstTable alphabet, reservedWords, delimiters;
        alphabet.loadFromFile("alphabet.txt");
        reservedWords.loadFromFile("reserved_words.txt");
        delimiters.loadFromFile("delimiters.txt");

        VarTable varTable;
        varTable.loadFromFile();

        showMenu(alphabet, reservedWords, delimiters, varTable);
    }
    catch (const exception& e) {
        cerr << "Error: " << e.what() << endl;
    }
    return 0;
}