/*
    SPDX-FileCopyrightText: 2020 Aleix Pol Gonzalez <aleixpol@kde.org>
    SPDX-FileCopyrightText: 2026 David Wang

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "outputstream.h"
#include "screencasting.h"

#include <QDebug>
#include <QGuiApplication>
#include <QScreen>

OutputStream::OutputStream(QObject *parent)
    : QObject(parent)
    , m_screencasting(std::make_unique<Screencasting>())
{
    // The global is bound asynchronously: a request made before activeChanged() would be
    // lost (this is what made ScreencastingRequest need a retry timer), so ask again then.
    connect(m_screencasting.get(), &Screencasting::activeChanged, this, &OutputStream::restart);
    // The named output may appear later (projector plugged in after start).
    connect(qGuiApp, &QGuiApplication::screenAdded, this, [this] {
        if (!m_stream) {
            restart();
        }
    });
}

OutputStream::~OutputStream()
{
    m_stream.reset(); // close the stream before the manager goes
}

QString OutputStream::outputName() const
{
    return m_outputName;
}

void OutputStream::setOutputName(const QString &outputName)
{
    if (m_outputName == outputName) {
        return;
    }
    m_outputName = outputName;
    Q_EMIT outputNameChanged();
    restart();
}

bool OutputStream::pointer() const
{
    return m_pointer;
}

void OutputStream::setPointer(bool pointer)
{
    if (m_pointer == pointer) {
        return;
    }
    m_pointer = pointer;
    Q_EMIT pointerChanged();
    restart();
}

uint OutputStream::nodeId() const
{
    return m_nodeId;
}

QString OutputStream::error() const
{
    return m_error;
}

void OutputStream::restart()
{
    m_stream.reset();
    setNodeId(0);

    if (m_outputName.isEmpty() || !m_screencasting->isActive()) {
        return;
    }

    const auto mode = m_pointer ? Screencasting::pointer_embedded : Screencasting::pointer_hidden;
    m_stream = m_screencasting->createOutputStream(m_outputName, mode);
    if (!m_stream) {
        return; // no such screen yet; screenAdded retries
    }
    setError(QString());

    ScreencastingStream *stream = m_stream.get();
    connect(stream, &ScreencastingStream::created, this, &OutputStream::setNodeId);
    connect(stream, &ScreencastingStream::closed, this, [this] {
        setNodeId(0);
    });
    connect(stream, &ScreencastingStream::failed, this, [this](const QString &error) {
        qWarning() << "OutputStream: screencast of" << m_outputName << "failed:" << error;
        setError(error);
        setNodeId(0);
    });
}

void OutputStream::setNodeId(uint nodeId)
{
    if (m_nodeId != nodeId) {
        m_nodeId = nodeId;
        Q_EMIT nodeIdChanged();
    }
}

void OutputStream::setError(const QString &error)
{
    if (m_error != error) {
        m_error = error;
        Q_EMIT errorChanged();
    }
}

#include "moc_outputstream.cpp"
