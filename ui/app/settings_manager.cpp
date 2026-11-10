/**
 * @file ui/app/settings_manager.cpp
 * @brief Hiện thực lớp SettingsManager đọc/ghi cấu hình tập trung qua
 * QSettings.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#include "settings_manager.h"
#include "core/core_bridge.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonObject>
#include <QStandardPaths>
#include <QTextStream>

namespace svm::app {

SettingsManager &SettingsManager::instance() {
  static SettingsManager s_instance;
  return s_instance;
}

SettingsManager::SettingsManager(QObject *parent)
    : QObject(parent), m_settings(QSettings::IniFormat, QSettings::UserScope,
                                  QStringLiteral("Hephaestus"),
                                  QStringLiteral("SecureVaultManager")) {
  connect(this, &SettingsManager::settingsChanged, this, [this]() {
    m_settings.sync();
    auto &bridge = core::CoreBridge::instance();
    if (!bridge.isInitialized()) {
      return;
    }
    QJsonObject obj;
    obj.insert(QStringLiteral("auto_lock_minutes"), autoLockMinutes());
    obj.insert(QStringLiteral("clipboard_clear_seconds"),
               clipboardClearSeconds());
    obj.insert(QStringLiteral("clear_clipboard_on_lock"),
               clearClipboardOnLock());
    obj.insert(QStringLiteral("biometric_enabled"), biometricEnabled());
    obj.insert(QStringLiteral("browser_bridge_enabled"),
               browserBridgeEnabled());
    obj.insert(QStringLiteral("ssh_agent_enabled"), sshAgentEnabled());
    obj.insert(QStringLiteral("ssh_prompt_approval"), sshPromptApproval());
    obj.insert(QStringLiteral("auto_empty_trash"), autoEmptyTrashDaysEnabled());
    obj.insert(QStringLiteral("favicon_fetch_enabled"), faviconFetchEnabled());
    obj.insert(QStringLiteral("theme"), themeMode() == ThemeMode::Light
                                            ? QStringLiteral("light")
                                            : QStringLiteral("dark"));
    obj.insert(QStringLiteral("language"), language() == AppLanguage::Vietnamese
                                               ? QStringLiteral("vi")
                                               : QStringLiteral("en"));
    bridge.saveCoreSettings(obj);
  });
}

ThemeMode SettingsManager::themeMode() const {
  return ThemeManager::instance().currentMode();
}

void SettingsManager::setThemeMode(ThemeMode mode) {
  if (ThemeManager::instance().currentMode() != mode) {
    m_settings.setValue(QStringLiteral("general/theme"),
                        static_cast<int>(mode));
    ThemeManager::instance().applyTheme(mode);
    emit settingsChanged();
  }
}

AppLanguage SettingsManager::language() const {
  return LanguageManager::instance().currentLanguage();
}

void SettingsManager::setLanguage(AppLanguage lang) {
  if (LanguageManager::instance().currentLanguage() != lang) {
    m_settings.setValue(QStringLiteral("general/language"),
                        static_cast<int>(lang));
    LanguageManager::instance().setLanguage(lang);
    emit settingsChanged();
  }
}

ListDensity SettingsManager::listDensity() const {
  const int val = m_settings
                      .value(QStringLiteral("general/list_density"),
                             static_cast<int>(ListDensity::Comfortable))
                      .toInt();
  return (val == static_cast<int>(ListDensity::Compact))
             ? ListDensity::Compact
             : ListDensity::Comfortable;
}

void SettingsManager::setListDensity(ListDensity density) {
  if (listDensity() != density) {
    m_settings.setValue(QStringLiteral("general/list_density"),
                        static_cast<int>(density));
    emit listDensityChanged(density);
    emit settingsChanged();
  }
}

bool SettingsManager::launchOnStartup() const {
  return m_settings.value(QStringLiteral("general/launch_on_startup"), false)
      .toBool();
}

void SettingsManager::setLaunchOnStartup(bool enable) {
  m_settings.setValue(QStringLiteral("general/launch_on_startup"), enable);

  const QString appPath = QCoreApplication::applicationFilePath();
#if defined(Q_OS_LINUX)
  const QString autoStartDir =
      QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) +
      QStringLiteral("/autostart");
  const QString desktopPath =
      autoStartDir + QStringLiteral("/secure-vault-manager.desktop");
  if (enable) {
    QDir().mkpath(autoStartDir);
    QFile f(desktopPath);
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
      QTextStream out(&f);
      out << "[Desktop Entry]\n"
          << "Type=Application\n"
          << "Name=Secure Vault Manager\n"
          << "Comment=Zero-Knowledge Password & Secret Manager\n"
          << "Exec=\"" << appPath << "\" --minimized\n"
          << "Terminal=false\n"
          << "X-GNOME-Autostart-enabled=true\n";
    }
  } else {
    QFile::remove(desktopPath);
  }
#elif defined(Q_OS_WIN)
  QSettings runReg(
      QStringLiteral("HKEY_CURRENT_"
                     "USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run"),
      QSettings::NativeFormat);
  if (enable) {
    runReg.setValue(QStringLiteral("SecureVaultManager"),
                    QStringLiteral("\"%1\" --minimized").arg(appPath));
  } else {
    runReg.remove(QStringLiteral("SecureVaultManager"));
  }
#elif defined(Q_OS_MAC)
  const QString agentsDir =
      QDir::homePath() + QStringLiteral("/Library/LaunchAgents");
  const QString plistPath =
      agentsDir + QStringLiteral("/io.hephaestus.secure-vault-manager.plist");
  if (enable) {
    QDir().mkpath(agentsDir);
    QFile f(plistPath);
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
      QTextStream out(&f);
      out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
          << "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" "
             "\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
          << "<plist version=\"1.0\"><dict>\n"
          << "<key>Label</key><string>io.hephaestus.secure-vault-manager</"
             "string>\n"
          << "<key>ProgramArguments</key><array><string>" << appPath
          << "</string><string>--minimized</string></array>\n"
          << "<key>RunAtLoad</key><true/>\n"
          << "</dict></plist>\n";
    }
  } else {
    QFile::remove(plistPath);
  }
#endif
  emit settingsChanged();
}

bool SettingsManager::closeToTray() const {
  return m_settings.value(QStringLiteral("general/close_to_tray"), true)
      .toBool();
}

void SettingsManager::setCloseToTray(bool enable) {
  m_settings.setValue(QStringLiteral("general/close_to_tray"), enable);
  emit settingsChanged();
}

bool SettingsManager::startMinimized() const {
  return m_settings.value(QStringLiteral("general/start_minimized"), false)
      .toBool();
}

void SettingsManager::setStartMinimized(bool enable) {
  m_settings.setValue(QStringLiteral("general/start_minimized"), enable);
  emit settingsChanged();
}

bool SettingsManager::faviconFetchEnabled() const {
  return m_settings
      .value(QStringLiteral("general/favicon_fetch_enabled"), false)
      .toBool();
}

bool SettingsManager::hasFaviconFetchSetting() const {
  return m_settings.contains(QStringLiteral("general/favicon_fetch_enabled"));
}

void SettingsManager::setFaviconFetchEnabled(bool enable) {
  m_settings.setValue(QStringLiteral("general/favicon_fetch_enabled"), enable);
  m_settings.sync();
  qInfo() << "[SettingsManager] favicon_fetch_enabled =" << enable
          << "file:" << m_settings.fileName()
          << "status:" << m_settings.status();
  emit settingsChanged();
}

int SettingsManager::autoLockMinutes() const {
  return m_settings.value(QStringLiteral("security/auto_lock_minutes"), 15)
      .toInt();
}

void SettingsManager::setAutoLockMinutes(int minutes) {
  if (autoLockMinutes() != minutes) {
    m_settings.setValue(QStringLiteral("security/auto_lock_minutes"), minutes);
    emit autoLockTimeoutChanged(minutes);
    emit settingsChanged();
  }
}

bool SettingsManager::lockOnSleep() const {
  return m_settings.value(QStringLiteral("security/lock_on_sleep"), true)
      .toBool();
}

void SettingsManager::setLockOnSleep(bool enable) {
  m_settings.setValue(QStringLiteral("security/lock_on_sleep"), enable);
  emit settingsChanged();
}

int SettingsManager::clipboardClearSeconds() const {
  return m_settings
      .value(QStringLiteral("security/clipboard_clear_seconds"), 30)
      .toInt();
}

void SettingsManager::setClipboardClearSeconds(int seconds) {
  if (clipboardClearSeconds() != seconds) {
    m_settings.setValue(QStringLiteral("security/clipboard_clear_seconds"),
                        seconds);
    emit clipboardTimeoutChanged(seconds);
    emit settingsChanged();
  }
}

bool SettingsManager::clearClipboardOnLock() const {
  return m_settings
      .value(QStringLiteral("security/clear_clipboard_on_lock"), true)
      .toBool();
}

void SettingsManager::setClearClipboardOnLock(bool enable) {
  m_settings.setValue(QStringLiteral("security/clear_clipboard_on_lock"),
                      enable);
  emit settingsChanged();
}

bool SettingsManager::biometricEnabled() const {
  return m_settings.value(QStringLiteral("security/biometric_enabled"), false)
      .toBool();
}

void SettingsManager::setBiometricEnabled(bool enable) {
  m_settings.setValue(QStringLiteral("security/biometric_enabled"), enable);
  emit settingsChanged();
}

bool SettingsManager::browserBridgeEnabled() const {
  return m_settings.value(QStringLiteral("browser/bridge_enabled"), true)
      .toBool();
}

void SettingsManager::setBrowserBridgeEnabled(bool enable) {
  m_settings.setValue(QStringLiteral("browser/bridge_enabled"), enable);
  emit settingsChanged();
}

bool SettingsManager::sshAgentEnabled() const {
  return m_settings.value(QStringLiteral("ssh/agent_enabled"), false).toBool();
}

void SettingsManager::setSshAgentEnabled(bool enable) {
  m_settings.setValue(QStringLiteral("ssh/agent_enabled"), enable);
  emit settingsChanged();
}

bool SettingsManager::sshPromptApproval() const {
  return m_settings.value(QStringLiteral("ssh/prompt_approval"), true).toBool();
}

void SettingsManager::setSshPromptApproval(bool enable) {
  m_settings.setValue(QStringLiteral("ssh/prompt_approval"), enable);
  emit settingsChanged();
}

bool SettingsManager::autoEmptyTrashDaysEnabled() const {
  return m_settings.value(QStringLiteral("data/auto_empty_trash"), true)
      .toBool();
}

void SettingsManager::setAutoEmptyTrashDaysEnabled(bool enable) {
  m_settings.setValue(QStringLiteral("data/auto_empty_trash"), enable);
  emit settingsChanged();
}

} // namespace svm::app
