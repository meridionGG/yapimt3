#include "parser.h"
#include <iostream>
#include <fstream>
#include <algorithm>

using namespace std;

string SyntaxAnalyzer::getTokenString(const Token& t) {
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

Symbol SyntaxAnalyzer::tokenToSymbol(const Token& t) {
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

string SyntaxAnalyzer::symToStr(Symbol s) {
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

SyntaxAnalyzer::SyntaxAnalyzer(const vector<Token>& tks, const ConstTable& rw, const ConstTable& delims, VarTable& vt)
    : tokens(tks), reservedWords(rw), delimiters(delims), varTable(vt) {
    
    parseTable.resize(110);

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
    addState(14, {Symbol::T_ID}, 93, true, false, false, true);
    addState(15, {Symbol::T_ASSIGN}, 16, true, false, false, true);

    // Extended Assignment for Arrays
    addState(93, {Symbol::T_ASSIGN}, 16, true, false, false, false); 
    addState(94, {Symbol::T_LBRACKET}, 95, true, false, false, true); 
    addState(95, {}, S_Expr, false, true, false, true);
    addState(96, {Symbol::T_RBRACKET}, 97, true, false, false, true, Action::DO_AEM);
    addState(97, {Symbol::T_ASSIGN}, 98, true, false, false, true);
    addState(98, {}, S_Expr, false, true, false, true);
    addState(99, {}, 18, false, false, false, true, Action::DO_ASSIGN);

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
    addState(77, {Symbol::T_ID}, 100, true, false, false, true);
    // Array Indexing in Factor
    addState(100, {Symbol::T_LBRACKET}, 102, false, false, false, false); // If [, jump to 102
    addState(101, {}, 0, false, false, true, true); // Else return
    addState(102, {Symbol::T_LBRACKET}, 103, true, false, false, true); // Consume [
    addState(103, {}, S_Expr, false, true, false, true); // Parse index
    addState(104, {Symbol::T_RBRACKET}, 0, true, false, true, true, Action::DO_AEM); // Consume ], return, do AEM

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

vector<string> SyntaxAnalyzer::parse() {
    int currentState = 1;
    size_t ip = 0;
    bool error = false;
    ofstream errOut("output/errors.txt", ios::app);
    ofstream traceOut("output/trace.txt");
    
    auto getSym = [&](size_t i) { return (i < tokens.size()) ? tokenToSymbol(tokens[i]) : Symbol::T_EOF; };

    // Pre-parse Semantic Analysis
    for (size_t i = 0; i < tokens.size(); ++i) {
        Symbol t = tokenToSymbol(tokens[i]);
        if (t == Symbol::T_INT || t == Symbol::T_FLOAT) {
            string typeName = symToStr(t);
            if (i + 1 < tokens.size() && tokenToSymbol(tokens[i+1]) == Symbol::T_ID) {
                string idName = getTokenString(tokens[i+1]);
                LexemeAttributes attr = varTable.getAttributes(idName);
                if (attr.type != "-") {
                    errOut << "[Line " << tokens[i+1].line << ", Col " << tokens[i+1].col << "] Semantic Error: Redeclaration of variable '" << idName << "'\n";
                    error = true;
                } else {
                    attr.type = typeName;
                    varTable.update(idName, attr);
                }
                
                // check array declaration
                if (i + 2 < tokens.size() && tokenToSymbol(tokens[i+2]) == Symbol::T_LBRACKET) {
                    if (i + 3 < tokens.size() && tokenToSymbol(tokens[i+3]) == Symbol::T_CONST) {
                        string constVal = getTokenString(tokens[i+3]);
                        if (constVal.find('.') != string::npos) {
                            errOut << "[Line " << tokens[i+3].line << ", Col " << tokens[i+3].col << "] Semantic Error: Array size must be an integer\n";
                            error = true;
                        } else {
                            attr.array_size = stoi(constVal);
                            attr.type = "array<" + typeName + ">";
                            attr.value = "[]";
                            varTable.update(idName, attr);
                        }
                    }
                }
            }
        } else if (t == Symbol::T_ASSIGN && i > 0 && tokenToSymbol(tokens[i-1]) == Symbol::T_ID) {
            string idName = getTokenString(tokens[i-1]);
            LexemeAttributes attr = varTable.getAttributes(idName);
            if (attr.type == "-") {
                errOut << "[Line " << tokens[i-1].line << ", Col " << tokens[i-1].col << "] Semantic Error: Assignment to undeclared variable '" << idName << "'\n";
                error = true;
            } else if (attr.is_constant) {
                // If it's a constant, check if it's the first initialization. Wait, our parser allows assignment to named constant?
                // Let's just say assignment to NAMED_CONSTANT is error.
                if (tokens[i-1].type == TokenType::NAMED_CONSTANT) {
                    // find if it's a declaration: if i-2 is T_INT
                    bool isDecl = (i >= 2 && (tokenToSymbol(tokens[i-2]) == Symbol::T_INT || tokenToSymbol(tokens[i-2]) == Symbol::T_FLOAT));
                    if (!isDecl) {
                        errOut << "[Line " << tokens[i-1].line << ", Col " << tokens[i-1].col << "] Semantic Error: Reassignment of named constant '" << idName << "'\n";
                        error = true;
                    }
                }
            }
            
            // type check on RHS for simple case
            if (i + 1 < tokens.size() && tokenToSymbol(tokens[i+1]) == Symbol::T_CONST) {
                string rhsVal = getTokenString(tokens[i+1]);
                bool isFloatRhs = (rhsVal.find('.') != string::npos);
                if (attr.type == "int" && isFloatRhs) {
                    errOut << "[Line " << tokens[i+1].line << ", Col " << tokens[i+1].col << "] Semantic Error: Type mismatch, cannot assign float to int\n";
                    error = true;
                }
            }
        }
    }


    while (true) {
        if (currentState <= 0 || currentState >= (int)parseTable.size()) break;

        StateRow row = parseTable[currentState];
        Symbol token = getSym(ip);

        traceOut << "State: " << currentState << " | Token: " << symToStr(token);

        bool match = row.terminals.empty() || (std::find(row.terminals.begin(), row.terminals.end(), token) != row.terminals.end());

        if (!match) {
            if (row.error) {
                traceOut << " -> Error (Panic Mode)\n";
                error = true;
                string lineCol = (ip < tokens.size()) ? ("[Line " + to_string(tokens[ip].line) + ", Col " + to_string(tokens[ip].col) + "] ") : "[EOF] ";
                errOut << lineCol << "Syntax Error: Unexpected token " << symToStr(token) << "\n";
                
                // Panic mode
                while (ip < tokens.size() && token != Symbol::T_SEMI && token != Symbol::T_EOF && token != Symbol::T_INT && token != Symbol::T_FLOAT) {
                    ip++;
                    token = getSym(ip);
                }
                if (token == Symbol::T_SEMI) {
                    ip++;
                    while (!stateStack.empty()) stateStack.pop();
                    currentState = 3; // return to StmtList
                    continue;
                } else if (token == Symbol::T_INT || token == Symbol::T_FLOAT) {
                    // Recovered at the start of a new statement without consuming a semicolon
                    while (!stateStack.empty()) stateStack.pop();
                    currentState = 3; // return to StmtList
                    continue;
                } else {
                    break;
                }
            } else {
                traceOut << " -> Match failed, trying next state: " << (currentState + 1) << "\n";
                currentState++;
                continue;
            }
        }

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

        if (row.stack) {
            stateStack.push(currentState + 1);
            traceOut << " [Push " << (currentState + 1) << " to stack]";
        }

        if (row.jump > 0) {
            traceOut << " -> Jump to: " << row.jump << "\n";
            currentState = row.jump;
            continue;
        }

        if (row.return_flag) {
            if (stateStack.empty()) {
                traceOut << " -> Return (Stack empty, halting)\n";
                break;
            }
            traceOut << " -> Return to: " << stateStack.top() << "\n";
            currentState = stateStack.top();
            stateStack.pop();
            continue;
        }

        if (row.jump == 0 && !row.return_flag) {
            traceOut << " -> Next state: " << (currentState + 1) << "\n";
            currentState++;
        }
    }

    cout << "\n=== Syntax Analysis (State Machine LL(1)) ===\n";
    if (error) cout << "Syntax errors found. Check output/errors.txt\n";
    else cout << "Syntax analysis successful.\n";
    
    cout << "POLIZ: ";
    ofstream pOut("output/poliz.txt");
    for (const auto& p : poliz) { cout << p << " "; pOut << p << " "; }
    cout << "\n"; pOut << "\n";
    return poliz;
}
