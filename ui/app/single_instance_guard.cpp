/**
 * @file ui/app/single_instance_guard.cpp
 * @brief Hiện thực cơ chế Single Instance qua QLockFile và QLocalServer.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#include "single_instance_guard.h"

#include <QCryptographicHash>
#include <QDebug>
#include <QDir>
#include <QLocalSocket>
#include <QStandardPaths>
#include <QThread>

namespace svm::app {

namespace {

QString resolveUserRuntimeDir() {
  QString dir =
      QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
  if (dir.isEmpty()) {
    dir = QDir::tempPath();
  }
  QDir().mkpath(dir);
  return dir;
}

QString buildScopedInstanceId(const QString &appKey) {
  QString user = qEnvironmentVariable("USER");
  if (user.isEmpty()) {
    user = qEnvironmentVariable("USERNAME");
  }
  const QByteArray digest =
      QCryptographicHash::hash((appKey + QLatin1Char(':') + user).toUtf8(),
                               QCryptographicHash::Sha256)
          .toHex();
  return QStringLiteral("svm-") + QString::fromLatin1(digest.left(16));
}

} // namespace

SingleInstanceGuard::SingleInstanceGuard(const QString &appKey, QObject *parent)
    : QObject(parent) {
  const QString scopedId = buildScopedInstanceId(appKey);
  const QString runtimeDir = resolveUserRuntimeDir();
  m_serverName = scopedId;
  m_lockFilePath =
      QDir(runtimeDir).filePath(scopedId + QStringLiteral(".lock"));
}

SingleInstanceGuard::~SingleInstanceGuard() {
  if (m_server) {
    m_server->close();
    QLocalServer::removeServer(m_serverName);
    m_server = nullptr;
  }
  if (m_lockFile && m_lockFile->isLocked()) {
    m_lockFile->unlock();
  }
}

bool SingleInstanceGuard::tryAcquire(const QString &command) {
  m_lockFile = std::make_unique<QLockFile>(m_lockFilePath);
  // Stale time = 0: chỉ coi khóa là cũ nếu tiến trình tạo ra nó đã chết.
  m_lockFile->setStaleLockTime(0);

  if (!m_lockFile->tryLock(100)) {
    QLocalSocket socket;
    socket.connectToServer(m_serverName, QIODevice::WriteOnly);
    if (socket.waitForConnected(1000)) {
      const QByteArray payload = command.toUtf8() + '\n';
      socket.write(payload);
      socket.flush();
      socket.waitForBytesWritten(500);
      socket.disconnectFromServer();
      return false;
    }

    // Trường hợp hiếm gặp: tiến trình cũ vừa thoát ngay lúc kiểm tra.
    QThread::msleep(120);
    if (!m_lockFile->tryLock(100)) {
      return false;
    }
  }

  QLocalServer::removeServer(m_serverName);
  m_server = new QLocalServer(this);
  m_server->setSocketOptions(QLocalServer::UserAccessOption);

  if (!m_server->listen(m_serverName)) {
    qWarning() << "[SingleInstance] Không thể lắng nghe QLocalServer:"
               << m_server->errorString();
  } else {
    connect(m_server, &QLocalServer::newConnection, this,
            &SingleInstanceGuard::onNewConnection);
  }

  return true;
}

void SingleInstanceGuard::onNewConnection() {
  if (!m_server) {
    return;
  }

  while (m_server->hasPendingConnections()) {
    QLocalSocket *client = m_server->nextPendingConnection();
    if (!client) {
      continue;
    }
    if (client->bytesAvailable() == 0) {
      client->waitForReadyRead(200);
    }
    const QString raw = QString::fromUtf8(client->readAll()).trimmed();
    const QString cmd = raw.isEmpty() ? QStringLiteral("SHOW") : raw;
    client->disconnectFromServer();
    client->deleteLater();
    emit activateRequested(cmd);
  }
}

} // namespace svm::app
