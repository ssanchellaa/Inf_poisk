#include "tokenizer.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <chrono>
#include <algorithm>
#include <cwctype>  // Используем этот заголовок для wide char функций
#include <locale>
#include <codecvt>

Tokenizer::Tokenizer() : total_chars(0) {
    // Устанавливаем локаль для русских символов
    try {
        russian_locale = std::locale("ru_RU.UTF-8");
    } catch (...) {
        russian_locale = std::locale();
    }
}

bool Tokenizer::is_digit_or_letter(wchar_t c) const {
    // Используем функции из cwctype (глобальное пространство имен)
    return iswalnum(c) || 
           (c >= L'а' && c <= L'я') || c == L'ё' ||
           (c >= L'А' && c <= L'Я') || c == L'Ё';
}

bool Tokenizer::is_word_char(wchar_t c, wchar_t prev_c, wchar_t next_c) const {
    // Буквы и цифры всегда входят в токены
    if (is_digit_or_letter(c)) {
        return true;
    }
    
    // Дефис внутри слова (например, "ко-ко")
    if (c == L'-' && prev_c != 0 && next_c != 0 && 
        is_digit_or_letter(prev_c) && is_digit_or_letter(next_c)) {
        return true;
    }
    
    // Апостроф внутри слова (например, "o'clock")
    if (c == L'\'' && prev_c != 0 && next_c != 0 &&
        is_digit_or_letter(prev_c) && is_digit_or_letter(next_c)) {
        return true;
    }
    
    // Амперсанд в названиях (например, "AT&T")
    if (c == L'&' && prev_c != 0 && next_c != 0 &&
        is_digit_or_letter(prev_c) && is_digit_or_letter(next_c)) {
        return true;
    }
    
    // Точка в числах (например, "3.14")
    if (c == L'.' && prev_c != 0 && next_c != 0 &&
        iswdigit(prev_c) && iswdigit(next_c)) {
        return true;
    }
    
    // Плюсы в "C++"
    if (c == L'+' && prev_c == L'C' && next_c == L'+') {
        return true;
    }
    
    return false;
}

wchar_t Tokenizer::to_lower(wchar_t c) const {
    // Используем функцию из cwctype (глобальное пространство имен)
    return towlower(c);
}

void Tokenizer::process_text(const std::string& text) {
    try {
        // Конвертируем UTF-8 в wstring
        std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
        std::wstring wtext = converter.from_bytes(text);
        
        std::wstring current_token;
        bool in_token = false;
        size_t len = wtext.length();
        
        for (size_t i = 0; i < len; i++) {
            wchar_t c = wtext[i];
            wchar_t prev_c = (i > 0) ? wtext[i-1] : 0;
            wchar_t next_c = (i < len-1) ? wtext[i+1] : 0;
            
            if (is_word_char(c, prev_c, next_c)) {
                current_token += to_lower(c);
                in_token = true;
            } else if (in_token) {
                if (!current_token.empty()) {
                    // Сохраняем токен
                    tokens.push_back(current_token);
                    total_chars += current_token.length();
                    
                    // Увеличиваем частоту
                    token_freq[current_token]++;
                    
                    current_token.clear();
                }
                in_token = false;
            }
        }
        
        // Последний токен, если есть
        if (!current_token.empty()) {
            tokens.push_back(current_token);
            total_chars += current_token.length();
            token_freq[current_token]++;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Ошибка обработки текста: " << e.what() << std::endl;
    }
}

void Tokenizer::process_file(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Ошибка открытия файла: " << filename << std::endl;
        return;
    }
    
    // Определяем размер файла
    file.seekg(0, std::ios::end);
    size_t file_size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    // Читаем весь файл
    std::string text(file_size, '\0');
    file.read(&text[0], file_size);
    file.close();
    
    process_text(text);
}

size_t Tokenizer::get_token_count() const {
    return tokens.size();
}

double Tokenizer::get_average_length() const {
    if (tokens.empty()) return 0.0;
    return static_cast<double>(total_chars) / tokens.size();
}

const std::vector<std::wstring>& Tokenizer::get_tokens() const {
    return tokens;
}

const std::map<std::wstring, size_t>& Tokenizer::get_token_frequencies() const {
    return token_freq;
}

std::vector<std::pair<std::wstring, size_t>> Tokenizer::get_top_tokens(size_t n) const {
    std::vector<std::pair<std::wstring, size_t>> sorted_tokens;
    
    // Копируем токены в вектор для сортировки
    for (const auto& pair : token_freq) {
        sorted_tokens.push_back(pair);
    }
    
    // Сортируем по частоте (по убыванию)
    std::sort(sorted_tokens.begin(), sorted_tokens.end(),
              [](const auto& a, const auto& b) {
                  return a.second > b.second;
              });
    
    // Возвращаем топ-N
    if (sorted_tokens.size() > n) {
        sorted_tokens.resize(n);
    }
    
    return sorted_tokens;
}

void Tokenizer::clear() {
    tokens.clear();
    token_freq.clear();
    total_chars = 0;
}

Tokenizer::Statistics Tokenizer::get_statistics() const {
    Statistics stats;
    stats.total_tokens = tokens.size();
    stats.total_chars = total_chars;
    stats.avg_token_length = get_average_length();
    return stats;
}