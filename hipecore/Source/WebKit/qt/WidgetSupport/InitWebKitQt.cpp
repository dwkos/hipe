/*
 * Copyright (C) 2015 The Qt Company Ltd.
 * Copyright (C) 2025-2026 General Development Systems
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
#include "InitWebKitQt.h"

#include "InitWebCoreQt.h"

namespace WebKit {

// hipecore: this used to wire up the QStyle-backed form-control theme
// (RenderThemeQStyle / QStyleFacadeImp) -- delegating every control paint to the live
// desktop QStyle, which on most Linux setups is a GTK style plugin that drags in
// libgtk/libgdk/libpango and a startup icon-theme scan. That whole path is gone;
// WebCore now always uses the self-contained "Mobile Qt" theme, which draws every
// control itself and needs no platform toolkit. Nothing is left to initialise here.
//
// (Historical note: searchCancelButton/searchCancelButtonPressed were also registered
// here once, via QApplication::style()->standardPixmap() -- another live-OS-style query
// with the same icon-theme-scan side effect. They are bundled resources now, in
// ImageQt.cpp's graphics() table. See project_hipecore_memory_footprint_audit memory.)
//
// Kept as an exported no-op so the QWebPagePrivate call site and the ABI are unchanged.
QWEBKITWIDGETS_EXPORT void initializeWebKitWidgets()
{
}

}
