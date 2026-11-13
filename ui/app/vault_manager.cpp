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
namespace svm::app {

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
    if (m_activeVaultId.isEmpty() ||
        (m_activeVaultId != QStringLiteral("all") &&
         vault(m_activeVaultId).id.isEmpty())) {
      m_activeVaultId = defaultVault().id;
      emit activeVaultChanged(m_activeVaultId);
    }
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
  m_recoveryKey.clear();
  m_biometricEnabled = false;
  m_isConfigured = false;
  m_isLocked = false;
  SettingsManager::instance().resetSecurityAndVaults();

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

  auto &bridge = core::CoreBridge::instance();
  if (bridge.isInitialized()) {
    if (!bridge.changeMasterPassword(oldPassword, newPassword)) {
      return false;
    }
    if (m_biometricEnabled) {
      bridge.enableBiometric(primaryVaultId(), newPassword);
    }
  }

  saveSettings();
  return true;
}

bool VaultManager::unlock(const QString &password) {
  SVM_ACTION_SCOPE("VaultManager", "unlock");
  if (!m_isConfigured) {
    return false;
  }

  auto &bridge = core::CoreBridge::instance();
  if (!bridge.isInitialized()) {
    return false;
  }

  const QString targetVault = primaryVaultId();
  const bool coreUnlocked = bridge.unlockVault(targetVault, password);

  if (coreUnlocked) {
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
  if (!bridge.isInitialized()) {
    return false;
  }

  const bool coreOk = bridge.unlockWithRecoveryKey(normalizedKey, newPassword);

  if (coreOk) {
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

  auto &bridge = core::CoreBridge::instance();
  QString newKey;
  if (bridge.isInitialized()) {
    newKey = bridge.rotateRecoveryKey(masterPassword);
  }
  if (newKey.isEmpty()) {
    return QString();
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
  auto &settingsMgr = SettingsManager::instance();
  m_isConfigured = settingsMgr.isConfigured();
  m_biometricEnabled = settingsMgr.biometricEnabled();

  auto &bridge = core::CoreBridge::instance();
  if (bridge.isInitialized()) {
    bool coreConfigured = false;
    const auto coreVaults = bridge.fetchVaults(&coreConfigured);
    m_isConfigured = coreConfigured;
    if (settingsMgr.isConfigured() != coreConfigured) {
      settingsMgr.setConfigured(coreConfigured);
    }
    if (!coreVaults.isEmpty()) {
      m_vaults = coreVaults;
    }
  }

  m_isLocked = true; // Mặc định khi mở ứng dụng thì luôn ở trạng thái khóa

  if (m_vaults.isEmpty()) {
    const QJsonArray arr = settingsMgr.cachedVaults();
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
  auto &settingsMgr = SettingsManager::instance();
  settingsMgr.setConfigured(m_isConfigured);
  settingsMgr.setBiometricEnabled(m_biometricEnabled);

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
  settingsMgr.setCachedVaults(arr);
}

} // namespace svm::app
