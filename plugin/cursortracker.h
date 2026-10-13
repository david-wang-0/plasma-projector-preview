/*
    SPDX-FileCopyrightText: 2026 David Wang

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

// The global pointer position, which a Wayland client cannot query and neither KWin's D-Bus
// API nor kpipewire exposes. We load a small KWin script that pushes workspace.cursorPos to
// us over D-Bus on every move, and unload it again when we go.

#pragma once

#include <QObject>
#include <QPointF>
#include <QString>
#include <qqmlregistration.h>

#include <memory>

class CursorReceiver;

class CursorTracker : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    // Pointer position in global logical coordinates, as KWin reports it (compare with
    // Screen.virtualX/virtualY/width/height).
    Q_PROPERTY(QPointF position READ position NOTIFY positionChanged)
    // false until the first position has arrived.
    Q_PROPERTY(bool valid READ valid NOTIFY validChanged)

public:
    explicit CursorTracker(QObject *parent = nullptr);
    ~CursorTracker() override;

    QPointF position() const;
    bool valid() const;

    // Called by CursorReceiver for each setPosition message from the KWin script.
    void setPosition(const QPointF &position);

Q_SIGNALS:
    void positionChanged();
    void validChanged();

private:
    void loadScript();
    void unloadScript();

    std::unique_ptr<CursorReceiver> m_receiver;
    QString m_scriptPath;  // the .js we wrote, empty if none
    QString m_pluginName;  // KWin's name for the loaded script, empty if not loaded
    QPointF m_position;
    bool m_registered = false; // we own objectPath on the session bus
    bool m_valid = false;
};
