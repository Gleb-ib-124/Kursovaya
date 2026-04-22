# Учебный разбор проекта TypoScript
> Для тех, кто не знает C++. Здесь объясняется каждое ключевое слово и конструкция, встречающаяся в коде.

---

## Содержание
1. [Что делает проект](#1-что-делает-проект)
2. [Структура файлов](#2-структура-файлов)
3. [Базовые понятия C++](#3-базовые-понятия-c)
4. [Типы данных](#4-типы-данных)
5. [Директивы препроцессора](#5-директивы-препроцессора)
6. [Ключевые слова языка](#6-ключевые-слова-языка)
7. [Операторы](#7-операторы)
8. [Структуры и классы](#8-структуры-и-классы)
9. [Стандартная библиотека C++](#9-стандартная-библиотека-c)
10. [MFC и Windows API](#10-mfc-и-windows-api)
11. [Разбор ключевых алгоритмов](#11-разбор-ключевых-алгоритмов)
12. [Быстрый словарь терминов](#12-быстрый-словарь-терминов)

---

## 1. Что делает проект

**TypoScript** — это Windows-приложение, которое принимает домен или URL и проверяет, не является ли он подделкой под известный сайт.

**Тайпосквоттинг** — атака на невнимательных пользователей: злоумышленник регистрирует домен, который выглядит почти как настоящий:
- `gooogle.com` — лишняя буква (google)
- `paypa1.com` — цифра 1 вместо буквы l
- `gооgle.com` — кириллические «о» вместо латинских

Программа находит такие домены с помощью **алгоритма Левенштейна** (подсчёт правок для превращения одной строки в другую) и **нормализации гомоглифов** (замена «похожих» символов на стандартные).

---

## 2. Структура файлов

```
TypoScript/
├── TyposquatDetector.h   — объявление движка (что существует)
├── TyposquatDetector.cpp — реализация движка (как работает)
├── TypoScriptDlg.h       — объявление главного окна
├── TypoScriptDlg.cpp     — реализация окна (кнопки, отчёт, diff)
├── TypoScript.h          — объявление приложения
├── TypoScript.cpp        — точка входа
├── pch.h / pch.cpp       — предкомпилированный заголовок
└── framework.h           — подключение MFC/Windows
```

**Правило:** `.h` — «оглавление» (объявления). `.cpp` — «текст книги» (реализация).

---

## 3. Базовые понятия C++

### Переменная
Именованная ячейка памяти, хранящая значение определённого типа.
```cpp
int dist = 2;        // целое число
bool found = false;  // логическое значение
```

### Функция
Именованный блок кода, который можно вызывать с параметрами.
```cpp
int Levenshtein(std::string_view a, std::string_view b) {
    // ...
    return dist; // возвращает число
}
```

### Метод
Функция, объявленная внутри класса. Вызывается через точку `.` (объект) или стрелку `->` (указатель).

### Lambda (лямбда-функция)
Безымянная функция, объявленная прямо внутри кода.
```cpp
auto mapCyrToLatin = [](const unsigned char b0, const unsigned char b1) -> char {
    // тело функции
};
```
- `[]` — захват переменных из внешнего контекста (здесь пусто — ничего не захватываем)
- `(b0, b1)` — параметры
- `-> char` — тип возвращаемого значения
- `auto` — тип выводится компилятором автоматически

---

## 4. Типы данных

| Тип | Что хранит | Пример |
|-----|-----------|--------|
| `int` | Целое число (32 бита) | `int dist = 2;` |
| `bool` | Истина или ложь | `bool ok = true;` |
| `char` | Один байт/символ | `char c = 'a';` |
| `unsigned char` | Байт 0–255 (без знака) | `unsigned char b = 0xD0;` |
| `std::size_t` | Беззнаковое целое для размеров/индексов | `size_t n = s.size();` |
| `BOOL` | Windows-версия bool: TRUE/FALSE | возврат из WinAPI |
| `COLORREF` | 32-битный цвет Windows: `RGB(r,g,b)` | цвет текста |
| `DWORD` | Беззнаковое 32-битное целое Windows | флаги, размеры |

### `unsigned char` vs `char`
```cpp
char c = 0xD0;           // -48 (со знаком — может быть отрицательным!)
unsigned char b = 0xD0;  // 208 (без знака — всегда 0..255)
```
При работе с UTF-8 байтами выше 0x7F **обязательно** нужен `unsigned char`, иначе сравнения с кодами типа `0xD0` будут неверными.

---

## 5. Директивы препроцессора

Начинаются с `#`. Обрабатываются до компиляции.

### `#pragma once`
```cpp
#pragma once
```
Подключить файл только один раз, даже если `#include` написан несколько раз.

### `#include`
```cpp
#include <algorithm>       // из стандартной библиотеки (угловые скобки)
#include "TyposquatDetector.h"  // из нашего проекта (кавычки)
```
Вставляет содержимое другого файла в это место.

### `#ifdef / #endif`
```cpp
#ifdef _DEBUG
#define new DEBUG_NEW  // только в режиме отладки
#endif
```
Код между `#ifdef` и `#endif` компилируется только если символ определён.

### `#define`
```cpp
#define new DEBUG_NEW
```
Текстовая замена: каждое `new` в коде заменяется на `DEBUG_NEW`.

---

## 6. Ключевые слова языка

### `namespace`
«Папка» для имён — исключает конфликты.
```cpp
namespace {
    // Анонимный namespace: видно только в этом .cpp файле
    bool IsSpaceAscii(char c) { ... }
    char ToLowerAscii(char c) { ... }
} // namespace
```
`std::` — тоже namespace: `std::string`, `std::vector`, `std::sort`.

### `struct`
Группирует несколько полей вместе. По умолчанию всё `public`.
```cpp
struct TyposquatHit {
    std::string canonicalPopular; // какой популярный домен
    int editDistance = 0;         // насколько отличается
    int weight = 0;               // вес риска
    bool highRisk = false;        // флаг высокого риска
};
```
`= 0` и `= false` — значения по умолчанию при создании объекта.

### `class`
Как `struct`, но по умолчанию всё `private`. Используется для сокрытия внутренней логики.

### `public` / `private`
```cpp
class TyposquatDetector {
public:
    TyposquatDetector();           // доступно всем
    TyposquatReport Analyze(...);  // доступно всем
private:
    std::vector<PopularDomain> popular_; // скрыто — только внутри класса
    static int Levenshtein(...);         // скрыто
};
```

### `const`
«Не изменять». Бывает в нескольких позициях:
```cpp
const int dist = 2;          // переменная не изменится
void Analyze() const;        // метод не меняет поля объекта
const PopularDomain& pop     // ссылка на неизменяемый объект
```

### `constexpr`
Константа, известная **на этапе компиляции**.
```cpp
inline constexpr int kTyposquatSuspiciousScoreThreshold = 3;
inline constexpr std::size_t kMaxTyposquatAnalyzeInputBytes = 512 * 1024;
```

### `inline`
Позволяет определить константу прямо в `.h` файле без ошибок компоновщика.

### `static`
В классе — метод принадлежит классу, а не конкретному объекту:
```cpp
static std::string ExtractHost(std::string_view raw);
// Вызов: TyposquatDetector::ExtractHost(...)
// Не нужно создавать объект TyposquatDetector
```

### `override`
Переопределяет метод базового класса; компилятор проверяет что такой метод существует.
```cpp
BOOL InitInstance() override; // переопределяем метод из CWinApp
```

### `explicit`
Запрещает неявные преобразования типов в конструктор.
```cpp
explicit CTypoScriptDlg(CWnd* pParent = nullptr);
```

### `noexcept`
Гарантия: функция никогда не бросает исключений.
```cpp
CTypoScriptApp() noexcept;
```

### `return`
Завершить функцию и вернуть значение.
```cpp
return {};        // вернуть пустую строку std::string
return report;    // вернуть объект TyposquatReport
return;           // выйти из void-функции
```

### `continue`
Пропустить оставшуюся часть тела цикла, перейти к следующей итерации.

### `break`
Немедленно выйти из цикла.

### `nullptr`
Нулевой указатель — «указатель в никуда».

---

## 7. Операторы

### Арифметические
| Оператор | Смысл |
|----------|-------|
| `+` `-` `*` `/` | Сложение, вычитание, умножение, деление |
| `+=` | `a += b` то же, что `a = a + b` |
| `++i` | Прибавить 1 к i (до использования) |

### Сравнения
| Оператор | Смысл |
|----------|-------|
| `==` | равно |
| `!=` | не равно |
| `<` `>` `<=` `>=` | меньше, больше, и т.д. |

### Логические
| Оператор | Смысл | Пример |
|----------|-------|--------|
| `&&` | И — оба должны быть true | `dist >= 0 && dist <= 2` |
| `\|\|` | ИЛИ — хотя бы одно true | `hasHighRisk \|\| score >= 3` |
| `!` | НЕ — инверсия | `!s.empty()` |

**Короткое замыкание:** В `A && B`, если A ложно — B не вычисляется. В `A || B`, если A истинно — B не вычисляется.

### Тернарный оператор `?:`
```cpp
// если dist > 0 — берём dist, иначе 0
int safeD = dist > 0 ? dist : 0;

// длиннее или равно — длиннее из двух
const std::size_t lenDiff =
    folded.size() > pop.domain.size()
        ? folded.size() - pop.domain.size()
        : pop.domain.size() - folded.size();
```

### Доступ к члену `.` и `->`
```cpp
report.suspicious        // доступ к полю объекта (не указатель)
lpMMI->ptMinTrackSize    // доступ через указатель (эквивалент (*lpMMI).field)
```

### Побитовое ИЛИ `|`
```cpp
CFile::modeRead | CFile::typeBinary
// складывает флаги в одно число
```

### Оператор разрешения области `::` 
```cpp
TyposquatDetector::Levenshtein(a, b);
// "функция Levenshtein из класса TyposquatDetector"
std::string   // "string из пространства имён std"
```

### Адрес `&` и разыменование `*`
```cpp
m_pMainWnd = &dlg;   // взять адрес объекта dlg → указатель
*ptr                  // получить значение по указателю ptr
```

---

## 8. Структуры и классы

### Конструктор
Метод с именем класса — вызывается при создании объекта.
```cpp
TyposquatDetector::TyposquatDetector() {
    popular_ = {
        {"google.com"},
        {"youtube.com"},
        // ...
    };
}
```
`{ {"google.com"}, ... }` — список инициализации вектора структур `PopularDomain`.

### Наследование
```cpp
class CTypoScriptDlg : public CDialogEx { ... };
// CTypoScriptDlg — дочерний класс (получает всё от CDialogEx)
// CDialogEx      — родительский класс
```

### Список инициализации базового класса
```cpp
CTypoScriptDlg::CTypoScriptDlg(CWnd* pParent)
    : CDialogEx(IDD_TYPOSCRIPT_DIALOG, pParent) {}
//   ^^^^^^^^^^ вызов конструктора родительского класса
```

### Ссылка `&` в параметрах
```cpp
static std::string HomoglyphFoldAscii(std::string_view s, bool& hadLookalikeMap);
//                                                         ^^^^
// hadLookalikeMap — ссылка: функция записывает результат в переменную вызывающего кода
// Это способ вернуть ВТОРОЙ результат (первый — через return)
```

### `= default`
```cpp
CTypoScriptApp::CTypoScriptApp() noexcept = default;
// Тело конструктора генерирует компилятор (пустое)
```

### Подчёркивание в конце имени поля
```cpp
std::vector<PopularDomain> popular_;
bool layoutReady_ = false;
```
Соглашение: `_` в конце → «это поле класса» (не локальная переменная).

---

## 9. Стандартная библиотека C++

### `std::string`
Строка переменной длины.
```cpp
std::string out;
out.reserve(s.size());    // зарезервировать память (избегаем перевыделений)
out.push_back(c);         // добавить символ в конец
out.empty();              // true если пустая
out.size();               // длина в байтах
out.front();              // первый символ
out.back();               // последний символ
out.pop_back();           // удалить последний символ
out.erase(0, 4);          // удалить 4 символа с позиции 0
out.data();               // указатель на байты
std::string empty = {};   // пустая строка через {}
```

### `std::string_view`
«Вид» на чужую строку — не копирует данные, просто хранит указатель и длину. Быстрее `std::string` для чтения.
```cpp
std::string_view s = "hello";
s.empty();         // проверка на пустоту
s.front();         // первый символ
s.back();          // последний символ
s.size();          // длина в байтах
s.data();          // указатель на байты
s.remove_prefix(1); // убрать 1 символ слева (не копирует!)
s.remove_suffix(1); // убрать 1 символ справа
s.find(':');        // найти символ, вернуть позицию или std::string::npos
s.substr(0, 4);     // подстрока (тоже string_view, без копирования)
```

### `std::vector<T>`
Динамический массив.
```cpp
std::vector<int> row(m + 1);   // вектор из m+1 нулей
row.size();                     // количество элементов
row[j];                         // доступ по индексу
row.push_back(x);               // добавить в конец
row.begin(), row.end();         // итераторы (начало и конец)
```

### `std::sort`
Сортировка вектора.
```cpp
std::sort(
    report.hits.begin(),
    report.hits.end(),
    [](const TyposquatHit& a, const TyposquatHit& b) {
        // функция сравнения: true если a должна стоять раньше b
        return a.editDistance < b.editDistance;
    });
```

### `std::min` / инициализатор списка `{...}`
```cpp
const int cur = (std::min)({del, ins, sub});
// (std::min) в скобках — обход Windows-макроса min(...)
// {del, ins, sub} — список из трёх значений, берём минимальное
```

### `std::memcmp`
Побайтовое сравнение двух участков памяти.
```cpp
std::memcmp(out.data(), www.data(), www.size()) == 0
// возвращает 0 если www.size() байт полностью совпадают
// используется для проверки префикса "www."
```

### `std::string::npos`
Специальное значение «не найдено» — результат `find()` при отсутствии символа.
```cpp
std::size_t colon = host.find(':');
if (colon != std::string::npos) {
    host = host.substr(0, colon); // отрезать порт
}
```

### `static_cast<T>`
Явное безопасное приведение типа.
```cpp
static_cast<char>(b0)           // unsigned char → char
static_cast<int>(m)             // size_t → int
static_cast<unsigned char>(s[i])// char → unsigned char
```

---

## 10. MFC и Windows API

**MFC** — библиотека классов, оборачивающая Windows API.

### Ключевые классы

| Класс | Что это |
|-------|---------|
| `CWinApp` | Само приложение; точка входа, цикл сообщений |
| `CDialogEx` | Диалоговое окно |
| `CWnd` | Базовый класс любого окна |
| `CRichEditCtrl` | Поле вывода с цветным текстом |
| `CComboBox` | Выпадающий список |
| `CRect` | Прямоугольник (left, top, right, bottom) |
| `CString` | Unicode-строка Windows (UTF-16) |

### Таблица сообщений MFC
```cpp
BEGIN_MESSAGE_MAP(CTypoScriptDlg, CDialogEx)
    ON_BN_CLICKED(IDC_BTN_CHECK, &CTypoScriptDlg::OnBnClickedCheck)
    ON_BN_CLICKED(IDC_BTN_CLEAR, &CTypoScriptDlg::OnBnClickedClear)
    ON_WM_SIZE()
    ON_WM_GETMINMAXINFO()
END_MESSAGE_MAP()
```
«Когда нажата кнопка IDC_BTN_CHECK → вызвать OnBnClickedCheck».

### `_T("строка")`
В Unicode-сборке — `L"строка"` (UTF-16). В ANSI — `"строка"`. Нужен для совместимости.

### CHARFORMAT2
Форматирование текста в RichEdit:
```cpp
CHARFORMAT2 cf = {};         // {} — обнулить все поля структуры
cf.cbSize = sizeof(cf);      // обязательный размер структуры
cf.dwMask = CFM_COLOR;       // меняем только цвет
cf.crTextColor = RGB(200,30,30); // красный
m_editOutput.SetSelectionCharFormat(cf);
m_editOutput.ReplaceSel(chunk);  // вставить цветной текст
```

### UTF-8 ↔ UTF-16 (CString ↔ std::string)
Внутри детектора строки — UTF-8 (`std::string`). В UI — UTF-16 (`CString`). При передаче туда-обратно используем Windows API:
```cpp
// UTF-16 → UTF-8
WideCharToMultiByte(CP_UTF8, 0, s, -1, nullptr, 0, nullptr, nullptr);
// UTF-8 → UTF-16
MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.data(), byteLen, nullptr, 0);
```

---

## 11. Разбор ключевых алгоритмов

### ExtractHost — разбор URL

```
"https://www.google.com:443/search?q=1#top"
          ↑ start=8
                  ↑ после www.
                      ↑ хост до ":"
                              ↑ отрезали порт
                                     ↑ отрезали path/query/fragment
Итог: "google.com"
```

```cpp
// 1. Найти "://" — взять всё после него
for (std::size_t i = 0; i + 2 < n; ++i) {
    if (p[i] == ':' && p[i+1] == '/' && p[i+2] == '/') {
        start = i + 3;
        break;
    }
}

// 2. Обрезать по /, ?, #, '\'
for (std::size_t i = start; i < n; ++i) {
    if (c == '/' || c == '?' || c == '#' || c == '\\') {
        end = i; break;
    }
}

// 3. Убрать порт (найти ':' вне IPv6-скобок)
colon = host.find(':');
if (colon != std::string::npos)
    host = host.substr(0, colon);

// 4. Убрать www.
if (out.size() > 4 && memcmp(out.data(), "www.", 4) == 0)
    out.erase(0, 4);
```

---

### HomoglyphFoldAscii — нормализация гомоглифов

Задача: привести к единому виду символы, которые выглядят одинаково, но имеют разные коды.

```
Ввод: "gооgle.com"  (о — кириллические U+043E)
UTF-8 байты о: 0xD0 0xBE

mapCyrToLatin(0xD0, 0xBE) → 'o'  (латинское)

Итог: "google.com"
hadLookalikeMap = true  → это спуфинг!
```

```cpp
for (std::size_t i = 0; i < s.size();) {
    const unsigned char b0 = static_cast<unsigned char>(s[i]);

    if (b0 < 0x80) {             // ASCII-байт (0..127)
        char c = ToLowerAscii(static_cast<char>(b0));
        if (c == '0') { c = 'o'; hadLookalikeMap = true; }  // 0→o
        if (c == '1') { c = 'l'; hadLookalikeMap = true; }  // 1→l
        out.push_back(c);
        ++i;
        continue;
    }

    if (i + 1 < s.size()) {       // двухбайтовый UTF-8 символ (0x80..0xFF)
        const unsigned char b1 = static_cast<unsigned char>(s[i + 1]);
        const char mapped = mapCyrToLatin(b0, b1);
        if (mapped != '\0') {
            out.push_back(mapped);
            hadLookalikeMap = true;
            i += 2;               // съели 2 байта
            continue;
        }
    }

    out.push_back(static_cast<char>(b0)); // неизвестный байт — копируем как есть
    ++i;
}
```

**Таблица заменяемых символов:**

| Кириллица | Заменяется на | Пример атаки |
|-----------|--------------|--------------|
| а (U+0430) | `a` | mаil.ru → mail.ru |
| е (U+0435) | `e` | facеbook.com |
| о (U+043E) | `o` | gооgle.com |
| р (U+0440) | `p` | раypal.com |
| с (U+0441) | `c` | уandex.ru |
| у (U+0443) | `y` | уoutube.com |
| х (U+0445) | `x` | — |
| к (U+043A) | `k` | — |
| 0 (цифра) | `o` | g00gle.com |
| 1 (цифра) | `l` | paypa1.com |

---

### Алгоритм Левенштейна

**Редакционное расстояние** — минимальное количество операций (вставка, удаление, замена), чтобы превратить одну строку в другую.

```
"gogle" → "google"
 g o g l e
 g o o g l e
       ↑ вставить 'o' → расстояние 1
```

```cpp
int TyposquatDetector::Levenshtein(std::string_view a, std::string_view b) {
    const std::size_t n = a.size(); // длина a
    const std::size_t m = b.size(); // длина b

    std::vector<int> row(m + 1);    // текущая строка DP-таблицы
    for (std::size_t j = 0; j <= m; ++j)
        row[j] = static_cast<int>(j); // row[j] = j (база: удалить j символов из b)

    for (std::size_t i = 1; i <= n; ++i) {
        int prev = static_cast<int>(i - 1); // dp[i-1][j-1] из предыдущей итерации
        row[0] = static_cast<int>(i);       // вставить i символов из a

        for (std::size_t j = 1; j <= m; ++j) {
            const int cost = (a[i-1] == b[j-1]) ? 0 : 1; // 0 если символы равны
            const int del = row[j] + 1;      // удалить символ из a
            const int ins = row[j-1] + 1;    // вставить символ в a
            const int sub = prev + cost;     // заменить или совпасть
            const int cur = (std::min)({del, ins, sub}); // выбрать минимум
            prev = row[j];
            row[j] = cur;
        }
    }
    return row[m]; // итоговое расстояние
}
```

**Алгоритм использует оптимизацию по памяти:** вместо матрицы n×m хранит только одну строку `row`, перезаписывая её слева направо.

---

### Скоринг и флаг suspicious

```cpp
// Вес зависит от расстояния:
// dist=1 → weight=5 (высокий риск, один символ отличия — явная атака)
// dist=2 → weight=3 (умеренный риск)

report.suspicious =
    hasHighRisk || report.totalScore >= kTyposquatSuspiciousScoreThreshold;
//  ^^^^^^^^^^^    ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
//  хотя бы одно  ИЛИ сумма весов >= 3
//  совпадение с
//  расстоянием 1
```

---

## 12. Быстрый словарь терминов

| Термин | Объяснение |
|--------|-----------|
| **Компилятор** | Переводит C++ код в машинный код (.exe) |
| **Препроцессор** | Обрабатывает `#include`, `#define` перед компиляцией |
| **Заголовок (.h)** | Объявления: «что существует» |
| **Единица компиляции (.cpp)** | Реализация: «как работает» |
| **Итератор** | «Курсор» для обхода коллекции |
| **Шаблон `<T>`** | Код, параметризованный типом: `vector<int>`, `vector<string>` |
| **RAII** | Ресурс привязан к объекту: открыл → деструктор закроет |
| **Гомоглиф** | Символы, визуально похожие, но разные в Unicode |
| **UTF-8** | Кодировка Unicode, 1–4 байта на символ |
| **UTF-16** | Кодировка Unicode, 2 байта на символ (используется в Windows API) |
| **Levenshtein distance** | Минимальное число правок для превращения одной строки в другую |
| **Тайпосквоттинг** | Регистрация доменов, похожих на популярные, для обмана пользователей |
| **DP (Dynamic Programming)** | Метод решения задач через таблицу промежуточных результатов |
| **Handle (HWND)** | Идентификатор объекта Windows (окна, контрола и т.д.) |
| **Message pump** | Цикл обработки сообщений Windows (клики, нажатия) |
| **Деструктор** | Метод, вызываемый при уничтожении объекта (освобождает ресурсы) |
