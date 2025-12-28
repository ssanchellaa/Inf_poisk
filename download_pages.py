import wikipediaapi
import mwparserfromhell
import os
import re
import time
import json
from tqdm import tqdm
from requests.exceptions import RequestException
import traceback

USER_AGENT = "UniversityProject/1.0 (student@university.edu)"
wiki_wiki = wikipediaapi.Wikipedia(
    user_agent=USER_AGENT,
    language='ru'
)

# Создаем папки для хранения
RAW_DIR = "corpus_raw"
CLEAN_DIR = "corpus_clean"
os.makedirs(RAW_DIR, exist_ok=True)
os.makedirs(CLEAN_DIR, exist_ok=True)

# Файл для записи прогресса
PROGRESS_FILE = "download_progress.json"

def load_progress():
    """Загружает прогресс скачивания из файла"""
    if os.path.exists(PROGRESS_FILE):
        try:
            with open(PROGRESS_FILE, 'r', encoding='utf-8') as f:
                return json.load(f)
        except:
            pass
    return {
        'last_index': 0,
        'successful': 0,
        'failed': 0,
        'failed_titles': []
    }

def save_progress(last_index, successful, failed, failed_titles):
    """Сохраняет прогресс скачивания в файл"""
    progress = {
        'last_index': last_index,
        'successful': successful,
        'failed': failed,
        'failed_titles': failed_titles,
        'timestamp': time.time()
    }
    with open(PROGRESS_FILE, 'w', encoding='utf-8') as f:
        json.dump(progress, f, ensure_ascii=False, indent=2)

def clean_wikitext(text):
    """Очищает вики-разметку, оставляя чистый текст."""
    try:
        wikicode = mwparserfromhell.parse(text)
        clean_text = wikicode.strip_code()
        clean_text = re.sub(r'\n{3,}', '\n\n', clean_text)
        return clean_text.strip()
    except Exception as e:
        print(f"    Ошибка при очистке текста: {e}")
        # Простая запасная очистка
        text = re.sub(r'\[\[.*?\]\]', '', text)
        text = re.sub(r'\{\{.*?\}\}', '', text, flags=re.DOTALL)
        text = re.sub(r'\'\'\'', '', text)
        return text.strip()

def download_with_retry(page_title, max_retries=3):
    """Скачивает статью с повторными попытками при ошибках."""
    for attempt in range(max_retries):
        try:
            if attempt > 0:
                wait_time = 2 ** attempt
                tqdm.write(f"    Повтор {attempt+1}/{max_retries}, жду {wait_time}сек...")
                time.sleep(wait_time)
            
            page = wiki_wiki.page(page_title)
            
            if not page.exists():
                tqdm.write(f"    Статья '{page_title}' не найдена")
                return None
            
            if not page.text or len(page.text.strip()) < 10:
                tqdm.write(f"    Статья '{page_title}' слишком короткая или пустая")
                return None
                
            return page.text
            
        except Exception as e:
            tqdm.write(f"    Ошибка при скачивании '{page_title}': {type(e).__name__}")
            if attempt == max_retries - 1:
                return None
            continue
    
    return None

def process_article(title, idx):
    """Обрабатывает одну статью, возвращает True если успешно"""
    try:
        # Создаем безопасное имя файла
        safe_title = re.sub(r'[<>:"/\\|?*]', '_', title)
        
        # Проверяем, не скачана ли уже статья
        raw_filename = os.path.join(RAW_DIR, f"{idx}_{safe_title}.wikitext")
        clean_filename = os.path.join(CLEAN_DIR, f"{idx}_{safe_title}.txt")
        
        # Если оба файла уже существуют, пропускаем
        if os.path.exists(raw_filename) and os.path.exists(clean_filename):
            file_size = os.path.getsize(clean_filename)
            if file_size > 100:  # файл не пустой
                return True, "уже существует"
        
        tqdm.write(f"  Скачиваю: {title}")
        
        # Скачиваем статью
        raw_text = download_with_retry(title)
        
        if raw_text is None:
            return False, "не удалось скачать"
        
        # Сохраняем сырой текст
        with open(raw_filename, "w", encoding="utf-8") as f:
            f.write(raw_text)
        
        # Очищаем и сохраняем чистый текст
        clean_text = clean_wikitext(raw_text)
        
        if len(clean_text.strip()) < 10:
            # Удаляем пустой файл
            if os.path.exists(raw_filename):
                os.remove(raw_filename)
            return False, "очищенный текст слишком короткий"
        
        with open(clean_filename, "w", encoding="utf-8") as f:
            f.write(clean_text)
        
        tqdm.write(f"  ✓ Сохранено: {title} ({len(clean_text)} символов)")
        return True, "успешно"
        
    except Exception as e:
        tqdm.write(f"  ✗ Ошибка при обработке '{title}': {str(e)[:100]}")
        return False, f"ошибка: {type(e).__name__}"

def main():
    # Загружаем список страниц
    try:
        with open("page_list.txt", "r", encoding="utf-8") as f:
            page_titles = [line.strip() for line in f.readlines()]
    except FileNotFoundError:
        print("Файл page_list.txt не найден! Сначала запустите get_category_pages.py")
        return
    
    print(f"Всего статей для скачивания: {len(page_titles)}")
    print(f"Папки: {RAW_DIR}/, {CLEAN_DIR}/")
    print("-" * 50)
    
    # Загружаем прогресс
    progress = load_progress()
    start_index = progress['last_index']
    successful = progress['successful']
    failed = progress['failed']
    failed_titles = progress['failed_titles']
    
    print(f"Продолжаем с индекса: {start_index}")
    print(f"Уже успешно: {successful}, ошибок: {failed}")
    
    # Проверяем существующие файлы
    existing_files = len([f for f in os.listdir(CLEAN_DIR) if f.endswith('.txt')])
    print(f"Найдено существующих файлов в {CLEAN_DIR}: {existing_files}")
    
    # Основной цикл скачивания
    batch_size = 50  # Сохраняем прогресс каждые 50 статей
    
    try:
        for idx in tqdm(range(start_index, len(page_titles)), 
                       desc="Скачивание", 
                       initial=start_index,
                       total=len(page_titles)):
            
            title = page_titles[idx]
            idx_num = idx + 1
            
            # Обрабатываем статью
            result, message = process_article(title, idx_num)
            
            if result:
                successful += 1
            else:
                failed += 1
                failed_titles.append(title)
            
            # Сохраняем прогресс каждые batch_size статей
            if (idx + 1) % batch_size == 0:
                save_progress(idx + 1, successful, failed, failed_titles)
                tqdm.write(f"Прогресс: {idx+1}/{len(page_titles)}, успешно: {successful}, ошибок: {failed}")
            
            # Пауза между запросами
            time.sleep(0.5)  # 0.5 секунды между запросами
    
    except KeyboardInterrupt:
        print("\n\nСкачивание прервано пользователем.")
    
    finally:
        # Финальное сохранение
        save_progress(min(idx + 1, len(page_titles)), successful, failed, failed_titles)
        
        print(f"\n{'='*50}")
        print(f"ИТОГИ СКАЧИВАНИЯ:")
        print(f"Всего обработано: {idx + 1 if 'idx' in locals() else start_index}")
        print(f"Успешно скачано: {successful}")
        print(f"Не удалось скачать: {failed}")
        
        if failed_titles:
            print(f"\nСтатьи с ошибками сохранены в failed_downloads.txt")
            with open("failed_downloads.txt", "w", encoding="utf-8") as f:
                for title in failed_titles:
                    f.write(title + "\n")

if __name__ == "__main__":
    main()