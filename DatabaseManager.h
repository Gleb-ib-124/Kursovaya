#pragma once
#include <afxdb.h>
#include <string>
#include <vector>

struct DbPopularDomain {
    int id = 0;
    std::string domain;
    int riskMultiplier = 1;
};

struct DbHomoglyph {
    std::string canonical;
    std::string lookalike;
    int riskWeight = 5;
};

struct DbHistoryRecord {
    int checkId = 0;
    std::string rawInput;
    std::string normalizedDomain;
    int totalScore = 0;
    bool isSuspicious = false;
    std::string checkedAt;
};

struct DbAttackStat {
    std::string domain;
    std::string category;
    int totalAttacks = 0;
    int highRiskCount = 0;
};

class CDatabaseManager {
public:
    std::vector<DbAttackStat> LoadAttackStatistics();
    std::string AddPopularDomainViaProc(const std::string& domain, int categoryId = 1);
    std::vector<DbHistoryRecord> LoadRecentHistory(int limit = 25);

    CDatabaseManager() = default;
    ~CDatabaseManager();

    bool Connect();
    void Disconnect();
    bool IsConnected() const;

    std::vector<DbPopularDomain> LoadPopularDomains();
    std::vector<DbHomoglyph> LoadHomoglyphs();

    int SaveCheckHistory(const std::string& rawInput, const std::string& normalizedDomain,
        int totalScore, bool isSuspicious, bool hasSpoofing);

    void SaveCheckMatch(int checkId, int domainId, int editDistance,
        int matchWeight, bool isHighRisk);

private:
    CDatabase m_db;
    static std::string Win1251ToUtf8(const CString& str);
    static CString Utf8ToWin1251(const std::string& utf8);
    static CString EscapeSql(const CString& text);
};
