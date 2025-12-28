import wikipediaapi
import re

# 1. Настройка пользовательского агента (обязательно)
USER_AGENT = "infpoisk/1.0" # Укажите ваши данные
wiki_wiki = wikipediaapi.Wikipedia(user_agent=USER_AGENT, language='ru')

# 2. Укажите название категории. Обратите внимание на префикс "Категория:" и регистр.
CATEGORY_NAME = "Категория:Актёры США"

def get_category_members(category_title, max_pages=50000):
    """Рекурсивно получает все страницы из категории и её подкатегорий."""
    category = wiki_wiki.page(category_title)
    page_titles = []

    if not category.exists():
        print(f"Категория '{category_title}' не найдена!")
        return page_titles

    print(f"Анализируем категорию: {category_title}")

    for member in category.categorymembers.values():
        # member.type может быть "page" (статья) или "subcat" (подкатегория)
        if member.ns == wikipediaapi.Namespace.CATEGORY:
            # Это подкатегория - заходим внутрь рекурсивно
            print(f"  Заходим в подкатегорию: {member.title}")
            page_titles.extend(get_category_members(member.title, max_pages))
        elif member.ns == wikipediaapi.Namespace.MAIN:
            # Это обычная статья - добавляем в список
            page_titles.append(member.title)

        if len(page_titles) >= max_pages:
            print(f"Достигнут лимит в {max_pages} страниц.")
            break

    return page_titles

if __name__ == "__main__":
    all_pages = get_category_members(CATEGORY_NAME, max_pages=40000)
    print(f"Всего найдено {len(all_pages)} страниц.")

    # 3. Сохраняем список в файл (просто названия статей)
    with open("page_list.txt", "w", encoding="utf-8") as f:
        for title in all_pages:
            f.write(title + "\n")
    print("Список сохранен в 'page_list.txt'")