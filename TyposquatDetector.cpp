#include "pch.h"               /* Предкомпилированный заголовок: ускоряет сборку и тянет MFC/WinAPI/STL. */
#include "TyposquatDetector.h" /* Объявления TyposquatDetector, TyposquatReport, TyposquatHit. */

/* algorithm:
 *   std::sort  - сортировка контейнеров
 *   std::min   - минимум из значений
 * cstring:
 *   std::memcmp - побайтное сравнение памяти */
#include <algorithm>
#include <cstring>

namespace {

/* IsSpaceAscii(c):
 * Проверяет, является ли символ ASCII-пробелом.
 *
 * Сигнатура:
 *   bool IsSpaceAscii(const char c)
 * Где:
 *   bool  - возвращает true/false
 *   const - локальная копия c не изменяется внутри функции
 *   char  - 1 байт символа
 *
 * Примеры:
 *   IsSpaceAscii(' ')  -> true
 *   IsSpaceAscii('\n') -> true
 *   IsSpaceAscii('a')  -> false */
bool IsSpaceAscii(const char c) {
    /* == оператор "равно"
     * || оператор логического ИЛИ (short-circuit) */
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

/* ToLowerAscii(c):
 * Приводит только латинские ASCII A..Z к a..z.
 * UTF-8 многобайтные символы здесь НЕ обрабатываем.
 *
 * Пример:
 *   ToLowerAscii('G') -> 'g'
 *   ToLowerAscii('7') -> '7'
 *   ToLowerAscii(static_cast<char>(0xD0)) -> 0xD0 (без изменений) */
char ToLowerAscii(const char c) {
    /* >= и <=: диапазонная проверка */
    if (c >= 'A' && c <= 'Z') {
        /* static_cast<char>(...):
         * явное приведение типа к char */
        return static_cast<char>(c - 'A' + 'a');
    }
    return c;
}

/* TrimAsciiSpaces(s):
 * Убирает пробелы слева и справа из string_view.
 *
 * Важно:
 *   std::string_view не владеет памятью (это "вид"),
 *   поэтому remove_prefix/remove_suffix ничего не копируют.
 *
 * Пример:
 *   s = "  abc  " -> "abc" */
std::string_view TrimAsciiSpaces(std::string_view s) {
    while (!s.empty() && IsSpaceAscii(s.front())) { /* front() - первый символ */
        s.remove_prefix(1);                         /* убираем 1 символ слева */
    }
    while (!s.empty() && IsSpaceAscii(s.back())) { /* back() - последний символ */
        s.remove_suffix(1);                         /* убираем 1 символ справа */
    }
    return s;
}

} // namespace

/* TyposquatDetector::TyposquatDetector()
 * Конструктор класса.
 *
 * TyposquatDetector:: - оператор области видимости (scope resolution):
 *   "эта функция принадлежит классу TyposquatDetector".
 *
 * Заполняем базу популярных доменов для сравнения. */
TyposquatDetector::TyposquatDetector() {
    /* popular_ - поле класса (vector<PopularDomain>) */
    popular_ = {
        {"google.com"},
        {"youtube.com"},
        {"facebook.com"},
        {"instagram.com"},
        {"twitter.com"},
        {"x.com"},
        {"microsoft.com"},
        {"apple.com"},
        {"amazon.com"},
        {"netflix.com"},
        {"paypal.com"},
        {"github.com"},
        {"wikipedia.org"},
        {"linkedin.com"},
        {"vk.com"},
        {"mail.ru"},
        {"yandex.ru"},
        {"ok.ru"},
        {"whatsapp.com"},
        {"telegram.org"},
        {"zoom.us"},
        {"dropbox.com"},
        {"adobe.com"},
        {"ebay.com"},
        {"bbc.co.uk"},
    };
}

/* PopularDomains() const:
 * Возвращает наружу список популярных доменов.
 *
 * Почему const в конце метода:
 *   метод не изменяет состояние объекта this.
 *
 * Почему возвращаем vector<string> по значению:
 *   UI получает независимую копию, не ломая внутреннюю базу detector'а. */
std::vector<std::string> TyposquatDetector::PopularDomains() const {
    std::vector<std::string> out; /* создаём пустой vector<string> */
    out.reserve(popular_.size()); /* reserve = заранее выделить память (ускоряет push_back) */
    for (const PopularDomain& item : popular_) {
        /* const PopularDomain&:
         * const - не изменяем элемент
         * &     - без копирования */
        out.push_back(item.domain); /* push_back добавляет элемент в конец vector */
    }
    return out;
}

/* HomoglyphFoldAscii(s, hadLookalikeMap):
 * Нормализует строку домена:
 *  - lower-case ASCII,
 *  - 0 -> o,
 *  - 1 -> l,
 *  - кириллические визуально похожие символы -> латиница.
 *
 * Параметры:
 *   s                - вход (string_view)
 *   hadLookalikeMap  - bool& (ссылка), заполняется функцией
 *
 * Почему bool&:
 *   нужно вернуть два результата:
 *   1) новую строку (через return),
 *   2) флаг "была ли подмена" (через ссылку). */
std::string TyposquatDetector::HomoglyphFoldAscii(std::string_view s, bool& hadLookalikeMap) {
    hadLookalikeMap = false; /* старт: подмен не было */
    std::string out;         /* здесь собираем нормализованный результат */
    out.reserve(s.size());   /* часто длина примерно такая же, резервируем ёмкость */

    /* Локальная lambda mapCyrToLatin:
     * принимает 2 байта UTF-8 (b0,b1), возвращает латинский char или '\0' если нет маппинга.
     *
     * Сигнатура lambda:
     *   [](const unsigned char b0, const unsigned char b1) -> char { ... }
     *
     * unsigned char важен:
     *   чтобы байты 0x80..0xFF не становились отрицательными. */
    auto mapCyrToLatin = [](const unsigned char b0, const unsigned char b1) -> char {
        if (b0 == 0xD0) {
            switch (b1) {
            case 0xB0: case 0x90: return 'a'; // а А
            case 0xB5: case 0x95: return 'e'; // е Е
            case 0xBE: case 0x9E: return 'o'; // о О
            case 0xA0: return 'p';            // Р
            case 0xA1: return 'c';            // С
            case 0xA3: return 'y';            // У
            case 0xA5: return 'x';            // Х
            case 0xA2: return 't';            // Т
            case 0xBA: case 0x9A: return 'k'; // к К
            case 0xBC: case 0x9C: return 'm'; // м М
            case 0xB2: case 0x92: return 'b'; // в В
            case 0xBD: case 0x9D: return 'h'; // н Н
            default: return '\0';
            }
        }
        if (b0 == 0xD1) {
            switch (b1) {
            case 0x80: return 'p'; // р
            case 0x81: return 'c'; // с
            case 0x83: return 'y'; // у
            case 0x85: return 'x'; // х
            case 0x82: return 't'; // т
            default: return '\0';
            }
        }
        return '\0';
    };

    /* Линейный проход по входу:
     * i - индекс в байтах (std::size_t, беззнаковый тип для размеров). */
    for (std::size_t i = 0; i < s.size();) {
        const unsigned char b0 = static_cast<unsigned char>(s[i]);

        if (b0 < 0x80) { /* ASCII-байт */
            char c = ToLowerAscii(static_cast<char>(b0)); /* lower-case для латиницы */
            if (c == '0') {
                c = 'o';
                hadLookalikeMap = true;
            } else if (c == '1') {
                c = 'l';
                hadLookalikeMap = true;
            }
            out.push_back(c);
            ++i;       /* увеличиваем индекс на 1 байт */
            continue;  /* идём к следующей итерации цикла */
        }

        if (i + 1 < s.size()) { /* пытаемся прочитать двухбайтовую последовательность */
            const unsigned char b1 = static_cast<unsigned char>(s[i + 1]);
            const char mapped = mapCyrToLatin(b0, b1);
            if (mapped != '\0') {
                out.push_back(mapped);
                hadLookalikeMap = true;
                i += 2;  /* съели два байта UTF-8 символа */
                continue;
            }
        }

        /* Если не распознали:
         * копируем байт как есть (чтобы не "ломать" строку молча). */
        out.push_back(static_cast<char>(b0));
        ++i;
    }
    return out;
}

/* ExtractHost(raw):
 * Извлекает хост из пользовательского ввода.
 *
 * Поддерживает:
 *  - "google.com"
 *  - "https://www.google.com/path?q=1"
 *  - "example.com:443"
 *
 * Возвращает:
 *  - нормализованный host (ASCII lower, без www., без завершающей точки)
 *  - или пустую строку {} при невалидном вводе. */
std::string TyposquatDetector::ExtractHost(std::string_view raw) {
    std::string_view s = TrimAsciiSpaces(raw); /* убираем пробелы по краям */
    if (s.empty()) {
        return {}; /* {} для std::string = пустая строка */
    }

    const char* p = s.data();      /* p - указатель на первый байт данных view */
    const std::size_t n = s.size();/* n - длина в байтах */

    std::size_t start = 0; /* где начинается host */
    /* Ищем "://". Если нашли, host начинается после него.
     * Пример:
     *   "https://abc.com" -> start = после "https://" */
    for (std::size_t i = 0; i + 2 < n; ++i) {
        if (p[i] == ':' && p[i + 1] == '/' && p[i + 2] == '/') {
            start = i + 3;
            break;
        }
    }
    /* Защита от строки вида "http://" (схема есть, host нет). */
    if (start >= n) {
        return {};
    }

    std::size_t end = n; /* конец host по умолчанию = конец строки */
    /* Обрезаем path/query/fragment:
     * host заканчивается на первом из: / ? # \ */
    for (std::size_t i = start; i < n; ++i) {
        const char c = p[i];
        if (c == '/' || c == '?' || c == '#' || c == '\\') {
            end = i;
            break;
        }
    }

    /* host = view на диапазон [start, end). */
    std::string_view host(p + start, end - start);
    host = TrimAsciiSpaces(host);
    if (host.empty()) {
        return {};
    }

    /* Отрезаем порт:
     * - обычный случай "example.com:443" -> "example.com"
     * - IPv6 в [..]: стараемся не резать внутри адреса */
    std::size_t colon = std::string::npos; /* npos = "не найдено" */
    if (!host.empty() && host.front() == '[') {
        const std::size_t br = host.find(']');
        if (br != std::string::npos) {
            const std::size_t after = br + 1;
            if (after < host.size() && host[after] == ':') {
                host = host.substr(0, after); /* оставляем [ipv6] без порта */
            }
        }
    } else {
        colon = host.find(':');
        if (colon != std::string::npos) {
            host = host.substr(0, colon);
        }
    }

    host = TrimAsciiSpaces(host);
    if (host.empty()) {
        return {};
    }

    /* out - владеющая строка, куда копируем host в lower-case ASCII. */
    std::string out;
    out.reserve(host.size());
    for (char c : host) { /* range-for: перебор каждого байта */
        out.push_back(ToLowerAscii(c));
    }

    /* Если префикс www., удаляем его.
     * memcmp(out.data(), www.data(), www.size()) == 0
     * означает "первые www.size() байт полностью совпадают". */
    const std::string_view www = "www.";
    if (out.size() > www.size() && std::memcmp(out.data(), www.data(), www.size()) == 0) {
        out.erase(0, www.size()); /* erase(pos,count): удалить count символов с позиции pos */
    }

    /* Удаляем хвостовые точки:
     * "google.com." -> "google.com" */
    while (!out.empty() && out.back() == '.') {
        out.pop_back();
    }

    return out;
}

/* Levenshtein(a,b):
 * Классическая edit-distance между строками a и b.
 *
 * Операции стоимостью 1:
 *  - удаление
 *  - вставка
 *  - замена
 *
 * Сложность:
 *  - время O(n*m)
 *  - память O(m), где m = b.size() */
int TyposquatDetector::Levenshtein(std::string_view a, std::string_view b) {
    const std::size_t n = a.size();
    const std::size_t m = b.size();

    if (n == 0) {
        return static_cast<int>(m);
    }
    if (m == 0) {
        return static_cast<int>(n);
    }

    std::vector<int> row(m + 1); /* row[j] = текущая DP-строка */
    for (std::size_t j = 0; j <= m; ++j) {
        row[static_cast<int>(j)] = static_cast<int>(j);
    }

    for (std::size_t i = 1; i <= n; ++i) {
        int prev = static_cast<int>(i - 1); /* старое значение dp[i-1][j-1] */
        row[0] = static_cast<int>(i);       /* dp[i][0] = i */
        for (std::size_t j = 1; j <= m; ++j) {
            const int cost = (a[i - 1] == b[j - 1]) ? 0 : 1;
            const int del = row[j] + 1;     /* удалить символ из a */
            const int ins = row[j - 1] + 1; /* вставить символ в a */
            const int sub = prev + cost;    /* заменить/совпасть */
            const int cur = (std::min)({del, ins, sub});
            prev = row[j];
            row[j] = cur;
        }
    }

    return row[m];
}

/* WeightForDistance(dist):
 * Переводим distance в "вес риска". */
int TyposquatDetector::WeightForDistance(const int dist) {
    if (dist <= 0) {
        return 0;
    }
    if (dist == 1) {
        return 5; /* максимально подозрительно */
    }
    if (dist == 2) {
        return 3; /* умеренно подозрительно */
    }
    return 0;
}

/* HighRiskForDistance(dist):
 * high-risk только если distance == 1. */
bool TyposquatDetector::HighRiskForDistance(const int dist) {
    return dist == 1;
}

/* Analyze(input):
 * Главный метод детектора.
 *
 * Алгоритм:
 * 1) ограничить размер входа
 * 2) извлечь host
 * 3) нормализовать look-alike символы
 * 4) проверить точное/почти точное совпадение с популярными доменами
 * 5) рассчитать score и флаг suspicious
 * 6) вернуть отчёт report */
TyposquatReport TyposquatDetector::Analyze(std::string_view input) const {
    TyposquatReport report;               /* report - итоговая структура */
    report.originalInputBytes = input.size();

    /* Ограничиваем анализ по размеру для стабильности/производительности. */
    std::string_view chunk = input;
    if (chunk.size() > kMaxTyposquatAnalyzeInputBytes) {
        chunk = chunk.substr(0, kMaxTyposquatAnalyzeInputBytes);
        report.inputTruncated = true;
    }
    report.analyzedInputBytes = chunk.size();

    const std::string hostRaw = ExtractHost(chunk);
    if (hostRaw.empty()) {
        /* Не удалось получить host — считаем ввод подозрительным/ошибочным. */
        report.hostExtractionFailed = true;
        report.suspicious = true;
        report.totalScore = 5;
        return report;
    }

    /* Нормализуем визуальные подмены. */
    bool hadLookalikeMap = false;
    std::string folded = HomoglyphFoldAscii(hostRaw, hadLookalikeMap);
    report.normalizedInput = folded;
    report.hasAlphabetSpoofing = hadLookalikeMap; /* true, если были look-alike замены */

    bool hasHighRisk = false;

    /* Ранний выход:
     * если после нормализации строка полностью равна популярному домену.
     *
     * Пример:
     *   ввод "gооgle.com" (кириллические о)
     *   folded -> "google.com"
     *   folded == pop.domain -> true
     *
     * Но если hadLookalikeMap=true, всё равно помечаем suspicious=true
     * (потому что это как раз признак подмены). */
    for (const PopularDomain& pop : popular_) {
        if (folded == pop.domain) {
            report.exactPopularMatch = true;
            if (hadLookalikeMap) {
                report.suspicious = true;
                report.totalScore = 5;
                report.hits.push_back({pop.domain, 1, 5, true});
            } else {
                report.suspicious = false;
                report.totalScore = 0;
                report.hits.clear();
            }
            return report;
        }
    }

    /* Ищем близкие кандидаты по Levenshtein (dist <= 2). */
    for (const PopularDomain& pop : popular_) {
        const int dist = Levenshtein(folded, pop.domain); /* const: не меняем дальше */
        if (dist <= 0 || dist > kTyposquatMaxReportedEditDistance) {
            continue;
        }

        /* Доп-фильтр на разницу длин:
         * если dist=2, но длины очень разные, это обычно "шум". */
        const std::size_t lenDiff =
            folded.size() > pop.domain.size() ? folded.size() - pop.domain.size()
                                              : pop.domain.size() - folded.size();
        if (dist == 2 && lenDiff > 2) {
            continue;
        }

        const int w = WeightForDistance(dist);
        if (w <= 0) {
            continue;
        }

        const bool hr = HighRiskForDistance(dist);
        report.hits.push_back({pop.domain, dist, w, hr});
        report.totalScore += w; /* += добавление вклада в общий score */
        if (hr) {
            hasHighRisk = true;
        }
    }

    /* Сортировка hit-ов:
     * 1) меньшая дистанция выше
     * 2) при равной дистанции больший вес выше
     * 3) при прочих равных по алфавиту */
    std::sort(
        report.hits.begin(),
        report.hits.end(),
        [](const TyposquatHit& a, const TyposquatHit& b) {
            if (a.editDistance != b.editDistance) {
                return a.editDistance < b.editDistance;
            }
            if (a.weight != b.weight) {
                return a.weight > b.weight;
            }
            return a.canonicalPopular < b.canonicalPopular;
        });

    /* Итог suspicious:
     * true, если есть high-risk ИЛИ если общий score выше порога. */
    report.suspicious =
        hasHighRisk || report.totalScore >= kTyposquatSuspiciousScoreThreshold;

    return report;
}
