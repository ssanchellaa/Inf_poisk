#ifndef STEMMER_H
#define STEMMER_H

#include <string>
#include <vector>
#include <unordered_set>

class RussianStemmer {
private:
    std::unordered_set<std::wstring> exceptions;
    std::vector<std::wstring> endings;
    
    bool is_vowel(wchar_t c) const;
    bool ends_with(const std::wstring& word, const std::wstring& ending) const;
    int count_vowels(const std::wstring& word) const;
    std::wstring remove_endings(const std::wstring& word) const;
    
public:
    RussianStemmer();
    
    std::wstring stem(const std::wstring& word) const;
    std::wstring stem(const std::string& utf8_word) const;
    
    std::vector<std::wstring> stem_batch(const std::vector<std::wstring>& words) const;
    
    void test() const;
};

#endif