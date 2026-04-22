#pragma once

/* Идентификаторы ресурсов (диалог, контролы и т.д.) */
#include "resource.h"

/* CTypoScriptApp:
 * класс приложения MFC.
 * Отвечает за старт/инициализацию процесса и запуск главного диалога. */
class CTypoScriptApp : public CWinApp {
public:
    /* noexcept: конструктор не бросает исключений. */
    CTypoScriptApp() noexcept;

    /* InitInstance: главная точка инициализации MFC-приложения. */
    BOOL InitInstance() override;

    /* Таблица сообщений MFC (макрос BEGIN/END_MESSAGE_MAP в .cpp). */
    DECLARE_MESSAGE_MAP()
};
