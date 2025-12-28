#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cstring>
#include <algorithm>  // <-- ДОБАВЬТЕ ЭТОТ ЗАГОЛОВОЧНЫЙ ФАЙЛ
#include <cstdint>    // <-- ДЛЯ uint32_t, uint16_t

class BooleanIndexReader {
private:
    struct FileHeader {
        char magic[4];
        uint32_t version;
        uint32_t doc_count;
        uint32_t term_count;
        uint32_t doc_table_offset;
        uint32_t term_dict_offset;
        uint32_t posting_offset;
        uint32_t header_size;
        uint32_t file_size;
    };
    
    struct DocumentInfo {
        std::string title;
        std::string path;
        uint32_t file_size;
        uint32_t token_count;
    };
    
    struct TermInfo {
        std::string term;
        uint32_t posting_offset;
        uint32_t posting_size;
        uint32_t total_occurrences;
    };
    
    FileHeader header;
    std::vector<DocumentInfo> documents;
    std::vector<TermInfo> term_dict;
    std::string index_file_path;
    
public:
    BooleanIndexReader(const std::string& file_path) : index_file_path(file_path) {}
    
    bool load_index() {
        std::ifstream in(index_file_path, std::ios::binary);
        if (!in.is_open()) {
            std::cerr << "Ошибка: не удалось открыть файл индекса" << std::endl;
            return false;
        }
        
        // Чтение заголовка
        in.read(reinterpret_cast<char*>(&header), sizeof(FileHeader));
        
        // Проверка магического числа
        if (std::string(header.magic, 4) != "BIND") {
            std::cerr << "Ошибка: неверный формат файла индекса" << std::endl;
            return false;
        }
        
        std::cout << "Информация об индексе:" << std::endl;
        std::cout << "  Версия формата: " << header.version << std::endl;
        std::cout << "  Документов: " << header.doc_count << std::endl;
        std::cout << "  Уникальных термов: " << header.term_count << std::endl;
        std::cout << "  Размер файла: " << header.file_size << " байт" << std::endl;
        
        // Чтение таблицы документов
        in.seekg(header.doc_table_offset);
        documents.reserve(header.doc_count);
        
        for (uint32_t i = 0; i < header.doc_count; ++i) {
            DocumentInfo doc;
            
            // Чтение заголовка
            uint32_t title_len;
            in.read(reinterpret_cast<char*>(&title_len), sizeof(title_len));
            doc.title.resize(title_len);
            in.read(&doc.title[0], title_len);
            
            // Чтение пути
            uint32_t path_len;
            in.read(reinterpret_cast<char*>(&path_len), sizeof(path_len));
            doc.path.resize(path_len);
            in.read(&doc.path[0], path_len);
            
            // Размер файла и количество токенов
            in.read(reinterpret_cast<char*>(&doc.file_size), sizeof(doc.file_size));
            in.read(reinterpret_cast<char*>(&doc.token_count), sizeof(doc.token_count));
            
            documents.push_back(doc);
        }
        
        // Чтение словаря термов
        in.seekg(header.term_dict_offset);
        term_dict.reserve(header.term_count);
        
        for (uint32_t i = 0; i < header.term_count; ++i) {
            TermInfo term;
            
            // Длина терма
            uint16_t term_len;
            in.read(reinterpret_cast<char*>(&term_len), sizeof(term_len));
            
            // Сам терм
            term.term.resize(term_len);
            in.read(&term.term[0], term_len);
            
            // Смещение и размер posting list
            in.read(reinterpret_cast<char*>(&term.posting_offset), sizeof(term.posting_offset));
            in.read(reinterpret_cast<char*>(&term.posting_size), sizeof(term.posting_size));
            in.read(reinterpret_cast<char*>(&term.total_occurrences), sizeof(term.total_occurrences));
            
            term_dict.push_back(term);
        }
        
        in.close();
        return true;
    }
    
    void print_document_info(uint32_t doc_id) {
        if (doc_id >= documents.size()) {
            std::cout << "Документ с ID " << doc_id << " не найден" << std::endl;
            return;
        }
        
        const DocumentInfo& doc = documents[doc_id];
        std::cout << "Документ ID: " << doc_id << std::endl;
        std::cout << "  Заголовок: " << doc.title << std::endl;
        std::cout << "  Путь: " << doc.path << std::endl;
        std::cout << "  Размер файла: " << doc.file_size << " байт" << std::endl;
        std::cout << "  Токенов: " << doc.token_count << std::endl;
    }
    
    void search_term(const std::string& term) {
        // Поиск терма в словаре (бинарный поиск)
        int left = 0, right = term_dict.size() - 1;
        int found_index = -1;
        
        while (left <= right) {
            int mid = left + (right - left) / 2;
            if (term_dict[mid].term == term) {
                found_index = mid;
                break;
            } else if (term_dict[mid].term < term) {
                left = mid + 1;
            } else {
                right = mid - 1;
            }
        }
        
        if (found_index == -1) {
            std::cout << "Терм '" << term << "' не найден в индексе" << std::endl;
            return;
        }
        
        const TermInfo& term_info = term_dict[found_index];
        
        std::cout << "Терм: '" << term << "'" << std::endl;
        std::cout << "  Всего вхождений: " << term_info.total_occurrences << std::endl;
        std::cout << "  Документов: " << (term_info.posting_size - 4) / 4 << std::endl;
        
        // Чтение списка документов
        std::ifstream in(index_file_path, std::ios::binary);
        in.seekg(header.posting_offset + term_info.posting_offset);
        
        uint32_t doc_count;
        in.read(reinterpret_cast<char*>(&doc_count), sizeof(doc_count));
        
        std::cout << "  Список документов (первые 10):" << std::endl;
        for (uint32_t i = 0; i < std::min(doc_count, (uint32_t)10); ++i) {
            uint32_t doc_id;
            in.read(reinterpret_cast<char*>(&doc_id), sizeof(doc_id));
            
            if (doc_id < documents.size()) {
                std::cout << "    " << doc_id << ". " << documents[doc_id].title << std::endl;
            }
        }
        
        if (doc_count > 10) {
            std::cout << "    ... и еще " << (doc_count - 10) << " документов" << std::endl;
        }
        
        in.close();
    }
    
    void print_term_stats(uint32_t count = 20) {
        count = std::min(count, static_cast<uint32_t>(term_dict.size()));
        
        std::cout << "Топ-" << count << " самых частых термов:" << std::endl;
        std::cout << "----------------------------------------" << std::endl;
        
        // Копируем и сортируем по total_occurrences
        std::vector<TermInfo> sorted_terms = term_dict;
        std::sort(sorted_terms.begin(), sorted_terms.end(),
            [](const TermInfo& a, const TermInfo& b) {
                return a.total_occurrences > b.total_occurrences;
            });
        
        for (uint32_t i = 0; i < count; ++i) {
            const TermInfo& term = sorted_terms[i];
            uint32_t doc_count = (term.posting_size - 4) / 4;
            std::cout << (i + 1) << ". '" << term.term << "' - "
                      << term.total_occurrences << " вхождений, "
                      << doc_count << " документов" << std::endl;
        }
    }
};

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cout << "Использование: " << argv[0] << " <файл_индекса>" << std::endl;
        return 1;
    }
    
    std::string index_file = argv[1];
    
    BooleanIndexReader reader(index_file);
    
    if (!reader.load_index()) {
        return 1;
    }
    
    // Тестовые операции
    std::cout << "\nПример работы с индексом:" << std::endl;
    std::cout << "------------------------" << std::endl;
    
    // Информация о первом документе
    reader.print_document_info(0);
    
    std::cout << "\n";
    
    // Поиск некоторых термов
    reader.search_term("актёр");
    
    std::cout << "\n";
    
    reader.search_term("фильм");
    
    std::cout << "\n";
    
    // Статистика по термам
    reader.print_term_stats(10);
    
    return 0;
}