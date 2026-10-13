/*
    SPDX-FileCopyrightText: 2020 Aleix Pol Gonzalez <aleixpol@kde.org>
    SPDX-FileCopyrightText: 2026 David Wang

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

// Derived from plasma-workspace's libtaskmanager/screencasting.h, which is not installed.
// Changes: only output streams, the pointer mode is the caller's choice, and the
// wl_output comes from Qt's public QNativeInterface::QWaylandScreen.

#pragma once

#include "qwayland-zkde-screencast-unstable-v1.h"

#include <QObject>
#include <QWaylandClientExtensionTemplate>

#include <memory>

class ScreencastingStream : public QObject, public QtWayland::zkde_screencast_stream_unstable_v1
{
    Q_OBJECT

public:
    ScreencastingStream();
    ~ScreencastingStream() override;

Q_SIGNALS:
    void created(quint32 nodeid);
    void failed(const QString &error);
    void closed();

protected:
    void zkde_screencast_stream_unstable_v1_created(uint32_t node) override;
    void zkde_screencast_stream_unstable_v1_closed() override;
    void zkde_screencast_stream_unstable_v1_failed(const QString &error) override;
};

class Screencasting : public QWaylandClientExtensionTemplate<Screencasting>, public QtWayland::zkde_screencast_unstable_v1
{
    Q_OBJECT

public:
    Screencasting();
    ~Screencasting() override;

    // nullptr while the global is not bound yet (see isActive()/activeChanged())
    // or when no screen has that name.
    std::unique_ptr<ScreencastingStream> createOutputStream(const QString &outputName, pointer mode);
};
