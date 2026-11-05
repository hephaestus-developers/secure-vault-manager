/**
 * @file ui/app/tray_manager.h
 * @brief Quản lý khay hệ thống (System Tray Icon).
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#pragma once

#include <QMenu>
#include <QObject>
#include <QSystemTrayIcon>

namespace svm {
class MainWindow;
} // namespace svm

namespace svm::app {

class TrayManager : public QObject {
  Q_OBJECT

public:
  explicit TrayManager(MainWindow *mainWindow, QObject *parent = nullptr);
  ~TrayManager() override = default;

  bool initialize();
  bool isSystemTrayAvailable() const;

  void showNotification(
      const QString &title, const QString &message,
      QSystemTrayIcon::MessageIcon icon = QSystemTrayIcon::Information);

  void setVaultLocked(bool locked);
  bool isVaultLocked() const { return m_isVaultLocked; }

public slots:
  void toggleMainWindow();
  void toggleVaultLock();

private slots:
  void onTrayActivated(QSystemTrayIcon::ActivationReason reason);
  void onThemeChanged();

private:
  void setupContextMenu();
  void updateTrayTooltipAndIcon();

  MainWindow *m_mainWindow{nullptr};
  QSystemTrayIcon *m_trayIcon{nullptr};
  QMenu *m_trayMenu{nullptr};

  QAction *m_openAction{nullptr};
  QAction *m_lockAction{nullptr};
  QAction *m_themeAction{nullptr};
  QAction *m_quitAction{nullptr};

  bool m_isVaultLocked{false};
};

} // namespace svm::app
