/**
 * @file SingleInstanceController.cpp
 * @brief Implementation of SingleInstanceController.
 */

#include "SingleInstanceController.hpp"

#include "WindowManager.hpp"

#include <QCryptographicHash>
#include <QDir>

SingleInstanceController::SingleInstanceController(QObject *parent)
    : QObject(parent) {}

SingleInstanceController::~SingleInstanceController() {
  if (m_server) {
    m_server->close();
    QLocalServer::removeServer(m_serverName);
  }
}

QString
SingleInstanceController::generateServerName(const QString &appKey) const {
  const QString userIdentifier = QDir::homePath();
  const QByteArray hash = QCryptographicHash::hash(userIdentifier.toUtf8(),
                                                   QCryptographicHash::Sha1)
                              .toHex();
  return QString("%1_%2").arg(appKey, QString::fromLatin1(hash.left(12)));
}

bool SingleInstanceController::checkOrStart(QApplication *app,
                                            const QString &appKey) {
  Q_UNUSED(app);
  m_serverName = generateServerName(appKey);

  // Try connecting to existing primary instance
  QLocalSocket socket;
  socket.connectToServer(m_serverName);
  if (socket.waitForConnected(400)) {
    // Another instance is running, send activation request
    socket.write("ACTIVATE\n");
    socket.waitForBytesWritten(500);
    socket.disconnectFromServer();
    return false; // Exit secondary instance
  }

  // No existing instance responding, clear stale socket file if any
  QLocalServer::removeServer(m_serverName);

  m_server = new QLocalServer(this);
  connect(m_server, &QLocalServer::newConnection, this,
          &SingleInstanceController::onNewConnection);

  if (!m_server->listen(m_serverName)) {
    // Fallback: cleanup and retry listen once
    QLocalServer::removeServer(m_serverName);
    m_server->listen(m_serverName);
  }

  return true; // Primary instance
}

void SingleInstanceController::onNewConnection() {
  if (!m_server) {
    return;
  }

  while (QLocalSocket *clientSocket = m_server->nextPendingConnection()) {
    connect(clientSocket, &QLocalSocket::readyRead, this, [clientSocket]() {
      const QByteArray data = clientSocket->readAll();
      if (data.contains("ACTIVATE")) {
        WindowManager::instance().activateLeafOrRoot();
      }
      clientSocket->disconnectFromServer();
    });

    connect(clientSocket, &QLocalSocket::disconnected, clientSocket,
            &QObject::deleteLater);
  }
}
