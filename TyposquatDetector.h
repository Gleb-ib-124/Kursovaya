#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>
#include "DatabaseManager.h"

inline constexpr int kTyposquatSuspiciousScoreThreshold = 3;
inline constexpr std::size_t kMaxTyposquatAnalyzeInputBytes = 512 * 1024;
inline constexpr int kTyposquatMaxReportedEditDistance = 2;

struct TyposquatHit {
    int domainId = 0;
    std::string canonicalPopular;
    int editDistance = 0;
    int weight = 0;
    bool highRisk = false;
};

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

class TyposquatDetector {
public:
    TyposquatDetector();

    void SetDatabaseData(const std::vector<DbPopularDomain>& dbDomains,
        const std::vector<DbHomoglyph>& dbHomoglyphs);

    TyposquatReport Analyze(std::string_view input) const;
    std::vector<std::string> PopularDomains() const;

private:
    struct PopularDomain {
        int id = 0;
        std::string domain;
        int riskMultiplier = 1;
    };

    std::vector<PopularDomain> popular_;
    std::vector<DbHomoglyph> customHomoglyphs_;

    static std::string ExtractHost(std::string_view raw, bool& hadInternalSpaces);
    std::string HomoglyphFoldAscii(std::string_view s, bool& hadLookalikeMap) const;
    static int Levenshtein(std::string_view a, std::string_view b);
    static int WeightForDistance(int dist);
    static bool HighRiskForDistance(int dist);
};
