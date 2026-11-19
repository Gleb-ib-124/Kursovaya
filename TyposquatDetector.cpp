#include "pch.h"
#include "TyposquatDetector.h"

#include <algorithm>
#include <cstring>

namespace {

    bool IsSpaceAscii(const char c) {
        return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
    }

    char ToLowerAscii(const char c) {
        if (c >= 'A' && c <= 'Z') {
            return static_cast<char>(c - 'A' + 'a');
        }
        return c;
    }

    std::string_view TrimAsciiSpaces(std::string_view s) {
        while (!s.empty() && IsSpaceAscii(s.front())) {
            s.remove_prefix(1);
        }
        while (!s.empty() && IsSpaceAscii(s.back())) {
            s.remove_suffix(1);
        }
        return s;
    }

} // namespace

TyposquatDetector::TyposquatDetector() {
    popular_ = {
        {0, "google.com", 2},
        {0, "yandex.ru", 2},
        {0, "vk.com", 2},
        {0, "paypal.com", 3},
        {0, "github.com", 2}
    };
}

void TyposquatDetector::SetDatabaseData(const std::vector<DbPopularDomain>& dbDomains,
    const std::vector<DbHomoglyph>& dbHomoglyphs) {
    if (!dbDomains.empty()) {
        popular_.clear();
        for (const auto& d : dbDomains) {
            popular_.push_back({ d.id, d.domain, d.riskMultiplier });
        }
    }
    if (!dbHomoglyphs.empty()) {
        customHomoglyphs_ = dbHomoglyphs;
    }
}

std::vector<std::string> TyposquatDetector::PopularDomains() const {
    std::vector<std::string> out;
    out.reserve(popular_.size());
    for (const PopularDomain& item : popular_) {
        out.push_back(item.domain);
    }
    return out;
}

std::string TyposquatDetector::HomoglyphFoldAscii(std::string_view s, bool& hadLookalikeMap) const {
    hadLookalikeMap = false;
    std::string work(s);

    if (!customHomoglyphs_.empty()) {
        for (const auto& rule : customHomoglyphs_) {
            if (rule.lookalike.empty() || rule.canonical.empty()) continue;
            size_t pos = 0;
            while ((pos = work.find(rule.lookalike, pos)) != std::string::npos) {
                work.replace(pos, rule.lookalike.size(), rule.canonical);
                hadLookalikeMap = true;
                pos += rule.canonical.size();
            }
        }
    }

    std::string out;
    out.reserve(work.size());

    auto mapCyrToLatin = [](const unsigned char b0, const unsigned char b1) -> char {
        if (b0 == 0xD0) {
            switch (b1) {
            case 0xB0: case 0x90: return 'a';
            case 0xB5: case 0x95: return 'e';
            case 0xB8: case 0x98: return 'u';
            case 0xBE: case 0x9E: return 'o';
            case 0xA0: return 'p';
            case 0xA1: return 'c';
            case 0xA3: return 'y';
            case 0xA5: return 'x';
            case 0xA2: return 't';
            case 0xBA: case 0x9A: return 'k';
            case 0xBC: case 0x9C: return 'm';
            case 0xB2: case 0x92: return 'b';
            case 0xBD: case 0x9D: return 'h';
            default: return '\0';
            }
        }
        if (b0 == 0xD1) {
            switch (b1) {
            case 0x80: return 'p';
            case 0x81: return 'c';
            case 0x83: return 'y';
            case 0x85: return 'x';
            case 0x82: return 't';
            default: return '\0';
            }
        }
        return '\0';
        };

    for (std::size_t i = 0; i < work.size();) {
        const unsigned char b0 = static_cast<unsigned char>(work[i]);

        if (b0 < 0x80) {
            char c = ToLowerAscii(static_cast<char>(b0));
            if (c == '0') {
                c = 'o';
                hadLookalikeMap = true;
            }
            else if (c == '1') {
                c = 'l';
                hadLookalikeMap = true;
            }
            out.push_back(c);
            ++i;
            continue;
        }

        if (i + 1 < work.size()) {
            const unsigned char b1 = static_cast<unsigned char>(work[i + 1]);
            const char mapped = mapCyrToLatin(b0, b1);
            if (mapped != '\0') {
                out.push_back(mapped);
                hadLookalikeMap = true;
                i += 2;
                continue;
            }
        }

        out.push_back(static_cast<char>(b0));
        ++i;
    }
    return out;
}

std::string TyposquatDetector::ExtractHost(std::string_view raw, bool& hadInternalSpaces) {
    hadInternalSpaces = false;
    std::string_view s = TrimAsciiSpaces(raw);
    if (s.empty()) return {};

    const char* p = s.data();
    const std::size_t n = s.size();

    std::size_t start = 0;
    for (std::size_t i = 0; i + 2 < n; ++i) {
        if (p[i] == ':' && p[i + 1] == '/' && p[i + 2] == '/') {
            start = i + 3;
            break;
        }
    }
    if (start >= n) return {};

    std::size_t end = n;
    for (std::size_t i = start; i < n; ++i) {
        const char c = p[i];
        if (c == '/' || c == '?' || c == '#' || c == '\\') {
            end = i;
            break;
        }
    }

    std::string_view host(p + start, end - start);
    host = TrimAsciiSpaces(host);
    if (host.empty()) return {};

    if (!host.empty() && host.front() == '[') {
        const std::size_t br = host.find(']');
        if (br != std::string::npos) {
            const std::size_t after = br + 1;
            if (after < host.size() && host[after] == ':') {
                host = host.substr(0, after);
            }
        }
    }
    else {
        const std::size_t colon = host.find(':');
        if (colon != std::string::npos) {
            host = host.substr(0, colon);
        }
    }

    host = TrimAsciiSpaces(host);
    if (host.empty()) return {};

    std::string out;
    out.reserve(host.size());
    for (char c : host) {
        if (IsSpaceAscii(c)) {
            hadInternalSpaces = true;
            continue;
        }
        out.push_back(ToLowerAscii(c));
    }

    const std::string_view www = "www.";
    if (out.size() > www.size() && std::memcmp(out.data(), www.data(), www.size()) == 0) {
        out.erase(0, www.size());
    }

    while (!out.empty() && out.back() == '.') {
        out.pop_back();
    }

    return out;
}

int TyposquatDetector::Levenshtein(std::string_view a, std::string_view b) {
    const std::size_t n = a.size();
    const std::size_t m = b.size();
    if (n == 0) return static_cast<int>(m);
    if (m == 0) return static_cast<int>(n);

    std::vector<std::vector<int>> d(n + 1, std::vector<int>(m + 1, 0));
    for (std::size_t i = 0; i <= n; ++i) d[i][0] = static_cast<int>(i);
    for (std::size_t j = 0; j <= m; ++j) d[0][j] = static_cast<int>(j);

    for (std::size_t i = 1; i <= n; ++i) {
        for (std::size_t j = 1; j <= m; ++j) {
            const int cost = (a[i - 1] == b[j - 1]) ? 0 : 1;
            int val = (std::min)({
                d[i - 1][j] + 1,
                d[i][j - 1] + 1,
                d[i - 1][j - 1] + cost
                });
            if (i > 1 && j > 1 && a[i - 1] == b[j - 2] && a[i - 2] == b[j - 1]) {
                val = (std::min)(val, d[i - 2][j - 2] + 1);
            }
            d[i][j] = val;
        }
    }
    return d[n][m];
}

int TyposquatDetector::WeightForDistance(const int dist) {
    if (dist <= 0) return 0;
    if (dist == 1) return 5;
    if (dist == 2) return 3;
    return 0;
}

bool TyposquatDetector::HighRiskForDistance(const int dist) {
    return dist == 1;
}

TyposquatReport TyposquatDetector::Analyze(std::string_view input) const {
    TyposquatReport report;
    report.originalInputBytes = input.size();

    std::string_view chunk = input;
    if (chunk.size() > kMaxTyposquatAnalyzeInputBytes) {
        chunk = chunk.substr(0, kMaxTyposquatAnalyzeInputBytes);
        report.inputTruncated = true;
    }
    report.analyzedInputBytes = chunk.size();

    bool hadInternalSpaces = false;
    const std::string hostRaw = ExtractHost(chunk, hadInternalSpaces);
    if (hostRaw.empty()) {
        report.hostExtractionFailed = true;
        report.suspicious = true;
        report.totalScore = 5;
        return report;
    }

    bool hadLookalikeMap = false;
    std::string folded = HomoglyphFoldAscii(hostRaw, hadLookalikeMap);
    report.normalizedInput = folded;
    report.hasAlphabetSpoofing = hadLookalikeMap;

    bool hasHighRisk = false;

    for (const PopularDomain& pop : popular_) {
        if (folded == pop.domain) {
            report.exactPopularMatch = true;
            if (hadLookalikeMap || hadInternalSpaces) {
                report.suspicious = true;
                const int w = 5 * pop.riskMultiplier + (hadInternalSpaces ? 5 : 0);
                report.totalScore = w;
                report.hits.push_back({ pop.id, pop.domain, 1, w, true });
            }
            else {
                report.suspicious = false;
                report.totalScore = 0;
                report.hits.clear();
            }
            return report;
        }
    }

    for (const PopularDomain& pop : popular_) {
        // Проверка на комбосквоттинг (например, yandex-pay.ru, vk-login.ru)
        const std::size_t dotPos = pop.domain.find('.');
        if (dotPos != std::string::npos && dotPos >= 2) {
            const std::string brand = pop.domain.substr(0, dotPos);
            if (folded.find(brand) != std::string::npos && folded != pop.domain) {
                static const std::vector<std::string> triggers = {
                    "login", "pay", "secure", "auth", "support", "verify", "account", "official"
                };
                for (const auto& kw : triggers) {
                    if (folded.find(kw) != std::string::npos) {
                        const int w = 5 * pop.riskMultiplier;
                        report.hits.push_back({ pop.id, pop.domain, 1, w, true });
                        report.totalScore += w;
                        hasHighRisk = true;
                        break;
                    }
                }
            }
        }
        const int dist = Levenshtein(folded, pop.domain);
        if (dist <= 0 || dist > kTyposquatMaxReportedEditDistance) {
            continue;
        }

        const std::size_t lenDiff =
            folded.size() > pop.domain.size() ? folded.size() - pop.domain.size()
            : pop.domain.size() - folded.size();
        if (dist == 2 && lenDiff > 2) {
            continue;
        }

        const int baseW = WeightForDistance(dist);
        if (baseW <= 0) {
            continue;
        }

        const int w = baseW * pop.riskMultiplier;
        const bool hr = HighRiskForDistance(dist);
        report.hits.push_back({ pop.id, pop.domain, dist, w, hr });
        report.totalScore += w;
        if (hr) {
            hasHighRisk = true;
        }
    }

    if (hadInternalSpaces) {
        report.totalScore += 5;
        hasHighRisk = true;
    }

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
    report.suspicious =
        hasHighRisk || report.totalScore >= kTyposquatSuspiciousScoreThreshold;
    return report;
}