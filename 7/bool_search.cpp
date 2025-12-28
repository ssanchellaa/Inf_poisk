#include <iostream>
#include <fstream>
#include <unordered_map>
#include <unordered_set>
#include <stack>
#include <algorithm>
#include <vector>
#include <filesystem>
#include <chrono>

namespace fs = std::filesystem;
using namespace std;

unordered_map<string, unordered_set<int>> idx;
unordered_map<int, string> docs;
unordered_set<int> all_docs;

string trim(string s) {
    s.erase(remove_if(s.begin(), s.end(), ::isspace), s.end());
    return s;
}

vector<string> tokenize(const string& text) {
    vector<string> tokens;
    string token;
    for (char c : text) {
        if (isalnum(c) || c >= 'а' && c <= 'я' || c >= 'А' && c <= 'Я') {
            token += tolower(c);
        } else if (!token.empty()) {
            tokens.push_back(token);
            token.clear();
        }
    }
    if (!token.empty()) tokens.push_back(token);
    return tokens;
}

void build_index(const string& dir) {
    int id = 1;
    for (const auto& entry : fs::directory_iterator(dir)) {
        if (entry.path().extension() == ".txt") {
            ifstream f(entry.path());
            string text((istreambuf_iterator<char>(f)), istreambuf_iterator<char>());
            for (const string& word : tokenize(text)) {
                idx[word].insert(id);
            }
            docs[id] = entry.path().string();
            all_docs.insert(id);
            id++;
        }
    }
}

unordered_set<int> eval(const string& expr) {
    stack<unordered_set<int>> st;
    string op;
    
    for (size_t i = 0; i < expr.size(); i++) {
        if (expr[i] == ' ') continue;
        
        if (expr[i] == '(') {
            size_t depth = 1;
            size_t j = i + 1;
            for (; j < expr.size() && depth > 0; j++) {
                if (expr[j] == '(') depth++;
                else if (expr[j] == ')') depth--;
            }
            st.push(eval(expr.substr(i+1, j-i-2)));
            i = j - 1;
        }
        else if (expr.substr(i, 2) == "&&") {
            op = "&&"; i++;
        }
        else if (expr.substr(i, 2) == "||") {
            op = "||"; i++;
        }
        else if (expr[i] == '!') {
            // Нужно обработать следующий терм
            if (i + 1 < expr.size()) {
                // Если после ! идет слово
                if (isalnum(expr[i+1]) || expr[i+1] == '(') {
                    // Пропускаем ! и обрабатываем следующий терм
                    continue;
                }
            }
            op = "!";
        }
        else {
            string word;
            while (i < expr.size() && (isalnum(expr[i]) || expr[i] >= 'а' && expr[i] <= 'я' || expr[i] >= 'А' && expr[i] <= 'Я')) {
                word += tolower(expr[i]);
                i++;
            }
            i--;
            
            if (!word.empty()) {
                unordered_set<int> res;
                if (idx.count(word)) {
                    res = idx[word];
                }
                st.push(res);
            }
        }
        
        // Обработка операций
        if (op == "&&" && st.size() >= 2) {
            auto b = st.top(); st.pop();
            auto a = st.top(); st.pop();
            unordered_set<int> res;
            for (int x : a) {
                if (b.count(x)) {
                    res.insert(x);
                }
            }
            st.push(res);
            op.clear();
        }
        else if (op == "||" && st.size() >= 2) {
            auto b = st.top(); st.pop();
            auto a = st.top(); st.pop();
            unordered_set<int> res = a;
            for (int x : b) {
                res.insert(x);
            }
            st.push(res);
            op.clear();
        }
        else if (op == "!") {
            if (!st.empty()) {
                auto a = st.top(); st.pop();
                unordered_set<int> res;
                for (int x : all_docs) {
                    if (!a.count(x)) {
                        res.insert(x);
                    }
                }
                st.push(res);
            }
            op.clear();
        }
    }
    
    return st.empty() ? unordered_set<int>{} : st.top();
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        cout << "Использование: " << argv[0] << " <директория> <запрос>" << endl;
        return 1;
    }
    
    auto start = chrono::high_resolution_clock::now();
    build_index(argv[1]);
    cout << "Индекс построен. Документов: " << docs.size() << endl;
    
    // Собираем запрос из всех аргументов
    string query;
    for (int i = 2; i < argc; i++) {
        query += string(argv[i]) + " ";
    }
    query = trim(query);
    
    cout << "Запрос: " << query << endl;
    
    auto results = eval(query);
    auto end = chrono::high_resolution_clock::now();
    
    cout << "Результатов: " << results.size() << endl;
    cout << "Время: " << chrono::duration<double>(end-start).count() << " сек" << endl;
    
    int cnt = 0;
    for (int id : results) {
        if (cnt++ >= 50) break;
        cout << id << ": " << docs[id] << endl;
    }
    
    if (results.size() > 50) {
        cout << "... и еще " << (results.size() - 50) << " документов" << endl;
    }
    
    return 0;
}