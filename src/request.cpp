/*
 * SPDX-FileCopyrightText: 2016 Red Hat Inc
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 *
 * SPDX-FileCopyrightText: 2016 Jan Grulich <jgrulich@redhat.com>
 */

#include "desktopportal.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(XdgRequestKdeRequest, "xdp-kde-request")

Request::Request(QObject *parent, const QString &portalName, const QVariant &data)
    : QDBusVirtualObject(parent)
    , m_data(data)
    , m_portalName(portalName)
{
}

Request::~Request()
{
}

bool Request::handleMessage(const QDBusMessage &message, const QDBusConnection &connection)
{
    Q_UNUSED(connection);

    /* Check to make sure we're getting properties on our interface */
    if (message.type() != QDBusMessage::MessageType::MethodCallMessage) {
        return false;
    }

    qCDebug(XdgRequestKdeRequest) << message.interface();
    qCDebug(XdgRequestKdeRequest) << message.member();
    qCDebug(XdgRequestKdeRequest) << message.path();

    if (message.interface() == QLatin1String("org.freedesktop.impl.portal.Request")) {
        if (message.member() == QLatin1String("Close")) {
            if (m_portalName == QLatin1String("org.freedesktop.impl.portal.Inhibit")) {
                QDBusMessage uninhibitMessage = QDBusMessage::createMethodCall(QStringLiteral("org.kde.Solid.PowerManagement"),
                                                                               QStringLiteral("/org/kde/Solid/PowerManagement/PolicyAgent"),
                                                                               QStringLiteral("org.kde.Solid.PowerManagement.PolicyAgent"),
                                                                               QStringLiteral("ReleaseInhibition"));

                uninhibitMessage << m_data.toUInt();

                QDBusPendingCall pendingCall = QDBusConnection::sessionBus().asyncCall(uninhibitMessage);
                QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(pendingCall);
                connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, message](QDBusPendingCallWatcher *watcher) {
                    QDBusPendingReply<> reply = *watcher;
                    QDBusMessage messageReply;
                    if (reply.isError()) {
                        qCDebug(XdgRequestKdeRequest) << "Uninhibit error: " << reply.error().message();
                        messageReply = message.createErrorReply(reply.error());
                    } else {
                        messageReply = message.createReply();
                        Q_EMIT closeRequested();
                    }

                    QDBusConnection::sessionBus().asyncCall(messageReply);
                });
            }
        }
    }

    return true;
}

QString Request::introspect(const QString &path) const
{
    QString nodes;

    if (path.startsWith(QLatin1String("/org/freedesktop/portal/desktop/request/"))) {
        nodes = QStringLiteral(
            "<interface name=\"org.freedesktop.impl.portal.Request\">"
            "    <method name=\"Close\">"
            "    </method>"
            "</interface>");
    }

    qCDebug(XdgRequestKdeRequest) << nodes;

    return nodes;
}
