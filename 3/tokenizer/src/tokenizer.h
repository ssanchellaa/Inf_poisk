#ifndef TOKENIZER_H
#define TOKENIZER_H

#include <vector>
#include <string>
#include <map>
#include <locale>
#include <codecvt>
#include <cstddef>

class Tokenizer {
private:
    std::vector<std::wstring> tokens;
    std::map<std::wstring, size_t> token_freq;
    size_t total_chars;
    std::locale russian_locale;
    
    bool is_word_char(wchar_t c, wchar_t prev_c = 0, wchar_t next_c = 0) const;
    wchar_t to_lower(wchar_t c) const;
    bool is_digit_or_letter(wchar_t c) const;
    
public:
    Tokenizer();
    
    void process_text(const std::string& text);
    void process_file(const std::string& filename);
    
    size_t get_token_count() const;
    double get_average_length() const;
    const std::vector<std::wstring>& get_tokens() const;
    const std::map<std::wstring, size_t>& get_token_frequencies() const;
    
    std::vector<std::pair<std::wstring, size_t>> get_top_tokens(size_t n) const;
    
    void clear();
    
    struct Statistics {
        size_t files_processed;
        size_t total_tokens;
        size_t total_chars;
        double avg_token_length;
        double processing_time_sec;
        double speed_kb_per_sec;
    };
    
    Statistics get_statistics() const;
};

#endif