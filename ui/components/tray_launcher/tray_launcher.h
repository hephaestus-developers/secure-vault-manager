/**
 * @file ui/components/tray_launcher/tray_launcher.h
 * @brief Cửa sổ Quick Launcher nổi bật cho khay hệ thống (Tray Launcher).
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#pragma once

#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>

namespace svm::components {

enum class LauncherActionId {
  OpenMainWindow,
  ToggleVaultLock,
  SearchCredentials,
  NewCredential,
  PasswordGenerator,
  ToggleTheme,
  QuitApplication
};

class TrayLauncher : public QDialog {
  Q_OBJECT

public:
  explicit TrayLauncher(QWidget *parent = nullptr);
  ~TrayLauncher() override = default;

  void showNearTray(const QRect &trayGeometry);
  void showCentered();
  void setVaultLocked(bool locked);
  bool isVaultLocked() const { return m_isVaultLocked; }

signals:
  void openMainWindowRequested();
  void toggleVaultLockRequested();
  void searchCredentialsRequested();
  void newCredentialRequested();
  void passwordGeneratorRequested();
  void toggleThemeRequested();
  void quitRequested();

protected:
  void changeEvent(QEvent *event) override;
  void keyPressEvent(QKeyEvent *event) override;
  bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
  void onSearchTextChanged(const QString &text);
  void onItemDoubleClicked(QListWidgetItem *item);
  void executeCurrentAction();

private:
  void setupUI();
  void populateActions();
  void addActionItem(LauncherActionId id, const QString &iconPath,
                     const QString &title, const QString &subtitle,
                     const QString &shortcut);
  void updateStatusBadge();

  QWidget *m_containerWidget{nullptr};
  QLabel *m_statusBadge{nullptr};
  QLineEdit *m_searchEdit{nullptr};
  QListWidget *m_listWidget{nullptr};

  bool m_isVaultLocked{false};
};

} // namespace svm::components
