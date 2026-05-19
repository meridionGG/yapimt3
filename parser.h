#pragma once
#include "lexertypes.h"
#include "tables.h"
#include <vector>
#include <string>
#include <stack>

enum class Symbol {
    Program, StmtList, Stmt, Type, DeclTail, Expr, RelOpTail, RelOp, 
    SimpleExpr, AddOpTail, AddOp, Term, MulOpTail, Factor, ArrayElems, ArrayElemsTail,
    T_INT, T_FLOAT, T_ID, T_CONST, T_ASSIGN, T_SEMI, T_LBRACKET, T_RBRACKET, 
    T_EQ, T_NEQ, T_LESS, T_GREATER, T_PLUS, T_MINUS, T_MUL, T_COMMA, T_EOF,
    EPS
};

enum class Action { NONE, DO_ASSIGN, DO_ADD, DO_MUL, DO_REL, DO_AEM, DO_SAVE_TYPE, DO_DECLARE, DO_CHECK_ASSIGN, DO_DECLARE_ARRAY };

struct StateRow {
    std::vector<Symbol> terminals;
    int jump;
    bool accept;
    bool stack;
    bool return_flag;
    bool error;
    Action action;
};

class SyntaxAnalyzer {
private:
    std::vector<Token> tokens;
    const ConstTable& reservedWords;
    const ConstTable& delimiters;
    VarTable& varTable;
    
    std::vector<StateRow> parseTable;
    std::stack<int> stateStack;
    std::stack<std::string> opStack;
    std::vector<std::string> poliz;
    
    std::string getTokenString(const Token& t);
    Symbol tokenToSymbol(const Token& t);
    std::string symToStr(Symbol s);

public:
    SyntaxAnalyzer(const std::vector<Token>& tks, const ConstTable& rw, const ConstTable& delims, VarTable& vt);
    std::vector<std::string> parse();
};
