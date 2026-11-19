#pragma once

/* resource.h:
 *   IDD/IDC идентификаторы элементов интерфейса из .rc файла.
 * TyposquatDetector.h:
 *   Ядро анализа доменов на typosquatting (логика сравнения и отчёт). */
#include "resource.h"
#include "TyposquatDetector.h"
#include "DatabaseManager.h"


/* CTypoScriptDlg:
 * Главное окно приложения (модальный диалог MFC).
 * Отвечает за:
 *  - визуальный интерфейс (поля, кнопки, выпадающий список, отчёт),
 *  - сбор входных данных,
 *  - вызов детектора,
 *  - форматированный вывод результата с подсветкой. */
class CTypoScriptDlg : public CDialogEx {
public:
    /* explicit: запрещаем неявное преобразование CWnd* -> CTypoScriptDlg. */
    explicit CTypoScriptDlg(CWnd* pParent = nullptr);

    /* IDD: идентификатор шаблона диалога в ресурсе TypoScript.rc. */
    enum { IDD = IDD_TYPOSCRIPT_DIALOG };

protected:
    /* DDX-таблица привязки HWND-контролов к полям класса (MFC Data Exchange). */
    void DoDataExchange(CDataExchange* pDX) override;
    /* OnInitDialog:
     * первичная настройка окна, лимитов текста, заполнение ComboBox. */
    BOOL OnInitDialog() override;
    /* Кнопка "Проверить": запускает анализ введенного домена. */
    afx_msg void OnBnClickedCheck();
    afx_msg void OnBnClickedAddDomain();
    afx_msg void OnBnClickedShowHistory();
    afx_msg void OnBnClickedShowStats();


    /* Кнопка "Очистить": очищает ввод, отчёт и сбрасывает выбор домена. */
    afx_msg void OnBnClickedClear();
    /* Кнопка "Загрузить из файла…": открывает CFileDialog, читает текстовый
     * файл и подставляет содержимое в поле ввода. */
    afx_msg void OnBnClickedLoad();
    /* Кнопка "Сохранить отчёт…": сохраняет содержимое RichEdit-отчёта
     * в файл в кодировке UTF-8 с BOM. */
    afx_msg void OnBnClickedSave();
    /* WM_SIZE: перерасчёт ручной раскладки при изменении размеров окна. */
    afx_msg void OnSize(UINT nType, int cx, int cy);
    /* WM_GETMINMAXINFO: ограничение минимального размера окна. */
    afx_msg void OnGetMinMaxInfo(MINMAXINFO* lpMMI);

    DECLARE_MESSAGE_MAP()

private:
    /* detector_:
     * экземпляр движка анализа, хранит список популярных доменов и правила. */
    TyposquatDetector detector_;
    CDatabaseManager dbManager_;
    CButton m_btnAddDomain;
    CButton m_btnShowHistory;
    CButton m_btnShowStats;


    /* layoutReady_:
     * защита от преждевременной раскладки, пока HWND контролов ещё не готовы. */
    bool layoutReady_ = false;
    /* m_editOutput:
     * RichEdit для отчёта (нужен, чтобы красить отдельные сегменты текста). */
    CRichEditCtrl m_editOutput;
    /* m_comboPopular:
     * выпадающий список популярных доменов для удобного тестирования. */
    CComboBox m_comboPopular;

    /* Ручная раскладка элементов интерфейса (координаты и размеры). */
    void LayoutControls(int cx, int cy);
    /* Формирование форматированного отчёта в RichEdit по результатам анализа. */
    void FillReportRich(const TyposquatReport& r, const CString& enteredHost);
    /* Вставка текста определённым цветом в конец RichEdit. */
    void AppendRichText(COLORREF cr, const CString& text);
    /* Градиентный цвет для числового score (зелёный->жёлтый->красный). */
    static COLORREF ScoreToGradientColor(int score);
    /* Заполнение ComboBox списком популярных доменов из detector_. */
    void FillPopularDomains();

    /* Конвертации строк:
     * UI (CString/UTF-16) <-> detector (std::string/UTF-8). */
    static std::string CStringToUtf8(const CString& s);
    static CString Utf8ToCString(const std::string& s);

    /* Чтение текстового файла в CString.
     * Поддерживает UTF-8 (с/без BOM) и ACP как fallback.
     * Возвращает FALSE и текст ошибки в `err`, если файл нельзя открыть/слишком большой. */
    static BOOL LoadTextFile(const CString& path, CString& out, CString& err);

    /* Сохранение CString в файл в кодировке UTF-8 с BOM (EF BB BF).
     * BOM обеспечивает корректное отображение русского текста в «Блокноте» Windows и Excel. */
    static BOOL SaveTextFileUtf8(const CString& path, const CString& text);
};
