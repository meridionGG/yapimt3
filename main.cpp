#include "lexertypes.h"
#include "utils.h"
#include "tables.h"
#include "lexer.h"
#include "parser.h"
#include "interpreter.h"

#include <iostream>
#include <fstream>
#include <string>
#include <vector>

using namespace std;

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
                ifstream f("output/tokens.txt");
                if (!f) cout << "Cannot open output/tokens.txt\n";
                else { cout << "\n--- output/tokens.txt ---\n"; string l; while (getline(f, l)) cout << l << "\n"; cout << "------------------\n"; }
                break;
            }
            case 13: {
                ifstream f("output/errors.txt");
                if (!f) cout << "Cannot open output/errors.txt\n";
                else { cout << "\n--- output/errors.txt ---\n"; string l; while (getline(f, l)) cout << l << "\n"; cout << "------------------\n"; }
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
        alphabet.loadFromFile("config/alphabet.txt");
        reservedWords.loadFromFile("config/reserved_words.txt");
        delimiters.loadFromFile("config/delimiters.txt");

        VarTable varTable;
        varTable.loadFromFile();

        showMenu(alphabet, reservedWords, delimiters, varTable);
    }
    catch (const exception& e) {
        cerr << "Error: " << e.what() << endl;
    }
    return 0;
}
