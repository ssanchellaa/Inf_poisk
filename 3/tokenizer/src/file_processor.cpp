#include "file_processor.h"
#include "tokenizer.h"
#include <iostream>
#include <filesystem>
#include <chrono>
#include <fstream>
#include <iomanip>

namespace fs = std::filesystem;

void FileProcessor::scan_directory(const std::string& path) {
    files.clear();
    
    try {
        for (const auto& entry : fs::directory_iterator(path)) {
            if (entry.is_regular_file() && entry.path().extension() == ".txt") {
                files.push_back(entry.path().string());
            }
        }
        
        std::cout << "Найдено файлов: " << files.size() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Ошибка сканирования директории: " << e.what() << std::endl;
    }
}

void FileProcessor::process_files(Tokenizer& tokenizer,
                                 size_t& total_tokens,
                                 double& total_time,
                                 size_t& total_bytes) {
    total_tokens = 0;
    total_time = 0.0;
    total_bytes = 0;
    processing_timings.clear();
    
    auto start_total = std::chrono::high_resolution_clock::now();
    
    for (size_t i = 0; i < files.size(); i++) {
        if (i % 100 == 0) {
            std::cout << "\rОбработка файла " << i+1 << " из " << files.size()
                      << " (" << (i*100/files.size()) << "%)" << std::flush;
        }
        
        // Читаем размер файла
        std::ifstream file(files[i], std::ios::binary | std::ios::ate);
        size_t file_size = file.tellg();
        file.close();
        total_bytes += file_size;
        
        // Обработка файла с замером времени
        auto start = std::chrono::high_resolution_clock::now();
        tokenizer.process_file(files[i]);
        auto end = std::chrono::high_resolution_clock::now();
        
        double file_time = std::chrono::duration<double>(end - start).count();
        processing_timings.push_back({file_size, file_time});
        total_time += file_time;
        total_tokens = tokenizer.get_token_count();
    }
    
    auto end_total = std::chrono::high_resolution_clock::now();
    double actual_total_time = std::chrono::duration<double>(end_total - start_total).count();
    total_time = actual_total_time; // Используем фактическое общее время
    
    std::cout << "\rОбработка завершена: " << files.size() << " файлов" << std::endl;
}