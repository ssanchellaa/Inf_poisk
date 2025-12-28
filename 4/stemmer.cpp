#include "stemmer.h"
#include <iostream>
#include <algorithm>
#include <codecvt>
#include <locale>
#include <cwctype>  // Добавляем для функций wide char

RussianStemmer::RussianStemmer() {
    // Инициализация исключений
    std::vector<std::wstring> exception_list = {
        L"это", L"что", L"как", L"так", L"здесь", L"там", L"где",
        L"кто", L"чем", L"сам", L"сама", L"само", L"сами"
    };
    
    for (const auto& word : exception_list) {
        exceptions.insert(word);
    }
    
    // Инициализация окончаний (по приоритету - от самых длинных к коротким)
    endings = {
        L"иями", L"иях", L"иям", L"иев", L"ием", L"ию", L"ие", L"ий", 
        L"ия", L"ии", L"ями", L"ях", L"ям", L"ев", L"ем", L"ю", L"е", 
        L"й", L"я", L"и", L"а", L"о", L"у", L"ы", L"ь"
    };
}

bool RussianStemmer::is_vowel(wchar_t c) const {
    // Используем towlower из cwctype (глобальное пространство имен)
    c = towlower(c);
    return c == L'а' || c == L'е' || c == L'ё' || c == L'и' || 
           c == L'о' || c == L'у' || c == L'ы' || c == L'э' || 
           c == L'ю' || c == L'я';
}

bool RussianStemmer::ends_with(const std::wstring& word, const std::wstring& ending) const {
    if (word.length() < ending.length()) return false;
    return word.substr(word.length() - ending.length()) == ending;
}

int RussianStemmer::count_vowels(const std::wstring& word) const {
    int count = 0;
    for (wchar_t c : word) {
        if (is_vowel(c)) count++;
    }
    return count;
}

std::wstring RussianStemmer::remove_endings(const std::wstring& word) const {
    std::wstring result = word;
    
    for (const auto& ending : endings) {
        if (ends_with(result, ending)) {
            std::wstring candidate = result.substr(0, result.length() - ending.length());
            
            // Проверяем, что после удаления окончания:
            // 1. Длина не менее 2 символов
            // 2. Есть хотя бы одна гласная
            if (candidate.length() >= 2 && count_vowels(candidate) > 0) {
                // Дополнительные правила для улучшения качества
                if (ends_with(candidate, L"ость") && candidate.length() > 4) {
                    candidate = candidate.substr(0, candidate.length() - 4);
                }
                else if (ends_with(candidate, L"тель") && candidate.length() > 4) {
                    candidate = candidate.substr(0, candidate.length() - 4);
                }
                else if (ends_with(candidate, L"ник") && candidate.length() > 3) {
                    candidate = candidate.substr(0, candidate.length() - 3);
                }
                else if (ends_with(candidate, L"ок") && candidate.length() > 2) {
                    candidate = candidate.substr(0, candidate.length() - 2);
                }
                
                return candidate;
            }
        }
    }
    
    return result;
}

std::wstring RussianStemmer::stem(const std::wstring& word) const {
    // Если слово слишком короткое или является исключением
    if (word.length() <= 3 || exceptions.find(word) != exceptions.end()) {
        return word;
    }
    
    std::wstring stemmed = remove_endings(word);
    
    // Проверяем, что стем не стал слишком коротким
    if (stemmed.length() < 2) {
        return word;
    }
    
    return stemmed;
}

std::wstring RussianStemmer::stem(const std::string& utf8_word) const {
    // Конвертируем UTF-8 в wstring
    std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
    std::wstring wword = converter.from_bytes(utf8_word);
    
    // Стемминг
    std::wstring stemmed = stem(wword);
    
    // Возвращаем wstring (или можно конвертировать обратно в UTF-8)
    return stemmed;
}

std::vector<std::wstring> RussianStemmer::stem_batch(const std::vector<std::wstring>& words) const {
    std::vector<std::wstring> result;
    result.reserve(words.size());
    
    for (const auto& word : words) {
        result.push_back(stem(word));
    }
    
    return result;
}

void RussianStemmer::test() const {
    std::cout << "Тестирование русского стеммера:" << std::endl;
    std::cout << "=================================" << std::endl;
    
    std::vector<std::wstring> test_words = {
        L"актёры", L"актёра", L"актёру", L"актёром", L"актёре",
        L"фильмы", L"фильма", L"фильму", L"фильмом", L"фильме",
        L"режиссёры", L"режиссёра", L"режиссёру", L"режиссёром",
        L"голливудский", L"голливудского", L"голливудскому",
        L"сниматься", L"снимается", L"снимался", L"снимались",
        L"прекрасный", L"прекрасного", L"прекрасному",
        L"американский", L"американского", L"американскому"
    };
    
    // Для вывода wstring в cout нужно конвертировать в UTF-8
    std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
    
    for (const auto& word : test_words) {
        std::wstring stemmed = stem(word);
        std::cout << converter.to_bytes(word) << " -> " 
                  << converter.to_bytes(stemmed) << std::endl;
    }
    
    // Тест на проблемные случаи
    std::cout << "\nПроблемные случаи:" << std::endl;
    std::cout << "-------------------" << std::endl;
    
    std::vector<std::pair<std::wstring, std::wstring>> problematic = {
        {L"мир", L"мир"},           // мир -> мир (правильно)
        {L"стекло", L"стекл"},      // стекло -> стекл (правильно)
        {L"писать", L"пис"},        // писать -> пис (упрощенно)
        {L"бежать", L"беж"},        // бежать -> беж (упрощенно)
        {L"хороший", L"хорош"},     // хороший -> хорош (правильно)
    };
    
    for (const auto& [word, expected] : problematic) {
        std::wstring stemmed = stem(word);
        std::cout << converter.to_bytes(word) << " -> " 
                  << converter.to_bytes(stemmed) 
                  << " (ожидалось: " << converter.to_bytes(expected) << ")" 
                  << (stemmed == expected ? " ✓" : " ✗") << std::endl;
    }
}