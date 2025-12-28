#ifndef FILE_PROCESSOR_H
#define FILE_PROCESSOR_H

#include <vector>
#include <string>
#include <utility> // для std::pair

// Предварительное объявление
class Tokenizer;

class FileProcessor {
private:
    std::vector<std::string> files;
    std::vector<std::pair<size_t, double>> processing_timings; // размер файла -> время обработки
    
public:
    void scan_directory(const std::string& path);
    void process_files(Tokenizer& tokenizer, 
                      size_t& total_tokens,
                      double& total_time,
                      size_t& total_bytes);
    
    size_t get_file_count() const { return files.size(); }
    const std::vector<std::string>& get_files() const { return files; }
    const std::vector<std::pair<size_t, double>>& get_timings() const { return processing_timings; }
};

#endif