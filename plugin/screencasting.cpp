/*
    SPDX-FileCopyrightText: 2020 Aleix Pol Gonzalez <aleixpol@kde.org>
    SPDX-FileCopyrightText: 2026 David Wang

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

// Derived from plasma-workspace's libtaskmanager/screencasting.cpp.

#include "screencasting.h"

#include <QDebug>
#include <QGuiApplication>
#include <QScreen>

ScreencastingStream::ScreencastingStream() = default;

ScreencastingStream::~ScreencastingStream()
{
    if (isInitialized()) {
        close();
    }
}

void ScreencastingStream::zkde_screencast_stream_unstable_v1_created(uint32_t node)
{
    Q_EMIT created(node);
}

void ScreencastingStream::zkde_screencast_stream_unstable_v1_closed()
{
    Q_EMIT closed();
}

void ScreencastingStream::zkde_screencast_stream_unstable_v1_failed(const QString &error)
{
    Q_EMIT failed(error);
}

// Bind at most the version this client code was generated for; Qt also caps it at the
// version the compositor advertises (KWin 6.7: 6).
Screencasting::Screencasting()
    : QWaylandClientExtensionTemplate<Screencasting>(zkde_screencast_unstable_v1::interface()->version)
{
    initialize();

    if (!isInitialized()) {
        qWarning() << "zkde_screencast_unstable_v1 not offered; the process's desktop file must list it in X-KDE-Wayland-Interfaces";
    }
}

Screencasting::~Screencasting()
{
    if (isActive()) {
        destroy();
    }
}

std::unique_ptr<ScreencastingStream> Screencasting::createOutputStream(const QString &outputName, pointer mode)
{
    if (!isActive()) {
        return nullptr;
    }

    wl_output *output = nullptr;
    const auto screens = qGuiApp->screens();
    for (QScreen *screen : screens) {
        if (screen->name() == outputName) {
            if (auto wayland = screen->nativeInterface<QNativeInterface::QWaylandScreen>()) {
                output = wayland->output();
            }
            break;
        }
    }

    if (!output) {
        return nullptr;
    }

    auto stream = std::make_unique<ScreencastingStream>();
    stream->setObjectName(outputName);
    stream->init(stream_output(output, mode));
    return stream;
}

#include "moc_screencasting.cpp"
