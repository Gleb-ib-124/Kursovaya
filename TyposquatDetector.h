#pragma once

/* Базовые STL-типы, используемые в публичных структурах отчёта и API детектора. */
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

/* Порог "подозрительности":
 * если суммарный вес найденных признаков >= 3, считаем домен подозрительным. */
inline constexpr int kTyposquatSuspiciousScoreThreshold = 3;

/* Ограничение размера анализируемого входа в байтах (защита от сверхдлинных строк). */
inline constexpr std::size_t kMaxTyposquatAnalyzeInputBytes = 512 * 1024;

/* Максимальная дистанция Левенштейна, которую мы ещё показываем в отчёте. */
inline constexpr int kTyposquatMaxReportedEditDistance = 2;

/* Одно совпадение с популярным доменом:
 * canonicalPopular - эталонный популярный домен,
 * editDistance     - насколько отличается введённый домен,
 * weight           - вклад в суммарный риск,
 * highRisk         - флаг "высокий риск". */
struct TyposquatHit {
    std::string canonicalPopular;
    int editDistance = 0;
    int weight = 0;
    bool highRisk = false;
};

/* Полный отчёт анализа.
 * Поля заполняются функцией Analyze(...) и читаются UI-слоем. */
struct TyposquatReport {
    bool suspicious = false;
    int totalScore = 0;
    std::string normalizedInput;
    std::vector<TyposquatHit> hits;
    bool inputTruncated = false;
    std::size_t originalInputBytes = 0;
    std::size_t analyzedInputBytes = 0;
    bool exactPopularMatch = false;
    bool hostExtractionFailed = false;
    bool hasAlphabetSpoofing = false;
};

/* TyposquatDetector:
 * независимый от UI движок анализа typosquatting.
 * Принимает строку/URL, выделяет домен, нормализует его,
 * сравнивает со списком популярных доменов и возвращает отчёт. */
class TyposquatDetector {
public:
    /* Конструктор заполняет внутренний список популярных доменов. */
    TyposquatDetector();

    /* Главный API метода анализа. */
    TyposquatReport Analyze(std::string_view input) const;
    /* Возвращает копию списка популярных доменов (для ComboBox в UI). */
    std::vector<std::string> PopularDomains() const;

private:
    /* Внутренняя запись эталонного домена. При желании можно расширить полями TLD/бренд и т.д. */
    struct PopularDomain {
        std::string domain;
    };

    /* База эталонных доменов для сравнения. */
    std::vector<PopularDomain> popular_;

    /* Извлечь хост из сырой строки/URL (без схемы, порта, пути, query и fragment). */
    static std::string ExtractHost(std::string_view raw);
    /* Нормализация "похожих" символов:
     * цифры/кириллица/латиница -> единая ASCII-форма для сравнения. */
    static std::string HomoglyphFoldAscii(std::string_view s, bool& hadLookalikeMap);
    /* Классическая дистанция Левенштейна. */
    static int Levenshtein(std::string_view a, std::string_view b);
    /* Вес риска в зависимости от расстояния. */
    static int WeightForDistance(int dist);
    /* Флаг высокого риска в зависимости от расстояния. */
    static bool HighRiskForDistance(int dist);
};
