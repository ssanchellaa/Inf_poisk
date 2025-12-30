#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <chrono>
#include <filesystem>
#include <codecvt>
#include <locale>
#include <cwctype>
#include <algorithm>
#include <cmath>
#include "stemmer.h"

// Добавьте эту строку
#ifdef _WIN32
#include <windows.h>
#endif

namespace fs = std::filesystem;

// Конвертер UTF-8 <-> wstring
std::wstring utf8_to_wstring(const std::string& utf8) {
    std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
    return converter.from_bytes(utf8);
}

std::string wstring_to_utf8(const std::wstring& ws) {
    std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
    return converter.to_bytes(ws);
}

class IndexerWithStemming {
private:
    RussianStemmer stemmer;
    
    // Инвертированный индекс: основа слова -> {документ_id -> {позиции}}
    std::unordered_map<std::wstring, std::unordered_map<int, std::vector<int>>> index;
    
    // Метаданные документов
    std::unordered_map<int, std::string> doc_paths;
    std::unordered_map<int, size_t> doc_sizes;
    std::unordered_map<int, size_t> doc_token_counts;
    int next_doc_id = 1;
    
    // Токенизация текста с поддержкой UTF-8
    std::vector<std::wstring> tokenize(const std::wstring& text) {
        std::vector<std::wstring> tokens;
        std::wstring current_token;
        
        for (wchar_t c : text) {
            // Используем функции из cwctype (глобальное пространство имен)
            if (iswalnum(c) || 
                (c >= L'а' && c <= L'я') || (c >= L'А' && c <= L'Я') ||
                c == L'ё' || c == L'Ё' ||
                c == L'-' || c == L'\'' || c == L'&') {
                
                current_token += towlower(c);
            } else if (!current_token.empty()) {
                // Проверяем минимальную длину токена
                if (current_token.length() >= 2) {
                    tokens.push_back(current_token);
                }
                current_token.clear();
            }
        }
        
        // Последний токен
        if (!current_token.empty() && current_token.length() >= 2) {
            tokens.push_back(current_token);
        }
        
        return tokens;
    }
    
public:
    // Публичные методы для доступа к данным
    
    // Получение пути документа по ID
    std::string get_document_path(int doc_id) const {
        auto it = doc_paths.find(doc_id);
        if (it != doc_paths.end()) {
            return it->second;
        }
        return "";
    }
    
    // Получение размера документа по ID
    size_t get_document_size(int doc_id) const {
        auto it = doc_sizes.find(doc_id);
        if (it != doc_sizes.end()) {
            return it->second;
        }
        return 0;
    }
    
    // Получение количества токенов в документе
    size_t get_document_token_count(int doc_id) const {
        auto it = doc_token_counts.find(doc_id);
        if (it != doc_token_counts.end()) {
            return it->second;
        }
        return 0;
    }
    
    // Получение количества документов
    size_t get_document_count() const {
        return doc_paths.size();
    }
    
    // Получение количества уникальных основ
    size_t get_unique_stems_count() const {
        return index.size();
    }
    


    // Индексация документа
    void index_document(const std::string& filepath) {
        // Используем std::ifstream с широкими символами для Windows
        std::ifstream file;
        
        #ifdef _WIN32
        // Для Windows конвертируем путь в wstring
        std::wstring wpath = utf8_to_wstring(filepath);
        file.open(wpath.c_str(), std::ios::binary);
        #else
        // Для Linux/Unix
        file.open(filepath.c_str(), std::ios::binary);
        #endif
        
        if (!file) {
            std::cerr << "Не удалось открыть файл: " << filepath << std::endl;
            return;
        }
        
        // Чтение файла
        file.seekg(0, std::ios::end);
        size_t size = file.tellg();
        std::string text(size, ' ');
        file.seekg(0);
        file.read(&text[0], size);
        file.close();
        
        int doc_id = next_doc_id++;
        doc_paths[doc_id] = filepath;
        doc_sizes[doc_id] = size;
        
        // Конвертируем в wstring для обработки UTF-8
        std::wstring wtext = utf8_to_wstring(text);
        
        // Токенизация
        auto tokens = tokenize(wtext);
        doc_token_counts[doc_id] = tokens.size();
        
        // Стемминг и индексация
        std::unordered_map<std::wstring, std::vector<int>> word_positions;
        
        for (size_t pos = 0; pos < tokens.size(); ++pos) {
            std::wstring original = tokens[pos];
            
            // Применяем стемминг
            std::wstring stemmed = stemmer.stem(original);
            
            // Сохраняем позицию для основы
            word_positions[stemmed].push_back(pos);
        }
        
        // Добавляем в общий индекс
        for (const auto& [stem, positions] : word_positions) {
            index[stem][doc_id] = positions;
        }
        
        if (doc_id % 100 == 0) {
            std::cout << "Проиндексирован документ #" << doc_id 
                    << ": " << filepath 
                    << " (токенов: " << tokens.size() << ")" << std::endl;
        }
    }
    
    // Индексация всех документов в директории
    void index_directory(const std::string& dirpath, int limit = 0) {
        std::cout << "Начало индексации с использованием стемминга..." << std::endl;
        auto start = std::chrono::high_resolution_clock::now();
        
        int file_count = 0;
        size_t total_tokens = 0;
        
        for (const auto& entry : fs::directory_iterator(dirpath)) {
            if (entry.path().extension() == ".txt") {
                index_document(entry.path().string());
                file_count++;
                
                if (limit > 0 && file_count >= limit) {
                    break;
                }
                
                if (file_count % 100 == 0) {
                    std::cout << "Обработано файлов: " << file_count << std::endl;
                }
            }
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration<double>(end - start);
        
        // Подсчет общего количества токенов
        for (const auto& [doc_id, count] : doc_token_counts) {
            total_tokens += count;
        }
        
        std::cout << "\nИндексация завершена!" << std::endl;
        std::cout << "Обработано документов: " << file_count << std::endl;
        std::cout << "Всего токенов: " << total_tokens << std::endl;
        std::cout << "Уникальных основ: " << index.size() << std::endl;
        std::cout << "Время индексации: " << duration.count() << " секунд" << std::endl;
        std::cout << "Скорость: " << (file_count / duration.count()) << " документов/сек" << std::endl;
    }
    
    // Поиск документов по запросу
    std::vector<int> search(const std::wstring& query, bool use_stemming = true) {
        std::vector<std::wstring> query_tokens = tokenize(query);
        std::unordered_map<int, double> doc_scores;
        
        for (const auto& token : query_tokens) {
            std::wstring search_token = use_stemming ? stemmer.stem(token) : token;
            
            if (index.find(search_token) != index.end()) {
                for (const auto& [doc_id, positions] : index[search_token]) {
                    // Более сложная оценка релевантности
                    double score = positions.size(); // Количество вхождений
                    
                    // Бонус за точное совпадение (если не используется стемминг)
                    if (!use_stemming) {
                        score *= 1.2;
                    }
                    
                    doc_scores[doc_id] += score;
                }
            }
        }
        
        // Нормализация по размеру документа
        for (auto& [doc_id, score] : doc_scores) {
            if (doc_sizes.find(doc_id) != doc_sizes.end()) {
                score /= (1.0 + std::log(doc_sizes[doc_id] / 1024.0));
            }
        }
        
        // Сортировка документов по релевантности
        std::vector<std::pair<int, double>> sorted_docs(doc_scores.begin(), doc_scores.end());
        std::sort(sorted_docs.begin(), sorted_docs.end(),
            [](const auto& a, const auto& b) { return a.second > b.second; });
        
        std::vector<int> result;
        for (const auto& [doc_id, score] : sorted_docs) {
            result.push_back(doc_id);
        }
        
        return result;
    }
    
    // Поиск документов по запросу в UTF-8
    std::vector<int> search_utf8(const std::string& query_utf8, bool use_stemming = true) {
        std::wstring query = utf8_to_wstring(query_utf8);
        return search(query, use_stemming);
    }
    
    // Статистика индекса
    void print_statistics() {
        size_t total_postings = 0;
        size_t total_docs = doc_paths.size();
        size_t total_tokens = 0;
        
        for (const auto& [stem, doc_map] : index) {
            for (const auto& [doc_id, positions] : doc_map) {
                total_postings += positions.size();
            }
        }
        
        for (const auto& [doc_id, count] : doc_token_counts) {
            total_tokens += count;
        }
        
        double avg_postings_per_word = index.empty() ? 0 : (double)total_postings / index.size();
        
        std::cout << "\nСтатистика индекса:" << std::endl;
        std::cout << "==================" << std::endl;
        std::cout << "Документов: " << total_docs << std::endl;
        std::cout << "Всего токенов: " << total_tokens << std::endl;
        std::cout << "Уникальных основ: " << index.size() << std::endl;
        std::cout << "Всего постингов: " << total_postings << std::endl;
        std::cout << "Среднее постингов на основу: " << avg_postings_per_word << std::endl;
        
        if (!index.empty()) {
            double total_length = 0;
            for (const auto& [stem, _] : index) {
                total_length += stem.length();
            }
            std::cout << "Средняя длина основы: " << (total_length / index.size()) 
                      << " символов" << std::endl;
        } else {
            std::cout << "Средняя длина основы: 0" << std::endl;
        }
    }
    
    // Оценка качества поиска
    void evaluate_search_quality() {
        std::cout << "\nОценка качества поиска:" << std::endl;
        std::cout << "=======================" << std::endl;
        
        struct TestQuery {
            std::string query_utf8;
            std::string description;
            bool expect_improvement; // Ожидается ли улучшение от стемминга
        };
        
        std::vector<TestQuery> test_queries = {
            {"актёр фильм", "Простой запрос с существительными", true},
            {"сниматься в кино", "Запрос с глаголом", true},
            {"известный режиссёр", "Запрос с прилагательным", true},
            {"голливудская премьера", "Сложный запрос", true},
            {"американский актёр", "Запрос с прилагательным", true},
            {"кино театр", "Два существительных", true}
        };
        
        double total_improvement = 0;
        int queries_with_improvement = 0;
        
        for (const auto& test : test_queries) {
            std::cout << "\nЗапрос: \"" << test.query_utf8 << "\" (" 
                      << test.description << ")" << std::endl;
            
            // Поиск без стемминга
            auto results_without = search_utf8(test.query_utf8, false);
            
            // Поиск со стеммингом
            auto results_with = search_utf8(test.query_utf8, true);
            
            std::cout << "  Без стемминга: " << results_without.size() << " документов" << std::endl;
            std::cout << "  Со стеммингом: " << results_with.size() << " документов" << std::endl;
            
            if (!results_without.empty()) {
                double improvement = ((double)results_with.size() / results_without.size() - 1.0) * 100;
                std::cout << "  Прирост полноты: " << improvement << "%" << std::endl;
                
                if (improvement > 0) {
                    total_improvement += improvement;
                    queries_with_improvement++;
                }
                
                // Вывод первых 3 результатов
                std::cout << "  Топ-3 результата со стеммингом:" << std::endl;
                for (int i = 0; i < std::min(3, (int)results_with.size()); i++) {
                    std::cout << "    " << (i+1) << ". Документ #" << results_with[i]
                              << " (" << get_document_path(results_with[i]) << ")" << std::endl;
                }
            } else {
                std::cout << "  Нет результатов без стемминга" << std::endl;
                if (results_with.size() > 0) {
                    std::cout << "  Стемминг нашел " << results_with.size() 
                              << " документов, где не нашел поиск без стемминга" << std::endl;
                }
            }
        }
        
        if (queries_with_improvement > 0) {
            std::cout << "\nСредний прирост полноты: " 
                      << (total_improvement / queries_with_improvement) << "%" << std::endl;
        }
    }
    
    // Анализ проблем стемминга
    void analyze_stemming_problems() {
        std::cout << "\nАнализ проблем стемминга:" << std::endl;
        std::cout << "=========================" << std::endl;
        
        // Примеры омонимов
        std::vector<std::string> homonyms_utf8 = {
            "замок",  // замок (двери) и замок (строение)
            "мука",   // мука (страдание) и мука (продукт)
            "орган",  // орган (инструмент) и орган (часть тела)
            "ключ",   // ключ (инструмент) и ключ (источник)
            "лук"     // лук (оружие) и лук (овощ)
        };
        
        std::cout << "Проблема омонимов:" << std::endl;
        for (const auto& word_utf8 : homonyms_utf8) {
            std::wstring word = utf8_to_wstring(word_utf8);
            std::wstring stemmed = stemmer.stem(word);
            std::cout << "  " << word_utf8 << " -> " << wstring_to_utf8(stemmed) 
                      << " (потеря смысла)" << std::endl;
        }
        
        // Примеры перестемминга
        std::vector<std::string> overstemming_examples_utf8 = {
            "программа", "программист", "программирование",
            "университет", "университетский",
            "информация", "информационный", "информатик"
        };
        
        std::cout << "\nПроблема перестемминга:" << std::endl;
        for (size_t i = 0; i < overstemming_examples_utf8.size(); i += 2) {
            if (i + 1 < overstemming_examples_utf8.size()) {
                std::wstring word1 = utf8_to_wstring(overstemming_examples_utf8[i]);
                std::wstring word2 = utf8_to_wstring(overstemming_examples_utf8[i + 1]);
                
                std::wstring stem1 = stemmer.stem(word1);
                std::wstring stem2 = stemmer.stem(word2);
                
                std::cout << "  " << overstemming_examples_utf8[i] << " -> " << wstring_to_utf8(stem1)
                          << ", " << overstemming_examples_utf8[i + 1] << " -> " << wstring_to_utf8(stem2);
                
                if (stem1 == stem2) {
                    std::cout << " (слишком агрессивно)" << std::endl;
                } else {
                    std::cout << " (правильно)" << std::endl;
                }
            }
        }
        
        // Вывод рекомендаций
        std::cout << "\nРекомендации по улучшению:" << std::endl;
        std::cout << "1. Использовать словарь исключений для частых омонимов" << std::endl;
        std::cout << "2. Добавить контекстный анализ для определения части речи" << std::endl;
        std::cout << "3. Использовать более сложные алгоритмы (Snowball, Porter2)" << std::endl;
        std::cout << "4. Комбинировать стемминг с n-граммами" << std::endl;
        std::cout << "5. Реализовать откат к оригинальному слову при низкой уверенности" << std::endl;
    }
    
    // Сохранение индекса (опционально)
    void save_index(const std::string& filename) {
        std::ofstream file(filename);
        if (!file) {
            std::cerr << "Не удалось сохранить индекс" << std::endl;
            return;
        }
        
        file << "Индекс с использованием стемминга\n";
        file << "================================\n";
        file << "Документов: " << doc_paths.size() << "\n";
        file << "Всего токенов: " << [this]() {
            size_t total = 0;
            for (const auto& [doc_id, count] : doc_token_counts) {
                total += count;
            }
            return total;
        }() << "\n";
        file << "Уникальных основ: " << index.size() << "\n\n";
        
        file << "Топ-50 самых частых основ:\n";
        std::vector<std::pair<std::wstring, size_t>> stem_freq;
        
        for (const auto& [stem, doc_map] : index) {
            size_t total_positions = 0;
            for (const auto& [doc_id, positions] : doc_map) {
                total_positions += positions.size();
            }
            stem_freq.emplace_back(stem, total_positions);
        }
        
        // Сортировка по частоте
        std::sort(stem_freq.begin(), stem_freq.end(),
            [](const auto& a, const auto& b) { return a.second > b.second; });
        
        for (size_t i = 0; i < std::min(stem_freq.size(), (size_t)50); i++) {
            file << i+1 << ". " << wstring_to_utf8(stem_freq[i].first) 
                 << " - " << stem_freq[i].second << " вхождений\n";
        }
        
        file.close();
        std::cout << "\nИндекс сохранен в " << filename << std::endl;
    }
};

int main(int argc, char* argv[]) {
     // Настройка кодировки для Windows
    #ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    #endif
    
    // Устанавливаем локаль для работы с русскими символами
    std::locale::global(std::locale(""));
    
    std::cout << "Лабораторная работа №4: Стемминг в поисковой системе" << std::endl;
    std::cout << "=====================================================" << std::endl;
    
    if (argc < 2) {
        std::cerr << "Использование: " << argv[0] << " <путь_к_корпусу> [лимит_документов]" << std::endl;
        std::cerr << "Пример: " << argv[0] << " corpus_clean 1000" << std::endl;
        return 1;
    }
    
    std::string corpus_path = argv[1];
    int limit = 0;
    
    if (argc > 2) {
        limit = std::stoi(argv[2]);
    }
    
    // Проверка существования корпуса
    if (!fs::exists(corpus_path)) {
        std::cerr << "Ошибка: директория '" << corpus_path << "' не найдена!" << std::endl;
        return 1;
    }
    
    // Тестирование стеммера
    RussianStemmer stemmer;
    std::cout << "\n1. Тестирование стеммера:" << std::endl;
    stemmer.test();
    
    // Индексация корпуса
    IndexerWithStemming indexer;
    std::cout << "\n2. Индексация корпуса:" << std::endl;
    indexer.index_directory(corpus_path, limit);
    
    // Статистика индекса
    indexer.print_statistics();
    
    // Оценка качества поиска
    indexer.evaluate_search_quality();
    
    // Анализ проблем
    indexer.analyze_stemming_problems();
    
    // Сохранение индекса
    indexer.save_index("stemming_index.txt");
    
    // Пример интерактивного поиска
    std::cout << "\n3. Интерактивный поиск (для выхода введите 'exit'):" << std::endl;
    
    std::string input;
    std::cin.ignore(); // Очищаем буфер ввода
    
    while (true) {
        std::cout << "\nВведите поисковый запрос: ";
        std::getline(std::cin, input);
        
        if (input == "exit" || input == "выход") {
            break;
        }
        
        auto results = indexer.search_utf8(input, true);
        
        std::cout << "Найдено документов: " << results.size() << std::endl;
        
        for (int i = 0; i < std::min(5, (int)results.size()); i++) {
            std::cout << "  " << (i+1) << ". Документ #" << results[i] 
                      << " (" << indexer.get_document_path(results[i]) << ")" << std::endl;
        }
        
        if (results.empty()) {
            std::cout << "  Попробуйте другой запрос или используйте более общие слова." << std::endl;
        }
    }
    
    std::cout << "\nРабота завершена. Результаты сохранены в stemming_index.txt" << std::endl;
    
    return 0;
}