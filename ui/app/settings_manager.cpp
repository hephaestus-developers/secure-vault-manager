/**
 * @file ui/app/settings_manager.cpp
 * @brief Hiện thực lớp SettingsManager quản lý cấu hình tập trung theo chuẩn
 * Single Source of Truth.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#include "settings_manager.h"
#include "core/core_bridge.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QStandardPaths>
#include <QTextStream>

namespace svm::app {

SettingsManager &SettingsManager::instance() {
  static SettingsManager s_instance;
  return s_instance;
}

SettingsManager::SettingsManager(QObject *parent) : QObject(parent) {
  loadDefaults();

  auto &bridge = core::CoreBridge::instance();
  connect(&bridge, &core::CoreBridge::coreSettingsChanged, this,
          &SettingsManager::onCoreSettingsUpdated);
}

void SettingsManager::loadDefaults() {
  m_cachedSettings.insert(QStringLiteral("version"), 1);
  m_cachedSettings.insert(QStringLiteral("theme"), QStringLiteral("dark"));
  m_cachedSettings.insert(QStringLiteral("language"), QStringLiteral("en"));
  m_cachedSettings.insert(QStringLiteral("list_density"),
                          QStringLiteral("comfortable"));
  m_cachedSettings.insert(QStringLiteral("auto_lock_minutes"), 15);
  m_cachedSettings.insert(QStringLiteral("clipboard_clear_seconds"), 30);
  m_cachedSettings.insert(QStringLiteral("clear_clipboard_on_lock"), true);
  m_cachedSettings.insert(QStringLiteral("lock_on_sleep"), true);
  m_cachedSettings.insert(QStringLiteral("close_to_tray"), true);
  m_cachedSettings.insert(QStringLiteral("launch_on_startup"), false);
  m_cachedSettings.insert(QStringLiteral("start_minimized"), false);
  m_cachedSettings.insert(QStringLiteral("biometric_enabled"), false);
  m_cachedSettings.insert(QStringLiteral("browser_bridge_enabled"), true);
  m_cachedSettings.insert(QStringLiteral("ssh_agent_enabled"), false);
  m_cachedSettings.insert(QStringLiteral("ssh_prompt_approval"), true);
  m_cachedSettings.insert(QStringLiteral("auto_empty_trash"), true);
  m_cachedSettings.insert(QStringLiteral("favicon_fetch_enabled"), false);

  QJsonObject sec;
  sec.insert(QStringLiteral("configured"), false);
  sec.insert(QStringLiteral("password_hash"), QString());
  sec.insert(QStringLiteral("password_salt"), QString());
  m_cachedSettings.insert(QStringLiteral("security"), sec);
  m_cachedSettings.insert(QStringLiteral("vaults"), QJsonArray());
}

void SettingsManager::loadFromCore() {
  auto &bridge = core::CoreBridge::instance();
  if (!bridge.isInitialized()) {
    return;
  }
  const QJsonObject coreObj = bridge.getCoreSettings();
  if (coreObj.isEmpty()) {
    return;
  }
  for (auto it = coreObj.constBegin(); it != coreObj.constEnd(); ++it) {
    m_cachedSettings.insert(it.key(), it.value());
  }
}

void SettingsManager::initialize() {
  if (m_initialized) {
    return;
  }
  loadFromCore();
  migrateLegacyIniIfNeeded();
  m_initialized = true;
}

void SettingsManager::migrateLegacyIniIfNeeded() {
  QSettings ini(QSettings::IniFormat, QSettings::UserScope,
                QStringLiteral("Hephaestus"),
                QStringLiteral("SecureVaultManager"));

  if (ini.value(QStringLiteral("Migration/UnifiedSettingsDone"), false)
          .toBool()) {
    return;
  }

  qInfo() << "[SettingsManager] Detected legacy INI settings. Starting "
             "one-time migration to settings.json..."
          << "fileName:" << ini.fileName() << "status:" << ini.status()
          << "keys:" << ini.allKeys();

  auto findIniVal = [&](const QStringList &candidates) -> QVariant {
    for (const QString &k : candidates) {
      if (ini.contains(k)) {
        return ini.value(k);
      }
    }
    return {};
  };

  bool changed = false;

  // 1. Theme
  const QVariant themeVal = findIniVal({QStringLiteral("general/theme"),
                                        QStringLiteral("General/theme"),
                                        QStringLiteral("%General/theme")});
  if (themeVal.isValid()) {
    const int val = themeVal.toInt();
    m_cachedSettings.insert(QStringLiteral("theme"),
                            (val == 1) ? QStringLiteral("light")
                                       : QStringLiteral("dark"));
    changed = true;
  }

  // 2. Language
  const QVariant langVal = findIniVal({QStringLiteral("general/language"),
                                       QStringLiteral("General/language"),
                                       QStringLiteral("%General/language")});
  if (langVal.isValid()) {
    const int val = langVal.toInt();
    m_cachedSettings.insert(QStringLiteral("language"),
                            (val == 1) ? QStringLiteral("vi")
                                       : QStringLiteral("en"));
    changed = true;
  }

  // 3. List Density
  const QVariant densityVal =
      findIniVal({QStringLiteral("general/list_density"),
                  QStringLiteral("General/list_density"),
                  QStringLiteral("%General/list_density")});
  if (densityVal.isValid()) {
    const int val = densityVal.toInt();
    m_cachedSettings.insert(QStringLiteral("list_density"),
                            (val == 1) ? QStringLiteral("compact")
                                       : QStringLiteral("comfortable"));
    changed = true;
  }

  // 4. Launch on Startup
  const QVariant startupVal =
      findIniVal({QStringLiteral("general/launch_on_startup"),
                  QStringLiteral("General/launch_on_startup")});
  if (startupVal.isValid()) {
    m_cachedSettings.insert(QStringLiteral("launch_on_startup"),
                            startupVal.toBool());
    changed = true;
  }

  // 5. Close to Tray
  const QVariant trayVal =
      findIniVal({QStringLiteral("general/close_to_tray"),
                  QStringLiteral("General/close_to_tray")});
  if (trayVal.isValid()) {
    m_cachedSettings.insert(QStringLiteral("close_to_tray"), trayVal.toBool());
    changed = true;
  }

  // 6. Start Minimized
  const QVariant minVal =
      findIniVal({QStringLiteral("general/start_minimized"),
                  QStringLiteral("General/start_minimized")});
  if (minVal.isValid()) {
    m_cachedSettings.insert(QStringLiteral("start_minimized"), minVal.toBool());
    changed = true;
  }

  // 7. Favicon Fetch
  const QVariant favVal =
      findIniVal({QStringLiteral("general/favicon_fetch_enabled"),
                  QStringLiteral("General/favicon_fetch_enabled")});
  if (favVal.isValid()) {
    m_cachedSettings.insert(QStringLiteral("favicon_fetch_enabled"),
                            favVal.toBool());
    changed = true;
  }

  // 8. Auto Lock Minutes
  const QVariant lockVal =
      findIniVal({QStringLiteral("security/auto_lock_minutes"),
                  QStringLiteral("Security/auto_lock_minutes")});
  if (lockVal.isValid()) {
    m_cachedSettings.insert(QStringLiteral("auto_lock_minutes"),
                            lockVal.toInt());
    changed = true;
  }

  // 9. Clipboard Clear Seconds
  const QVariant clipVal =
      findIniVal({QStringLiteral("security/clipboard_clear_seconds"),
                  QStringLiteral("Security/clipboard_clear_seconds")});
  if (clipVal.isValid()) {
    m_cachedSettings.insert(QStringLiteral("clipboard_clear_seconds"),
                            clipVal.toInt());
    changed = true;
  }

  // 10. Clear Clipboard on Lock
  const QVariant clipLockVal =
      findIniVal({QStringLiteral("security/clear_clipboard_on_lock"),
                  QStringLiteral("Security/clear_clipboard_on_lock")});
  if (clipLockVal.isValid()) {
    m_cachedSettings.insert(QStringLiteral("clear_clipboard_on_lock"),
                            clipLockVal.toBool());
    changed = true;
  }

  // 11. Lock on Sleep
  const QVariant sleepVal =
      findIniVal({QStringLiteral("security/lock_on_sleep"),
                  QStringLiteral("Security/lock_on_sleep")});
  if (sleepVal.isValid()) {
    m_cachedSettings.insert(QStringLiteral("lock_on_sleep"), sleepVal.toBool());
    changed = true;
  }

  // 12. Biometric Enabled
  const QVariant bioVal =
      findIniVal({QStringLiteral("security/biometric_enabled"),
                  QStringLiteral("Security/biometric_enabled"),
                  QStringLiteral("Security/BiometricEnabled")});
  if (bioVal.isValid()) {
    m_cachedSettings.insert(QStringLiteral("biometric_enabled"),
                            bioVal.toBool());
    changed = true;
  }

  // 13. Browser Bridge Enabled
  const QVariant bridgeVal =
      findIniVal({QStringLiteral("browser/bridge_enabled"),
                  QStringLiteral("Browser/bridge_enabled")});
  if (bridgeVal.isValid()) {
    m_cachedSettings.insert(QStringLiteral("browser_bridge_enabled"),
                            bridgeVal.toBool());
    changed = true;
  }

  // 14. SSH Agent Enabled
  const QVariant sshVal = findIniVal({QStringLiteral("ssh/agent_enabled"),
                                      QStringLiteral("Ssh/agent_enabled")});
  if (sshVal.isValid()) {
    m_cachedSettings.insert(QStringLiteral("ssh_agent_enabled"),
                            sshVal.toBool());
    changed = true;
  }

  // 15. SSH Prompt Approval
  const QVariant sshPromptVal =
      findIniVal({QStringLiteral("ssh/prompt_approval"),
                  QStringLiteral("Ssh/prompt_approval")});
  if (sshPromptVal.isValid()) {
    m_cachedSettings.insert(QStringLiteral("ssh_prompt_approval"),
                            sshPromptVal.toBool());
    changed = true;
  }

  // 16. Auto Empty Trash
  const QVariant trashVal =
      findIniVal({QStringLiteral("data/auto_empty_trash"),
                  QStringLiteral("Data/auto_empty_trash")});
  if (trashVal.isValid()) {
    m_cachedSettings.insert(QStringLiteral("auto_empty_trash"),
                            trashVal.toBool());
    changed = true;
  }

  // 17. Security Credentials
  QJsonObject sec =
      m_cachedSettings.value(QStringLiteral("security")).toObject();
  const QVariant confVal =
      findIniVal({QStringLiteral("Security/IsConfigured"),
                  QStringLiteral("security/is_configured")});
  if (confVal.isValid()) {
    sec.insert(QStringLiteral("configured"), confVal.toBool());
    changed = true;
  }
  const QVariant hashVal =
      findIniVal({QStringLiteral("Security/PasswordHash"),
                  QStringLiteral("security/password_hash")});
  if (hashVal.isValid()) {
    sec.insert(QStringLiteral("password_hash"), hashVal.toString());
    changed = true;
  }
  const QVariant saltVal =
      findIniVal({QStringLiteral("Security/PasswordSalt"),
                  QStringLiteral("security/password_salt")});
  if (saltVal.isValid()) {
    sec.insert(QStringLiteral("password_salt"), saltVal.toString());
    changed = true;
  }
  m_cachedSettings.insert(QStringLiteral("security"), sec);

  // 18. Vaults List JSON
  const QVariant vaultsVal = findIniVal(
      {QStringLiteral("Vaults/ListJson"), QStringLiteral("vaults/list_json")});
  if (vaultsVal.isValid()) {
    const QString jsonStr = vaultsVal.toString();
    if (!jsonStr.isEmpty()) {
      const QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8());
      if (doc.isArray()) {
        m_cachedSettings.insert(QStringLiteral("vaults"), doc.array());
        changed = true;
      }
    }
  }

  // Save migrated settings down to Rust Core
  saveToCore();

  // Clear all legacy keys from INI
  ini.clear();
  ini.setValue(QStringLiteral("Migration/UnifiedSettingsDone"), true);
  ini.sync();

  // Clean default QSettings if contaminated
  QSettings defaultSettings;
  if (defaultSettings.contains(QStringLiteral("General/Language"))) {
    defaultSettings.remove(QStringLiteral("General/Language"));
    defaultSettings.sync();
  }

  qInfo() << "[SettingsManager] Migration from legacy INI to settings.json "
             "completed successfully. Changed ="
          << changed;
}

void SettingsManager::saveToCore() {
  if (m_syncingFromCore) {
    return;
  }
  auto &bridge = core::CoreBridge::instance();
  if (bridge.isInitialized()) {
    bridge.saveCoreSettings(m_cachedSettings);
  }
}

void SettingsManager::onCoreSettingsUpdated(const QJsonObject &settings) {
  if (settings.isEmpty()) {
    return;
  }
  m_syncingFromCore = true;
  for (auto it = settings.constBegin(); it != settings.constEnd(); ++it) {
    m_cachedSettings.insert(it.key(), it.value());
  }
  m_syncingFromCore = false;

  emit settingsChanged();
  emit autoLockTimeoutChanged(autoLockMinutes());
  emit clipboardTimeoutChanged(clipboardClearSeconds());
  emit listDensityChanged(listDensity());
}

ThemeMode SettingsManager::themeMode() const {
  const QString val = m_cachedSettings.value(QStringLiteral("theme"))
                          .toString(QStringLiteral("dark"));
  return (val == QStringLiteral("light")) ? ThemeMode::Light : ThemeMode::Dark;
}

void SettingsManager::setThemeMode(ThemeMode mode) {
  const QString str = (mode == ThemeMode::Light) ? QStringLiteral("light")
                                                 : QStringLiteral("dark");
  if (m_cachedSettings.value(QStringLiteral("theme")).toString() != str) {
    m_cachedSettings.insert(QStringLiteral("theme"), str);
    saveToCore();
    ThemeManager::instance().applyTheme(mode);
    emit settingsChanged();
  }
}

AppLanguage SettingsManager::language() const {
  const QString val = m_cachedSettings.value(QStringLiteral("language"))
                          .toString(QStringLiteral("en"));
  return (val == QStringLiteral("vi")) ? AppLanguage::Vietnamese
                                       : AppLanguage::English;
}

void SettingsManager::setLanguage(AppLanguage lang) {
  const QString str = (lang == AppLanguage::Vietnamese) ? QStringLiteral("vi")
                                                        : QStringLiteral("en");
  if (m_cachedSettings.value(QStringLiteral("language")).toString() != str) {
    m_cachedSettings.insert(QStringLiteral("language"), str);
    saveToCore();
    LanguageManager::instance().setLanguage(lang);
    emit settingsChanged();
  }
}

ListDensity SettingsManager::listDensity() const {
  const QString val = m_cachedSettings.value(QStringLiteral("list_density"))
                          .toString(QStringLiteral("comfortable"));
  return (val == QStringLiteral("compact")) ? ListDensity::Compact
                                            : ListDensity::Comfortable;
}

void SettingsManager::setListDensity(ListDensity density) {
  if (listDensity() != density) {
    m_cachedSettings.insert(QStringLiteral("list_density"),
                            density == ListDensity::Compact
                                ? QStringLiteral("compact")
                                : QStringLiteral("comfortable"));
    saveToCore();
    emit listDensityChanged(density);
    emit settingsChanged();
  }
}

bool SettingsManager::launchOnStartup() const {
  return m_cachedSettings.value(QStringLiteral("launch_on_startup"))
      .toBool(false);
}

void SettingsManager::setLaunchOnStartup(bool enable) {
  m_cachedSettings.insert(QStringLiteral("launch_on_startup"), enable);
  saveToCore();

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
  return m_cachedSettings.value(QStringLiteral("close_to_tray")).toBool(true);
}

void SettingsManager::setCloseToTray(bool enable) {
  m_cachedSettings.insert(QStringLiteral("close_to_tray"), enable);
  saveToCore();
  emit settingsChanged();
}

bool SettingsManager::startMinimized() const {
  return m_cachedSettings.value(QStringLiteral("start_minimized"))
      .toBool(false);
}

void SettingsManager::setStartMinimized(bool enable) {
  m_cachedSettings.insert(QStringLiteral("start_minimized"), enable);
  saveToCore();
  emit settingsChanged();
}

bool SettingsManager::faviconFetchEnabled() const {
  return m_cachedSettings.value(QStringLiteral("favicon_fetch_enabled"))
      .toBool(false);
}

bool SettingsManager::hasFaviconFetchSetting() const {
  return m_cachedSettings.contains(QStringLiteral("favicon_fetch_enabled"));
}

void SettingsManager::setFaviconFetchEnabled(bool enable) {
  m_cachedSettings.insert(QStringLiteral("favicon_fetch_enabled"), enable);
  saveToCore();
  qInfo() << "[SettingsManager] favicon_fetch_enabled =" << enable;
  emit settingsChanged();
}

int SettingsManager::autoLockMinutes() const {
  return m_cachedSettings.value(QStringLiteral("auto_lock_minutes")).toInt(15);
}

void SettingsManager::setAutoLockMinutes(int minutes) {
  if (autoLockMinutes() != minutes) {
    m_cachedSettings.insert(QStringLiteral("auto_lock_minutes"), minutes);
    saveToCore();
    emit autoLockTimeoutChanged(minutes);
    emit settingsChanged();
  }
}

bool SettingsManager::lockOnSleep() const {
  return m_cachedSettings.value(QStringLiteral("lock_on_sleep")).toBool(true);
}

void SettingsManager::setLockOnSleep(bool enable) {
  m_cachedSettings.insert(QStringLiteral("lock_on_sleep"), enable);
  saveToCore();
  emit settingsChanged();
}

int SettingsManager::clipboardClearSeconds() const {
  return m_cachedSettings.value(QStringLiteral("clipboard_clear_seconds"))
      .toInt(30);
}

void SettingsManager::setClipboardClearSeconds(int seconds) {
  if (clipboardClearSeconds() != seconds) {
    m_cachedSettings.insert(QStringLiteral("clipboard_clear_seconds"), seconds);
    saveToCore();
    emit clipboardTimeoutChanged(seconds);
    emit settingsChanged();
  }
}

bool SettingsManager::clearClipboardOnLock() const {
  return m_cachedSettings.value(QStringLiteral("clear_clipboard_on_lock"))
      .toBool(true);
}

void SettingsManager::setClearClipboardOnLock(bool enable) {
  m_cachedSettings.insert(QStringLiteral("clear_clipboard_on_lock"), enable);
  saveToCore();
  emit settingsChanged();
}

bool SettingsManager::biometricEnabled() const {
  return m_cachedSettings.value(QStringLiteral("biometric_enabled"))
      .toBool(false);
}

void SettingsManager::setBiometricEnabled(bool enable) {
  m_cachedSettings.insert(QStringLiteral("biometric_enabled"), enable);
  saveToCore();
  emit settingsChanged();
}

bool SettingsManager::browserBridgeEnabled() const {
  return m_cachedSettings.value(QStringLiteral("browser_bridge_enabled"))
      .toBool(true);
}

void SettingsManager::setBrowserBridgeEnabled(bool enable) {
  m_cachedSettings.insert(QStringLiteral("browser_bridge_enabled"), enable);
  saveToCore();
  emit settingsChanged();
}

bool SettingsManager::sshAgentEnabled() const {
  return m_cachedSettings.value(QStringLiteral("ssh_agent_enabled"))
      .toBool(false);
}

void SettingsManager::setSshAgentEnabled(bool enable) {
  m_cachedSettings.insert(QStringLiteral("ssh_agent_enabled"), enable);
  saveToCore();
  emit settingsChanged();
}

bool SettingsManager::sshPromptApproval() const {
  return m_cachedSettings.value(QStringLiteral("ssh_prompt_approval"))
      .toBool(true);
}

void SettingsManager::setSshPromptApproval(bool enable) {
  m_cachedSettings.insert(QStringLiteral("ssh_prompt_approval"), enable);
  saveToCore();
  emit settingsChanged();
}

bool SettingsManager::autoEmptyTrashDaysEnabled() const {
  return m_cachedSettings.value(QStringLiteral("auto_empty_trash"))
      .toBool(true);
}

void SettingsManager::setAutoEmptyTrashDaysEnabled(bool enable) {
  m_cachedSettings.insert(QStringLiteral("auto_empty_trash"), enable);
  saveToCore();
  emit settingsChanged();
}

bool SettingsManager::isConfigured() const {
  const QJsonObject sec =
      m_cachedSettings.value(QStringLiteral("security")).toObject();
  return sec.value(QStringLiteral("configured")).toBool(false);
}

void SettingsManager::setConfigured(bool configured) {
  QJsonObject sec =
      m_cachedSettings.value(QStringLiteral("security")).toObject();
  sec.insert(QStringLiteral("configured"), configured);
  m_cachedSettings.insert(QStringLiteral("security"), sec);
  saveToCore();
  emit settingsChanged();
}

QJsonArray SettingsManager::cachedVaults() const {
  return m_cachedSettings.value(QStringLiteral("vaults")).toArray();
}

void SettingsManager::setCachedVaults(const QJsonArray &vaults) {
  m_cachedSettings.insert(QStringLiteral("vaults"), vaults);
  saveToCore();
}

void SettingsManager::resetSecurityAndVaults() {
  QJsonObject sec;
  sec.insert(QStringLiteral("configured"), false);
  m_cachedSettings.insert(QStringLiteral("security"), sec);
  m_cachedSettings.insert(QStringLiteral("vaults"), QJsonArray());
  m_cachedSettings.insert(QStringLiteral("biometric_enabled"), false);
  saveToCore();
  emit settingsChanged();
}

} // namespace svm::app
