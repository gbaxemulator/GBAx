/* Copyright (c) 2026 GBAx contributors
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. See http://mozilla.org/MPL/2.0/. */
#pragma once

#include <QLocalSocket>
#include <QTimer>
#include <functional>
#include <memory>

namespace QGBA {

// Never wait for another process on the GUI thread. The owner bounds lifetime.
inline void sendLauncherCommand(QObject* owner, const QString& server, const QByteArray& command,
	std::function<void()> failed = {}) {
	auto* socket = new QLocalSocket(owner);
	const auto done = std::make_shared<bool>(false);
	auto finish = [socket, done, failed](bool success) {
		if (*done) return;
		*done = true;
		socket->abort();
		socket->deleteLater();
		if (!success && failed) failed();
	};
	QObject::connect(socket, &QLocalSocket::connected, socket, [socket, command, finish]() {
		if (socket->write(command) != command.size()) finish(false);
	});
	QObject::connect(socket, &QLocalSocket::bytesWritten, socket, [socket, finish](qint64) {
		if (socket->bytesToWrite() == 0) finish(true);
	});
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
	QObject::connect(socket, &QLocalSocket::errorOccurred, socket, [finish](QLocalSocket::LocalSocketError) { finish(false); });
#else
	QObject::connect(socket, qOverload<QLocalSocket::LocalSocketError>(&QLocalSocket::error), socket, [finish](QLocalSocket::LocalSocketError) { finish(false); });
#endif
	QTimer::singleShot(1500, socket, [finish]() { finish(false); });
	socket->connectToServer(server, QIODevice::WriteOnly);
}

} // namespace QGBA
