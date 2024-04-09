/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-CLA-applies
 *
 * MuseScore
 * Music Composition & Notation
 *
 * Copyright (C) 2021 MuseScore BVBA and others
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#include "diagnosticsmodule.h"

#include <QQmlEngine>

#include "modularity/ioc.h"
#include "global/iglobalconfiguration.h"
#include "ui/iinteractiveuriregister.h"
#include "ui/iuiactionsregister.h"

#include "internal/diagnosticsconfiguration.h"
#include "internal/diagnosticsactions.h"
#include "internal/diagnosticsactionscontroller.h"
#include "internal/diagnosticspathsregister.h"
#include "internal/savediagnosticfilesscenario.h"

#include "internal/crashhandler/crashhandler.h"

#include "view/diagnosticspathsmodel.h"
#include "view/system/profilerviewmodel.h"

#include "view/keynav/diagnosticnavigationmodel.h"
#include "view/keynav/abstractkeynavdevitem.h"
#include "view/keynav/keynavdevsection.h"
#include "view/keynav/keynavdevsubsection.h"
#include "view/keynav/keynavdevcontrol.h"

#include "view/diagnosticaccessiblemodel.h"

#include "devtools/crashhandlerdevtoolsmodel.h"

#include "muse_framework_config.h"

#include "log.h"

using namespace muse::diagnostics;
using namespace muse;
using namespace muse::modularity;

std::string DiagnosticsModule::moduleName() const
{
    return "diagnostics";
}

void DiagnosticsModule::registerExports()
{
    m_configuration = std::make_shared<DiagnosticsConfiguration>();
    m_actionsController = std::make_shared<DiagnosticsActionsController>();

    ioc()->registerExport<IDiagnosticsPathsRegister>(moduleName(), new DiagnosticsPathsRegister());
    ioc()->registerExport<IDiagnosticsConfiguration>(moduleName(), m_configuration);
    ioc()->registerExport<ISaveDiagnosticFilesScenario>(moduleName(), new SaveDiagnosticFilesScenario());
}

void DiagnosticsModule::resolveImports()
{
    auto ir = ioc()->resolve<muse::ui::IInteractiveUriRegister>(moduleName());
    if (ir) {
        ir->registerQmlUri(Uri("muse://diagnostics/system/paths"), "MuseScore/Diagnostics/DiagnosticPathsDialog.qml");
        ir->registerQmlUri(Uri("muse://diagnostics/system/profiler"), "MuseScore/Diagnostics/DiagnosticProfilerDialog.qml");
        ir->registerQmlUri(Uri("muse://diagnostics/navigation/tree"), "MuseScore/Diagnostics/DiagnosticNavigationDialog.qml");
        ir->registerQmlUri(Uri("muse://diagnostics/accessible/tree"), "MuseScore/Diagnostics/DiagnosticAccessibleDialog.qml");
    }

    auto ar = ioc()->resolve<muse::ui::IUiActionsRegister>(moduleName());
    if (ar) {
        ar->reg(std::make_shared<DiagnosticsActions>());
    }
}

void DiagnosticsModule::registerUiTypes()
{
    qmlRegisterType<DiagnosticsPathsModel>("MuseScore.Diagnostics", 1, 0, "DiagnosticsPathsModel");
    qmlRegisterType<ProfilerViewModel>("MuseScore.Diagnostics", 1, 0, "ProfilerViewModel");

    qmlRegisterType<DiagnosticNavigationModel>("MuseScore.Diagnostics", 1, 0, "DiagnosticNavigationModel");
    qmlRegisterUncreatableType<AbstractKeyNavDevItem>("MuseScore.Diagnostics", 1, 0, "AbstractKeyNavDevItem", "Cannot create a Abstract");
    qmlRegisterUncreatableType<KeyNavDevSubSection>("MuseScore.Diagnostics", 1, 0, "KeyNavDevSubSection", "Cannot create");
    qmlRegisterUncreatableType<KeyNavDevSection>("MuseScore.Diagnostics", 1, 0, "KeyNavDevSection", "Cannot create a KeyNavDevSection");
    qmlRegisterUncreatableType<KeyNavDevControl>("MuseScore.Diagnostics", 1, 0, "KeyNavDevControl", "Cannot create a KeyNavDevControl");

    qmlRegisterType<DiagnosticAccessibleModel>("MuseScore.Diagnostics", 1, 0, "DiagnosticAccessibleModel");

    qmlRegisterType<CrashHandlerDevToolsModel>("MuseScore.Diagnostics", 1, 0, "CrashHandlerDevToolsModel");
}

void DiagnosticsModule::onInit(const IApplication::RunMode& mode)
{
    if (mode == IApplication::RunMode::AudioPluginRegistration) {
        return;
    }

    m_configuration->init();
    m_actionsController->init();

    auto globalConf = modularity::ioc()->resolve<IGlobalConfiguration>(moduleName());
    IF_ASSERT_FAILED(globalConf) {
        return;
    }

#ifdef MUSE_MODULE_DIAGNOSTICS_CRASHPAD_CLIENT

    static CrashHandler s_crashHandler;

#ifdef _MSC_VER
    muse::io::path_t handlerFile("crashpad_handler.exe");
#else
    muse::io::path_t handlerFile("crashpad_handler");
#endif // _MSC_VER

    muse::io::path_t handlerPath = globalConf->appBinDirPath() + "/" + handlerFile;
    muse::io::path_t dumpsDir = globalConf->userAppDataPath() + "/logs/dumps";
    fileSystem()->makePath(dumpsDir);
    std::string serverUrl(MUE_CRASH_REPORT_URL);

    if (!m_configuration->isDumpUploadAllowed()) {
        serverUrl.clear();
        LOGD() << "not allowed dump upload";
    } else {
        LOGD() << "crash server url: " << serverUrl;
    }

    bool ok = s_crashHandler.start(handlerPath, dumpsDir, serverUrl);
    if (!ok) {
        LOGE() << "failed start crash handler";
    } else {
        LOGI() << "success start crash handler";
    }

#else
    LOGW() << "crash handling disabled";
#endif // MUSE_MODULE_DIAGNOSTICS_CRASHPAD_CLIENT
}