/**
 * @file ui/app/vault_manager.cpp
 * @brief Hiện thực lớp quản lý trạng thái vòng đời kho lưu trữ (VaultManager).
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#include "vault_manager.h"
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

void VaultManager::refreshVaultsFromCore() {
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
        m_activeVaultId = QStringLiteral("all");
        emit activeVaultChanged(m_activeVaultId);
      }
      saveSettings();
      emit vaultListChanged();
      return true;
    }
  }
  return false;
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
                                              const QString &initialVaultName) {
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
  defaultVault.iconName = QStringLiteral("folder_open");
  defaultVault.accentColor = QStringLiteral("#3B82F6");
  defaultVault.description =
      QStringLiteral("Primary personal credentials & logins");
  defaultVault.isDefault = true;
  defaultVault.createdAt = QDateTime::currentDateTime();
  m_vaults.append(defaultVault);

  models::UiVault workVault;
  workVault.id = QStringLiteral("work");
  workVault.name = QStringLiteral("Work Vault");
  workVault.iconName = QStringLiteral("folder_empty");
  workVault.accentColor = QStringLiteral("#8B5CF6");
  workVault.description = QStringLiteral("Company, cloud & server credentials");
  workVault.isDefault = false;
  workVault.createdAt = QDateTime::currentDateTime();
  m_vaults.append(workVault);

  // Khởi tạo kho mã hóa trong Rust Core
  auto &bridge = core::CoreBridge::instance();
  if (bridge.isInitialized()) {
    bridge.createVault(defaultVault, password, m_recoveryKey, true);
    bridge.enableBiometric(QStringLiteral("personal"), password);
    const auto coreVaults = bridge.fetchVaults();
    if (!coreVaults.isEmpty()) {
      m_vaults = coreVaults;
    }
  }

  m_isConfigured = true;
  m_isLocked = false;
  m_activeVaultId = QStringLiteral("all");

  saveSettings();
  emit configuredChanged(m_isConfigured);
  emit lockStateChanged(m_isLocked);
  emit vaultListChanged();
  emit activeVaultChanged(m_activeVaultId);
  return true;
}

bool VaultManager::changeMasterPassword(const QString &oldPassword,
                                        const QString &newPassword) {
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
      bridge.enableBiometric(QStringLiteral("personal"), newPassword);
    }
  }

  m_passwordHash = hashPassword(newPassword, m_passwordSalt);
  saveSettings();
  return true;
}

bool VaultManager::unlock(const QString &password) {
  if (!m_isConfigured) {
    return false;
  }

  auto &bridge = core::CoreBridge::instance();
  bool coreUnlocked = false;
  if (bridge.isInitialized()) {
    coreUnlocked = bridge.unlockVault(QStringLiteral("personal"), password);
  }

  const bool localMatch =
      !m_passwordHash.isEmpty() &&
      (hashPassword(password, m_passwordSalt) == m_passwordHash);

  if (coreUnlocked || localMatch) {
    if (!coreUnlocked && bridge.isInitialized()) {
      // Tự động đồng bộ / khởi tạo kho mã hóa Rust Core nếu trước đó chỉ có cấu
      // hình QSettings
      models::UiVault defaultVault;
      defaultVault.id = QStringLiteral("personal");
      defaultVault.name = m_vaults.isEmpty() ? QStringLiteral("Personal Vault")
                                             : m_vaults.first().name;
      defaultVault.iconName = QStringLiteral("folder_open");
      defaultVault.accentColor = QStringLiteral("#3B82F6");
      defaultVault.description =
          QStringLiteral("Primary personal credentials & logins");
      defaultVault.isDefault = true;
      bridge.createVault(defaultVault, password, m_recoveryKey, true);
      bridge.enableBiometric(QStringLiteral("personal"), password);
    }
    if (m_passwordHash.isEmpty() || !localMatch) {
      if (m_passwordSalt.isEmpty()) {
        m_passwordSalt = QStringLiteral("svm_default_salt");
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
  return false;
}

bool VaultManager::unlockWithRecoveryKey(const QString &recoveryKey,
                                         const QString &newPassword) {
  if (!m_isConfigured) {
    return false;
  }
  const QString normalizedKey = recoveryKey.trimmed().toUpper();
  auto &bridge = core::CoreBridge::instance();
  bool coreOk = false;
  if (bridge.isInitialized()) {
    coreOk = bridge.unlockWithRecoveryKey(normalizedKey, newPassword);
  }

  if (coreOk || (!m_recoveryKey.isEmpty() && normalizedKey == m_recoveryKey)) {
    if (!newPassword.isEmpty()) {
      if (m_passwordSalt.isEmpty()) {
        m_passwordSalt = QStringLiteral("svm_default_salt");
      }
      m_passwordHash = hashPassword(newPassword, m_passwordSalt);
      if (!coreOk && bridge.isInitialized()) {
        models::UiVault defaultVault;
        defaultVault.id = QStringLiteral("personal");
        defaultVault.name = QStringLiteral("Personal Vault");
        bridge.createVault(defaultVault, newPassword, normalizedKey, true);
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
  if (!m_isConfigured) {
    return false;
  }
  auto &bridge = core::CoreBridge::instance();
  if (bridge.isInitialized()) {
    bridge.unlockWithBiometrics(QStringLiteral("personal"));
  }
  m_isLocked = false;
  refreshVaultsFromCore();
  emit lockStateChanged(m_isLocked);
  return true;
}

void VaultManager::lock() {
  auto &bridge = core::CoreBridge::instance();
  if (bridge.isInitialized()) {
    bridge.lockVault();
  }
  if (!m_isLocked) {
    m_isLocked = true;
    emit lockStateChanged(m_isLocked);
  }
}

QString VaultManager::generateRecoveryKey() {
  const char chars[] = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789"; // Loại bỏ O, 0, I, 1
  const int charCount = sizeof(chars) - 1;
  QStringList groups;
  for (int g = 0; g < 4; ++g) {
    QString grp;
    for (int i = 0; i < 4; ++i) {
      const int idx = QRandomGenerator::system()->bounded(charCount);
      grp.append(QLatin1Char(chars[idx]));
    }
    groups.append(grp);
  }
  return QStringLiteral("SVM-%1").arg(groups.join(QLatin1Char('-')));
}

QString VaultManager::hashPassword(const QString &password,
                                   const QString &salt) const {
  QByteArray data = (salt + password).toUtf8();
  return QString::fromLatin1(
      QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex());
}

void VaultManager::loadSettings() {
  QSettings settings;
  m_isConfigured =
      settings.value(QStringLiteral("Security/IsConfigured"), false).toBool();
  m_passwordHash =
      settings.value(QStringLiteral("Security/PasswordHash")).toString();
  m_passwordSalt =
      settings.value(QStringLiteral("Security/PasswordSalt")).toString();
  m_recoveryKey =
      settings.value(QStringLiteral("Security/RecoveryKey")).toString();
  m_biometricEnabled =
      settings.value(QStringLiteral("Security/BiometricEnabled"), false)
          .toBool();

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
                             .toString(QStringLiteral("folder_open"));
            v.accentColor = obj.value(QStringLiteral("accentColor"))
                                .toString(QStringLiteral("#3B82F6"));
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

  // Nếu đã cấu hình nhưng chưa có danh sách vault, khởi tạo cặp mặc định
  if (m_isConfigured && m_vaults.isEmpty()) {
    models::UiVault defaultVault;
    defaultVault.id = QStringLiteral("personal");
    defaultVault.name = QStringLiteral("Personal Vault");
    defaultVault.iconName = QStringLiteral("folder_open");
    defaultVault.accentColor = QStringLiteral("#3B82F6");
    defaultVault.isDefault = true;
    m_vaults.append(defaultVault);

    models::UiVault workVault;
    workVault.id = QStringLiteral("work");
    workVault.name = QStringLiteral("Work Vault");
    workVault.iconName = QStringLiteral("folder_empty");
    workVault.accentColor = QStringLiteral("#8B5CF6");
    workVault.isDefault = false;
    m_vaults.append(workVault);
    saveSettings();
  }
}

void VaultManager::saveSettings() {
  QSettings settings;
  settings.setValue(QStringLiteral("Security/IsConfigured"), m_isConfigured);
  settings.setValue(QStringLiteral("Security/PasswordHash"), m_passwordHash);
  settings.setValue(QStringLiteral("Security/PasswordSalt"), m_passwordSalt);
  settings.setValue(QStringLiteral("Security/RecoveryKey"), m_recoveryKey);
  settings.setValue(QStringLiteral("Security/BiometricEnabled"),
                    m_biometricEnabled);

  QJsonArray arr;
  for (const auto &v : m_vaults) {
    QJsonObject obj;
    obj.insert(QStringLiteral("id"), v.id);
    obj.insert(QStringLiteral("name"), v.name);
    obj.insert(QStringLiteral("iconName"), v.iconName);
    obj.insert(QStringLiteral("accentColor"), v.accentColor);
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
