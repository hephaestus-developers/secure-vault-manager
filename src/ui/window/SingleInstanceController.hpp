/**
 * @file SingleInstanceController.hpp
 * @brief IPC controller ensuring single application instance and forwarding
 * activation requests.
 */

#pragma once

#include <QApplication>
#include <QLocalServer>
#include <QLocalSocket>
#include <QObject>
#include <QString>

/**
 * @class SingleInstanceController
 * @brief Enforces single running instance of Secure Vault Manager using
 * QLocalServer / QLocalSocket IPC.
 *
 * If an existing instance is running, forwards activation commands and
 * gracefully exits. If primary, listens for secondary activation signals to
 * raise the active window.
 */
class SingleInstanceController : public QObject {
  Q_OBJECT

public:
  /**
   * @brief Constructs the single instance controller.
   * @param parent Optional QObject parent.
   */
  explicit SingleInstanceController(QObject *parent = nullptr);

  /**
   * @brief Destructor. Closes server and unlinks socket.
   */
  ~SingleInstanceController() override;

  /**
   * @brief Checks if another instance is running; if not, starts listening as
   * primary.
   * @param app Application instance.
   * @param appKey Unique identifier prefix for IPC socket.
   * @return True if this is the primary instance, false if a secondary instance
   * was detected and notified.
   */
  bool checkOrStart(QApplication *app,
                    const QString &appKey = "SecureVaultManagerSingleInstance");

private slots:
  void onNewConnection();

private:
  QString generateServerName(const QString &appKey) const;

  QLocalServer *m_server{nullptr};
  QString m_serverName;
};
