#include "interpreter.h"
#include <iostream>
#include <stack>
#include <stdexcept>
#include <cctype>
#include <climits>

using namespace std;

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
