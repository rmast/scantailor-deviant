/*
    Scan Tailor - Interactive post-processing tool for scanned pages.
    Copyright (C)  Joseph Artsimovich <joseph.artsimovich@gmail.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "config.h"
#include "Application.h"
#include "MainWindow.h"
#include "PngMetadataLoader.h"
#include "TiffMetadataLoader.h"
#include "JpegMetadataLoader.h"
#ifdef ENABLE_OPENJPEG
#include "Jp2MetadataLoader.h"
#endif
#include "GenericMetadataLoader.h"
#include "settings/ini_keys.h"
#include <QMetaType>
#include <QtPlugin>
#include <QLocale>
#include <QDir>
#include <QString>
#include <QStringList>
#include <QTranslator>
#include "settings/globalstaticsettings.h"
#include <Qt>
#include <string.h>

#include "CommandLine.h"
#include "SimpleMcp.h"

int main(int argc, char** argv)
{
    Application app(argc, argv);

#ifdef _WIN32
    // Get rid of all references to Qt's installation directory.
    app.setLibraryPaths(QStringList(app.applicationDirPath()));
#endif

    // This information is used by QSettings.
    // Must be done before CommandLine created
    app.setApplicationName(APPLICATION_NAME);
    app.setOrganizationName(ORGANIZATION_NAME);
    app.setOrganizationDomain(ORGANIZATION_DOMAIN);
    QSettings::setDefaultFormat(QSettings::IniFormat);

    // parse command line arguments
    CommandLine cli(app.arguments());
    CommandLine::set(cli);

    if (cli.isError()) {
        cli.printHelp();
        return 1;
    }

    if (cli.hasHelp()) {
        cli.printHelp();
        return 0;
    }

    QSettings settings;
    GlobalStaticSettings::applyAppStyle(settings);

    PngMetadataLoader::registerMyself();
    TiffMetadataLoader::registerMyself();
    JpegMetadataLoader::registerMyself();
#ifdef ENABLE_OPENJPEG
    Jp2MetadataLoader::registerMyself();
#endif
    // should be the last one as the most dumb and loads whole image into mem
    GenericMetadataLoader::registerMyself();

    MainWindow* main_wnd = new MainWindow();
    main_wnd->setAttribute(Qt::WA_DeleteOnClose);

    // Simple MCP server for UI introspection (alternative to qtmcp)
    SimpleMcp* mcpServer = nullptr;
    if (qEnvironmentVariableIsSet("SCANTAILOR_MCP_ENABLE")) {
        mcpServer = new SimpleMcp(&app);
        mcpServer->start();
        
        // Show the TCP port for easy connection
        qDebug() << "=== SimpleMCP Server Ready ===";
        qDebug() << "TCP Port:" << mcpServer->getPort();
        qDebug() << "Connect with: echo '{\"jsonrpc\":\"2.0\",\"method\":\"tools/call\",\"params\":{\"name\":\"count_spline_points\",\"arguments\":{}},\"id\":\"1\"}' | nc localhost" << mcpServer->getPort();
        qDebug() << "===============================";
        
        // For GUI mode, you can test MCP functions by setting environment variable
        // SCANTAILOR_MCP_TEST to execute a specific command
        if (qEnvironmentVariableIsSet("SCANTAILOR_MCP_TEST")) {
            QString testCommand = qEnvironmentVariable("SCANTAILOR_MCP_TEST");
            if (testCommand.isEmpty()) {
                testCommand = R"({"jsonrpc":"2.0","method":"tools/call","params":{"name":"count_spline_points","arguments":{}},"id":"test1"})";
            }
            
            // Use a timer to execute the test after the GUI is fully loaded
            QTimer::singleShot(2000, [mcpServer, testCommand]() {
                QString result = mcpServer->executeCommand(testCommand);
                qDebug() << "MCP Test Result:" << result;
            });
        }
    }

    QObject::connect(main_wnd, &MainWindow::settingsUpdateRequest, CommandLine::updateSettings);

    if (cli.hasLanguage()) {
        main_wnd->changeLanguage(cli.getLanguage(), true);
    }

    if (settings.value(_key_app_maximized, _key_app_maximized_def) == false) {
        main_wnd->show();
    } else {
        //main_wnd->showMaximized(); // didn't work for some Win machines.
        QTimer::singleShot(0, main_wnd, &QMainWindow::showMaximized);
    }

    if (!cli.projectFile().isEmpty()) {
        main_wnd->openProject(cli.projectFile());
    }

    return app.exec();
}
