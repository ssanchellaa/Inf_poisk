#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <chrono>
#include <cstring>
#include <cctype>
#include <filesystem>

namespace fs = std::filesystem;

class BooleanIndexBuilder {
private:
    // Структура для хранения информации о документе
    struct Document {
        std::string title;      // Заголовок документа
        std::string path;       // Полный путь к файлу
        size_t file_size;       // Размер файла в байтах
        uint32_t token_count;   // Количество токенов в документе
    };
    
    // Структура для хранения информации о терме
    struct TermInfo {
        std::vector<uint32_t> doc_ids;     // Список документов, содержащих терм
        size_t total_occurrences;          // Общее количество вхождений терма
    };
    
    // Данные индекса
    std::vector<Document> documents;                          // Прямой индекс
    std::unordered_map<std::string, TermInfo> term_index;     // Обратный индекс
    std::vector<std::string> sorted_terms;                    // Отсортированные термы
    
    // Статистика
    struct Statistics {
        size_t total_documents = 0;      // Всего документов
        size_t total_tokens = 0;         // Всего токенов
        size_t unique_terms = 0;         // Уникальных термов
        size_t total_bytes = 0;          // Общий объем данных
        double indexing_time = 0.0;      // Время индексации
        double avg_term_length = 0.0;    // Средняя длина терма
    } stats;
    
    // Константы
    static constexpr uint32_t FILE_FORMAT_VERSION = 1;
    static constexpr char FILE_MAGIC[5] = "BIND";  // Boolean INDex
    static constexpr size_t HEADER_SIZE = 32;      // Размер заголовка в байтах
    
public:
    BooleanIndexBuilder() {
        reset_statistics();
    }
    
    // Основной метод построения индекса
    bool build_index(const std::string& corpus_path) {
        auto start_time = std::chrono::high_resolution_clock::now();
        
        std::cout << "Начало построения индекса..." << std::endl;
        std::cout << "Корпус: " << corpus_path << std::endl;
        
        try {
            // 1. Сканирование директории
            if (!scan_directory(corpus_path)) {
                std::cerr << "Ошибка: не удалось найти файлы в " << corpus_path << std::endl;
                return false;
            }
            
            std::cout << "Найдено файлов: " << stats.total_documents << std::endl;
            
            // 2. Обработка документов
            for (uint32_t doc_id = 0; doc_id < stats.total_documents; ++doc_id) {
                if (!process_document(doc_id)) {
                    std::cerr << "Предупреждение: не удалось обработать документ " 
                              << documents[doc_id].path << std::endl;
                    continue;
                }
                
                // Вывод прогресса
                if ((doc_id + 1) % 1000 == 0 || (doc_id + 1) == stats.total_documents) {
                    std::cout << "\rОбработано документов: " << (doc_id + 1) 
                              << " из " << stats.total_documents
                              << " (" << ((doc_id + 1) * 100 / stats.total_documents) << "%)";
                    std::cout.flush();
                }
            }
            
            std::cout << std::endl;
            
            // 3. Подготовка словаря термов
            prepare_term_dictionary();
            
            // 4. Расчет статистики
            calculate_statistics();
            
            auto end_time = std::chrono::high_resolution_clock::now();
            stats.indexing_time = std::chrono::duration<double>(end_time - start_time).count();
            
            std::cout << "Построение индекса завершено успешно!" << std::endl;
            
            return true;
            
        } catch (const std::exception& e) {
            std::cerr << "Критическая ошибка при построении индекса: " << e.what() << std::endl;
            return false;
        }
    }
    
    // Сохранение индекса в бинарный файл
    bool save_index(const std::string& output_path) {
        std::cout << "Сохранение индекса в файл: " << output_path << std::endl;
        
        try {
            std::ofstream out(output_path, std::ios::binary);
            if (!out.is_open()) {
                std::cerr << "Ошибка: не удалось создать файл " << output_path << std::endl;
                return false;
            }
            
            // 1. Запись заголовка
            write_file_header(out);
            
            // 2. Запись таблицы документов (прямой индекс)
            size_t doc_table_offset = write_document_table(out);
            
            // 3. Запись словаря термов
            size_t term_dict_offset = write_term_dictionary(out);
            
            // 4. Запись posting lists
            write_posting_lists(out);
            
            // 5. Обновление заголовка со смещениями
            update_file_header(out, doc_table_offset, term_dict_offset);
            
            out.close();
            
            std::cout << "Индекс успешно сохранен." << std::endl;
            return true;
            
        } catch (const std::exception& e) {
            std::cerr << "Ошибка при сохранении индекса: " << e.what() << std::endl;
            return false;
        }
    }
    
    // Вывод статистики
    void print_statistics() const {
        std::cout << "\n================================================" << std::endl;
        std::cout << "СТАТИСТИКА ПОСТРОЕНИЯ ИНДЕКСА" << std::endl;
        std::cout << "================================================" << std::endl;
        
        std::cout << "Общая статистика:" << std::endl;
        std::cout << "  Документов:          " << stats.total_documents << std::endl;
        std::cout << "  Уникальных термов:   " << stats.unique_terms << std::endl;
        std::cout << "  Всего токенов:       " << stats.total_tokens << std::endl;
        std::cout << "  Объём данных:        " << format_bytes(stats.total_bytes) << std::endl;
        
        std::cout << "\nКачество индекса:" << std::endl;
        std::cout << "  Средняя длина терма: " << stats.avg_term_length << " символов" << std::endl;
        std::cout << "  Токенов на документ: " 
                  << (stats.total_documents > 0 ? stats.total_tokens / stats.total_documents : 0) 
                  << std::endl;
        std::cout << "  Термов на документ:  " 
                  << (stats.total_documents > 0 ? stats.unique_terms / stats.total_documents : 0) 
                  << std::endl;
        
        std::cout << "\nПроизводительность:" << std::endl;
        std::cout << "  Время индексации:    " << stats.indexing_time << " секунд" << std::endl;
        std::cout << "  Скорость индексации: " 
                  << (stats.indexing_time > 0 ? stats.total_documents / stats.indexing_time : 0)
                  << " документов/сек" << std::endl;
        std::cout << "  Скорость обработки:  " 
                  << (stats.indexing_time > 0 ? stats.total_bytes / 1024.0 / stats.indexing_time : 0)
                  << " КБ/сек" << std::endl;
        std::cout << "  Скорость обработки:  " 
                  << (stats.indexing_time > 0 ? stats.total_tokens / stats.indexing_time : 0)
                  << " токенов/сек" << std::endl;
        
        // Примеры самых частых термов
        if (!sorted_terms.empty()) {
            std::cout << "\nТоп-10 самых частых термов:" << std::endl;
            size_t count = std::min<size_t>(10, sorted_terms.size());
            
            // Создаем временный вектор для сортировки по частоте
            std::vector<std::pair<std::string, size_t>> top_terms;
            for (size_t i = 0; i < count; ++i) {
                const std::string& term = sorted_terms[i];
                const TermInfo& info = term_index.at(term);
                top_terms.emplace_back(term, info.doc_ids.size());
            }
            
            // Сортируем по убыванию частоты
            std::sort(top_terms.begin(), top_terms.end(),
                [](const auto& a, const auto& b) { return a.second > b.second; });
            
            for (size_t i = 0; i < top_terms.size(); ++i) {
                const auto& [term, freq] = top_terms[i];
                std::cout << "  " << (i + 1) << ". '" << term << "' - " 
                          << freq << " документов" << std::endl;
            }
        }
        
        std::cout << "================================================" << std::endl;
    }
    
private:
    // Сброс статистики
    void reset_statistics() {
        stats = Statistics();
    }
    
    // Сканирование директории с документами
    bool scan_directory(const std::string& corpus_path) {
        try {
            // Проверка существования директории
            if (!fs::exists(corpus_path) || !fs::is_directory(corpus_path)) {
                std::cerr << "Ошибка: " << corpus_path << " не является директорией" << std::endl;
                return false;
            }
            
            // Сбор всех .txt файлов
            std::vector<fs::path> file_paths;
            for (const auto& entry : fs::directory_iterator(corpus_path)) {
                if (entry.is_regular_file() && entry.path().extension() == ".txt") {
                    file_paths.push_back(entry.path());
                }
            }
            
            // Сортировка по имени для воспроизводимости
            std::sort(file_paths.begin(), file_paths.end());
            
            // Инициализация документов
            documents.reserve(file_paths.size());
            for (const auto& filepath : file_paths) {
                Document doc;
                doc.path = filepath.string();
                doc.title = filepath.stem().string();  // Имя файла без расширения
                doc.file_size = fs::file_size(filepath);
                doc.token_count = 0;
                
                documents.push_back(doc);
            }
            
            stats.total_documents = documents.size();
            return stats.total_documents > 0;
            
        } catch (const std::exception& e) {
            std::cerr << "Ошибка при сканировании директории: " << e.what() << std::endl;
            return false;
        }
    }
    
    // Обработка одного документа
    bool process_document(uint32_t doc_id) {
        if (doc_id >= documents.size()) {
            return false;
        }
        
        Document& doc = documents[doc_id];
        
        try {
            // Чтение файла
            std::ifstream file(doc.path, std::ios::binary);
            if (!file.is_open()) {
                return false;
            }
            
            // Определение размера файла
            file.seekg(0, std::ios::end);
            size_t file_size = file.tellg();
            file.seekg(0, std::ios::beg);
            
            // Чтение содержимого
            std::string content(file_size, '\0');
            file.read(&content[0], file_size);
            file.close();
            
            stats.total_bytes += file_size;
            
            // Токенизация
            std::unordered_map<std::string, uint32_t> term_counts;  // Термы и их частоты в документе
            
            std::string current_token;
            bool in_token = false;
            
            for (char c : content) {
                if (is_valid_token_char(c)) {
                    // Добавляем символ к текущему токену (приводим к нижнему регистру)
                    current_token += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                    in_token = true;
                } else if (in_token) {
                    // Завершаем токен
                    if (current_token.length() > 1) {  // Игнорируем однобуквенные токены
                        term_counts[current_token]++;
                        stats.total_tokens++;
                        doc.token_count++;
                    }
                    current_token.clear();
                    in_token = false;
                }
            }
            
            // Последний токен
            if (in_token && current_token.length() > 1) {
                term_counts[current_token]++;
                stats.total_tokens++;
                doc.token_count++;
            }
            
            // Добавление термов в обратный индекс
            for (const auto& term_pair : term_counts) {
                const std::string& term = term_pair.first;
                TermInfo& info = term_index[term];
                
                // Добавляем документ в список (без дубликатов, так как term_counts уникальны)
                info.doc_ids.push_back(doc_id);
                info.total_occurrences += term_pair.second;
            }
            
            return true;
            
        } catch (const std::exception& e) {
            std::cerr << "Ошибка при обработке документа " << doc.path << ": " << e.what() << std::endl;
            return false;
        }
    }
    
    // Проверка, является ли символ допустимым в токене
    bool is_valid_token_char(char c) const {
        // Русские буквы
        if ((c >= 'а' && c <= 'я') || (c >= 'А' && c <= 'Я') || c == 'ё' || c == 'Ё') {
            return true;
        }
        
        // Английские буквы
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) {
            return true;
        }
        
        // Цифры
        if (c >= '0' && c <= '9') {
            return true;
        }
        
        // Некоторые специальные символы, которые могут быть частью слова
        if (c == '-' || c == '\'' || c == '&') {
            return true;
        }
        
        return false;
    }
    
    // Подготовка словаря термов (сортировка)
    void prepare_term_dictionary() {
        sorted_terms.reserve(term_index.size());
        
        // Копируем все термы
        for (const auto& term_pair : term_index) {
            sorted_terms.push_back(term_pair.first);
        }
        
        // Сортировка по алфавиту
        std::sort(sorted_terms.begin(), sorted_terms.end());
        
        // Сортировка doc_ids в каждом терме
        for (auto& term_pair : term_index) {
            std::sort(term_pair.second.doc_ids.begin(), term_pair.second.doc_ids.end());
            // Удаление дубликатов (на всякий случай)
            auto last = std::unique(term_pair.second.doc_ids.begin(), term_pair.second.doc_ids.end());
            term_pair.second.doc_ids.erase(last, term_pair.second.doc_ids.end());
        }
        
        stats.unique_terms = sorted_terms.size();
    }
    
    // Расчет статистики
    void calculate_statistics() {
        // Расчет средней длины терма
        double total_term_length = 0.0;
        for (const auto& term : sorted_terms) {
            total_term_length += term.length();
        }
        
        if (stats.unique_terms > 0) {
            stats.avg_term_length = total_term_length / stats.unique_terms;
        }
    }
    
    // Форматирование размера в байтах
    std::string format_bytes(size_t bytes) const {
        const char* units[] = {"Б", "КБ", "МБ", "ГБ", "ТБ"};
        size_t unit_index = 0;
        double size = static_cast<double>(bytes);
        
        while (size >= 1024.0 && unit_index < 4) {
            size /= 1024.0;
            unit_index++;
        }
        
        char buffer[32];
        snprintf(buffer, sizeof(buffer), "%.2f %s", size, units[unit_index]);
        return std::string(buffer);
    }
    
    // Запись заголовка файла
    void write_file_header(std::ofstream& out) {
        // Магическое число (4 байта)
        out.write(FILE_MAGIC, 4);
        
        // Версия формата (4 байта)
        uint32_t version = FILE_FORMAT_VERSION;
        out.write(reinterpret_cast<const char*>(&version), sizeof(version));
        
        // Количество документов (4 байта)
        uint32_t doc_count = static_cast<uint32_t>(documents.size());
        out.write(reinterpret_cast<const char*>(&doc_count), sizeof(doc_count));
        
        // Количество уникальных термов (4 байта)
        uint32_t term_count = static_cast<uint32_t>(sorted_terms.size());
        out.write(reinterpret_cast<const char*>(&term_count), sizeof(term_count));
        
        // Заполнители для смещений (будут обновлены позже)
        uint32_t placeholder = 0;
        for (int i = 0; i < 5; ++i) {  // 5 * 4 = 20 байт
            out.write(reinterpret_cast<const char*>(&placeholder), sizeof(placeholder));
        }
    }
    
    // Запись таблицы документов
    size_t write_document_table(std::ofstream& out) {
        size_t start_pos = out.tellp();
        
        for (const Document& doc : documents) {
            // Длина заголовка + заголовок
            uint32_t title_len = static_cast<uint32_t>(doc.title.size());
            out.write(reinterpret_cast<const char*>(&title_len), sizeof(title_len));
            out.write(doc.title.c_str(), title_len);
            
            // Длина пути + путь
            uint32_t path_len = static_cast<uint32_t>(doc.path.size());
            out.write(reinterpret_cast<const char*>(&path_len), sizeof(path_len));
            out.write(doc.path.c_str(), path_len);
            
            // Размер файла
            uint32_t file_size = static_cast<uint32_t>(doc.file_size);
            out.write(reinterpret_cast<const char*>(&file_size), sizeof(file_size));
            
            // Количество токенов
            uint32_t token_count = doc.token_count;
            out.write(reinterpret_cast<const char*>(&token_count), sizeof(token_count));
        }
        
        return start_pos;
    }
    
    // Запись словаря термов
    size_t write_term_dictionary(std::ofstream& out) {
        size_t start_pos = out.tellp();
        
        // Вычисляем смещения для posting lists заранее
        std::vector<uint32_t> posting_offsets;
        posting_offsets.reserve(sorted_terms.size());
        
        uint32_t current_offset = 0;
        
        for (const std::string& term : sorted_terms) {
            const TermInfo& info = term_index.at(term);
            uint32_t list_size = 4 + (static_cast<uint32_t>(info.doc_ids.size()) * 4); // K + doc_ids
            
            posting_offsets.push_back(current_offset);
            current_offset += list_size;
        }
        
        // Записываем словарь
        for (size_t i = 0; i < sorted_terms.size(); ++i) {
            const std::string& term = sorted_terms[i];
            const TermInfo& info = term_index.at(term);
            
            // Длина терма (2 байта, максимальная длина 65535)
            uint16_t term_len = static_cast<uint16_t>(term.size());
            out.write(reinterpret_cast<const char*>(&term_len), sizeof(term_len));
            
            // Сам терм
            out.write(term.c_str(), term_len);
            
            // Смещение к posting list
            uint32_t offset = posting_offsets[i];
            out.write(reinterpret_cast<const char*>(&offset), sizeof(offset));
            
            // Размер posting list в байтах
            uint32_t list_size = 4 + (static_cast<uint32_t>(info.doc_ids.size()) * 4);
            out.write(reinterpret_cast<const char*>(&list_size), sizeof(list_size));
            
            // Общее количество вхождений
            uint32_t total_occurrences = static_cast<uint32_t>(info.total_occurrences);
            out.write(reinterpret_cast<const char*>(&total_occurrences), sizeof(total_occurrences));
        }
        
        return start_pos;
    }
    
    // Запись posting lists
    void write_posting_lists(std::ofstream& out) {
        for (const std::string& term : sorted_terms) {
            const TermInfo& info = term_index.at(term);
            
            // Количество документов в списке
            uint32_t doc_count = static_cast<uint32_t>(info.doc_ids.size());
            out.write(reinterpret_cast<const char*>(&doc_count), sizeof(doc_count));
            
            // Список идентификаторов документов
            for (uint32_t doc_id : info.doc_ids) {
                out.write(reinterpret_cast<const char*>(&doc_id), sizeof(doc_id));
            }
        }
    }
    
    // Обновление заголовка файла
    void update_file_header(std::ofstream& out, size_t doc_table_offset, size_t term_dict_offset) {
        // Сохраняем текущую позицию
        size_t current_pos = out.tellp();
        
        // Переходим к началу файла
        out.seekp(16, std::ios::beg);  // Пропускаем магическое число, версию, doc_count, term_count
        
        // Записываем смещение таблицы документов
        uint32_t doc_offset = static_cast<uint32_t>(doc_table_offset);
        out.write(reinterpret_cast<const char*>(&doc_offset), sizeof(doc_offset));
        
        // Записываем смещение словаря термов
        uint32_t term_offset = static_cast<uint32_t>(term_dict_offset);
        out.write(reinterpret_cast<const char*>(&term_offset), sizeof(term_offset));
        
        // Записываем смещение posting lists (сразу после словаря)
        uint32_t posting_offset = static_cast<uint32_t>(term_dict_offset + calculate_term_dict_size());
        out.write(reinterpret_cast<const char*>(&posting_offset), sizeof(posting_offset));
        
        // Размер заголовка
        uint32_t header_size = HEADER_SIZE;
        out.write(reinterpret_cast<const char*>(&header_size), sizeof(header_size));
        
        // Размер файла
        uint32_t file_size = static_cast<uint32_t>(current_pos);
        out.write(reinterpret_cast<const char*>(&file_size), sizeof(file_size));
        
        // Возвращаемся на исходную позицию
        out.seekp(current_pos);
    }
    
    // Расчет размера словаря термов
    size_t calculate_term_dict_size() const {
        size_t size = 0;
        
        for (const std::string& term : sorted_terms) {
            size += 2;  // Длина терма (2 байта)
            size += term.size();  // Сам терм
            size += 4;  // Смещение к posting list
            size += 4;  // Размер posting list
            size += 4;  // Общее количество вхождений
        }
        
        return size;
    }
};

// Главная функция программы
int main(int argc, char* argv[]) {
    std::cout << "================================================" << std::endl;
    std::cout << "Лабораторная работа №6: Построение булева индекса" << std::endl;
    std::cout << "================================================" << std::endl;
    
    // Проверка аргументов командной строки
    if (argc != 3) {
        std::cout << "Использование: " << argv[0] << " <путь_к_корпусу> <выходной_файл>" << std::endl;
        std::cout << std::endl;
        std::cout << "Аргументы:" << std::endl;
        std::cout << "  <путь_к_корпусу> - директория с очищенными текстами (.txt файлы)" << std::endl;
        std::cout << "  <выходной_файл>  - путь для сохранения бинарного индекса" << std::endl;
        std::cout << std::endl;
        std::cout << "Пример:" << std::endl;
        std::cout << "  " << argv[0] << " corpus_clean boolean_index.bin" << std::endl;
        return 1;
    }
    
    std::string corpus_path = argv[1];
    std::string output_file = argv[2];
    
    try {
        // Создание и настройка индексатора
        BooleanIndexBuilder index_builder;
        
        // Построение индекса
        std::cout << "Этап 1: Построение индекса..." << std::endl;
        if (!index_builder.build_index(corpus_path)) {
            std::cerr << "Ошибка: не удалось построить индекс" << std::endl;
            return 1;
        }
        
        // Сохранение индекса
        std::cout << "\nЭтап 2: Сохранение индекса..." << std::endl;
        if (!index_builder.save_index(output_file)) {
            std::cerr << "Ошибка: не удалось сохранить индекс" << std::endl;
            return 1;
        }
        
        // Вывод статистики
        std::cout << "\nЭтап 3: Формирование отчета..." << std::endl;
        index_builder.print_statistics();
        
        std::cout << "\nРабота успешно завершена!" << std::endl;
        std::cout << "Индекс сохранен в файл: " << output_file << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "\nОшибка выполнения: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}