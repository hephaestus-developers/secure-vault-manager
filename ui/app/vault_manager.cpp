/**
 * @file ui/app/vault_manager.cpp
 * @brief Hiện thực lớp quản lý trạng thái vòng đời kho lưu trữ (VaultManager).
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#include "vault_manager.h"
#include "app/dev_logger.h"
#include "app/settings_manager.h"
#include "core/core_bridge.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRandomGenerator>
#include <QSettings>

namespace svm::app {

namespace {
QSettings openUnifiedSettings() {
  return QSettings(QSettings::IniFormat, QSettings::UserScope,
                   QStringLiteral("Hephaestus"),
                   QStringLiteral("SecureVaultManager"));
}
} // namespace

VaultManager &VaultManager::instance() {
  static VaultManager s_instance;
  return s_instance;
}

VaultManager::VaultManager() : QObject(nullptr) {}

void VaultManager::initialize() {
  loadSettings();

  auto &bridge = core::CoreBridge::instance();
  connect(&bridge, &core::CoreBridge::vaultLockedEvent, this, [this]() {
    if (!m_isLocked) {
      m_isLocked = true;
      m_recoveryKey.clear();
      emit lockStateChanged(m_isLocked);
    }
  });

  connect(&bridge, &core::CoreBridge::vaultUnlockedEvent, this, [this]() {
    if (m_isLocked) {
      m_isLocked = false;
      refreshVaultsFromCore();
      if (m_activeVaultId.isEmpty() && !m_vaults.isEmpty()) {
        m_activeVaultId = VaultManager::defaultVault().id;
        emit activeVaultChanged(m_activeVaultId);
      }
      emit lockStateChanged(m_isLocked);
    }
  });
}

bool VaultManager::isConfigured() const { return m_isConfigured; }

bool VaultManager::isLocked() const { return m_isLocked; }

QString VaultManager::activeVaultId() const { return m_activeVaultId; }

void VaultManager::setActiveVaultId(const QString &vaultId) {
  if (m_activeVaultId != vaultId) {
    m_activeVaultId = vaultId;
    emit activeVaultChanged(m_activeVaultId);
  }
}

QList<models::UiVault> VaultManager::vaults() const { return m_vaults; }

models::UiVault VaultManager::vault(const QString &vaultId) const {
  for (const auto &v : m_vaults) {
    if (v.id == vaultId) {
      return v;
    }
  }
  return {};
}

models::UiVault VaultManager::activeVault() const {
  return vault(m_activeVaultId);
}

models::UiVault VaultManager::defaultVault() const {
  for (const auto &v : m_vaults) {
    if (v.isDefault) {
      return v;
    }
  }
  return m_vaults.isEmpty() ? models::UiVault{} : m_vaults.first();
}

void VaultManager::refreshVaultsFromCore() {
  SVM_ACTION_SCOPE("VaultManager", "refreshVaultsFromCore");
  auto &bridge = core::CoreBridge::instance();
  if (!bridge.isInitialized() || !bridge.isVaultUnlocked()) {
    return;
  }
  bool coreConfigured = false;
  const QList<models::UiVault> coreVaults = bridge.fetchVaults(&coreConfigured);
  if (!coreVaults.isEmpty()) {
    m_vaults = coreVaults;
    saveSettings();
    emit vaultListChanged();
  }
}

bool VaultManager::createVault(const models::UiVault &vault) {
  for (const auto &v : m_vaults) {
    if (v.id == vault.id) {
      return false;
    }
  }
  models::UiVault newV = vault;
  if (newV.id.isEmpty()) {
    newV.id =
        QStringLiteral("vault_%1").arg(QDateTime::currentMSecsSinceEpoch());
  }

  auto &bridge = core::CoreBridge::instance();
  if (bridge.isInitialized() && bridge.isVaultUnlocked()) {
    QString createdId;
    if (bridge.createVault(newV, QString(), QString(), false, &createdId) &&
        !createdId.isEmpty()) {
      newV.id = createdId;
    }
  }

  m_vaults.append(newV);
  saveSettings();
  emit vaultCreated(newV);
  emit vaultListChanged();
  setActiveVaultId(newV.id);
  return true;
}

bool VaultManager::updateVault(const models::UiVault &vault) {
  for (int i = 0; i < m_vaults.size(); ++i) {
    if (m_vaults[i].id == vault.id) {
      m_vaults[i] = vault;
      auto &bridge = core::CoreBridge::instance();
      if (bridge.isInitialized() && bridge.isVaultUnlocked()) {
        bridge.updateVault(vault);
      }
      saveSettings();
      emit vaultListChanged();
      return true;
    }
  }
  return false;
}

bool VaultManager::deleteVault(const QString &vaultId) {
  for (int i = 0; i < m_vaults.size(); ++i) {
    if (m_vaults[i].id == vaultId) {
      if (m_vaults[i].isDefault) {
        return false; // Không cho xóa Vault mặc định
      }
      auto &bridge = core::CoreBridge::instance();
      if (bridge.isInitialized() && bridge.isVaultUnlocked()) {
        bridge.deleteVault(vaultId);
      }
      m_vaults.removeAt(i);
      if (m_activeVaultId == vaultId) {
        m_activeVaultId = VaultManager::defaultVault().id;
        emit activeVaultChanged(m_activeVaultId);
      }
      saveSettings();
      emit vaultListChanged();
      return true;
    }
  }
  return false;
}

bool VaultManager::resetAllData() {
  auto &bridge = core::CoreBridge::instance();
  if (bridge.isInitialized()) {
    bridge.resetStorage();
  }

  m_vaults.clear();
  m_activeVaultId.clear();
  m_passwordHash.clear();
  m_passwordSalt.clear();
  m_recoveryKey.clear();
  m_biometricEnabled = false;
  m_isConfigured = false;
  m_isLocked = false;
  SettingsManager::instance().setBiometricEnabled(false);

  QSettings settings = openUnifiedSettings();
  settings.remove(QStringLiteral("Security"));
  settings.remove(QStringLiteral("Vaults"));
  settings.sync();

  emit configuredChanged(false);
  emit lockStateChanged(false);
  emit vaultListChanged();
  emit activeVaultChanged(QString());

  return true;
}

bool VaultManager::setDefaultVault(const QString &vaultId) {
  bool changed = false;
  for (auto &v : m_vaults) {
    bool shouldBeDefault = (v.id == vaultId);
    if (v.isDefault != shouldBeDefault) {
      v.isDefault = shouldBeDefault;
      changed = true;
    }
  }
  if (changed) {
    auto &bridge = core::CoreBridge::instance();
    if (bridge.isInitialized() && bridge.isVaultUnlocked()) {
      bridge.setDefaultVault(vaultId);
    }
    saveSettings();
    emit vaultListChanged();
  }
  return changed;
}

bool VaultManager::setupInitialMasterPassword(const QString &password,
                                              const QString &recoveryKey,
                                              const QString &initialVaultName,
                                              bool enableBiometric) {
  SVM_ACTION_SCOPE("VaultManager", "setupInitialMasterPassword");
  if (password.trimmed().isEmpty()) {
    return false;
  }

  // Tạo salt ngẫu nhiên 16 bytes
  QByteArray saltBytes;
  saltBytes.resize(16);
  QRandomGenerator::system()->fillRange(
      reinterpret_cast<quint32 *>(saltBytes.data()),
      saltBytes.size() / sizeof(quint32));
  m_passwordSalt = QString::fromLatin1(saltBytes.toHex());
  m_passwordHash = hashPassword(password, m_passwordSalt);
  m_recoveryKey = recoveryKey.trimmed().toUpper();

  m_vaults.clear();
  models::UiVault defaultVault;
  defaultVault.id = QStringLiteral("personal");
  defaultVault.name = initialVaultName.trimmed().isEmpty()
                          ? QStringLiteral("Personal Vault")
                          : initialVaultName.trimmed();
  defaultVault.iconName = QStringLiteral("vault_personal");
  defaultVault.description =
      QStringLiteral("Primary personal credentials & logins");
  defaultVault.isDefault = true;
  defaultVault.createdAt = QDateTime::currentDateTime();
  m_vaults.append(defaultVault);

  m_biometricEnabled = false;
  // Khởi tạo kho mã hóa trong Rust Core
  auto &bridge = core::CoreBridge::instance();
  if (bridge.isInitialized()) {
    if (!bridge.createVault(defaultVault, password, m_recoveryKey, true)) {
      return false;
    }
    if (enableBiometric && bridge.isBiometricAvailable()) {
      m_biometricEnabled = bridge.enableBiometric(defaultVault.id, password);
    }
    const auto coreVaults = bridge.fetchVaults();
    if (!coreVaults.isEmpty()) {
      m_vaults = coreVaults;
    }
  }
  SettingsManager::instance().setBiometricEnabled(m_biometricEnabled);

  m_isConfigured = true;
  m_isLocked = false;
  m_activeVaultId = VaultManager::defaultVault().id;

  saveSettings();
  emit configuredChanged(m_isConfigured);
  emit lockStateChanged(m_isLocked);
  emit vaultListChanged();
  emit activeVaultChanged(m_activeVaultId);
  return true;
}

bool VaultManager::changeMasterPassword(const QString &oldPassword,
                                        const QString &newPassword) {
  SVM_ACTION_SCOPE("VaultManager", "changeMasterPassword");
  if (!m_isConfigured || newPassword.trimmed().length() < 8) {
    return false;
  }
  if (!validateMasterPassword(oldPassword)) {
    return false;
  }

  auto &bridge = core::CoreBridge::instance();
  if (bridge.isInitialized()) {
    if (!bridge.changeMasterPassword(oldPassword, newPassword)) {
      return false;
    }
    if (m_biometricEnabled) {
      bridge.enableBiometric(primaryVaultId(), newPassword);
    }
  }

  m_passwordHash = hashPassword(newPassword, m_passwordSalt);
  saveSettings();
  return true;
}

bool VaultManager::unlock(const QString &password) {
  SVM_ACTION_SCOPE("VaultManager", "unlock");
  if (!m_isConfigured) {
    return false;
  }

  auto &bridge = core::CoreBridge::instance();
  bool coreUnlocked = false;
  bool coreConfigured = false;
  const QString targetVault = primaryVaultId();
  if (bridge.isInitialized()) {
    const auto _coreVaults = bridge.fetchVaults(&coreConfigured);
    Q_UNUSED(_coreVaults);
    coreUnlocked = bridge.unlockVault(targetVault, password);
  }

  const bool localMatch =
      !m_passwordHash.isEmpty() &&
      (hashPassword(password, m_passwordSalt) == m_passwordHash);

  // Nếu Rust Core đã có kho mã hóa thì bắt buộc giải mã Rust Core thành công.
  const bool canUnlock =
      coreConfigured ? coreUnlocked : (coreUnlocked || localMatch);

  if (canUnlock) {
    if (!coreConfigured && !coreUnlocked && bridge.isInitialized()) {
      // Tự động đồng bộ / khởi tạo kho mã hóa Rust Core nếu trước đó chỉ có cấu
      // hình QSettings
      models::UiVault defaultVault;
      defaultVault.id = targetVault;
      defaultVault.name = m_vaults.isEmpty() ? QStringLiteral("Personal Vault")
                                             : m_vaults.first().name;
      defaultVault.iconName = QStringLiteral("vault_personal");
      defaultVault.description =
          QStringLiteral("Primary personal credentials & logins");
      defaultVault.isDefault = true;
      if (bridge.createVault(defaultVault, password, m_recoveryKey, true) &&
          m_biometricEnabled && bridge.isBiometricAvailable()) {
        m_biometricEnabled = bridge.enableBiometric(targetVault, password);
      }
    }
    if (m_passwordHash.isEmpty() || !localMatch) {
      if (m_passwordSalt.isEmpty()) {
        QByteArray saltBytes(16, Qt::Uninitialized);
        QRandomGenerator::system()->fillRange(
            reinterpret_cast<quint32 *>(saltBytes.data()),
            saltBytes.size() / static_cast<int>(sizeof(quint32)));
        m_passwordSalt = QString::fromLatin1(saltBytes.toHex());
      }
      m_passwordHash = hashPassword(password, m_passwordSalt);
      saveSettings();
    }
    m_isLocked = false;
    refreshVaultsFromCore();
    emit lockStateChanged(m_isLocked);
    return true;
  }
  return false;
}

bool VaultManager::validateMasterPassword(const QString &password) const {
  if (!m_isConfigured) {
    return false;
  }
  if (!m_passwordHash.isEmpty() &&
      hashPassword(password, m_passwordSalt) == m_passwordHash) {
    return true;
  }
  auto &bridge = core::CoreBridge::instance();
  if (bridge.isInitialized()) {
    return bridge.unlockVault(primaryVaultId(), password);
  }
  return false;
}

bool VaultManager::unlockWithRecoveryKey(const QString &recoveryKey,
                                         const QString &newPassword) {
  SVM_ACTION_SCOPE("VaultManager", "unlockWithRecoveryKey");
  if (!m_isConfigured) {
    return false;
  }
  const QString normalizedKey = normalizeRecoveryKey(recoveryKey);
  auto &bridge = core::CoreBridge::instance();
  bool coreOk = false;
  if (bridge.isInitialized()) {
    coreOk = bridge.unlockWithRecoveryKey(normalizedKey, newPassword);
  }

  if (coreOk || (!m_recoveryKey.isEmpty() && normalizedKey == m_recoveryKey)) {
    if (!newPassword.isEmpty()) {
      if (m_passwordSalt.isEmpty()) {
        QByteArray saltBytes(16, Qt::Uninitialized);
        QRandomGenerator::system()->fillRange(
            reinterpret_cast<quint32 *>(saltBytes.data()),
            saltBytes.size() / static_cast<int>(sizeof(quint32)));
        m_passwordSalt = QString::fromLatin1(saltBytes.toHex());
      }
      m_passwordHash = hashPassword(newPassword, m_passwordSalt);
      if (!coreOk && bridge.isInitialized()) {
        models::UiVault initVault;
        initVault.id = primaryVaultId();
        initVault.name = QStringLiteral("Personal Vault");
        bridge.createVault(initVault, newPassword, normalizedKey, true);
      }
      saveSettings();
    }
    m_isLocked = false;
    refreshVaultsFromCore();
    emit lockStateChanged(m_isLocked);
    return true;
  }
  return false;
}

bool VaultManager::unlockWithBiometrics() {
  SVM_ACTION_SCOPE("VaultManager", "unlockWithBiometrics");
  if (!m_isConfigured || !isBiometricEnabled()) {
    return false;
  }
  auto &bridge = core::CoreBridge::instance();
  if (!bridge.isInitialized()) {
    return false;
  }
  if (!bridge.unlockWithBiometrics(primaryVaultId())) {
    return false;
  }
  m_isLocked = false;
  refreshVaultsFromCore();
  emit lockStateChanged(m_isLocked);
  return true;
}

bool VaultManager::isBiometricAvailable() const {
  return core::CoreBridge::instance().isBiometricAvailable();
}

bool VaultManager::isBiometricEnabled() const {
  return m_biometricEnabled && isBiometricAvailable();
}

bool VaultManager::setBiometricEnabled(bool enable) {
  SVM_ACTION_SCOPE("VaultManager", "setBiometricEnabled");
  auto &bridge = core::CoreBridge::instance();
  const QString targetVault = primaryVaultId();
  if (enable) {
    if (!isBiometricAvailable() || !bridge.isInitialized()) {
      return false;
    }
    if (!bridge.enableBiometric(targetVault, QString())) {
      return false;
    }
    m_biometricEnabled = true;
  } else {
    if (bridge.isInitialized() && !bridge.disableBiometric(targetVault)) {
      return false;
    }
    m_biometricEnabled = false;
  }
  saveSettings();
  SettingsManager::instance().setBiometricEnabled(m_biometricEnabled);
  return true;
}

void VaultManager::lock() {
  SVM_ACTION_SCOPE("VaultManager", "lock");
  auto &bridge = core::CoreBridge::instance();
  if (bridge.isInitialized()) {
    bridge.lockVault();
  }
  m_recoveryKey.clear();
  if (!m_isLocked) {
    m_isLocked = true;
    emit lockStateChanged(m_isLocked);
  }
}

QString VaultManager::recoveryKey() const {
  if (!isUnlocked()) {
    return QString();
  }
  if (!m_recoveryKey.isEmpty()) {
    return m_recoveryKey;
  }
  auto &bridge = core::CoreBridge::instance();
  if (bridge.isInitialized() && bridge.isVaultUnlocked()) {
    return bridge.recoveryKey();
  }
  return QString();
}

QString VaultManager::rotateRecoveryKey(const QString &masterPassword) {
  SVM_ACTION_SCOPE("VaultManager", "rotateRecoveryKey");
  if (!m_isConfigured || !isUnlocked()) {
    return QString();
  }
  if (!validateMasterPassword(masterPassword)) {
    return QString();
  }

  auto &bridge = core::CoreBridge::instance();
  QString newKey;
  if (bridge.isInitialized()) {
    newKey = bridge.rotateRecoveryKey(masterPassword);
  }
  if (newKey.isEmpty()) {
    newKey = generateRecoveryKey();
  }
  m_recoveryKey = newKey.trimmed().toUpper();
  saveSettings();
  return newKey;
}

QString VaultManager::generateRecoveryKey() {
  const char chars[] = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789"; // Loại bỏ O, 0, I, 1
  const int charCount = sizeof(chars) - 1;
  QStringList groups;
  for (int g = 0; g < 6; ++g) {
    QString grp;
    for (int i = 0; i < 4; ++i) {
      const int idx = QRandomGenerator::system()->bounded(charCount);
      grp.append(QLatin1Char(chars[idx]));
    }
    groups.append(grp);
  }
  return groups.join(QLatin1Char('-'));
}

QString VaultManager::normalizeRecoveryKey(const QString &raw) {
  QString clean;
  for (const QChar &ch : raw.trimmed().toUpper()) {
    if (ch.isLetterOrNumber()) {
      clean.append(ch);
    }
  }
  QString body = clean;
  if (body.startsWith(QStringLiteral("SVM")) &&
      (body.length() == 19 || body.length() == 27)) {
    body = body.mid(3);
  }
  if (body.length() == 24) {
    QStringList groups;
    for (int i = 0; i < 24; i += 4) {
      groups.append(body.mid(i, 4));
    }
    return groups.join(QLatin1Char('-'));
  }
  if (body.length() == 16) {
    QStringList groups;
    for (int i = 0; i < 16; i += 4) {
      groups.append(body.mid(i, 4));
    }
    return QStringLiteral("SVM-%1").arg(groups.join(QLatin1Char('-')));
  }
  return raw.trimmed().toUpper();
}

QString VaultManager::hashPassword(const QString &password,
                                   const QString &salt) const {
  QByteArray data = (salt + password).toUtf8();
  return QString::fromLatin1(
      QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex());
}

QString VaultManager::primaryVaultId() const {
  const QString defId = defaultVault().id;
  if (!defId.isEmpty()) {
    return defId;
  }
  if (!m_activeVaultId.isEmpty() && m_activeVaultId != QStringLiteral("all")) {
    return m_activeVaultId;
  }
  return QStringLiteral("personal");
}

void VaultManager::loadSettings() {
  QSettings settings = openUnifiedSettings();

  // Di trú tự động một lần từ cấu hình QSettings mặc định cũ sang
  // Hephaestus/SecureVaultManager.ini
  if (!settings
           .value(QStringLiteral("Migration/LegacyVaultSettingsDone"), false)
           .toBool()) {
    QSettings legacySettings;
    if (!settings.contains(QStringLiteral("Security/IsConfigured")) &&
        (legacySettings.contains(QStringLiteral("Security/IsConfigured")) ||
         legacySettings.contains(QStringLiteral("Vaults/ListJson")))) {
      const QStringList keysToMigrate = {
          QStringLiteral("Security/IsConfigured"),
          QStringLiteral("Security/PasswordHash"),
          QStringLiteral("Security/PasswordSalt"),
          QStringLiteral("Security/BiometricEnabled"),
          QStringLiteral("Vaults/ListJson"),
      };
      for (const QString &key : keysToMigrate) {
        if (legacySettings.contains(key)) {
          settings.setValue(key, legacySettings.value(key));
        }
      }
    }
    legacySettings.remove(QStringLiteral("Security/RecoveryKey"));
    legacySettings.sync();
    settings.setValue(QStringLiteral("Migration/LegacyVaultSettingsDone"),
                      true);
    settings.sync();
  }

  m_isConfigured =
      settings.value(QStringLiteral("Security/IsConfigured"), false).toBool();
  m_passwordHash =
      settings.value(QStringLiteral("Security/PasswordHash")).toString();
  m_passwordSalt =
      settings.value(QStringLiteral("Security/PasswordSalt")).toString();
  settings.remove(QStringLiteral("Security/RecoveryKey"));
  m_recoveryKey.clear();
  m_biometricEnabled =
      settings.value(QStringLiteral("Security/BiometricEnabled"), false)
          .toBool();
  if (SettingsManager::instance().biometricEnabled() != m_biometricEnabled) {
    SettingsManager::instance().setBiometricEnabled(m_biometricEnabled);
  }

  auto &bridge = core::CoreBridge::instance();
  if (bridge.isInitialized()) {
    bool coreConfigured = false;
    const auto coreVaults = bridge.fetchVaults(&coreConfigured);
    if (coreConfigured && !m_isConfigured) {
      m_isConfigured = true;
    }
    if (!coreVaults.isEmpty()) {
      m_vaults = coreVaults;
    }
  }

  m_isLocked = true; // Mặc định khi mở ứng dụng thì luôn ở trạng thái khóa

  if (m_vaults.isEmpty()) {
    const QString jsonStr =
        settings.value(QStringLiteral("Vaults/ListJson")).toString();
    if (!jsonStr.isEmpty()) {
      QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8());
      if (doc.isArray()) {
        const QJsonArray arr = doc.array();
        for (const auto &val : arr) {
          if (val.isObject()) {
            QJsonObject obj = val.toObject();
            models::UiVault v;
            v.id = obj.value(QStringLiteral("id")).toString();
            v.name = obj.value(QStringLiteral("name")).toString();
            v.iconName = obj.value(QStringLiteral("iconName"))
                             .toString(QStringLiteral("vault_personal"));
            v.description = obj.value(QStringLiteral("description")).toString();
            v.itemCount = obj.value(QStringLiteral("itemCount")).toInt(0);
            v.isDefault = obj.value(QStringLiteral("isDefault")).toBool(false);
            v.createdAt = QDateTime::fromString(
                obj.value(QStringLiteral("createdAt")).toString(), Qt::ISODate);
            if (!v.id.isEmpty()) {
              m_vaults.append(v);
            }
          }
        }
      }
    }
  }

  // Nếu đã cấu hình nhưng chưa có danh sách vault, khởi tạo kho mặc định
  if (m_isConfigured && m_vaults.isEmpty()) {
    models::UiVault defaultVault;
    defaultVault.id = QStringLiteral("personal");
    defaultVault.name = QStringLiteral("Personal Vault");
    defaultVault.iconName = QStringLiteral("vault_personal");
    defaultVault.isDefault = true;
    m_vaults.append(defaultVault);
    saveSettings();
  }

  if (m_activeVaultId.isEmpty() || m_activeVaultId == QStringLiteral("all")) {
    m_activeVaultId = VaultManager::defaultVault().id;
  }
}

void VaultManager::saveSettings() {
  QSettings settings = openUnifiedSettings();
  settings.setValue(QStringLiteral("Security/IsConfigured"), m_isConfigured);
  settings.setValue(QStringLiteral("Security/PasswordHash"), m_passwordHash);
  settings.setValue(QStringLiteral("Security/PasswordSalt"), m_passwordSalt);
  settings.remove(QStringLiteral("Security/RecoveryKey"));
  settings.setValue(QStringLiteral("Security/BiometricEnabled"),
                    m_biometricEnabled);

  QJsonArray arr;
  for (const auto &v : m_vaults) {
    QJsonObject obj;
    obj.insert(QStringLiteral("id"), v.id);
    obj.insert(QStringLiteral("name"), v.name);
    obj.insert(QStringLiteral("iconName"), v.iconName);
    obj.insert(QStringLiteral("description"), v.description);
    obj.insert(QStringLiteral("itemCount"), v.itemCount);
    obj.insert(QStringLiteral("isDefault"), v.isDefault);
    obj.insert(QStringLiteral("createdAt"), v.createdAt.toString(Qt::ISODate));
    arr.append(obj);
  }
  QJsonDocument doc(arr);
  settings.setValue(QStringLiteral("Vaults/ListJson"),
                    QString::fromUtf8(doc.toJson(QJsonDocument::Compact)));
}

} // namespace svm::app
