import os
import json

def get_final_stats():
    # Статистика по очищенным файлам
    clean_files = [f for f in os.listdir("corpus_clean") if f.endswith('.txt')]
    raw_files = [f for f in os.listdir("corpus_raw") if f.endswith('.wikitext')]
    
    clean_size = 0
    raw_size = 0
    doc_sizes = []
    text_sizes = []
    
    # Размеры всех файлов
    for f in clean_files:
        size = os.path.getsize(os.path.join("corpus_clean", f))
        clean_size += size
        text_sizes.append(size)
    
    for f in raw_files:
        size = os.path.getsize(os.path.join("corpus_raw", f))
        raw_size += size
        doc_sizes.append(size)
    
    print("=" * 50)
    print("ФИНАЛЬНАЯ СТАТИСТИКА КОРПУСА")
    print("=" * 50)
    print(f"Количество документов: {len(clean_files)}")
    print(f"Размер сырых данных: {raw_size:,} байт ({raw_size/1024**2:.2f} МБ)")
    print(f"Размер очищенного текста: {clean_size:,} байт ({clean_size/1024**2:.2f} МБ)")
    print(f"Средний размер документа: {sum(doc_sizes)/len(doc_sizes):,.0f} байт" if doc_sizes else "Нет данных")
    print(f"Средний объём текста: {sum(text_sizes)/len(text_sizes):,.0f} байт" if text_sizes else "Нет данных")
    
    # Дополнительная информация
    print(f"\nДОПОЛНИТЕЛЬНО:")
    print(f"Папка corpus_raw: {len(raw_files)} файлов")
    print(f"Папка corpus_clean: {len(clean_files)} файлов")
    
    # Сохраняем в файл для отчета
    stats = {
        "documents_count": len(clean_files),
        "raw_size_bytes": raw_size,
        "raw_size_mb": raw_size/1024**2,
        "clean_size_bytes": clean_size,
        "clean_size_mb": clean_size/1024**2,
        "avg_doc_size": sum(doc_sizes)/len(doc_sizes) if doc_sizes else 0,
        "avg_text_size": sum(text_sizes)/len(text_sizes) if text_sizes else 0
    }
    
    with open("corpus_stats.json", "w", encoding="utf-8") as f:
        json.dump(stats, f, indent=2, ensure_ascii=False)
    
    print(f"\nСтатистика сохранена в corpus_stats.json")

if __name__ == "__main__":
    get_final_stats()