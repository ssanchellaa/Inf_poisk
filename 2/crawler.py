import yaml
import sys
import time
import hashlib
from urllib.parse import urlparse, urljoin, urlunparse
from datetime import datetime
import requests
from bs4 import BeautifulSoup
from pymongo import MongoClient, ASCENDING, DESCENDING
import signal
import logging

class SearchCrawler:
    def __init__(self, config_path):
        """Инициализация краулера с конфигурацией из YAML-файла"""
        self.load_config(config_path)
        self.setup_logging()
        self.connect_to_db()
        self.setup_signal_handlers()
        self.running = True
        
    def load_config(self, config_path):
        """Загрузка конфигурации из YAML-файла"""
        try:
            with open(config_path, 'r', encoding='utf-8') as f:
                config = yaml.safe_load(f)
            
            self.db_config = config.get('db', {})
            self.logic_config = config.get('logic', {})
            self.seed_urls = config.get('seeds', [])
            self.restrictions = config.get('restrictions', {})
            
            # Параметры с значениями по умолчанию
            self.delay = self.logic_config.get('delay', 1)
            self.max_pages = self.logic_config.get('max_pages', 1000)
            self.revisit_interval = self.logic_config.get('revisit_interval', 604800)
            self.user_agent = self.logic_config.get('user_agent', 'SearchCrawler/1.0')
            self.timeout = self.logic_config.get('timeout', 10)
            self.max_retries = self.logic_config.get('max_retries', 3)
            
            self.allowed_domains = self.restrictions.get('allowed_domains', [])
            self.disallowed_paths = self.restrictions.get('disallowed_paths', [])
            
            self.logger.info(f"Конфигурация загружена из {config_path}")
            
        except Exception as e:
            print(f"Ошибка загрузки конфигурации: {e}")
            sys.exit(1)
    
    def setup_logging(self):
        """Настройка логирования"""
        logging.basicConfig(
            level=logging.INFO,
            format='%(asctime)s - %(name)s - %(levelname)s - %(message)s',
            handlers=[
                logging.FileHandler('crawler.log', encoding='utf-8'),
                logging.StreamHandler()
            ]
        )
        self.logger = logging.getLogger('SearchCrawler')
    
    def connect_to_db(self):
        """Подключение к MongoDB"""
        try:
            host = self.db_config.get('host', 'localhost')
            port = self.db_config.get('port', 27017)
            
            if self.db_config.get('username') and self.db_config.get('password'):
                connection_string = f"mongodb://{self.db_config['username']}:{self.db_config['password']}@{host}:{port}/"
            else:
                connection_string = f"mongodb://{host}:{port}/"
            
            self.client = MongoClient(connection_string)
            self.db = self.client[self.db_config.get('database', 'search_engine')]
            self.collection = self.db[self.db_config.get('collection', 'documents')]
            
            # Создание индексов для ускорения поиска
            self.collection.create_index([('url', ASCENDING)], unique=True)
            self.collection.create_index([('next_fetch', ASCENDING)])
            
            self.logger.info(f"Подключено к MongoDB: {host}:{port}")
            
        except Exception as e:
            self.logger.error(f"Ошибка подключения к MongoDB: {e}")
            sys.exit(1)
    
    def setup_signal_handlers(self):
        """Настройка обработчиков сигналов для graceful shutdown"""
        signal.signal(signal.SIGINT, self.signal_handler)
        signal.signal(signal.SIGTERM, self.signal_handler)
    
    def signal_handler(self, signum, frame):
        """Обработчик сигналов остановки"""
        self.logger.info(f"Получен сигнал {signum}. Останавливаю краулер...")
        self.running = False
    
    def normalize_url(self, url):
        """Нормализация URL (убираем фрагменты, сортируем параметры и т.д.)"""
        try:
            parsed = urlparse(url)
            
            # Убираем фрагменты (#)
            parsed = parsed._replace(fragment='')
            
            # Приводим к нижнему регистру схему и хост
            scheme = parsed.scheme.lower()
            netloc = parsed.netloc.lower()
            
            # Убираем стандартные порты
            if scheme == 'http' and parsed.port == 80:
                netloc = parsed.hostname.lower()
            elif scheme == 'https' and parsed.port == 443:
                netloc = parsed.hostname.lower()
            
            # Нормализуем путь (убираем дублирующиеся слэши)
            path = parsed.path
            while '//' in path:
                path = path.replace('//', '/')
            
            # Сортируем query-параметры
            query = parsed.query
            if query:
                params = query.split('&')
                params.sort()
                query = '&'.join(params)
            
            normalized = urlunparse((scheme, netloc, path, '', query, ''))
            return normalized
            
        except Exception as e:
            self.logger.error(f"Ошибка нормализации URL {url}: {e}")
            return url
    
    def is_allowed_url(self, url):
        """Проверка, можно ли обходить данный URL"""
        try:
            parsed = urlparse(url)
            
            # Проверка домена
            if self.allowed_domains:
                domain_allowed = False
                for domain in self.allowed_domains:
                    if parsed.netloc.endswith(domain):
                        domain_allowed = True
                        break
                if not domain_allowed:
                    return False
            
            # Проверка запрещенных путей
            for disallowed in self.disallowed_paths:
                if disallowed in url:
                    return False
            
            return True
            
        except Exception as e:
            self.logger.error(f"Ошибка проверки URL {url}: {e}")
            return False
    
    def calculate_content_hash(self, content):
        """Вычисление хэша содержимого для определения изменений"""
        return hashlib.md5(content.encode('utf-8')).hexdigest()
    
    def fetch_page(self, url):
        """Загрузка страницы с повторами при ошибках"""
        headers = {
            'User-Agent': self.user_agent,
            'Accept': 'text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8',
            'Accept-Language': 'ru-RU,ru;q=0.9,en-US;q=0.8,en;q=0.7',
        }
        
        for attempt in range(self.max_retries):
            try:
                response = requests.get(
                    url,
                    headers=headers,
                    timeout=self.timeout,
                    allow_redirects=True
                )
                response.raise_for_status()
                
                # Проверяем, что это HTML
                content_type = response.headers.get('content-type', '').lower()
                if 'text/html' not in content_type:
                    self.logger.warning(f"URL {url} не является HTML: {content_type}")
                    return None
                
                return response.text
                
            except requests.exceptions.RequestException as e:
                if attempt < self.max_retries - 1:
                    wait_time = 2 ** attempt  # Экспоненциальная задержка
                    self.logger.warning(f"Попытка {attempt+1}/{self.max_retries} для {url} не удалась. Жду {wait_time} сек...")
                    time.sleep(wait_time)
                else:
                    self.logger.error(f"Не удалось загрузить {url} после {self.max_retries} попыток: {e}")
        
        return None
    
    def extract_links(self, html, base_url):
        """Извлечение всех ссылок из HTML"""
        links = set()
        
        try:
            soup = BeautifulSoup(html, 'html.parser')
            
            for link_tag in soup.find_all('a', href=True):
                href = link_tag['href']
                
                # Пропускаем якорные ссылки и javascript
                if href.startswith('#') or href.startswith('javascript:'):
                    continue
                
                # Преобразуем относительные URL в абсолютные
                absolute_url = urljoin(base_url, href)
                
                # Нормализуем URL
                normalized_url = self.normalize_url(absolute_url)
                
                # Проверяем, можно ли обходить этот URL
                if self.is_allowed_url(normalized_url):
                    links.add(normalized_url)
            
        except Exception as e:
            self.logger.error(f"Ошибка извлечения ссылок из {base_url}: {e}")
        
        return links
    
    def extract_title(self, html):
        """Извлечение заголовка страницы"""
        try:
            soup = BeautifulSoup(html, 'html.parser')
            title_tag = soup.find('title')
            return title_tag.get_text().strip() if title_tag else 'Без заголовка'
        except Exception as e:
            self.logger.error(f"Ошибка извлечения заголовка: {e}")
            return 'Без заголовка'
    
    def needs_revisit(self, doc):
        """Проверка, нужно ли перезагружать страницу"""
        if not doc:
            return True
        
        last_fetch = doc.get('fetch_time', 0)
        current_time = int(time.time())
        
        # Проверяем, истек ли интервал перепроверки
        return (current_time - last_fetch) > self.revisit_interval
    
    def process_url(self, url):
        """Обработка одного URL"""
        self.logger.info(f"Обработка: {url}")
        
        # Проверяем, есть ли страница уже в базе
        existing_doc = self.collection.find_one({'url': url})
        
        # Если страница есть и не нужно перепроверять, пропускаем
        if existing_doc and not self.needs_revisit(existing_doc):
            self.logger.info(f"  Пропускаем (еще не истек интервал перепроверки)")
            return []
        
        # Загружаем страницу
        html = self.fetch_page(url)
        if not html:
            return []
        
        # Вычисляем хэш содержимого
        content_hash = self.calculate_content_hash(html)
        
        # Если страница уже есть и хэш не изменился, просто обновляем время
        if existing_doc and existing_doc.get('content_hash') == content_hash:
            self.collection.update_one(
                {'url': url},
                {
                    '$set': {
                        'fetch_time': int(time.time()),
                        'next_fetch': int(time.time()) + self.revisit_interval
                    }
                }
            )
            self.logger.info(f"  Контент не изменился, обновлено время проверки")
        else:
            # Извлекаем заголовок
            title = self.extract_title(html)
            
            # Сохраняем/обновляем документ
            document = {
                'url': url,
                'raw_html': html,
                'source_title': title,
                'fetch_time': int(time.time()),
                'content_hash': content_hash,
                'next_fetch': int(time.time()) + self.revisit_interval,
                'status': 'success'
            }
            
            self.collection.update_one(
                {'url': url},
                {'$set': document},
                upsert=True
            )
            
            self.logger.info(f"  Сохранено: {title}")
        
        # Извлекаем ссылки
        links = self.extract_links(html, url)
        
        # Добавляем новые ссылки в очередь
        new_links = []
        for link in links:
            # Проверяем, есть ли уже такая страница в базе
            existing = self.collection.find_one({'url': link})
            
            # Если нет или нужно перепроверить, добавляем в очередь
            if not existing or self.needs_revisit(existing):
                new_links.append(link)
        
        return new_links
    
    def get_next_url(self):
        """Получение следующего URL для обработки"""
        # Ищем страницы, которые нужно обновить (истек next_fetch)
        doc_to_update = self.collection.find_one(
            {'next_fetch': {'$lt': int(time.time())}},
            sort=[('next_fetch', ASCENDING)]
        )
        
        if doc_to_update:
            return doc_to_update['url']
        
        # Если нет страниц для обновления, берем из начального списка
        if self.seed_urls:
            url = self.seed_urls.pop(0)
            return url
        
        return None
    
    def run(self):
        """Основной цикл работы краулера"""
        self.logger.info("Запуск поискового робота...")
        
        processed_count = 0
        
        while self.running and processed_count < self.max_pages:
            try:
                # Получаем следующий URL
                url = self.get_next_url()
                
                if not url:
                    self.logger.info("Нет URL для обработки. Завершаю работу.")
                    break
                
                # Обрабатываем URL
                new_links = self.process_url(url)
                
                # Добавляем новые ссылки в seed_urls для дальнейшей обработки
                for link in new_links:
                    if link not in self.seed_urls:
                        self.seed_urls.append(link)
                
                # Увеличиваем счетчик
                processed_count += 1
                
                # Выводим статистику
                total_docs = self.collection.count_documents({})
                self.logger.info(f"Статистика: обработано {processed_count}/{self.max_pages}, всего в БД: {total_docs}")
                
                # Задержка между запросами
                if self.running and processed_count < self.max_pages:
                    time.sleep(self.delay)
                
            except Exception as e:
                self.logger.error(f"Ошибка в основном цикле: {e}")
                time.sleep(self.delay * 2)  # Удвоенная задержка при ошибке
        
        self.logger.info(f"Краулер остановлен. Обработано страниц: {processed_count}")
    
    def print_stats(self):
        """Вывод статистики"""
        total = self.collection.count_documents({})
        today = datetime.now().date()
        today_timestamp = int(time.mktime(today.timetuple()))
        
        today_count = self.collection.count_documents({
            'fetch_time': {'$gte': today_timestamp}
        })
        
        print(f"\n{'='*50}")
        print("СТАТИСТИКА КРАУЛЕРА:")
        print(f"Всего страниц в базе: {total}")
        print(f"Загружено сегодня: {today_count}")
        
        # Топ доменов
        pipeline = [
            {'$group': {'_id': {'$substr': ['$url', 0, 50]}, 'count': {'$sum': 1}}},
            {'$sort': {'count': -1}},
            {'$limit': 5}
        ]
        
        try:
            top_pages = list(self.collection.aggregate(pipeline))
            print("\nТоп-5 страниц:")
            for i, page in enumerate(top_pages, 1):
                print(f"  {i}. {page['_id']} - {page['count']}")
        except:
            pass

def main():
    """Основная функция"""
    if len(sys.argv) != 2:
        print("Использование: python crawler.py <путь_к_config.yaml>")
        sys.exit(1)
    
    config_path = sys.argv[1]
    
    try:
        crawler = SearchCrawler(config_path)
        crawler.run()
        crawler.print_stats()
    except KeyboardInterrupt:
        print("\nКраулер остановлен пользователем.")
    except Exception as e:
        print(f"Ошибка: {e}")
        sys.exit(1)

if __name__ == "__main__":
    main()