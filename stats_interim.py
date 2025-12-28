import os

def get_stats():
    raw_files = [f for f in os.listdir("corpus_raw") if f.endswith('.wikitext')]
    clean_files = [f for f in os.listdir("corpus_clean") if f.endswith('.txt')]
    
    raw_size = sum(os.path.getsize(os.path.join("corpus_raw", f)) for f in raw_files)
    clean_size = sum(os.path.getsize(os.path.join("corpus_clean", f)) for f in clean_files)
    
    print(f"ПРОМЕЖУТОЧНАЯ СТАТИСТИКА:")
    print(f"Скачано статей: {len(clean_files)}")
    print(f"Размер сырых данных: {raw_size:,} байт ({raw_size/1024**2:.1f} МБ)")
    print(f"Размер очищенного текста: {clean_size:,} байт ({clean_size/1024**2:.1f} МБ)")
    
    if clean_files:
        avg_raw = raw_size / len(clean_files)
        avg_clean = clean_size / len(clean_files)
        print(f"Средний размер сырого документа: {avg_raw:,.0f} байт")
        print(f"Средний объём текста в документе: {avg_clean:,.0f} байт")
        
        # Прогноз на весь корпус
        total_articles = 37197
        if len(clean_files) > 100:  # только если достаточно данных
            estimated_total = clean_size * total_articles / len(clean_files)
            print(f"\nПРОГНОЗ на весь корпус (37,197 статей):")
            print(f"Ориентировочный размер: {estimated_total/1024**3:.1f} ГБ")
            print(f"Осталось скачать: {total_articles - len(clean_files)} статей")

get_stats()