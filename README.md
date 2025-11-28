# 📋 ПОЛНЫЙ ОТЧЕТ ДЛЯ GITHUB: Фаззинг-тестирование DragonflyDB

## 🎯 Содержание
1. [Цель работы](#1-цель-работы)
2. [Объект тестирования](#2-объект-тестирования)
3. [Методология](#3-методология)
4. [Этапы работы](#4-этапы-работы)
5. [Результаты](#5-результаты)
6. [Контрольные суммы](#6-контрольные-суммы)
7. [Выводы](#7-выводы)
8. [Структура отчёта](#8-структура-отчёта)

---

## 1. 🎯 Цель работы

Фаззинг-тестирование парсера RESP протокола базы данных DragonflyDB для проверки устойчивости к невалидным и случайным входным данным.
Задачи:
- Проверить устойчивость парсера к некорректным данным
- Оценить покрытие кода при фаззинг-тестировании
- Проанализировать производительность и стабильность системы

## 2. 🔍 Объект тестирования

### Компоненты DragonflyDB:
- **Парсер RESP протокола** (Redis Serialization Protocol)
- **Валидатор команд** (GET, SET, PING, HSET, HGET, LPUSH, etc.)
- **Обработчик ошибок** и исключительных ситуаций

### Формат RESP:
```
*3           ← массив из 3 элементов
$3           ← строка длиной 3 байта
SET          ← команда
$5           ← строка длиной 5 байт
hello        ← ключ
$5           ← строка длиной 5 байт
world        ← значение
```

---

## 3. ⚙️ Методология
Инструменты:
- **AFL++ 4.35a** - фаззинг-фреймворк
- **Kali Linux** - операционная система
- **GCC 14.3.0** - компилятор с инструментацией
- **LCOV/GCOV** - анализ покрытия кода
Параметры тестирования:
- **Время**: 4 часа
- **Память**: 512 МБ на тест
- **Таймаут**: 1 секунда на тест
- **Режим**: инструментированный бинарник

---

## 4. 📈 Этапы работы

Этап 1: Подготовка среды
```bash
# Установка AFL++
cd ~
git clone https://github.com/AFLplusplus/AFLplusplus.git
cd AFLplusplus
make -j$(nproc) all

# Установка зависимостей
sudo apt install -y clang lld llvm g++ lcov gcovr
```

### Этап 2: Сбор контрольных сумм
```bash
echo "🧮 Подсчёт контрольных сумм файлов парсера..."

# Общая контрольная сумма интерфейса
cat parser_files.txt | sort | xargs cat 2>/dev/null | sha256sum > parser_interface_checksum.txt

# Индивидуальные контрольные суммы
echo "📊 Индивидуальные контрольные суммы:"
while read file; do
    if [ -f "$file" ]; then
        sha256sum "$file"
    fi
done < parser_files.txt > individual_checksums.txt

echo "=== РЕЗУЛЬТАТЫ ПОДСЧЕТА КОНТРОЛЬНЫХ СУММ ==="
echo "🎯 Общая контрольная сумма интерфейса парсера:"
cat parser_interface_checksum.txt
echo ""
echo "📋 Индивидуальные суммы:"
cat individual_checksums.txt
echo ""

```
https://github.com/Kiki-Mokko/dragonfly-fuzzing-kali/blob/main/docs/CHECKSUMS.md#control-sums-of-dragonfly-parser-files

### Этап 3: Создание тестовой обёртки
Разработана программа-фаззер на C++, имитирующая работу парсера RESP протокола.

**Основные функции:**
- Парсинг массивов команд (`*`)
- Обработка bulk strings (`$`)
- Валидация простых строк (`+`)
- Обработка ошибок (`-`)
- Работа с числами (`:`)
 (см. src/dragonfly_parser_fuzzer.cpp)
 
### Этап 4: Подготовка тестовых данных
Создан корпус из 5 валидных RESP команд:
- `PING` - проверка доступности
- `GET/SET` - базовые операции
- `HSET/HGET` - работа с хэшами
- `LPUSH/RPUSH` - операции со списками
- Смешанные типы данных
<img width="516" height="714" alt="image" src="https://github.com/user-attachments/assets/e42094ce-bb01-46b1-95bc-baaecd980cec" />


### Этап 5: Запуск фаззинга
```bash
echo "Компиляция исправленной обёртки..."
$HOME/AFLplusplus/afl-clang-fast++ -g -O1 -o dragonfly_parser_fuzzer dragonfly_parser_fuzzer.cpp

echo "ПРОВЕРКА РАЗНЫХ КОДОВ ВОЗВРАТА:"
for testfile in fuzzing_corpus/*.resp; do
    echo -n "Тест $(basename $testfile): "
    ./dragonfly_parser_fuzzer < "$testfile"
    echo " результат: $?"
done
🚀 Запускаем фаззинг с исправленной обёрткой:
echo "ЗАПУСК ФАЗЗИНГА С ИСПРАВЛЕННОЙ ОБЁРТКОЙ..."
echo "Время начала: $(date)"

# Запускаем фаззинг
timeout 14400 $HOME/AFLplusplus/afl-fuzz \
    -i fuzzing_corpus/ \
    -o fuzzing_results/ \
    -M main_fuzzer \
    -t 1000 \
    -m 512M \
    -- ./dragonfly_parser_fuzzer @@

timeout 14400 afl-fuzz -i fuzzing_corpus/ -o fuzzing_results/ \
    -M main_fuzzer -t 1000 -m 512M -- ./dragonfly_parser_fuzzer @@
```
<img width="571" height="174" alt="image" src="https://github.com/user-attachments/assets/5ed31c44-4e73-4b6b-8da2-b475e8012667" />
<img width="791" height="577" alt="image" src="https://github.com/user-attachments/assets/24bc6796-c198-47c9-a80a-c0fd40dea7a5" />



### Этап 6: Анализ покрытия
```bash
echo "СБОР РЕЗУЛЬТАТОВ ФАЗЗИНГА..."
echo "Время завершения: $(date)"
echo ""

# 1. Основная статистика AFL++
echo "=== ОСНОВНАЯ СТАТИСТИКА AFL++ ==="
cat fuzzing_results/main_fuzzer/fuzzer_stats
echo ""

# 2. Детальный анализ
echo "=== ДЕТАЛЬНЫЙ АНАЛИЗ РЕЗУЛЬТАТОВ ==="
echo "Уникальных тест-кейсов: $(find fuzzing_results/main_fuzzer/queue/ -type f 2>/dev/null | wc -l)"
echo "Найдено сбоев: $(find fuzzing_results/main_fuzzer/crashes/ -type f 2>/dev/null | wc -l)"
echo "Найдено зависаний: $(find fuzzing_results/main_fuzzer/hangs/ -type f 2>/dev/null | wc -l)"
echo "Выполнено циклов: $(grep cycles_done fuzzing_results/main_fuzzer/fuzzer_stats | cut -d: -f2 | tr -d ' ')"
echo "Скорость тестирования: $(grep execs_per_sec fuzzing_results/main_fuzzer/fuzzer_stats | cut -d: -f2 | tr -d ' ') тестов/сек"

📸 ШАГ 1.3: Делаем скриншоты результатов
💻 СКРИНШОТ 1: Статистика AFL++
bash

# В ТЕРМИНАЛЕ выполните чтобы увидеть статистику:
cat fuzzing_results/main_fuzzer/fuzzer_stats

# СДЕЛАЙТЕ СКРИНШОТ терминала с этой статистикой
# Сохраните как: fuzzing_stats.jpg

📁 СКРИНШОТ 2: Список тест-кейсов
bash

# Покажем список найденных тестов
find fuzzing_results/main_fuzzer/queue/ -type f 2>/dev/null | head -20

# СДЕЛАЙТЕ СКРИНШОТ списка тестов
# Сохраните как: test_cases_list.jpg

🖥️ СКРИНШОТ 3: Общая статистика
bash

# Покажем общую сводку
echo "=== ИТОГОВАЯ СТАТИСТИКА ==="
grep -E "(execs_done|execs_per_sec|paths_total|unique_crashes|stability)" fuzzing_results/main_fuzzer/fuzzer_stats

# СДЕЛАЙТЕ СКРИНШОТ итоговой статистики
# Сохраните как: summary_stats.jpg

📈 ЭТАП 2: Получение графиков фаззинга
🎨 ШАГ 2.1: Генерируем графики
bash

# Убедитесь что вы в правильной папке:
cd ~/dragonfly_fuzzing_kali
pwd

# Создаем графики из данных AFL++
echo "📈 ГЕНЕРАЦИЯ ГРАФИКОВ AFL++..."
$HOME/AFLplusplus/afl-plot fuzzing_results/main_fuzzer/ fuzzing_plots/

# Проверяем что создалось
echo "✅ Графики созданы!"
ls -la fuzzing_plots/



СПОСОБ 1: Используем GCC вместо Clang (рекомендуется)
🗂️ ДИРЕКТОРИЯ: ~/dragonfly_fuzzing_kali/
bash

cd ~/dragonfly_fuzzing_kali

echo "🔄 ПЕРЕКОМПИЛЯЦИЯ С GCC ДЛЯ ПОКРЫТИЯ..."

# 1. Удаляем старые файлы
find . -name "*.gcda" -delete
find . -name "*.gcno" -delete
find . -name "*.gcov" -delete
rm -f dragonfly_parser_fuzzer_gcov

# 2. Компилируем с GCC (а не Clang)
g++ -g -O0 -fprofile-arcs -ftest-coverage -lgcov \
    -o dragonfly_parser_fuzzer_gcov dragonfly_parser_fuzzer.cpp

# 3. Проверяем
echo "📊 Проверка компиляции:"
file dragonfly_parser_fuzzer_gcov
find . -name "*.gcno" | head -3

🚀 Запускаем тесты с GCC версией
bash

echo "🔄 ЗАПУСК ТЕСТОВ С GCC..."

# Запускаем все тесты
TOTAL_CASES=$(find fuzzing_results/main_fuzzer/queue/ -type f 2>/dev/null | wc -l)
echo "📁 Обработка $TOTAL_CASES тестов..."

COUNTER=0
for testcase in fuzzing_results/main_fuzzer/queue/id*; do
    COUNTER=$((COUNTER + 1))
    if [ $((COUNTER % 50)) -eq 0 ]; then
        echo "Обработано $COUNTER/$TOTAL_CASES..."
    fi
    ./dragonfly_parser_fuzzer_gcov "$testcase" >/dev/null 2>&1
done

echo "✅ Тесты выполнены"

📊 Пробуем LCOV с GCC
bash

echo "🔄 LCOV С GCC..."

lcov --capture --directory . --output-file coverage.info

if [ -f "coverage.info" ]; then
    echo "✅ LCOV С GCC СРАБОТАЛ!"
    
    # Создаем HTML отчёт
    genhtml coverage.info --output-directory coverage_report_gcc --title "Покрытие кода (GCC)"
    
    if [ -f "coverage_report_gcc/index.html" ]; then
        echo "✅ HTML отчёт создан: coverage_report_gcc/index.html"
        xdg-open coverage_report_gcc/index.html 2>/dev/null || echo "Откройте вручную"
    fi
else
    echo "❌ LCOV с GCC тоже не сработал"
fi
