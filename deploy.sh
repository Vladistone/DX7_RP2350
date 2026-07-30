#!/bin/bash

# Автоматический переход в корень проекта, где лежит сам скрипт
cd "$(dirname "$0")"

# Цвета для красивого вывода в терминал Mac
GREEN='\033[0;32m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}=== Запуск автоматического деплоя на GitHub ===${NC}"

# Проверяем статус Git
if [ -n "$(git status --porcelain)" ]; then
    echo -e "Обнаружены изменения. Подготовка файлов..."
    
    # Индексируем всё (сработает ваш настроенный .gitignore)
    git add .
    
    # Генерируем сообщение коммита с текущей датой и временем
    COMMIT_MSG="Update: $(date '+%Y-%m-%d %H:%M:%S')"
    
    echo -e "Фиксация изменений с сообщением: ${GREEN}\"$COMMIT_MSG\"${NC}"
    git commit -m "$COMMIT_MSG"
    
    # Отправляем на GitHub
    echo -e "Отправка на удаленный репозиторий через SSH..."
    git push origin main
    
    if [ $? -eq 0 ]; then
        echo -e "${GREEN}=== Успешно отправлено! ===${NC}"
    else
        echo "Ошибка при выполнении git push."
    fi
else
    echo -e "${GREEN}Изменений не обнаружено. Репозиторий чист.${NC}"
fi
