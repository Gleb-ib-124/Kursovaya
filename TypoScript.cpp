#include "pch.h"
#include "TypoScript.h"
#include "TypoScriptDlg.h"

#ifdef _DEBUG
/* Отладочная версия оператора new (диагностика утечек в MFC Debug). */
#define new DEBUG_NEW
#endif

/* Глобальный singleton-объект приложения.
 * В MFC обычно существует ровно один экземпляр CWinApp. */
CTypoScriptApp theApp;

/* Карта сообщений приложения (в данном случае без пользовательских сообщений). */
BEGIN_MESSAGE_MAP(CTypoScriptApp, CWinApp)
END_MESSAGE_MAP()

CTypoScriptApp::CTypoScriptApp() noexcept = default;

BOOL CTypoScriptApp::InitInstance() {
    /* Базовая инициализация MFC-подсистем. */
    CWinApp::InitInstance();

    /* RichEdit2 обязателен, т.к. отчёт выводится в CRichEditCtrl с цветовой подсветкой. */
    if (!AfxInitRichEdit2()) {
        AfxMessageBox(_T("Не удалось загрузить Rich Edit (Riched20.dll)."), MB_ICONERROR);
        return FALSE;
    }

    /* Префикс ветки реестра для настроек приложения (если позже добавим сохранение prefs). */
    SetRegistryKey(_T("TypoScriptCoursework"));

    /* Запуск главного модального диалога. */
    CTypoScriptDlg dlg;
    m_pMainWnd = &dlg;
    dlg.DoModal();

    /* FALSE: после закрытия модального окна приложение завершается. */
    return FALSE;
}
