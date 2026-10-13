/*
    SPDX-FileCopyrightText: 2020 Aleix Pol Gonzalez <aleixpol@kde.org>
    SPDX-FileCopyrightText: 2026 David Wang

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

// Modelled on plasma-workspace's libtaskmanager/screencastingrequest.h (ScreencastingRequest),
// but for outputs only, with a choice of pointer mode (embedded by default), and requesting
// the stream again once the Wayland global is bound instead of dropping an early request.

#pragma once

#include <QObject>
#include <QString>
#include <qqmlregistration.h>

#include <memory>

class Screencasting;
class ScreencastingStream;

class OutputStream : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    // Name of the output to stream, as in QScreen::name / Screen.name (e.g. "HDMI-A-1").
    Q_PROPERTY(QString outputName READ outputName WRITE setOutputName NOTIFY outputNameChanged)
    // true: KWin draws the pointer into the frames; false: no pointer.
    Q_PROPERTY(bool pointer READ pointer WRITE setPointer NOTIFY pointerChanged)
    // PipeWire node of the stream, 0 while there is none. Feed it to PipeWireSourceItem.nodeId.
    Q_PROPERTY(uint nodeId READ nodeId NOTIFY nodeIdChanged)
    // Last error KWin reported for the stream, empty if none.
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)

public:
    explicit OutputStream(QObject *parent = nullptr);
    ~OutputStream() override;

    QString outputName() const;
    void setOutputName(const QString &outputName);
    bool pointer() const;
    void setPointer(bool pointer);
    uint nodeId() const;
    QString error() const;

Q_SIGNALS:
    void outputNameChanged();
    void pointerChanged();
    void nodeIdChanged();
    void errorChanged();

private:
    void restart();
    void setNodeId(uint nodeId);
    void setError(const QString &error);

    std::unique_ptr<Screencasting> m_screencasting;
    std::unique_ptr<ScreencastingStream> m_stream;
    QString m_outputName;
    QString m_error;
    bool m_pointer = true;
    uint m_nodeId = 0;
};
