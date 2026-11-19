#include "pch.h"
#include "TypoScriptDlg.h"
#include "DatabaseManager.h"


/* algorithm: min/max/sort utilities
 * vector   : динамические массивы для diff-масок и DP */
#include <algorithm>
#include <vector>

constexpr UINT IDC_BTN_ADD_DOMAIN = 2001;
constexpr UINT IDC_BTN_SHOW_HISTORY = 2002;
constexpr UINT IDC_BTN_SHOW_STATS = 2003;

namespace {

constexpr DWORD kMaxInputEditTchars = 256 * 1024;
constexpr DWORD kMaxOutputRichTchars = 512 * 1024;
constexpr ULONGLONG kMaxFileLoadBytes = 2 * 1024 * 1024;  /* лимит на размер открываемого файла — 2 МБ */
constexpr int kMinClientWidth = 700;
constexpr int kMinClientHeight = 520;

/* DiffOp:
 * тип изменения символа при сравнении введённого и "оригинального" домена. */
enum class DiffOp {
    Keep,
    Replace,
    Insert,
    Delete
};

struct DiffToken {
    char entered = '\0';
    char original = '\0';
    DiffOp op = DiffOp::Keep;
};

/* Таблица "похожих" кириллических символов для поясняющего блока отчёта.
 * Используется только для текстового объяснения в UI, детектор имеет свою
 * полноценную нормализацию в TyposquatDetector.cpp. */
char MapCyrillicLookalikeToLatin(const wchar_t ch) {
    switch (ch) {
    case L'а': return 'a';
    case L'е': return 'e';
    case L'о': return 'o';
    case L'р': return 'p';
    case L'с': return 'c';
    case L'у': return 'y';
    case L'х': return 'x';
    case L'к': return 'k';
    case L'м': return 'm';
    case L'т': return 't';
    case L'в': return 'b';
    case L'н': return 'h';
    default: return '\0';
    }
}

/* BuildDiffTokens:
 * строит по Левенштейну "выравнивание" двух строк посимвольно,
 * чтобы в отчёте можно было показать, что удалено/вставлено/заменено.
 *
 * entered  - что ввёл пользователь,
 * original - эталонный популярный домен.
 *
 * Выход: последовательность токенов DiffToken в порядке слева направо. */
std::vector<DiffToken> BuildDiffTokens(const std::string& entered, const std::string& original) {
    const std::size_t n = entered.size();
    const std::size_t m = original.size();
    /* Классическая DP-матрица размера (n+1) x (m+1), храним в одном векторе. */
    std::vector<int> dp((n + 1) * (m + 1), 0);
    auto at = [&](std::size_t i, std::size_t j) -> int& { return dp[i * (m + 1) + j]; };
    /* Инициализация границ:
     * dist(i,0)=i (удалить i символов), dist(0,j)=j (вставить j символов). */
    for (std::size_t i = 0; i <= n; ++i) {
        at(i, 0) = static_cast<int>(i);
    }
    for (std::size_t j = 0; j <= m; ++j) {
        at(0, j) = static_cast<int>(j);
    }
    for (std::size_t i = 1; i <= n; ++i) {
        for (std::size_t j = 1; j <= m; ++j) {
            const int cost = entered[i - 1] == original[j - 1] ? 0 : 1;
            const int del = at(i - 1, j) + 1;
            const int ins = at(i, j - 1) + 1;
            const int sub = at(i - 1, j - 1) + cost;
            at(i, j) = (std::min)({del, ins, sub});
        }
    }

    std::vector<DiffToken> tokens;
    std::size_t i = n;
    std::size_t j = m;
    /* Обратный проход (backtracking) для восстановления конкретных операций. */
    while (i > 0 || j > 0) {
        if (i > 0 && j > 0 && entered[i - 1] == original[j - 1] && at(i, j) == at(i - 1, j - 1)) {
            tokens.push_back({entered[i - 1], original[j - 1], DiffOp::Keep});
            --i;
            --j;
            continue;
        }
        if (i > 0 && j > 0 && at(i, j) == at(i - 1, j - 1) + 1) {
            tokens.push_back({entered[i - 1], original[j - 1], DiffOp::Replace});
            --i;
            --j;
            continue;
        }
        if (i > 0 && at(i, j) == at(i - 1, j) + 1) {
            tokens.push_back({entered[i - 1], ' ', DiffOp::Delete});
            --i;
            continue;
        }
        tokens.push_back({' ', original[j - 1], DiffOp::Insert});
        --j;
    }
    std::reverse(tokens.begin(), tokens.end());
    return tokens;
}

} // namespace

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

CTypoScriptDlg::CTypoScriptDlg(CWnd* pParent)
    : CDialogEx(IDD_TYPOSCRIPT_DIALOG, pParent) {
}

/* DoDataExchange:
 * связываем ID контролов из .rc с полями класса (MFC DDX). */
void CTypoScriptDlg::DoDataExchange(CDataExchange* pDX) {
    CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_EDIT_OUTPUT, m_editOutput);
    DDX_Control(pDX, IDC_COMBO_POPULAR, m_comboPopular);
}

BEGIN_MESSAGE_MAP(CTypoScriptDlg, CDialogEx)
    ON_BN_CLICKED(IDC_BTN_CHECK, &CTypoScriptDlg::OnBnClickedCheck)
    ON_BN_CLICKED(IDC_BTN_CLEAR, &CTypoScriptDlg::OnBnClickedClear)
    ON_BN_CLICKED(IDC_BTN_LOAD,  &CTypoScriptDlg::OnBnClickedLoad)
    ON_BN_CLICKED(IDC_BTN_SAVE,  &CTypoScriptDlg::OnBnClickedSave)
    ON_WM_SIZE()
    ON_WM_GETMINMAXINFO()
    ON_BN_CLICKED(IDC_BTN_ADD_DOMAIN, &CTypoScriptDlg::OnBnClickedAddDomain)
    ON_BN_CLICKED(IDC_BTN_SHOW_HISTORY, &CTypoScriptDlg::OnBnClickedShowHistory)
    ON_BN_CLICKED(IDC_BTN_SHOW_STATS, &CTypoScriptDlg::OnBnClickedShowStats)


END_MESSAGE_MAP()

BOOL CTypoScriptDlg::OnInitDialog() {
    CDialogEx::OnInitDialog();

    SetWindowText(_T("TypoScript — анализ домена на typosquatting"));

    CFont* pDlgFont = GetFont();
    m_btnAddDomain.Create(_T("В белый список"), WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP, CRect(0, 0, 0, 0), this, IDC_BTN_ADD_DOMAIN);
    m_btnShowHistory.Create(_T("Журнал БД"), WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP, CRect(0, 0, 0, 0), this, IDC_BTN_SHOW_HISTORY);
    m_btnShowStats.Create(_T("Статистика атак"), WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP, CRect(0, 0, 0, 0), this, IDC_BTN_SHOW_STATS);
    if (pDlgFont) {
        m_btnShowStats.SetFont(pDlgFont);
    }
    if (pDlgFont) {
        m_btnAddDomain.SetFont(pDlgFont);
        m_btnShowHistory.SetFont(pDlgFont);
    }

    if (CWnd* pIn = GetDlgItem(IDC_EDIT_INPUT)) {
        /* Ограничение длины ввода в edit-контроле (защита UI от перегрузки). */
        pIn->SendMessage(EM_SETLIMITTEXT, static_cast<WPARAM>(kMaxInputEditTchars), 0);
    }
    m_editOutput.LimitText(kMaxOutputRichTchars); /* Лимит для RichEdit отчёта. */
    if (dbManager_.Connect()) {
        detector_.SetDatabaseData(dbManager_.LoadPopularDomains(), dbManager_.LoadHomoglyphs());
        SetWindowText(_T("TypoScript — анализ домена (БД: Ред База Данных подключена)"));
    }
    else {
        SetWindowText(_T("TypoScript — анализ домена (Автономный режим)"));
    }
    FillPopularDomains();
    m_comboPopular.SetCurSel(0);

    layoutReady_ = true;
    CRect r;
    GetClientRect(&r);
    LayoutControls(r.Width(), r.Height());
    return TRUE;
}

void CTypoScriptDlg::FillPopularDomains() {
    m_comboPopular.ResetContent();
    m_comboPopular.AddString(_T("Выберите популярный домен")); /* Пункт-плейсхолдер. */
    const std::vector<std::string> domains = detector_.PopularDomains();
    for (const std::string& d : domains) {
        m_comboPopular.AddString(Utf8ToCString(d));
    }
}

void CTypoScriptDlg::OnGetMinMaxInfo(MINMAXINFO* lpMMI) {
    CDialogEx::OnGetMinMaxInfo(lpMMI);
    CRect r(0, 0, kMinClientWidth, kMinClientHeight); /* Минимальный клиентский размер. */
    AdjustWindowRectEx(&r, GetStyle(), FALSE, GetExStyle());
    lpMMI->ptMinTrackSize.x = r.Width();
    lpMMI->ptMinTrackSize.y = r.Height();
}

void CTypoScriptDlg::OnSize(UINT nType, int cx, int cy) {
    CDialogEx::OnSize(nType, cx, cy);
    if (nType == SIZE_MINIMIZED) {
        return;
    }
    LayoutControls(cx, cy);
}

void CTypoScriptDlg::LayoutControls(int cx, int cy) {
    if (!layoutReady_ || cx < 100 || cy < 100) {
        return;
    }

    const int m = 16;
    const int gap = 10;
    const int lblH = 18;
    const int topEditH = 100;
    const int comboVisibleH = 28;
    const int comboDropH = 220;
    const int btnW = 150;
    const int btnH = 34;
    const int rightW = btnW + 8;
    const int leftW = cx - (m * 2) - rightW - gap;

    if (CWnd* p = GetDlgItem(IDC_STATIC_IN)) {
        p->MoveWindow(m, m, 160, lblH);
    }
    if (CWnd* p = GetDlgItem(IDC_EDIT_INPUT)) {
        p->MoveWindow(m + 110, m - 2, cx - (m + 110) - m, topEditH);
    }

    const int blockTop = m + lblH + 6 + topEditH + 16;
    if (CWnd* p = GetDlgItem(IDC_STATIC_POPULAR)) {
        p->MoveWindow(m, blockTop, 220, lblH);
    }
    const int comboTop = blockTop + lblH + 6;
    if (CWnd* p = GetDlgItem(IDC_COMBO_POPULAR)) {
        p->MoveWindow(m + 10, comboTop, leftW - 10, comboDropH);
    }

    const int dbBtnTop = comboTop + comboVisibleH + 10;
    const int dbBtnH = 28;
    const int dbBtnW = (leftW - 10 - gap * 2) / 3;
    if (m_btnAddDomain.GetSafeHwnd()) {
        m_btnAddDomain.MoveWindow(m + 10, dbBtnTop, dbBtnW, dbBtnH);
    }
    if (m_btnShowHistory.GetSafeHwnd()) {
        m_btnShowHistory.MoveWindow(m + 10 + dbBtnW + gap, dbBtnTop, dbBtnW, dbBtnH);
    }
    if (m_btnShowStats.GetSafeHwnd()) {
        m_btnShowStats.MoveWindow(m + 10 + (dbBtnW + gap) * 2, dbBtnTop, dbBtnW, dbBtnH);
    }

    const int btnX = cx - m - btnW;
    int btnY = blockTop;
    if (CWnd* p = GetDlgItem(IDC_BTN_CHECK)) {
        p->MoveWindow(btnX, btnY, btnW, btnH);
    }
    btnY += btnH + gap;
    if (CWnd* p = GetDlgItem(IDC_BTN_CLEAR)) {
        p->MoveWindow(btnX, btnY, btnW, btnH);
    }
    btnY += btnH + gap;
    if (CWnd* p = GetDlgItem(IDC_BTN_LOAD)) {
        p->MoveWindow(btnX, btnY, btnW, btnH);
    }
    btnY += btnH + gap;
    if (CWnd* p = GetDlgItem(IDC_BTN_SAVE)) {
        p->MoveWindow(btnX, btnY, btnW, btnH);
    }

    const int outTop = (std::max)(dbBtnTop + dbBtnH, btnY + btnH) + 14;
    const int outH = (std::max)(80, cy - outTop - m);
    if (m_editOutput.GetSafeHwnd()) {
        m_editOutput.MoveWindow(m + 10, outTop, cx - (m * 2) - 10, outH);
    }
}

std::string CTypoScriptDlg::CStringToUtf8(const CString& s) {
#ifdef UNICODE
    if (s.IsEmpty()) {
        return {};
    }
    /* Двухпроходная конвертация:
     * 1) узнаём необходимый размер,
     * 2) заполняем буфер UTF-8. */
    const int len = WideCharToMultiByte(CP_UTF8, 0, s, -1, nullptr, 0, nullptr, nullptr);
    if (len <= 1) {
        return {};
    }
    std::string out(static_cast<size_t>(len - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, s, -1, out.data(), len, nullptr, nullptr);
    return out;
#else
    return std::string(static_cast<LPCSTR>(s));
#endif
}

CString CTypoScriptDlg::Utf8ToCString(const std::string& s) {
#ifdef UNICODE
    if (s.empty()) {
        return CString();
    }
    const int byteLen = static_cast<int>(s.size());
    /* Сначала пытаемся как строгий UTF-8, потом fallback на ACP. */
    int wlen = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.data(), byteLen, nullptr, 0);
    if (wlen <= 0) {
        wlen = MultiByteToWideChar(CP_ACP, 0, s.data(), byteLen, nullptr, 0);
        if (wlen <= 0) {
            return CString();
        }
        CStringW w;
        LPWSTR p = w.GetBuffer(wlen);
        MultiByteToWideChar(CP_ACP, 0, s.data(), byteLen, p, wlen);
        w.ReleaseBuffer(wlen);
        return CString(w);
    }
    CStringW w;
    LPWSTR p = w.GetBuffer(wlen);
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.data(), byteLen, p, wlen);
    w.ReleaseBuffer(wlen);
    return CString(w);
#else
    return CString(s.c_str());
#endif
}

COLORREF CTypoScriptDlg::ScoreToGradientColor(int score) {
    int s = score;
    if (s < 0) {
        s = 0;
    }
    if (s > 100) {
        s = 100;
    }
    const float t = s / 100.0f; /* нормализация в диапазон [0..1] */
    const float gR = 25.0f, gG = 150.0f, gB = 55.0f;
    const float yR = 255.0f, yG = 210.0f, yB = 0.0f;
    const float rR = 215.0f, rG = 35.0f, rB = 30.0f;
    float R = 0.0f, G = 0.0f, B = 0.0f;
    if (t <= 0.5f) { /* зелёный -> жёлтый */
        const float u = t / 0.5f;
        R = gR + (yR - gR) * u;
        G = gG + (yG - gG) * u;
        B = gB + (yB - gB) * u;
    } else { /* жёлтый -> красный */
        const float u = (t - 0.5f) / 0.5f;
        R = yR + (rR - yR) * u;
        G = yG + (rG - yG) * u;
        B = yB + (rB - yB) * u;
    }
    return RGB(
        static_cast<BYTE>(R + 0.5f),
        static_cast<BYTE>(G + 0.5f),
        static_cast<BYTE>(B + 0.5f));
}

void CTypoScriptDlg::AppendRichText(COLORREF cr, const CString& text) {
    CHARFORMAT2 cf = {};
    cf.cbSize = sizeof(cf);
    cf.dwMask = CFM_COLOR;
    cf.crTextColor = cr;
    constexpr int kMaxChunk = 32000; /* безопасный размер вставки в RichEdit за один вызов */
    const int total = text.GetLength();
    for (int off = 0; off < total; off += kMaxChunk) {
        const int take = (std::min)(kMaxChunk, total - off);
        const CString chunk = text.Mid(off, take);
        const long n = m_editOutput.GetWindowTextLength();
        m_editOutput.SetSel(n, n);
        m_editOutput.SetSelectionCharFormat(cf);
        m_editOutput.ReplaceSel(chunk);
    }
}

void CTypoScriptDlg::FillReportRich(const TyposquatReport& r, const CString& rawInput) {
    m_editOutput.SetWindowText(_T(""));

    // 1. Крупный заголовок-вердикт
    if (r.hostExtractionFailed) {
        AppendRichText(RGB(190, 40, 40), _T("[!] ОШИБКА: НЕКОРРЕКТНЫЙ АДРЕС ДОМЕНА\r\n"));
        AppendRichText(RGB(120, 120, 120), _T("──────────────────────────────────────────────────\r\n"));
        AppendRichText(RGB(50, 50, 50), _T("Не удалось выделить доменное имя. Проверьте правильность ввода.\r\n"));
        return;
    }

    if (r.suspicious) {
        AppendRichText(RGB(195, 35, 35), _T("[!] ВНИМАНИЕ: ОБНАРУЖЕН ПОДОЗРИТЕЛЬНЫЙ ДОМЕН\r\n"));
    }
    else if (r.exactPopularMatch) {
        AppendRichText(RGB(30, 135, 65), _T("[OK] БЕЗОПАСНО: ОФИЦИАЛЬНЫЙ ДОВЕРЕННЫЙ ДОМЕН\r\n"));
    }
    else {
        AppendRichText(RGB(45, 110, 165), _T("[i] НЕЙТРАЛЬНО: ПРИЗНАКОВ ПОДДЕЛКИ НЕ НАЙДЕНО\r\n"));
    }

    AppendRichText(RGB(170, 170, 170), _T("──────────────────────────────────────────────────\r\n\r\n"));

    // 2. Сводка параметров
    AppendRichText(RGB(70, 70, 70), _T("Основные параметры анализа:\r\n"));

    CString normLine;
    normLine.Format(_T("  • Проверенный адрес:   %s\r\n"), Utf8ToCString(r.normalizedInput).GetString());
    AppendRichText(RGB(30, 30, 30), normLine);

    CString scoreLine;
    scoreLine.Format(_T("  • Индекс риска:        %d балл(ов)\r\n"), r.totalScore);
    AppendRichText(r.suspicious ? RGB(195, 35, 35) : RGB(30, 30, 30), scoreLine);

    CString spoofLine;
    spoofLine.Format(_T("  • Подмена алфавита:    %s\r\n\r\n"),
        r.hasAlphabetSpoofing ? _T("Да (обнаружены похожие символы)") : _T("Нет"));
    AppendRichText(r.hasAlphabetSpoofing ? RGB(195, 35, 35) : RGB(80, 80, 80), spoofLine);

    // 3. Пошаговое сравнение символов (если найдены похожие домены)
    if (!r.hits.empty()) {
        const TyposquatHit& top = r.hits.front();
        CString wOrig = Utf8ToCString(top.canonicalPopular);

        CString wEntered = rawInput;
        wEntered.Trim();
        wEntered.MakeLower();
        if (wEntered.Left(8) == _T("https://")) wEntered.Delete(0, 8);
        else if (wEntered.Left(7) == _T("http://")) wEntered.Delete(0, 7);
        if (wEntered.Left(4) == _T("www.")) wEntered.Delete(0, 4);

        AppendRichText(RGB(70, 70, 70), _T("Сравнение с оригинальным доменом:\r\n"));
        AppendRichText(RGB(30, 30, 30), _T("  • Введённый адрес:  "));

        for (int i = 0; i < wEntered.GetLength(); ++i) {
            CString ch(wEntered[i]);
            bool isDiff = (i >= wOrig.GetLength() || wEntered[i] != wOrig[i]);
            AppendRichText(isDiff ? RGB(210, 30, 30) : RGB(30, 30, 30), ch);
        }
        AppendRichText(RGB(30, 30, 30), _T("\r\n"));

        CString origLine;
        origLine.Format(_T("  • Эталон в базе:    %s\r\n\r\n"), wOrig.GetString());
        AppendRichText(RGB(30, 120, 60), origLine);

        // 4. Список похожих доменов понятным языком
        AppendRichText(RGB(70, 70, 70), _T("Найденные совпадения:\r\n"));
        for (const auto& hit : r.hits) {
            CString hitLine;
            hitLine.Format(_T("  • %s — отличий в символах: %d, вес риска: %d (%s)\r\n"),
                Utf8ToCString(hit.canonicalPopular).GetString(),
                hit.editDistance,
                hit.weight,
                hit.highRisk ? _T("высокий риск") : _T("умеренный риск"));
            AppendRichText(hit.highRisk ? RGB(195, 35, 35) : RGB(180, 100, 20), hitLine);
        }
        AppendRichText(RGB(30, 30, 30), _T("\r\n"));
    }

    // 5. Итоговая рекомендация пользователю
    AppendRichText(RGB(70, 70, 70), _T("Рекомендация:\r\n"));
    if (r.suspicious) {
        AppendRichText(RGB(195, 35, 35),
            _T("  Не переходите по этой ссылке и не вводите логины, пароли или данные карт. ")
            _T("Высока вероятность фишинговой атаки.\r\n"));
    }
    else if (r.exactPopularMatch) {
        AppendRichText(RGB(30, 135, 65),
            _T("  Домен полностью совпадает с оригиналом из доверенного списка. Переход безопасен.\r\n"));
    }
    else {
        AppendRichText(RGB(60, 60, 60),
            _T("  Сходства с известными защищаемыми брендами не выявлено. Соблюдайте стандартную осторожность.\r\n"));
    }
}


void CTypoScriptDlg::OnBnClickedCheck() {
    CString input;
    GetDlgItemText(IDC_EDIT_INPUT, input);
    input.Trim();
    if (input.IsEmpty()) {
        const int idx = m_comboPopular.GetCurSel();
        if (idx > 0) {
            m_comboPopular.GetLBText(idx, input);
            SetDlgItemText(IDC_EDIT_INPUT, input);
        }
    }
    const std::string u8 = CStringToUtf8(input);
    const TyposquatReport r = detector_.Analyze(u8);
    FillReportRich(r, input);

    if (dbManager_.IsConnected() && !u8.empty()) {
        int checkId = dbManager_.SaveCheckHistory(
            u8, r.normalizedInput, r.totalScore, r.suspicious, r.hasAlphabetSpoofing);

        if (checkId > 0) {
            for (const auto& hit : r.hits) {
                if (hit.domainId > 0) {
                    dbManager_.SaveCheckMatch(
                        checkId, hit.domainId, hit.editDistance, hit.weight, hit.highRisk);
                }
            }
            CString dbMsg;
            dbMsg.Format(_T("\r\n──────────────────────────────────────────────────\r\n")
                _T("[БД] Проверка сохранена в журнал аудита (запись №%d).\r\n"), checkId);
            AppendRichText(RGB(50, 100, 160), dbMsg);
        }
    }
}

void CTypoScriptDlg::OnBnClickedClear() {
    /* Полный сброс состояния формы в "чистый" режим. */
    SetDlgItemText(IDC_EDIT_INPUT, _T(""));
    m_editOutput.SetWindowText(_T(""));
    m_comboPopular.SetCurSel(0);
}

/* ---------- Загрузка домена из файла ---------- */

BOOL CTypoScriptDlg::LoadTextFile(const CString& path, CString& out, CString& err) {
    CFile f;
    if (!f.Open(path, CFile::modeRead | CFile::typeBinary)) {
        err = _T("Не удалось открыть файл для чтения.");
        return FALSE;
    }
    const ULONGLONG n = f.GetLength();
    if (n > kMaxFileLoadBytes) {
        err = _T("Файл слишком большой (лимит 2 МБ).");
        return FALSE;
    }
    if (n == 0) {
        out.Empty();
        return TRUE;
    }

    std::vector<BYTE> buf(static_cast<size_t>(n));
    f.Read(buf.data(), static_cast<UINT>(n));

    /* Заменяем нулевые байты пробелами — иначе они оборвут C-строку при дальнейшей обработке. */
    for (BYTE& b : buf) {
        if (b == 0) {
            b = 0x20;
        }
    }

    /* Пропускаем UTF-8 BOM (EF BB BF), если он есть в начале файла. */
    size_t off = 0;
    if (n >= 3 && buf[0] == 0xEF && buf[1] == 0xBB && buf[2] == 0xBF) {
        off = 3;
    }

    const LPCCH pBytes = reinterpret_cast<LPCCH>(buf.data() + off);
    const int   byteLen = static_cast<int>(n - off);

    /* Сначала пытаемся декодировать как UTF-8; при неудаче — как ACP (Windows ANSI). */
    int wlen = MultiByteToWideChar(CP_UTF8, 0, pBytes, byteLen, nullptr, 0);
    if (wlen == 0 && byteLen > 0) {
        wlen = MultiByteToWideChar(CP_ACP, 0, pBytes, byteLen, nullptr, 0);
        CStringW w;
        LPWSTR wp = w.GetBuffer(wlen);
        MultiByteToWideChar(CP_ACP, 0, pBytes, byteLen, wp, wlen);
        w.ReleaseBuffer(wlen);
        out = CString(w);
        return TRUE;
    }
    CStringW w;
    LPWSTR wp = w.GetBuffer(wlen);
    MultiByteToWideChar(CP_UTF8, 0, pBytes, byteLen, wp, wlen);
    w.ReleaseBuffer(wlen);
    out = CString(w);
    return TRUE;
}

BOOL CTypoScriptDlg::SaveTextFileUtf8(const CString& path, const CString& text) {
    CFile f;
    if (!f.Open(path, CFile::modeCreate | CFile::modeWrite | CFile::typeBinary)) {
        return FALSE;
    }
    /* Пишем BOM, чтобы «Блокнот» и Excel однозначно определяли UTF-8. */
    const BYTE bom[] = {0xEF, 0xBB, 0xBF};
    f.Write(bom, 3);

#ifdef UNICODE
    const int n = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
    if (n <= 1) {
        return TRUE;
    }
    CStringA utf8;
    LPSTR p = utf8.GetBuffer(n - 1);
    WideCharToMultiByte(CP_UTF8, 0, text, -1, p, n, nullptr, nullptr);
    utf8.ReleaseBuffer(n - 1);
    f.Write(utf8.GetString(), static_cast<UINT>(n - 1));
#else
    f.Write(text, text.GetLength());
#endif
    return TRUE;
}

void CTypoScriptDlg::OnBnClickedLoad() {
    CFileDialog dlg(TRUE, _T("txt"), nullptr, OFN_FILEMUSTEXIST | OFN_HIDEREADONLY,
        _T("Текстовые файлы (*.txt)|*.txt|Все файлы (*.*)|*.*||"), this);
    if (dlg.DoModal() != IDOK) {
        return;
    }

    CStdioFile file;
    if (!file.Open(dlg.GetPathName(), CFile::modeRead | CFile::typeBinary)) {
        AfxMessageBox(_T("Не удалось открыть выбранный файл."));
        return;
    }

    ULONGLONG fileLen = file.GetLength();
    if (fileLen == 0) {
        AfxMessageBox(_T("Файл пуст."));
        return;
    }

    std::string rawBytes(static_cast<size_t>(fileLen), '\0');
    file.Read(rawBytes.data(), static_cast<UINT>(fileLen));
    file.Close();

    if (rawBytes.size() >= 3 &&
        static_cast<unsigned char>(rawBytes[0]) == 0xEF &&
        static_cast<unsigned char>(rawBytes[1]) == 0xBB &&
        static_cast<unsigned char>(rawBytes[2]) == 0xBF) {
        rawBytes.erase(0, 3);
    }

    std::vector<std::string> lines;
    std::string currentLine;
    for (char ch : rawBytes) {
        if (ch == '\r' || ch == '\n') {
            if (!currentLine.empty()) {
                lines.push_back(currentLine);
                currentLine.clear();
            }
        }
        else {
            currentLine.push_back(ch);
        }
    }
    if (!currentLine.empty()) {
        lines.push_back(currentLine);
    }

    if (lines.empty()) {
        AfxMessageBox(_T("В файле не найдено ни одного домена."));
        return;
    }

    if (lines.size() == 1) {
        SetDlgItemText(IDC_EDIT_INPUT, Utf8ToCString(lines[0]));
        OnBnClickedCheck();
        return;
    }

    SetDlgItemText(IDC_EDIT_INPUT, Utf8ToCString(rawBytes));
    m_editOutput.SetWindowText(_T(""));

    CString header;
    header.Format(_T("=== ПАКЕТНАЯ ПРОВЕРКА СПИСКА ДОМЕНОВ (всего строк: %zu) ===\r\n"), lines.size());
    AppendRichText(RGB(30, 70, 140), header);
    AppendRichText(RGB(170, 170, 170), _T("──────────────────────────────────────────────────────────────\r\n\r\n"));

    int totalChecked = 0;
    int suspiciousCount = 0;
    int safeCount = 0;
    int savedToDbCount = 0;

    for (const std::string& line : lines) {
        const TyposquatReport r = detector_.Analyze(line);
        ++totalChecked;

        if (r.suspicious) {
            ++suspiciousCount;
        }
        else {
            ++safeCount;
        }

        if (dbManager_.IsConnected()) {
            int checkId = dbManager_.SaveCheckHistory(
                line, r.normalizedInput, r.totalScore, r.suspicious, r.hasAlphabetSpoofing);
            if (checkId > 0) {
                ++savedToDbCount;
                for (const auto& hit : r.hits) {
                    if (hit.domainId > 0) {
                        dbManager_.SaveCheckMatch(
                            checkId, hit.domainId, hit.editDistance, hit.weight, hit.highRisk);
                    }
                }
            }
        }

        CString row;
        if (r.hostExtractionFailed) {
            row.Format(_T("  %2d. [ОШИБКА]   %s — некорректный адрес\r\n"),
                totalChecked, Utf8ToCString(line).GetString());
            AppendRichText(RGB(160, 80, 20), row);
        }
        else if (r.suspicious) {
            CString target = r.hits.empty() ? CString(_T("неизвестный эталон"))
                : Utf8ToCString(r.hits.front().canonicalPopular);
            row.Format(_T("  %2d. [УГРОЗА]   %-25s -> похож на %s (риск: %d)\r\n"),
                totalChecked,
                Utf8ToCString(r.normalizedInput).GetString(),
                target.GetString(),
                r.totalScore);
            AppendRichText(RGB(195, 35, 35), row);
        }
        else {
            row.Format(_T("  %2d. [ЧИСТО]    %s\r\n"),
                totalChecked, Utf8ToCString(r.normalizedInput).GetString());
            AppendRichText(RGB(30, 130, 60), row);
        }
    }

    AppendRichText(RGB(170, 170, 170), _T("\r\n──────────────────────────────────────────────────────────────\r\n"));
    CString summary;
    summary.Format(_T("Итого проверено: %d | Безопасных: %d | Подозрительных: %d\r\n"),
        totalChecked, safeCount, suspiciousCount);
    AppendRichText(RGB(40, 40, 40), summary);

    if (dbManager_.IsConnected()) {
        CString dbSummary;
        dbSummary.Format(_T("[БД] В журнал аудита записано проверок: %d.\r\n"), savedToDbCount);
        AppendRichText(RGB(50, 100, 160), dbSummary);
    }
}

void CTypoScriptDlg::OnBnClickedSave() {
    /* Берём текущее содержимое отчёта (RichEdit) и предлагаем сохранить. */
    CString text;
    m_editOutput.GetWindowText(text);

    if (text.IsEmpty()) {
        AfxMessageBox(
            _T("Отчёт пуст — сначала выполните анализ кнопкой «Проверить»."),
            MB_ICONINFORMATION);
        return;
    }

    static constexpr LPCTSTR kSaveFilter =
        _T("Отчёт UTF-8 (*.txt)|*.txt|")
        _T("Текст (*.txt;*.log;*.md;*.csv)|*.txt;*.log;*.md;*.csv|")
        _T("Все файлы (*.*)|*.*||");

    CFileDialog dlg(FALSE, _T("txt"), _T("typo_report.txt"),
        OFN_OVERWRITEPROMPT | OFN_HIDEREADONLY, kSaveFilter, this);
    if (dlg.DoModal() != IDOK) {
        return;
    }

    if (!SaveTextFileUtf8(dlg.GetPathName(), text)) {
        AfxMessageBox(_T("Не удалось сохранить файл."), MB_ICONERROR);
    }
}
void CTypoScriptDlg::OnBnClickedAddDomain() {
    if (!dbManager_.IsConnected()) {
        AfxMessageBox(_T("База данных не подключена."));
        return;
    }

    CString input;
    GetDlgItemText(IDC_EDIT_INPUT, input);
    input.Trim();
    if (input.IsEmpty()) {
        AfxMessageBox(_T("Введите доменное имя в верхнее поле (например, sberbank.ru)."));
        return;
    }

    const std::string u8 = CStringToUtf8(input);
    const std::string status = dbManager_.AddPopularDomainViaProc(u8, 1);

    if (status == "INSERTED") {
        detector_.SetDatabaseData(dbManager_.LoadPopularDomains(), dbManager_.LoadHomoglyphs());
        FillPopularDomains();
        m_comboPopular.SetCurSel(0);
        AfxMessageBox(_T("Домен успешно добавлен в базу данных через хранимую процедуру SP_ADD_POPULAR_DOMAIN!"));
    }
    else if (status == "ALREADY_EXISTS") {
        AfxMessageBox(_T("Этот домен уже присутствует в базе данных."));
    }
    else {
        AfxMessageBox(_T("Ошибка при вызове хранимой процедуры добавления домена."));
    }
}

void CTypoScriptDlg::OnBnClickedShowHistory() {
    if (!dbManager_.IsConnected()) {
        AfxMessageBox(_T("База данных не подключена."));
        return;
    }

    auto history = dbManager_.LoadRecentHistory(25);
    m_editOutput.SetWindowText(_T(""));
    AppendRichText(RGB(30, 70, 140), _T("=== ЖУРНАЛ ПРОВЕРОК ИЗ РЕД БАЗЫ ДАННЫХ (последние 25 записей) ===\r\n\r\n"));

    if (history.empty()) {
        AppendRichText(RGB(100, 100, 100), _T("Журнал проверок пока пуст.\r\n"));
        return;
    }

    for (const auto& rec : history) {
        CString line;
        line.Format(_T("[ID: %d] %s | Ввод: %s -> Норм.: %s | Вес: %d | Статус: %s\r\n"),
            rec.checkId,
            Utf8ToCString(rec.checkedAt).GetString(),
            Utf8ToCString(rec.rawInput).GetString(),
            Utf8ToCString(rec.normalizedDomain).GetString(),
            rec.totalScore,
            rec.isSuspicious ? _T("ПОДОЗРИТЕЛЬНО") : _T("OK"));

        COLORREF color = rec.isSuspicious ? RGB(180, 30, 30) : RGB(30, 120, 50);
        AppendRichText(color, line);
    }
}
void CTypoScriptDlg::OnBnClickedShowStats() {
    if (!dbManager_.IsConnected()) {
        AfxMessageBox(_T("База данных не подключена."));
        return;
    }

    auto stats = dbManager_.LoadAttackStatistics();
    m_editOutput.SetWindowText(_T(""));
    AppendRichText(RGB(30, 70, 140), _T("=== АНАЛИТИКА АТАК ПО ДАННЫМ ПРЕДСТАВЛЕНИЯ V_ATTACK_STATISTICS ===\r\n"));
    AppendRichText(RGB(170, 170, 170), _T("------------------------------------------------------------------\r\n\r\n"));

    if (stats.empty()) {
        AppendRichText(RGB(100, 100, 100), _T("Статистика атак пока пуста.\r\n"));
        return;
    }

    for (const auto& st : stats) {
        CString line;
        line.Format(_T("  • Домен: %-18s | Категория: %-28s | Всего угроз: %d (критических: %d)\r\n"),
            Utf8ToCString(st.domain).GetString(),
            Utf8ToCString(st.category).GetString(),
            st.totalAttacks,
            st.highRiskCount);

        COLORREF color = (st.highRiskCount > 0) ? RGB(180, 35, 35) : RGB(40, 110, 60);
        AppendRichText(color, line);
    }
}