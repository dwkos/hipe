/*
 * Copyright (C) 2015 The Qt Company Ltd.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "TestsController.h"

#include <QGuiApplication>

void messageHandler(QtMsgType type, const QMessageLogContext&, const QString& message)
{
    if (type == QtCriticalMsg) {
        fprintf(stderr, "%s\n", qPrintable(message));
        return;
    }

    // Do nothing
}

int main(int argc, char** argv)
{
    bool suppressQtDebugOutput = true; // Suppress debug output from Qt if not started with --verbose.

    for (int i = 1; i < argc; ++i) {
        if (!qstrcmp(argv[i], "--verbose"))
            suppressQtDebugOutput = false;
    }

    // Has to be done before QApplication is constructed in case
    // QApplication itself produces debug output.
    if (suppressQtDebugOutput)
        qInstallMessageHandler(messageHandler);

    QGuiApplication app(argc, argv);

    return TestWebKitAPI::TestsController::singleton().run(argc, argv) ? EXIT_SUCCESS : EXIT_FAILURE;
}
