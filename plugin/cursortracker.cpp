/*
    SPDX-FileCopyrightText: 2026 David Wang

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "cursortracker.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusVirtualObject>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QStandardPaths>

namespace
{
const QString kwinService = QStringLiteral("org.kde.KWin");
const QString scriptingPath = QStringLiteral("/Scripting");
const QString scriptingInterface = QStringLiteral("org.kde.kwin.Scripting");

const QString objectPath = QStringLiteral("/CursorTracker");
const QString interfaceName = QStringLiteral("io.github.david_wang_0.PlasmaProjectorPreview.CursorTracker");
const QString pluginPrefix = QStringLiteral("plasma-projector-preview-cursor-");

QString pluginName(qint64 pid)
{
    return pluginPrefix + QString::number(pid);
}

QDBusMessage scriptingCall(const QString &method, const QVariantList &arguments)
{
    QDBusMessage message = QDBusMessage::createMethodCall(kwinService, scriptingPath, scriptingInterface, method);
    message.setArguments(arguments);
    return message;
}

// The process name of pid, empty if there is no such process.
QByteArray processName(const QString &pid)
{
    QFile comm(QStringLiteral("/proc/%1/comm").arg(pid));
    return comm.open(QIODevice::ReadOnly) ? comm.readAll().trimmed() : QByteArray();
}

// A preview killed hard (SIGKILL, crash) cannot unload its script, which then keeps calling
// a bus name that is gone. Unload the scripts of previews that are no longer running, and our
// own name too in case a dead preview had our pid. A pid now used by some other program does
// not count as running.
void unloadStaleScripts(QDir dir)
{
    const QByteArray ourName = processName(QStringLiteral("self"));
    const QString ourPid = QString::number(QCoreApplication::applicationPid());
    const QStringList files = dir.entryList({QStringLiteral("cursor-*.js")}, QDir::Files);
    for (const QString &file : files) {
        const QString pid = file.sliced(7, file.size() - 10); // cursor-<pid>.js
        bool isNumber = false;
        pid.toLongLong(&isNumber);
        if (isNumber && pid != ourPid && processName(pid) == ourName) {
            continue; // another live preview
        }
        if (isNumber) {
            QDBusConnection::sessionBus().asyncCall(scriptingCall(QStringLiteral("unloadScript"), {pluginPrefix + pid}));
        }
        dir.remove(file);
    }
}
}

// Receives setPosition from the KWin script. A virtual object rather than an exported slot
// because KWin's callDBus marshals each JS number by its value: integral coordinates arrive
// as int32 ("i"), fractional ones as double ("d"), and a slot only matches one signature.
class CursorReceiver : public QDBusVirtualObject
{
public:
    explicit CursorReceiver(CursorTracker *tracker)
        : m_tracker(tracker)
    {
    }

    QString introspect(const QString &path) const override
    {
        Q_UNUSED(path)
        return QStringLiteral(
                   "  <interface name=\"%1\">\n"
                   "    <!-- x and y may each be an int32 or a double -->\n"
                   "    <method name=\"setPosition\">\n"
                   "      <arg name=\"x\" type=\"d\" direction=\"in\"/>\n"
                   "      <arg name=\"y\" type=\"d\" direction=\"in\"/>\n"
                   "    </method>\n"
                   "  </interface>\n")
            .arg(interfaceName);
    }

    bool handleMessage(const QDBusMessage &message, const QDBusConnection &connection) override
    {
        if (message.interface() != interfaceName || message.member() != QLatin1String("setPosition")) {
            return false; // QtDBus answers with an UnknownMethod error
        }
        const QVariantList arguments = message.arguments();
        bool xOk = false;
        bool yOk = false;
        const double x = arguments.size() == 2 ? arguments[0].toDouble(&xOk) : 0;
        const double y = arguments.size() == 2 ? arguments[1].toDouble(&yOk) : 0;
        if (!xOk || !yOk) {
            connection.send(message.createErrorReply(QDBusError::InvalidArgs, QStringLiteral("expected two numbers")));
            return true;
        }
        // QtDBus may call us from its own thread; setPosition belongs to the GUI thread. With
        // the tracker as context, a call still queued when it is destroyed is dropped.
        QMetaObject::invokeMethod(m_tracker, [tracker = m_tracker, x, y] {
            tracker->setPosition(QPointF(x, y));
        });
        if (message.isReplyRequired()) {
            connection.send(message.createReply());
        }
        return true;
    }

private:
    CursorTracker *const m_tracker;
};

CursorTracker::CursorTracker(QObject *parent)
    : QObject(parent)
    , m_receiver(std::make_unique<CursorReceiver>(this))
{
    if (!QDBusConnection::sessionBus().registerVirtualObject(objectPath, m_receiver.get())) {
        qWarning() << "CursorTracker: cannot register" << objectPath << "on the session bus"
                   << "(only one CursorTracker per process)";
        return;
    }
    m_registered = true;
    loadScript();
    // A closing plasmawindowed may exit without destroying the QML scene.
    connect(qApp, &QCoreApplication::aboutToQuit, this, &CursorTracker::unloadScript);
}

CursorTracker::~CursorTracker()
{
    unloadScript();
    if (m_registered) {
        QDBusConnection::sessionBus().unregisterObject(objectPath);
    }
}

QPointF CursorTracker::position() const
{
    return m_position;
}

bool CursorTracker::valid() const
{
    return m_valid;
}

void CursorTracker::setPosition(const QPointF &position)
{
    if (m_position != position) {
        m_position = position;
        Q_EMIT positionChanged();
    }
    if (!m_valid) {
        m_valid = true;
        Q_EMIT validChanged();
    }
}

void CursorTracker::loadScript()
{
    const QString runtimeDir = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if (runtimeDir.isEmpty()) {
        qWarning() << "CursorTracker: no XDG_RUNTIME_DIR, not tracking the pointer";
        return;
    }
    const QDir dir(runtimeDir + QStringLiteral("/plasma-projector-preview"));
    if (!dir.mkpath(QStringLiteral("."))) {
        qWarning() << "CursorTracker: cannot create" << dir.path();
        return;
    }
    unloadStaleScripts(dir);

    const QDBusConnection bus = QDBusConnection::sessionBus();
    const qint64 pid = QCoreApplication::applicationPid();
    m_scriptPath = dir.filePath(QStringLiteral("cursor-%1.js").arg(pid));
    // Our unique bus name, so that several previews each get their own updates.
    const QString script = QStringLiteral(
                               "// Loaded by plasma-projector-preview (pid %1): sends the pointer position to it.\n"
                               "function send() {\n"
                               "    const p = workspace.cursorPos;\n"
                               "    callDBus(\"%2\", \"%3\", \"%4\", \"setPosition\", p.x, p.y);\n"
                               "}\n"
                               "workspace.cursorPosChanged.connect(send);\n"
                               "send();\n")
                               .arg(QString::number(pid), bus.baseService(), objectPath, interfaceName);
    QFile file(m_scriptPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate) || file.write(script.toUtf8()) < 0) {
        qWarning() << "CursorTracker: cannot write" << m_scriptPath << file.errorString();
        m_scriptPath.clear();
        return;
    }
    file.close();

    // loadScript returns the script's id (-1 on failure); it starts once run() is called on
    // /Scripting/Script<id>.
    m_pluginName = pluginName(pid);
    auto *watcher = new QDBusPendingCallWatcher(bus.asyncCall(scriptingCall(QStringLiteral("loadScript"), {m_scriptPath, m_pluginName})), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *watcher) {
        watcher->deleteLater();
        const QDBusPendingReply<int> reply = *watcher;
        if (reply.isError() || reply.value() < 0) {
            qWarning() << "CursorTracker: KWin did not load" << m_scriptPath << reply.error().message();
            return;
        }
        const QString path = scriptingPath + QStringLiteral("/Script") + QString::number(reply.value());
        QDBusConnection::sessionBus().asyncCall(QDBusMessage::createMethodCall(kwinService, path, QStringLiteral("org.kde.kwin.Script"), QStringLiteral("run")));
    });
}

void CursorTracker::unloadScript()
{
    if (!m_pluginName.isEmpty()) {
        // Blocking (briefly), so that the call is out before the process exits.
        QDBusConnection::sessionBus().call(scriptingCall(QStringLiteral("unloadScript"), {m_pluginName}), QDBus::Block, 1000);
        m_pluginName.clear();
    }
    if (!m_scriptPath.isEmpty()) {
        QFile::remove(m_scriptPath);
        m_scriptPath.clear();
    }
}

#include "moc_cursortracker.cpp"
