#include "Lexer.h"
#include "Utils.h"
#include <fstream>
#include <iostream>
#include <cctype>

using namespace std;

vector<Token> tokenizeFile(const string& filepath, const ConstTable& alphabet, const ConstTable& reservedWords, const ConstTable& delimiters, VarTable& varTable) {
    ifstream file(filepath);
    if (!file.is_open()) {
        cerr << "Cannot open file " << filepath << endl;
        return {};
    }

    ofstream tokensOut("output/tokens.txt");
    ofstream errorsOut("output/errors.txt");
    vector<Token> tokens;

    State state = State::START;
    string buffer = "";
    size_t current_line = 1;
    size_t current_col = 0;
    size_t start_col = 0;

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
                            attr.type = "-";
                            attr.value = "-";
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
                if (c == ' ' || c == '\t' || c == '\r') {}
                else if (c == '\n') { current_line++; current_col = 0; }
                else if (c == '/') {
                    char next_c;
                    if (file.get(next_c)) {
                        if (next_c == '/') { state = State::IN_COMMENT; buffer = "//"; }
                        else if (next_c == '*') { state = State::IN_COMMENT; buffer = "/*"; }
                        else { file.unget(); buffer = c; state = State::IN_OPERATOR; start_col = current_col; }
                    } else { buffer = c; state = State::IN_OPERATOR; start_col = current_col; }
                }
                else if (isalpha((unsigned char)c)) { buffer = c; start_col = current_col; state = State::IN_ID; }
                else if (isdigit((unsigned char)c)) { buffer = c; start_col = current_col; state = State::IN_NUMBER; }
                else if (c == '=' || c == '!' || c == '<' || c == '>' || c == '+' || c == '-' || c == '*') { buffer = c; start_col = current_col; state = State::IN_OPERATOR; }
                else if (delimiters.contains(string(1, c))) {
                    int index = delimiters.getIndex(string(1, c));
                    tokens.push_back({ TokenType::DELIMITER, (size_t)(index < 0 ? 0 : index), current_line, current_col });
                }
                else {
                    errorsOut << "[Line " << current_line << ", Col " << current_col << "] Lexical Error: Unknown symbol '" << c << "'. Switching to PANIC mode.\n";
                    state = State::PANIC; buffer = c;
                }
                break;
            }
            case State::IN_COMMENT: {
                if (buffer == "//") {
                    if (c == '\n') { current_line++; current_col = 0; state = State::START; buffer = ""; }
                }
                else if (buffer == "/*") {
                    if (c == '*') {
                        char next_c;
                        if (file.get(next_c)) {
                            if (next_c == '/') { state = State::START; buffer = ""; }
                            else { file.unget(); }
                        }
                    }
                    else if (c == '\n') { current_line++; current_col = 0; }
                }
                break;
            }
            case State::IN_ID: {
                if (isalnum((unsigned char)c) || c == '_' || c == '$') { buffer += c; }
                else if (isSafeBreak(c)) { processBuffer(); state = State::START; buffer = ""; process_char = true; }
                else {
                    buffer += c;
                    errorsOut << "[Line " << current_line << ", Col " << start_col << "] Lexical Error: Invalid character '" << c << "' in identifier '" << buffer << "'. Switching to PANIC mode.\n";
                    state = State::PANIC;
                }
                break;
            }
            case State::IN_NUMBER: {
                if (isdigit((unsigned char)c) || c == '.') { buffer += c; }
                else if (isSafeBreak(c)) { processBuffer(); state = State::START; buffer = ""; process_char = true; }
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
                    state = State::START; buffer = "";
                }
                else { processBuffer(); state = State::START; buffer = ""; process_char = true; }
                break;
            }
            case State::PANIC: {
                if (isSafeBreak(c)) { state = State::START; buffer = ""; process_char = true; }
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

    cout << "Tokenization complete. Check output/tokens.txt and output/errors.txt.\n";
    return tokens;
}
