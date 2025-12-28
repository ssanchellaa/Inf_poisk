# resume_download.py
import json
import os

def check_progress():
    """Проверяет текущий прогресс скачивания"""
    if not os.path.exists("download_progress.json"):
        print("Файл прогресса не найден. Начните заново с download_pages_robust.py")
        return
    
    with open("download_progress.json", "r", encoding="utf-8") as f:
        progress = json.load(f)
    
    total = 37197  # Общее количество статей
    downloaded = progress['successful']
    failed = progress['failed']
    last_index = progress['last_index']
    
    print(f"Текущий прогресс:")
    print(f"  Обработано статей: {last_index} из {total} ({last_index/total*100:.1f}%)")
    print(f"  Успешно скачано: {downloaded}")
    print(f"  Ошибок: {failed}")
    print(f"  Последний индекс: {last_index}")
    
    if last_index >= total:
        print("\nВсе статьи уже обработаны!")
    else:
        print(f"\nДля продолжения запустите: python download_pages_robust.py")

if __name__ == "__main__":
    check_progress()