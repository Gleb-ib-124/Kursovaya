#include "pch.h"
#include "DatabaseManager.h"

CDatabaseManager::~CDatabaseManager() {
    Disconnect();
}

bool CDatabaseManager::Connect() {
    if (m_db.IsOpen()) {
        return true;
    }
    try {
        return m_db.OpenEx(_T("DSN=TypoScriptDSN;UID=SYSDBA;PWD=masterkey;ROLE=ROLE_OPERATOR;"), CDatabase::noOdbcDialog) != FALSE;
    }
    catch (CDBException* e) {
        e->Delete();
        return false;
    }
}

void CDatabaseManager::Disconnect() {
    if (m_db.IsOpen()) {
        m_db.Close();
    }
}

bool CDatabaseManager::IsConnected() const {
    return m_db.IsOpen() != FALSE;
}

std::string CDatabaseManager::Win1251ToUtf8(const CString& str) {
#ifdef UNICODE
    if (str.IsEmpty()) return {};
    int len = WideCharToMultiByte(CP_UTF8, 0, str, -1, nullptr, 0, nullptr, nullptr);
    if (len <= 1) return {};
    std::string out(static_cast<size_t>(len - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, str, -1, out.data(), len, nullptr, nullptr);
    return out;
#else
    return std::string(static_cast<LPCSTR>(str));
#endif
}

CString CDatabaseManager::Utf8ToWin1251(const std::string& utf8) {
#ifdef UNICODE
    if (utf8.empty()) return CString();
    int wlen = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
    if (wlen <= 0) return CString();
    CStringW w;
    LPWSTR p = w.GetBuffer(wlen);
    MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), p, wlen);
    w.ReleaseBuffer(wlen);
    return CString(w);
#else
    return CString(utf8.c_str());
#endif
}

CString CDatabaseManager::EscapeSql(const CString& text) {
    CString res = text;
    res.Replace(_T("'"), _T("''"));
    return res;
}

std::vector<DbPopularDomain> CDatabaseManager::LoadPopularDomains() {
    std::vector<DbPopularDomain> result;
    if (!m_db.IsOpen()) return result;

    try {
        CRecordset rs(&m_db);
        CString sql = _T("SELECT D.DOMAIN_ID, D.DOMAIN_NAME, C.RISK_MULTIPLIER ")
            _T("FROM DOMAINS_POPULAR D ")
            _T("JOIN DOMAIN_CATEGORIES C ON D.CATEGORY_ID = C.CATEGORY_ID ")
            _T("WHERE D.IS_ACTIVE = 1 ORDER BY D.DOMAIN_NAME");

        rs.Open(CRecordset::forwardOnly, sql, CRecordset::readOnly);
        while (!rs.IsEOF()) {
            CString strId, strDomain, strMult;
            rs.GetFieldValue((short)0, strId);
            rs.GetFieldValue((short)1, strDomain);
            rs.GetFieldValue((short)2, strMult);

            strDomain.Trim();
            DbPopularDomain item;
            item.id = _ttoi(strId);
            item.domain = Win1251ToUtf8(strDomain);
            item.riskMultiplier = _ttoi(strMult);
            result.push_back(item);

            rs.MoveNext();
        }
        rs.Close();
    }
    catch (CDBException* e) {
        e->Delete();
    }
    return result;
}

std::vector<DbHomoglyph> CDatabaseManager::LoadHomoglyphs() {
    std::vector<DbHomoglyph> result;
    if (!m_db.IsOpen()) return result;

    try {
        CRecordset rs(&m_db);
        CString sql = _T("SELECT CANONICAL_CHAR, LOOKALIKE_CHAR, RISK_WEIGHT FROM HOMOGLYPHS");

        rs.Open(CRecordset::forwardOnly, sql, CRecordset::readOnly);
        while (!rs.IsEOF()) {
            CString strCanon, strLook, strWeight;
            rs.GetFieldValue((short)0, strCanon);
            rs.GetFieldValue((short)1, strLook);
            rs.GetFieldValue((short)2, strWeight);

            strCanon.Trim();
            strLook.Trim();

            DbHomoglyph item;
            item.canonical = Win1251ToUtf8(strCanon);
            item.lookalike = Win1251ToUtf8(strLook);
            item.riskWeight = _ttoi(strWeight);
            result.push_back(item);

            rs.MoveNext();
        }
        rs.Close();
    }
    catch (CDBException* e) {
        e->Delete();
    }
    return result;
}

int CDatabaseManager::SaveCheckHistory(const std::string& rawInput, const std::string& normalizedDomain,
    int totalScore, bool isSuspicious, bool hasSpoofing) {
    if (!m_db.IsOpen()) return -1;

    int newCheckId = -1;
    try {
        CRecordset rsId(&m_db);
        rsId.Open(CRecordset::forwardOnly, _T("SELECT NEXT VALUE FOR SEQ_CHECK_ID FROM RDB$DATABASE"), CRecordset::readOnly);
        if (!rsId.IsEOF()) {
            CString strId;
            rsId.GetFieldValue((short)0, strId);
            newCheckId = _ttoi(strId);
        }
        rsId.Close();

        if (newCheckId <= 0) return -1;

        CString rawClean = EscapeSql(Utf8ToWin1251(rawInput)).Left(500);
        CString normClean = EscapeSql(Utf8ToWin1251(normalizedDomain)).Left(250);

        CString sql;
        sql.Format(_T("INSERT INTO CHECK_HISTORY (CHECK_ID, RAW_INPUT, NORMALIZED_DOMAIN, TOTAL_SCORE, IS_SUSPICIOUS, HAS_SPOOFING) ")
            _T("VALUES (%d, '%s', '%s', %d, %d, %d)"),
            newCheckId,
            rawClean.GetString(),
            normClean.GetString(),
            totalScore,
            isSuspicious ? 1 : 0,
            hasSpoofing ? 1 : 0);

        m_db.ExecuteSQL(sql);
    }
    catch (CDBException* e) {
        e->Delete();
        return -1;
    }
    return newCheckId;
}

void CDatabaseManager::SaveCheckMatch(int checkId, int domainId, int editDistance,
    int matchWeight, bool isHighRisk) {
    if (!m_db.IsOpen() || checkId <= 0 || domainId <= 0) return;

    try {
        CString sql;
        sql.Format(_T("INSERT INTO CHECK_MATCHES (CHECK_ID, DOMAIN_ID, EDIT_DISTANCE, MATCH_WEIGHT, IS_HIGH_RISK) ")
            _T("VALUES (%d, %d, %d, %d, %d)"),
            checkId, domainId, editDistance, matchWeight, isHighRisk ? 1 : 0);

        m_db.ExecuteSQL(sql);
    }
    catch (CDBException* e) {
        e->Delete();
    }
}
std::string CDatabaseManager::AddPopularDomainViaProc(const std::string& domain, int categoryId) {
    if (!m_db.IsOpen()) return "ERROR_NO_DB";

    std::string status = "ERROR";
    try {
        CString cleanDomain = EscapeSql(Utf8ToWin1251(domain)).Left(250);
        CString sql;
        sql.Format(_T("SELECT OUT_DOMAIN_ID, OUT_STATUS FROM SP_ADD_POPULAR_DOMAIN('%s', %d)"),
            cleanDomain.GetString(), categoryId);

        CRecordset rs(&m_db);
        rs.Open(CRecordset::forwardOnly, sql, CRecordset::readOnly);
        if (!rs.IsEOF()) {
            CString strStatus;
            rs.GetFieldValue((short)1, strStatus);
            strStatus.Trim();
            status = Win1251ToUtf8(strStatus);
        }
        rs.Close();
    }
    catch (CDBException* e) {
        e->Delete();
    }
    return status;
}

std::vector<DbHistoryRecord> CDatabaseManager::LoadRecentHistory(int limit) {
    std::vector<DbHistoryRecord> result;
    if (!m_db.IsOpen()) return result;

    try {
        CString sql;
        sql.Format(_T("SELECT FIRST %d CHECK_ID, RAW_INPUT, NORMALIZED_DOMAIN, TOTAL_SCORE, IS_SUSPICIOUS, ")
            _T("CAST(CHECKED_AT AS VARCHAR(24)) FROM CHECK_HISTORY ORDER BY CHECK_ID DESC"), limit);

        CRecordset rs(&m_db);
        rs.Open(CRecordset::forwardOnly, sql, CRecordset::readOnly);
        while (!rs.IsEOF()) {
            CString sId, sRaw, sNorm, sScore, sSusp, sDate;
            rs.GetFieldValue((short)0, sId);
            rs.GetFieldValue((short)1, sRaw);
            rs.GetFieldValue((short)2, sNorm);
            rs.GetFieldValue((short)3, sScore);
            rs.GetFieldValue((short)4, sSusp);
            rs.GetFieldValue((short)5, sDate);

            sRaw.Trim();
            sNorm.Trim();
            sDate.Trim();

            DbHistoryRecord rec;
            rec.checkId = _ttoi(sId);
            rec.rawInput = Win1251ToUtf8(sRaw);
            rec.normalizedDomain = Win1251ToUtf8(sNorm);
            rec.totalScore = _ttoi(sScore);
            rec.isSuspicious = (_ttoi(sSusp) != 0);
            rec.checkedAt = Win1251ToUtf8(sDate);
            result.push_back(rec);

            rs.MoveNext();
        }
        rs.Close();
    }
    catch (CDBException* e) {
        e->Delete();
    }
    return result;
}
std::vector<DbAttackStat> CDatabaseManager::LoadAttackStatistics() {
    std::vector<DbAttackStat> result;
    if (!m_db.IsOpen()) return result;

    try {
        CString sql = _T("SELECT D.DOMAIN_NAME, C.CATEGORY_NAME, ")
            _T("COUNT(M.MATCH_ID) AS TOTAL_ATTACKS, ")
            _T("SUM(CASE WHEN M.IS_HIGH_RISK = 1 THEN 1 ELSE 0 END) AS HIGH_RISK_COUNT ")
            _T("FROM DOMAINS_POPULAR D ")
            _T("JOIN DOMAIN_CATEGORIES C ON D.CATEGORY_ID = C.CATEGORY_ID ")
            _T("JOIN CHECK_MATCHES M ON D.DOMAIN_ID = M.DOMAIN_ID ")
            _T("GROUP BY D.DOMAIN_NAME, C.CATEGORY_NAME ")
            _T("ORDER BY 3 DESC");

        CRecordset rs(&m_db);
        rs.Open(CRecordset::forwardOnly, sql, CRecordset::readOnly);
        while (!rs.IsEOF()) {
            CString sDom, sCat, sTot, sHigh;
            rs.GetFieldValue((short)0, sDom);
            rs.GetFieldValue((short)1, sCat);
            rs.GetFieldValue((short)2, sTot);
            rs.GetFieldValue((short)3, sHigh);

            sDom.Trim();
            sCat.Trim();

            DbAttackStat st;
            st.domain = Win1251ToUtf8(sDom);
            st.category = Win1251ToUtf8(sCat);
            st.totalAttacks = _ttoi(sTot);
            st.highRiskCount = _ttoi(sHigh);
            result.push_back(st);

            rs.MoveNext();
        }
        rs.Close();
    }
    catch (CDBException* e) {
        e->Delete();
    }
    return result;
}