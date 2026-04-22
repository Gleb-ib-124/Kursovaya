#include "pch.h"
#include "TypoScriptDlg.h"

/* algorithm: min/max/sort utilities
 * vector   : динамические массивы для diff-масок и DP */
#include <algorithm>
#include <vector>

namespace {

constexpr DWORD kMaxInputEditTchars = 256 * 1024;
constexpr DWORD kMaxOutputRichTchars = 512 * 1024;
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
    ON_WM_SIZE()
    ON_WM_GETMINMAXINFO()
END_MESSAGE_MAP()

BOOL CTypoScriptDlg::OnInitDialog() {
    CDialogEx::OnInitDialog();

    SetWindowText(_T("TypoScript — анализ домена на typosquatting"));

    if (CWnd* pIn = GetDlgItem(IDC_EDIT_INPUT)) {
        /* Ограничение длины ввода в edit-контроле (защита UI от перегрузки). */
        pIn->SendMessage(EM_SETLIMITTEXT, static_cast<WPARAM>(kMaxInputEditTchars), 0);
    }
    m_editOutput.LimitText(kMaxOutputRichTchars); /* Лимит для RichEdit отчёта. */
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

    /* Геометрические константы интерфейса:
     * m             - внешний отступ,
     * gap           - зазор между блоками,
     * lblH          - высота подписи,
     * topEditH      - высота верхнего поля ввода,
     * comboVisibleH - видимая часть ComboBox,
     * comboDropH    - высота при раскрытии списка,
     * btnW/btnH     - размер кнопок справа. */
    const int m = 16;
    const int gap = 10;
    const int lblH = 18;
    const int topEditH = 100;
    const int comboVisibleH = 28;
    const int comboDropH = 220;
    const int btnW = 150;
    const int btnH = 34;
    const int rightW = btnW + 8;
    const int leftW = cx - (m * 2) - rightW - gap; /* ширина левой колонки */

    if (CWnd* p = GetDlgItem(IDC_STATIC_IN)) {
        p->MoveWindow(m, m, 160, lblH);
    }
    if (CWnd* p = GetDlgItem(IDC_EDIT_INPUT)) {
        p->MoveWindow(m + 110, m - 2, cx - (m + 110) - m, topEditH);
    }

    const int blockTop = m + lblH + 6 + topEditH + 16; /* начало блока "Популярные домены" */
    if (CWnd* p = GetDlgItem(IDC_STATIC_POPULAR)) {
        p->MoveWindow(m, blockTop, 220, lblH);
    }
    const int comboTop = blockTop + lblH + 6;
    if (CWnd* p = GetDlgItem(IDC_COMBO_POPULAR)) {
        p->MoveWindow(m + 10, comboTop, leftW - 10, comboDropH);
    }

    const int btnLeft = m + leftW + gap;
    const int clearTop = comboTop;
    if (CWnd* p = GetDlgItem(IDC_BTN_CLEAR)) {
        p->MoveWindow(btnLeft, clearTop, btnW, btnH);
    }
    const int checkTop = clearTop + btnH + 8;
    if (CWnd* p = GetDlgItem(IDC_BTN_CHECK)) {
        p->MoveWindow(btnLeft, checkTop, btnW, btnH);
    }

    /* Нижняя граница верхних контролов: берём максимум между комбобоксом и кнопками,
     * чтобы нижнее поле отчёта никогда не наезжало на кнопки. */
    const int controlsBottom = (std::max)(comboTop + comboVisibleH, checkTop + btnH);
    const int outputTop = controlsBottom + 20;
    const int outputH = (std::max)(120, cy - outputTop - m);
    if (CWnd* p = GetDlgItem(IDC_EDIT_OUTPUT)) {
        p->MoveWindow(m + 10, outputTop, cx - (m + 10) - (m + 10), outputH);
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

void CTypoScriptDlg::FillReportRich(const TyposquatReport& r, const CString& enteredHost) {
    m_editOutput.SetWindowText(_T(""));

    const COLORREF kText = RGB(0, 0, 0);
    const COLORREF kHighRisk = RGB(200, 30, 30);

    if (r.exactPopularMatch && !r.suspicious) {
        AppendRichText(RGB(30, 120, 40), _T("Совпадает с популярным доменом. Признаков сквоттинга не найдено.\r\n\r\n"));
    }

    AppendRichText(kText, _T("Подозрительно: "));
    AppendRichText(kText, r.suspicious ? _T("ДА") : _T("НЕТ"));
    AppendRichText(kText, _T("\r\nСумма весов: "));
    CString num;
    num.Format(_T("%d"), r.totalScore);
    AppendRichText(ScoreToGradientColor(r.totalScore), num);
    AppendRichText(kText, _T("\r\nНормализованный домен: "));
    AppendRichText(kText, Utf8ToCString(r.normalizedInput));
    AppendRichText(kText, _T("\r\n"));

    if (!r.hits.empty()) {
        /* Для детального объяснения берём самый близкий (первый после сортировки) домен. */
        const std::string& original = r.hits.front().canonicalPopular;
        const std::vector<DiffToken> tokens = BuildDiffTokens(r.normalizedInput, original);
        std::string enteredAligned;
        std::string originalAligned;
        std::vector<bool> enteredDiff;
        std::vector<bool> originalDiff;
        enteredAligned.reserve(tokens.size());
        originalAligned.reserve(tokens.size());
        enteredDiff.reserve(tokens.size());
        originalDiff.reserve(tokens.size());
        for (const DiffToken& t : tokens) {
            enteredAligned.push_back(t.entered);
            originalAligned.push_back(t.original);
            const bool changed = t.op != DiffOp::Keep;
            enteredDiff.push_back(changed);
            originalDiff.push_back(changed);
        }

        /* appendColoredAscii:
         * выводит строку сегментами одного цвета:
         * обычный сегмент - чёрный, отличающийся - красный. */
        auto appendColoredAscii = [&](const std::string& text, const std::vector<bool>& mask) {
            std::size_t pos = 0;
            while (pos < text.size()) {
                const bool isDiff = mask[pos];
                std::size_t end = pos + 1;
                while (end < text.size() && mask[end] == isDiff) {
                    ++end;
                }
                AppendRichText(
                    isDiff ? kHighRisk : kText,
                    Utf8ToCString(text.substr(pos, end - pos)));
                pos = end;
            }
        };

        AppendRichText(kText, _T("Разница с наиболее похожим популярным доменом:\r\n"));
        AppendRichText(kText, _T("  Ввели:    "));
        appendColoredAscii(enteredAligned, enteredDiff);
        AppendRichText(kText, _T("\r\n  Оригинал: "));
        appendColoredAscii(originalAligned, originalDiff);
        AppendRichText(kText, _T("\r\nКрасным отмечены замененные/лишние/пропущенные символы.\r\n"));

        /* Дополнительное текстовое пояснение "латиница vs кириллица". */
        CString lowered(enteredHost);
        lowered.MakeLower();
        bool foundHomoglyph = false;
        for (int i = 0; i < lowered.GetLength() && i < static_cast<int>(original.size()); ++i) {
            const wchar_t wc = lowered[i];
            const char mapped = MapCyrillicLookalikeToLatin(wc);
            if (mapped != '\0' && mapped == original[static_cast<std::size_t>(i)]) {
                if (!foundHomoglyph) {
                    AppendRichText(kText, _T("Обнаружена подмена алфавита:\r\n"));
                    foundHomoglyph = true;
                }
                CString line;
                line.Format(
                    _T("  • Позиция %d: в оригинале латинская '%c', во вводе похожая кириллическая '%c'.\r\n"),
                    i + 1,
                    original[static_cast<std::size_t>(i)],
                    static_cast<wchar_t>(wc));
                AppendRichText(kHighRisk, line);
            }
        }
        if (foundHomoglyph) {
            AppendRichText(
                kHighRisk,
                _T("  Это признак typosquatting: латиница подменена похожими символами другого алфавита.\r\n"));
        }
    }

    AppendRichText(kText, _T("\r\nПохожие популярные домены:\r\n"));

    if (r.hits.empty()) {
        AppendRichText(kText, _T("— нет —\r\n"));
        return;
    }
    for (const TyposquatHit& h : r.hits) {
        CString line;
        line.Format(
            _T("  • %hs  (distance=%d, вес=%d)%s\r\n"),
            h.canonicalPopular.c_str(),
            h.editDistance,
            h.weight,
            h.highRisk ? _T(" [высокий риск]") : _T(""));
        AppendRichText(h.highRisk ? kHighRisk : kText, line);
    }
}

void CTypoScriptDlg::OnBnClickedCheck() {
    CString input;
    GetDlgItemText(IDC_EDIT_INPUT, input);
    input.Trim();
    if (input.IsEmpty()) {
        /* UX-логика: если поле ввода пустое, берём выбранный домен из ComboBox. */
        const int idx = m_comboPopular.GetCurSel();
        if (idx > 0) {
            m_comboPopular.GetLBText(idx, input);
            SetDlgItemText(IDC_EDIT_INPUT, input);
        }
    }
    const std::string u8 = CStringToUtf8(input);
    const TyposquatReport r = detector_.Analyze(u8);
    FillReportRich(r, input);
}

void CTypoScriptDlg::OnBnClickedClear() {
    /* Полный сброс состояния формы в "чистый" режим. */
    SetDlgItemText(IDC_EDIT_INPUT, _T(""));
    m_editOutput.SetWindowText(_T(""));
    m_comboPopular.SetCurSel(0);
}
