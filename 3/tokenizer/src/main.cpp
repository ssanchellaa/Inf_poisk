#include <iostream>
#include <fstream>
#include <chrono>
#include <iomanip>
#include <vector>
#include <algorithm>
#include <locale>
#include <codecvt>
#include <cwctype>  // Добавляем для towlower
#include "tokenizer.h"
#include "file_processor.h"

// Функция для конвертации wstring в UTF-8 string
std::string wstring_to_utf8(const std::wstring& wstr) {
    std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
    return converter.to_bytes(wstr);
}

// Функция для вывода топ-N токенов
void print_top_tokens(const Tokenizer& tokenizer, size_t n) {
    std::cout << "\nТОП-" << n << " самых частых токенов:" << std::endl;
    std::cout << std::string(50, '-') << std::endl;
    std::cout << std::left << std::setw(5) << "№" 
              << std::setw(20) << "Токен" 
              << std::setw(15) << "Частота" 
              << std::setw(15) << "Доля (%)" << std::endl;
    std::cout << std::string(50, '-') << std::endl;
    
    auto top_tokens = tokenizer.get_top_tokens(n);
    size_t total_tokens = tokenizer.get_token_count();
    
    for (size_t i = 0; i < top_tokens.size(); i++) {
        const auto& [token, freq] = top_tokens[i];
        double percentage = (static_cast<double>(freq) / total_tokens) * 100.0;
        
        std::cout << std::left << std::setw(5) << i+1
                  << std::setw(20) << wstring_to_utf8(token).substr(0, 18)
                  << std::setw(15) << freq
                  << std::fixed << std::setprecision(4) << percentage << "%" << std::endl;
    }
}

// Функция для анализа зависимости времени от размера файла
void analyze_performance(const std::vector<std::pair<size_t, double>>& timings) {
    if (timings.empty()) return;
    
    // Сортируем по размеру файла
    auto sorted_timings = timings;
    std::sort(sorted_timings.begin(), sorted_timings.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });
    
    // Группируем по диапазонам размеров
    std::vector<std::pair<std::string, double>> size_groups = {
        {"< 1KB", 0}, {"1-10KB", 0}, {"10-100KB", 0}, {"> 100KB", 0}
    };
    std::vector<int> group_counts = {0, 0, 0, 0};
    
    for (const auto& [size, time] : sorted_timings) {
        double size_kb = size / 1024.0;
        if (size_kb < 1) {
            size_groups[0].second += time;
            group_counts[0]++;
        } else if (size_kb < 10) {
            size_groups[1].second += time;
            group_counts[1]++;
        } else if (size_kb < 100) {
            size_groups[2].second += time;
            group_counts[2]++;
        } else {
            size_groups[3].second += time;
            group_counts[3]++;
        }
    }
    
    std::cout << "\nЗависимость времени обработки от размера файла:" << std::endl;
    std::cout << std::string(50, '-') << std::endl;
    for (size_t i = 0; i < size_groups.size(); i++) {
        if (group_counts[i] > 0) {
            double avg_time = size_groups[i].second / group_counts[i];
            std::cout << std::left << std::setw(10) << size_groups[i].first
                      << ": " << group_counts[i] << " файлов, "
                      << "среднее время: " << std::fixed << std::setprecision(3) 
                      << avg_time << " сек" << std::endl;
        }
    }
}

void print_statistics(const Tokenizer& tokenizer, 
                     const FileProcessor& processor,
                     size_t files_processed,
                     size_t total_bytes,
                     double total_time) {
    
    auto stats = tokenizer.get_statistics();
    
    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << "СТАТИСТИКА ТОКЕНИЗАЦИИ" << std::endl;
    std::cout << std::string(60, '=') << std::endl;
    
    std::cout << std::left << std::setw(35) << "Файлов обработано:" 
              << files_processed << std::endl;
    std::cout << std::left << std::setw(35) << "Общий объем данных:" 
              << total_bytes << " байт (" 
              << std::fixed << std::setprecision(2) << total_bytes / 1024.0 / 1024.0 
              << " МБ)" << std::endl;
    std::cout << std::left << std::setw(35) << "Всего токенов:" 
              << stats.total_tokens << std::endl;
    std::cout << std::left << std::setw(35) << "Уникальных токенов:" 
              << tokenizer.get_token_frequencies().size() << std::endl;
    std::cout << std::left << std::setw(35) << "Средняя длина токена:" 
              << std::fixed << std::setprecision(2) << stats.avg_token_length 
              << " символов" << std::endl;
    std::cout << std::left << std::setw(35) << "Общее время обработки:" 
              << std::fixed << std::setprecision(2) << total_time 
              << " секунд" << std::endl;
    
    double speed_kb_per_sec = (total_bytes / 1024.0) / total_time;
    double speed_mb_per_sec = speed_kb_per_sec / 1024.0;
    double tokens_per_sec = stats.total_tokens / total_time;
    
    std::cout << std::left << std::setw(35) << "Скорость обработки:" 
              << std::fixed << std::setprecision(2) << speed_kb_per_sec 
              << " КБ/сек (" << speed_mb_per_sec << " МБ/сек)" << std::endl;
    std::cout << std::left << std::setw(35) << "Производительность:" 
              << std::fixed << std::setprecision(0) << tokens_per_sec 
              << " токенов/сек" << std::endl;
}

int main(int argc, char* argv[]) {
    // Устанавливаем локаль для корректного вывода
    std::locale::global(std::locale(""));
    
    std::cout << "Лабораторная работа №3: Токенизация" << std::endl;
    std::cout << "====================================" << std::endl;
    
    // Путь к данным
    std::string data_path = "corpus_clean";
    if (argc > 1) {
        data_path = argv[1];
    }
    
    std::cout << "Директория с данными: " << data_path << std::endl;
    
    // Инициализация
    Tokenizer tokenizer;
    FileProcessor processor;
    
    // Сканирование директории
    std::cout << "Сканирование файлов..." << std::endl;
    processor.scan_directory(data_path);
    
    if (processor.get_file_count() == 0) {
        std::cerr << "Файлы не найдены! Убедитесь, что:" << std::endl;
        std::cerr << "1. Директория corpus_clean существует" << std::endl;
        std::cerr << "2. В ней есть .txt файлы" << std::endl;
        std::cerr << "3. Вы запускаете программу из правильной директории" << std::endl;
        return 1;
    }
    
    // Обработка файлов
    std::cout << "Начало обработки..." << std::endl;
    
    size_t total_tokens = 0;
    size_t total_bytes = 0;
    double total_time = 0.0;
    
    processor.process_files(tokenizer, total_tokens, total_time, total_bytes);
    
    // Вывод статистики
    print_statistics(tokenizer, processor, processor.get_file_count(), 
                     total_bytes, total_time);
    
    // Вывод топ-20 токенов
    print_top_tokens(tokenizer, 20);
    
    // Анализ производительности
    analyze_performance(processor.get_timings());
    
    // Примеры токенов
    std::cout << "\nПримеры токенов (первые 10):" << std::endl;
    auto tokens = tokenizer.get_tokens();
    for (size_t i = 0; i < std::min((size_t)10, tokens.size()); i++) {
        std::cout << i+1 << ". '" << wstring_to_utf8(tokens[i]) << "'" << std::endl;
    }
    
    // Примеры сложных токенов
    std::cout << "\nПримеры сложных токенов:" << std::endl;
    std::vector<std::wstring> examples = {
        L"ко-ко", L"c++", L"3.14", L"о'коннор", L"at&t"
    };
    
    for (const auto& example : examples) {
        std::cout << "  " << wstring_to_utf8(example) << " -> ";
        // Создаем временный токенайзер для примера
        Tokenizer temp_tokenizer;
        std::string example_str = wstring_to_utf8(example);
        temp_tokenizer.process_text(example_str);
        auto temp_tokens = temp_tokenizer.get_tokens();
        if (!temp_tokens.empty()) {
            std::cout << "'" << wstring_to_utf8(temp_tokens[0]) << "'";
        }
        std::cout << std::endl;
    }
    
    // Сохранение результатов
    std::cout << "\nСохранить результаты в файл? (y/n): ";
    char choice;
    std::cin >> choice;
    
    if (choice == 'y' || choice == 'Y') {
        std::ofstream out("tokenization_results.txt");
        if (out.is_open()) {
            out << "Результаты токенизации\n";
            out << "======================\n";
            out << "Файлов: " << processor.get_file_count() << "\n";
            out << "Общий объем: " << total_bytes << " байт\n";
            out << "Токенов: " << total_tokens << "\n";
            out << "Уникальных токенов: " << tokenizer.get_token_frequencies().size() << "\n";
            out << "Средняя длина: " << tokenizer.get_average_length() << "\n";
            out << "Время: " << total_time << " сек\n";
            out << "Скорость: " << (total_bytes / 1024.0) / total_time << " КБ/сек\n";
            out << "\nТоп-20 токенов:\n";
            
            auto top_tokens = tokenizer.get_top_tokens(20);
            for (size_t i = 0; i < top_tokens.size(); i++) {
                out << i+1 << ". " << wstring_to_utf8(top_tokens[i].first) 
                    << " - " << top_tokens[i].second << "\n";
            }
            
            out.close();
            std::cout << "Результаты сохранены в tokenization_results.txt" << std::endl;
        }
    }
    
    std::cout << "\nТокенизация завершена успешно!" << std::endl;
    
    return 0;
}