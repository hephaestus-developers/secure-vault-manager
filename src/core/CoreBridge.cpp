/**
 * @file ui/core/CoreBridge.cpp
 * @brief Hiện thực lớp CoreBridge kết nối với Rust Core qua FFI CXX Bridge.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#include "core/CoreBridge.hpp"

#include "core/LogCategories.hpp"
#include "ui/app/SettingsManager.hpp"

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaObject>
#include <QPointer>
#include <QThreadPool>
#include <QUrl>

namespace svm::core {

namespace {

constexpr int SVM_EVENT_VAULT_LOCKED = 1;
constexpr int SVM_EVENT_VAULT_UNLOCKED = 2;
constexpr int SVM_EVENT_ITEMS_CHANGED = 3;
constexpr int SVM_EVENT_PROMPT = 4;
constexpr int SVM_EVENT_PROMPT_CANCELLED = 5;
constexpr int SVM_EVENT_BRIDGE_STATUS = 7;
constexpr int SVM_EVENT_SSH_STATUS = 8;
constexpr int SVM_EVENT_BRIDGE_CLIENTS_CHANGED = 10;
constexpr int SVM_EVENT_SETTINGS_CHANGED = 11;

inline rust::Str rStr(const QByteArray &b) {
  return rust::Str(b.constData(), static_cast<size_t>(b.size()));
}

inline QString qStr(const rust::String &s) {
  return QString::fromUtf8(s.data(), static_cast<qsizetype>(s.size()));
}

QString categoryToCode(models::ItemCategory cat) {
  switch (cat) {
  case models::ItemCategory::Login:
    return QStringLiteral("login");
  case models::ItemCategory::Passkey:
    return QStringLiteral("passkey");
  case models::ItemCategory::Card:
    return QStringLiteral("card");
  case models::ItemCategory::Identity:
    return QStringLiteral("identity");
  case models::ItemCategory::SecureNote:
    return QStringLiteral("secure_note");
  case models::ItemCategory::SshKey:
    return QStringLiteral("ssh_key");
  case models::ItemCategory::Database:
    return QStringLiteral("database");
  case models::ItemCategory::Server:
    return QStringLiteral("server");
  case models::ItemCategory::Wifi:
    return QStringLiteral("wifi");
  }
  return QStringLiteral("login");
}

models::ItemCategory categoryFromCode(const QString &code) {
  const QString c = code.trimmed().toLower();
  if (c == QStringLiteral("passkey") || c == QStringLiteral("passkeys")) {
    return models::ItemCategory::Passkey;
  }
  if (c == QStringLiteral("card") || c == QStringLiteral("credit_card")) {
    return models::ItemCategory::Card;
  }
  if (c == QStringLiteral("identity") || c == QStringLiteral("identities")) {
    return models::ItemCategory::Identity;
  }
  if (c == QStringLiteral("secure_note") || c == QStringLiteral("note")) {
    return models::ItemCategory::SecureNote;
  }
  if (c == QStringLiteral("ssh_key") || c == QStringLiteral("ssh")) {
    return models::ItemCategory::SshKey;
  }
  if (c == QStringLiteral("database") || c == QStringLiteral("db")) {
    return models::ItemCategory::Database;
  }
  if (c == QStringLiteral("server") || c == QStringLiteral("servers")) {
    return models::ItemCategory::Server;
  }
  if (c == QStringLiteral("wifi") || c == QStringLiteral("wi-fi")) {
    return models::ItemCategory::Wifi;
  }
  return models::ItemCategory::Login;
}

QString customFieldKindToCode(models::CustomFieldKind kind) {
  switch (kind) {
  case models::CustomFieldKind::Text:
    return QStringLiteral("text");
  case models::CustomFieldKind::Concealed:
    return QStringLiteral("concealed");
  case models::CustomFieldKind::Boolean:
    return QStringLiteral("boolean");
  case models::CustomFieldKind::Date:
    return QStringLiteral("date");
  }
  return QStringLiteral("text");
}

models::CustomFieldKind customFieldKindFromCode(const QString &code) {
  const QString c = code.trimmed().toLower();
  if (c == QStringLiteral("concealed") || c == QStringLiteral("hidden") ||
      c == QStringLiteral("password")) {
    return models::CustomFieldKind::Concealed;
  }
  if (c == QStringLiteral("boolean") || c == QStringLiteral("bool")) {
    return models::CustomFieldKind::Boolean;
  }
  if (c == QStringLiteral("date")) {
    return models::CustomFieldKind::Date;
  }
  return models::CustomFieldKind::Text;
}

} // namespace

CoreBridge &CoreBridge::instance() {
  static CoreBridge s_instance;
  return s_instance;
}

CoreBridge::CoreBridge() {
  m_hardened = svm::ffi::harden_process();
  if (m_hardened) {
    qInfo() << "[CoreBridge] Process memory hardened (PR_SET_DUMPABLE=0, "
               "anti-debug).";
  }
}

void CoreBridge::shutdown() {
  if (m_shuttingDown.exchange(true)) {
    return;
  }
  if (m_context) {
    (*m_context)->set_event_callback(0, 0);
  }
  QThreadPool::globalInstance()->clear();
  QThreadPool::globalInstance()->waitForDone(5500);
  if (m_context) {
    stopBridge();
    stopSshAgent();
    m_context.reset();
  }
}

CoreBridge::~CoreBridge() {
  m_shuttingDown.store(true);
  if (m_context) {
    (*m_context)->set_event_callback(0, 0);
    stopBridge();
    stopSshAgent();
    m_context.reset();
  }
}

void CoreBridge::onRustEventCallback(int32_t kind, const char *payloadJson,
                                     void *userData) {
  auto *self = static_cast<CoreBridge *>(userData);
  if (!self || self->m_shuttingDown.load()) {
    return;
  }
  const QString payload =
      payloadJson ? QString::fromUtf8(payloadJson) : QStringLiteral("{}");
  const int kindInt = static_cast<int>(kind);
  QMetaObject::invokeMethod(
      self,
      [self, kindInt, payload]() { self->handleRustEvent(kindInt, payload); },
      Qt::QueuedConnection);
}

void CoreBridge::handleRustEvent(int kind, const QString &payloadJson) {
  emit coreEventReceived(kind, payloadJson);
  if (kind == SVM_EVENT_VAULT_LOCKED) {
    emit vaultLockedEvent();
  } else if (kind == SVM_EVENT_VAULT_UNLOCKED) {
    emit vaultUnlockedEvent();
  } else if (kind == SVM_EVENT_ITEMS_CHANGED) {
    emit itemsChangedEvent();
  } else if (kind == SVM_EVENT_PROMPT) {
    const QJsonObject promptObj =
        QJsonDocument::fromJson(payloadJson.toUtf8()).object();
    emit promptRequestedEvent(promptObj);
  } else if (kind == SVM_EVENT_BRIDGE_CLIENTS_CHANGED) {
    emit bridgeClientsChangedEvent();
  } else if (kind == SVM_EVENT_SETTINGS_CHANGED) {
    const QJsonObject settingsObj =
        QJsonDocument::fromJson(payloadJson.toUtf8()).object();
    emit coreSettingsChanged(settingsObj);
  }
}

bool CoreBridge::initialize(const QString &dataDir) {
  SVM_ACTION_SCOPE_DETAIL("CoreBridge", "initialize",
                          dataDir.isEmpty() ? QStringLiteral("default")
                                            : dataDir);
  if (m_context) {
    return true;
  }

  try {
    const QByteArray dirBytes = dataDir.toUtf8();
    rust::Box<svm::ffi::FfiContext> box =
        svm::ffi::create_context(rStr(dirBytes));
    m_context =
        std::make_unique<rust::Box<svm::ffi::FfiContext>>(std::move(box));
    (*m_context)
        ->set_event_callback(
            reinterpret_cast<size_t>(&CoreBridge::onRustEventCallback),
            reinterpret_cast<size_t>(this));
    qInfo() << "[CoreBridge] SvmContext initialized successfully. Core version:"
            << coreVersion();
    emit initializedChanged(true);
    return true;
  } catch (const std::exception &e) {
    qCritical() << "[CoreBridge] Failed to create SvmContext:" << e.what();
    emit errorOccurred(QString::fromUtf8(e.what()));
    return false;
  }
}

bool CoreBridge::resetStorage() {
  SVM_ACTION_SCOPE("CoreBridge", "resetStorage");
  if (!m_context && !initialize()) {
    return false;
  }
  auto res = (*m_context)->reset_vault();
  if (!res.success) {
    emit errorOccurred(qStr(res.error_message));
    return false;
  }
  app::SettingsManager::instance().resetSecurityAndVaults();
  return true;
}

bool CoreBridge::isInitialized() const { return m_context != nullptr; }

QString CoreBridge::coreVersion() const {
  return qStr(svm::ffi::core_version());
}

QString CoreBridge::lastError() const { return QString(); }

/* ========================================================================= */
/* Quản lý Kho (Multi-Vault & Vòng đời Khóa/Mở khóa)                          */
/* ========================================================================= */

QString CoreBridge::listVaults() {
  if (!m_context) {
    return QStringLiteral(R"({"vaults":[]})");
  }

  auto res = (*m_context)->list_vaults();
  if (!res.success) {
    emit errorOccurred(qStr(res.error_message));
    return QStringLiteral(R"({"vaults":[]})");
  }

  return qStr(res.value);
}

QList<models::UiVault> CoreBridge::fetchVaults(bool *outConfigured) {
  SVM_ACTION_SCOPE("CoreBridge", "fetchVaults");
  QList<models::UiVault> result;
  const QString jsonStr = listVaults();
  QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8());
  if (!doc.isObject()) {
    if (outConfigured) {
      *outConfigured = false;
    }
    return result;
  }

  const QJsonObject root = doc.object();
  if (outConfigured) {
    *outConfigured = root.value(QStringLiteral("configured")).toBool(false);
  }

  const QJsonArray arr = root.value(QStringLiteral("vaults")).toArray();
  for (const auto &val : arr) {
    if (!val.isObject()) {
      continue;
    }
    const QJsonObject obj = val.toObject();
    models::UiVault v;
    v.id = obj.value(QStringLiteral("id")).toString();
    v.name = obj.value(QStringLiteral("name")).toString();
    v.iconName = obj.value(QStringLiteral("iconName"))
                     .toString(QStringLiteral("vault_personal"));
    v.description = obj.value(QStringLiteral("description")).toString();
    v.itemCount = obj.value(QStringLiteral("itemCount")).toInt(0);
    v.isDefault = obj.value(QStringLiteral("isDefault")).toBool(false);
    const qint64 createdTs =
        static_cast<qint64>(obj.value(QStringLiteral("createdAt")).toDouble(0));
    if (createdTs > 0) {
      v.createdAt = (createdTs > 100000000000LL)
                        ? QDateTime::fromMSecsSinceEpoch(createdTs)
                        : QDateTime::fromSecsSinceEpoch(createdTs);
    } else {
      v.createdAt = QDateTime::currentDateTime();
    }
    if (!v.id.isEmpty()) {
      result.append(v);
    }
  }
  return result;
}

bool CoreBridge::createVault(const models::UiVault &vault,
                             const QString &password,
                             const QString &recoveryKey, bool initialSetup,
                             QString *outVaultId) {
  SVM_ACTION_SCOPE_DETAIL("CoreBridge", "createVault",
                          QStringLiteral("name=\"%1\", initial=%2")
                              .arg(vault.name)
                              .arg(initialSetup));
  if (!m_context) {
    return false;
  }

  QJsonObject meta;
  if (!vault.id.isEmpty()) {
    meta.insert(QStringLiteral("id"), vault.id);
  }
  meta.insert(QStringLiteral("name"), vault.name);
  meta.insert(QStringLiteral("iconName"), vault.iconName);
  meta.insert(QStringLiteral("description"), vault.description);
  meta.insert(QStringLiteral("isDefault"), vault.isDefault);
  meta.insert(QStringLiteral("initialSetup"), initialSetup);
  if (!recoveryKey.isEmpty()) {
    meta.insert(QStringLiteral("recoveryKey"), recoveryKey);
  }

  const QByteArray metaJson =
      QJsonDocument(meta).toJson(QJsonDocument::Compact);
  const QByteArray pwdUtf8 = password.toUtf8();
  auto res = (*m_context)->create_vault(rStr(metaJson), rStr(pwdUtf8));
  if (!res.success) {
    emit errorOccurred(qStr(res.error_message));
    return false;
  }
  if (outVaultId) {
    *outVaultId = qStr(res.value);
  }
  return true;
}

bool CoreBridge::updateVault(const models::UiVault &vault) {
  SVM_ACTION_SCOPE_DETAIL("CoreBridge", "updateVault",
                          QStringLiteral("id=\"%1\"").arg(vault.id));
  if (!m_context || vault.id.isEmpty()) {
    return false;
  }

  QJsonObject meta;
  meta.insert(QStringLiteral("name"), vault.name);
  meta.insert(QStringLiteral("iconName"), vault.iconName);
  meta.insert(QStringLiteral("description"), vault.description);

  const QByteArray vidUtf8 = vault.id.toUtf8();
  const QByteArray metaJson =
      QJsonDocument(meta).toJson(QJsonDocument::Compact);
  auto res = (*m_context)->rename_vault(rStr(vidUtf8), rStr(metaJson));
  if (!res.success) {
    emit errorOccurred(qStr(res.error_message));
    return false;
  }
  return true;
}

bool CoreBridge::deleteVault(const QString &vaultId, const QString &password) {
  SVM_ACTION_SCOPE_DETAIL("CoreBridge", "deleteVault",
                          QStringLiteral("id=\"%1\"").arg(vaultId));
  if (!m_context || vaultId.isEmpty()) {
    return false;
  }
  const QByteArray vidUtf8 = vaultId.toUtf8();
  const QByteArray pwdUtf8 = password.toUtf8();
  auto res = (*m_context)->delete_vault(rStr(vidUtf8), rStr(pwdUtf8));
  if (!res.success) {
    emit errorOccurred(qStr(res.error_message));
    return false;
  }
  return true;
}

bool CoreBridge::setDefaultVault(const QString &vaultId) {
  SVM_ACTION_SCOPE_DETAIL("CoreBridge", "setDefaultVault",
                          QStringLiteral("id=\"%1\"").arg(vaultId));
  if (!m_context || vaultId.isEmpty()) {
    return false;
  }
  const QByteArray vidUtf8 = vaultId.toUtf8();
  auto res = (*m_context)->set_default_vault(rStr(vidUtf8));
  if (!res.success) {
    emit errorOccurred(qStr(res.error_message));
    return false;
  }
  return true;
}

bool CoreBridge::unlockVault(const QString &vaultId, const QString &password) {
  SVM_ACTION_SCOPE_DETAIL("CoreBridge", "unlockVault",
                          QStringLiteral("vaultId=\"%1\"").arg(vaultId));
  if (!m_context) {
    return false;
  }
  const QByteArray vidUtf8 = vaultId.toUtf8();
  const QByteArray pwdUtf8 = password.toUtf8();
  auto res = (*m_context)->unlock_vault(rStr(vidUtf8), rStr(pwdUtf8));
  if (!res.success) {
    emit errorOccurred(qStr(res.error_message));
    return false;
  }
  return res.value;
}

bool CoreBridge::unlockWithRecoveryKey(const QString &recoveryKey,
                                       const QString & /*newPassword*/) {
  SVM_ACTION_SCOPE("CoreBridge", "unlockWithRecoveryKey");
  return unlockVault(QStringLiteral("personal"), recoveryKey);
}

bool CoreBridge::unlockWithBiometrics(const QString & /*vaultId*/) {
  SVM_ACTION_SCOPE("CoreBridge", "unlockWithBiometrics");
  if (!m_context) {
    return false;
  }
  auto res = (*m_context)->unlock_biometric();
  if (!res.success) {
    emit errorOccurred(qStr(res.error_message));
    return false;
  }
  return res.value;
}

bool CoreBridge::lockVault() {
  SVM_ACTION_SCOPE("CoreBridge", "lockVault");
  if (!m_context) {
    return false;
  }
  auto res = (*m_context)->lock_vault();
  if (!res.success) {
    emit errorOccurred(qStr(res.error_message));
    return false;
  }
  return res.value;
}

bool CoreBridge::isVaultUnlocked() const {
  if (!m_context) {
    return false;
  }
  return (*m_context)->is_unlocked();
}

QString CoreBridge::recoveryKey() const {
  if (!m_context) {
    return {};
  }
  auto res = (*m_context)->get_recovery_key();
  if (!res.success) {
    return {};
  }
  return qStr(res.value);
}

QString CoreBridge::rotateRecoveryKey(const QString &masterPassword) {
  SVM_ACTION_SCOPE("CoreBridge", "rotateRecoveryKey");
  if (!m_context) {
    return {};
  }
  const QByteArray pwUtf8 = masterPassword.toUtf8();
  auto res = (*m_context)->rotate_recovery_key(rStr(pwUtf8));
  if (!res.success) {
    emit errorOccurred(qStr(res.error_message));
    return {};
  }
  return qStr(res.value);
}

bool CoreBridge::changeMasterPassword(const QString &oldPassword,
                                      const QString &newPassword,
                                      const QString & /*vaultId*/) {
  SVM_ACTION_SCOPE("CoreBridge", "changeMasterPassword");
  if (!m_context) {
    return false;
  }
  const QByteArray oldUtf8 = oldPassword.toUtf8();
  const QByteArray newUtf8 = newPassword.toUtf8();
  auto res = (*m_context)->change_password(rStr(oldUtf8), rStr(newUtf8));
  if (!res.success) {
    emit errorOccurred(qStr(res.error_message));
    return false;
  }
  return true;
}

bool CoreBridge::isBiometricAvailable() const {
  return svm::ffi::is_biometric_available();
}

bool CoreBridge::enableBiometric(const QString & /*vaultId*/,
                                 const QString &password) {
  if (!m_context) {
    return false;
  }
  const QByteArray pwdUtf8 = password.toUtf8();
  auto res = (*m_context)->enable_biometric(rStr(pwdUtf8));
  return res.success && res.value;
}

bool CoreBridge::disableBiometric(const QString & /*vaultId*/) {
  if (!m_context) {
    return false;
  }
  auto res = (*m_context)->disable_biometric();
  return res.success && res.value;
}

void CoreBridge::reportIdle(quint64 idleMs) {
  if (m_context) {
    (*m_context)->report_idle(idleMs);
  }
}

/* ========================================================================= */
/* Thao tác Bản ghi (Vault Items & Trash)                                     */
/* ========================================================================= */

models::UiVaultItem CoreBridge::itemFromJson(const QJsonObject &obj) {
  models::UiVaultItem item;
  item.id = obj.value(QStringLiteral("id")).toString();
  item.vaultId = obj.value(QStringLiteral("vaultId")).toString();
  item.title = obj.value(QStringLiteral("title")).toString();
  item.category =
      categoryFromCode(obj.value(QStringLiteral("category")).toString());
  item.username = obj.value(QStringLiteral("username")).toString();
  item.password = obj.value(QStringLiteral("password")).toString();
  item.websiteUrl = obj.value(QStringLiteral("websiteUrl")).toString();
  item.notes = obj.value(QStringLiteral("notes")).toString();
  item.totpSecret = obj.value(QStringLiteral("totpSecret")).toString();

  item.isFavorite = obj.value(QStringLiteral("isFavorite")).toBool(false);
  item.isArchived = obj.value(QStringLiteral("isArchived")).toBool(false);
  item.isDeleted = obj.value(QStringLiteral("isDeleted")).toBool(false);
  item.hasTotp = obj.value(QStringLiteral("hasTotp"))
                     .toBool(!item.totpSecret.trimmed().isEmpty());
  item.hasPasskey = obj.value(QStringLiteral("hasPasskey"))
                        .toBool(item.category == models::ItemCategory::Passkey);
  item.passkeyCredentialId =
      obj.value(QStringLiteral("passkeyCredentialId")).toString();
  item.passkeyRpId = obj.value(QStringLiteral("passkeyRpId")).toString();
  item.passkeyAlgorithm =
      obj.value(QStringLiteral("passkeyAlgorithm")).toInt(0);
  item.passkeyUserHandle =
      obj.value(QStringLiteral("passkeyUserHandle")).toString();
  item.passkeySignCount = static_cast<quint32>(
      std::max(0, obj.value(QStringLiteral("passkeySignCount")).toInt(0)));
  item.isSshLoaded = obj.value(QStringLiteral("isSshLoaded")).toBool(false);

  item.sshPublicKey = obj.value(QStringLiteral("sshPublicKey")).toString();
  item.sshFingerprint = obj.value(QStringLiteral("sshFingerprint")).toString();
  item.cardNumber = obj.value(QStringLiteral("cardNumber")).toString();
  item.cardExpiry = obj.value(QStringLiteral("cardExpiry")).toString();
  item.cardCvv = obj.value(QStringLiteral("cardCvv")).toString();
  item.dbHost = obj.value(QStringLiteral("dbHost")).toString();
  item.dbPort = obj.value(QStringLiteral("dbPort")).toInt(5432);
  item.dbName = obj.value(QStringLiteral("dbName")).toString();
  item.wifiSsid = obj.value(QStringLiteral("wifiSsid")).toString();
  item.wifiSecurity = obj.value(QStringLiteral("wifiSecurity")).toString();
  item.passwordStrengthScore =
      obj.value(QStringLiteral("passwordStrengthScore")).toInt(85);

  const qint64 createdTs =
      static_cast<qint64>(obj.value(QStringLiteral("createdAt")).toDouble(0));
  const qint64 updatedTs =
      static_cast<qint64>(obj.value(QStringLiteral("updatedAt")).toDouble(0));

  auto parseEpoch = [](qint64 ts) -> QDateTime {
    if (ts <= 0) {
      return QDateTime::currentDateTime();
    }
    return (ts > 100000000000LL) ? QDateTime::fromMSecsSinceEpoch(ts)
                                 : QDateTime::fromSecsSinceEpoch(ts);
  };

  item.createdAt = parseEpoch(createdTs);
  item.updatedAt = (updatedTs > 0) ? parseEpoch(updatedTs) : item.createdAt;

  const QJsonArray cfArr = obj.value(QStringLiteral("customFields")).toArray();
  for (const auto &cfVal : cfArr) {
    if (!cfVal.isObject()) {
      continue;
    }
    const QJsonObject cfObj = cfVal.toObject();
    models::UiCustomField cf;
    cf.id = cfObj.value(QStringLiteral("id")).toString();
    cf.label = cfObj.value(QStringLiteral("label")).toString();
    cf.value = cfObj.value(QStringLiteral("value")).toString();
    cf.kind =
        customFieldKindFromCode(cfObj.value(QStringLiteral("kind")).toString());
    item.customFields.append(cf);
  }

  return item;
}

QJsonObject CoreBridge::itemToJson(const models::UiVaultItem &item) {
  QJsonObject obj;
  obj.insert(QStringLiteral("id"), item.id);
  obj.insert(QStringLiteral("vaultId"), item.vaultId);
  obj.insert(QStringLiteral("title"), item.title);
  obj.insert(QStringLiteral("category"), categoryToCode(item.category));
  obj.insert(QStringLiteral("username"), item.username);
  obj.insert(QStringLiteral("password"), item.password);
  obj.insert(QStringLiteral("websiteUrl"), item.websiteUrl);
  obj.insert(QStringLiteral("notes"), item.notes);
  obj.insert(QStringLiteral("totpSecret"), item.totpSecret);

  obj.insert(QStringLiteral("isFavorite"), item.isFavorite);
  obj.insert(QStringLiteral("isArchived"), item.isArchived);
  obj.insert(QStringLiteral("isDeleted"), item.isDeleted);
  obj.insert(QStringLiteral("hasTotp"),
             item.hasTotp || !item.totpSecret.trimmed().isEmpty());
  obj.insert(QStringLiteral("hasPasskey"),
             item.hasPasskey || item.category == models::ItemCategory::Passkey);
  obj.insert(QStringLiteral("passkeyCredentialId"), item.passkeyCredentialId);
  obj.insert(QStringLiteral("passkeyRpId"), item.passkeyRpId);
  obj.insert(QStringLiteral("passkeyAlgorithm"), item.passkeyAlgorithm);
  obj.insert(QStringLiteral("passkeyUserHandle"), item.passkeyUserHandle);
  obj.insert(QStringLiteral("passkeySignCount"),
             static_cast<qint64>(item.passkeySignCount));
  obj.insert(QStringLiteral("isSshLoaded"), item.isSshLoaded);

  obj.insert(QStringLiteral("sshPublicKey"), item.sshPublicKey);
  obj.insert(QStringLiteral("sshFingerprint"), item.sshFingerprint);
  obj.insert(QStringLiteral("cardNumber"), item.cardNumber);
  obj.insert(QStringLiteral("cardExpiry"), item.cardExpiry);
  obj.insert(QStringLiteral("cardCvv"), item.cardCvv);
  obj.insert(QStringLiteral("dbHost"), item.dbHost);
  obj.insert(QStringLiteral("dbPort"), item.dbPort);
  obj.insert(QStringLiteral("dbName"), item.dbName);
  obj.insert(QStringLiteral("wifiSsid"), item.wifiSsid);
  obj.insert(QStringLiteral("wifiSecurity"), item.wifiSecurity);
  obj.insert(QStringLiteral("passwordStrengthScore"),
             item.passwordStrengthScore);

  const qint64 createdMs = item.createdAt.isValid()
                               ? item.createdAt.toMSecsSinceEpoch()
                               : QDateTime::currentMSecsSinceEpoch();
  const qint64 updatedMs = item.updatedAt.isValid()
                               ? item.updatedAt.toMSecsSinceEpoch()
                               : QDateTime::currentMSecsSinceEpoch();
  obj.insert(QStringLiteral("createdAt"), createdMs);
  obj.insert(QStringLiteral("updatedAt"), updatedMs);

  QJsonArray cfArr;
  for (const auto &cf : item.customFields) {
    QJsonObject cfObj;
    cfObj.insert(QStringLiteral("id"), cf.id);
    cfObj.insert(QStringLiteral("label"), cf.label);
    cfObj.insert(QStringLiteral("value"), cf.value);
    cfObj.insert(QStringLiteral("kind"), customFieldKindToCode(cf.kind));
    cfArr.append(cfObj);
  }
  obj.insert(QStringLiteral("customFields"), cfArr);
  return obj;
}

QList<models::UiVaultItem> CoreBridge::listItems(const QString &filterJson) {
  SVM_ACTION_SCOPE("CoreBridge", "listItems");
  QList<models::UiVaultItem> items;
  if (!m_context) {
    return items;
  }
  const QByteArray fJson =
      filterJson.isEmpty() ? QByteArray("{}") : filterJson.toUtf8();
  auto res = (*m_context)->list_items(rStr(fJson));
  if (!res.success) {
    return items;
  }
  const QJsonDocument doc = QJsonDocument::fromJson(
      QByteArray(res.value.data(), static_cast<qsizetype>(res.value.size())));
  if (!doc.isObject()) {
    return items;
  }
  const QJsonArray arr = doc.object().value(QStringLiteral("items")).toArray();
  for (const auto &val : arr) {
    if (val.isObject()) {
      items.append(itemFromJson(val.toObject()));
    }
  }
  return items;
}

std::optional<models::UiVaultItem> CoreBridge::getItem(const QString &itemId) {
  SVM_ACTION_SCOPE_DETAIL("CoreBridge", "getItem",
                          QStringLiteral("id=\"%1\"").arg(itemId));
  if (!m_context || itemId.isEmpty()) {
    return std::nullopt;
  }
  const QByteArray idUtf8 = itemId.toUtf8();
  auto res = (*m_context)->get_item(rStr(idUtf8));
  if (!res.success) {
    return std::nullopt;
  }
  const QJsonDocument doc = QJsonDocument::fromJson(
      QByteArray(res.value.data(), static_cast<qsizetype>(res.value.size())));
  if (!doc.isObject()) {
    return std::nullopt;
  }
  return itemFromJson(doc.object());
}

QString CoreBridge::upsertItem(const models::UiVaultItem &item) {
  SVM_ACTION_SCOPE_DETAIL("CoreBridge", "upsertItem",
                          QStringLiteral("title=\"%1\"").arg(item.title));
  if (!m_context) {
    return QString();
  }
  const QJsonObject obj = itemToJson(item);
  const QByteArray jsonUtf8 = QJsonDocument(obj).toJson(QJsonDocument::Compact);
  auto res = (*m_context)->upsert_item(rStr(jsonUtf8));
  if (!res.success) {
    emit errorOccurred(qStr(res.error_message));
    return QString();
  }
  return qStr(res.value);
}

bool CoreBridge::trashItem(const QString &itemId) {
  SVM_ACTION_SCOPE_DETAIL("CoreBridge", "trashItem",
                          QStringLiteral("id=\"%1\"").arg(itemId));
  if (!m_context || itemId.isEmpty()) {
    return false;
  }
  const QByteArray idUtf8 = itemId.toUtf8();
  auto res = (*m_context)->trash_item(rStr(idUtf8));
  return res.success && res.value;
}

bool CoreBridge::restoreItem(const QString &itemId) {
  SVM_ACTION_SCOPE_DETAIL("CoreBridge", "restoreItem",
                          QStringLiteral("id=\"%1\"").arg(itemId));
  if (!m_context || itemId.isEmpty()) {
    return false;
  }
  const QByteArray idUtf8 = itemId.toUtf8();
  auto res = (*m_context)->restore_item(rStr(idUtf8));
  return res.success && res.value;
}

bool CoreBridge::purgeItem(const QString &itemId) {
  SVM_ACTION_SCOPE_DETAIL("CoreBridge", "purgeItem",
                          QStringLiteral("id=\"%1\"").arg(itemId));
  if (!m_context || itemId.isEmpty()) {
    return false;
  }
  const QByteArray idUtf8 = itemId.toUtf8();
  auto res = (*m_context)->purge_item(rStr(idUtf8));
  return res.success && res.value;
}

bool CoreBridge::emptyTrash() {
  SVM_ACTION_SCOPE("CoreBridge", "emptyTrash");
  if (!m_context) {
    return false;
  }
  const QByteArray allTrash = "__all_trash__";
  auto res = (*m_context)->purge_item(rStr(allTrash));
  return res.success && res.value;
}

QString CoreBridge::revealField(const QString &itemId, const QString &fieldId) {
  SVM_ACTION_SCOPE_DETAIL(
      "CoreBridge", "revealField",
      QStringLiteral("item=\"%1\", field=\"%2\"").arg(itemId, fieldId));
  if (!m_context || itemId.isEmpty() || fieldId.isEmpty()) {
    return QString();
  }
  const QByteArray idUtf8 = itemId.toUtf8();
  const QByteArray fldUtf8 = fieldId.toUtf8();
  auto res = (*m_context)->reveal_field(rStr(idUtf8), rStr(fldUtf8));
  if (!res.success) {
    return QString();
  }
  return qStr(res.value);
}

/* ========================================================================= */
/* Lịch sử Sửa đổi & Tệp Đính kèm                                          */
/* ========================================================================= */

QList<models::UiItemRevision> CoreBridge::listHistory(const QString &itemId) {
  QList<models::UiItemRevision> list;
  if (!m_context || itemId.isEmpty()) {
    return list;
  }
  const QByteArray idUtf8 = itemId.toUtf8();
  auto res = (*m_context)->list_history(rStr(idUtf8));
  if (!res.success) {
    return list;
  }
  const QJsonDocument doc = QJsonDocument::fromJson(
      QByteArray(res.value.data(), static_cast<qsizetype>(res.value.size())));
  const QJsonArray arr = doc.array();
  for (const auto &val : arr) {
    if (!val.isObject())
      continue;
    const QJsonObject obj = val.toObject();
    models::UiItemRevision rev;
    rev.revision = obj.value(QStringLiteral("revision")).toInt(1);
    rev.timestamp =
        static_cast<qint64>(obj.value(QStringLiteral("timestamp")).toDouble(0));
    rev.snapshot =
        itemFromJson(obj.value(QStringLiteral("snapshot")).toObject());
    list.append(rev);
  }
  return list;
}

bool CoreBridge::restoreHistory(const QString &itemId, int revision) {
  if (!m_context || itemId.isEmpty() || revision < 0) {
    return false;
  }
  const QByteArray idUtf8 = itemId.toUtf8();
  auto res = (*m_context)->restore_history(rStr(idUtf8), revision);
  if (!res.success) {
    emit errorOccurred(qStr(res.error_message));
    return false;
  }
  return true;
}

QList<models::UiAttachmentMeta>
CoreBridge::listAttachments(const QString &itemId) {
  QList<models::UiAttachmentMeta> list;
  if (!m_context || itemId.isEmpty()) {
    return list;
  }
  const QByteArray idUtf8 = itemId.toUtf8();
  auto res = (*m_context)->list_attachments(rStr(idUtf8));
  if (!res.success) {
    return list;
  }
  const QJsonDocument doc = QJsonDocument::fromJson(
      QByteArray(res.value.data(), static_cast<qsizetype>(res.value.size())));
  const QJsonArray arr = doc.array();
  for (const auto &val : arr) {
    if (!val.isObject())
      continue;
    const QJsonObject obj = val.toObject();
    models::UiAttachmentMeta meta;
    meta.id = obj.value(QStringLiteral("id")).toString();
    meta.itemId = obj.value(QStringLiteral("itemId")).toString();
    meta.fileName = obj.value(QStringLiteral("fileName")).toString();
    meta.sizeBytes = static_cast<quint64>(
        obj.value(QStringLiteral("sizeBytes")).toDouble(0));
    meta.createdAt =
        static_cast<qint64>(obj.value(QStringLiteral("createdAt")).toDouble(0));
    list.append(meta);
  }
  return list;
}

std::optional<models::UiAttachmentMeta>
CoreBridge::addAttachment(const QString &itemId, const QString &srcPath) {
  if (!m_context || itemId.isEmpty() || srcPath.isEmpty()) {
    return std::nullopt;
  }
  const QByteArray idUtf8 = itemId.toUtf8();
  const QByteArray pathUtf8 = srcPath.toUtf8();
  auto res = (*m_context)->add_attachment(rStr(idUtf8), rStr(pathUtf8));
  if (!res.success) {
    emit errorOccurred(qStr(res.error_message));
    return std::nullopt;
  }
  const QJsonObject obj =
      QJsonDocument::fromJson(
          QByteArray(res.value.data(),
                     static_cast<qsizetype>(res.value.size())))
          .object();
  models::UiAttachmentMeta meta;
  meta.id = obj.value(QStringLiteral("id")).toString();
  meta.itemId = obj.value(QStringLiteral("itemId")).toString();
  meta.fileName = obj.value(QStringLiteral("fileName")).toString();
  meta.sizeBytes =
      static_cast<quint64>(obj.value(QStringLiteral("sizeBytes")).toDouble(0));
  meta.createdAt =
      static_cast<qint64>(obj.value(QStringLiteral("createdAt")).toDouble(0));
  return meta;
}

bool CoreBridge::exportAttachment(const QString &attachmentId,
                                  const QString &dstPath) {
  if (!m_context || attachmentId.isEmpty() || dstPath.isEmpty()) {
    return false;
  }
  const QByteArray aid = attachmentId.toUtf8();
  const QByteArray dst = dstPath.toUtf8();
  auto res = (*m_context)->export_attachment(rStr(aid), rStr(dst));
  return res.success && res.value;
}

bool CoreBridge::deleteAttachment(const QString &attachmentId) {
  if (!m_context || attachmentId.isEmpty()) {
    return false;
  }
  const QByteArray aid = attachmentId.toUtf8();
  auto res = (*m_context)->delete_attachment(rStr(aid));
  if (!res.success) {
    emit errorOccurred(qStr(res.error_message));
    return false;
  }
  return true;
}

/* ========================================================================= */
/* Tiện ích Mật mã & TOTP (RFC 6238)                                        */
/* ========================================================================= */

QPair<QString, int> CoreBridge::generateTotp(const QString &itemIdOrSecret,
                                             quint64 unixTs) {
  SVM_ACTION_SCOPE("CoreBridge", "generateTotp");
  if (!m_context || itemIdOrSecret.trimmed().isEmpty()) {
    return {QStringLiteral("--- ---"), 30};
  }
  const QByteArray secretUtf8 = itemIdOrSecret.trimmed().toUtf8();
  auto res = (*m_context)->generate_totp(rStr(secretUtf8), unixTs);
  if (!res.success) {
    return {QStringLiteral("--- ---"), 30};
  }
  return {qStr(res.code), res.remaining_seconds};
}

QJsonObject CoreBridge::parseOtpAuth(const QString &uri) {
  const QByteArray uriUtf8 = uri.trimmed().toUtf8();
  auto res = svm::ffi::parse_otpauth(rStr(uriUtf8));
  if (!res.success) {
    return {};
  }
  return QJsonDocument::fromJson(
             QByteArray(res.value.data(),
                        static_cast<qsizetype>(res.value.size())))
      .object();
}

QString CoreBridge::generatePassword(int length, bool upper, bool lower,
                                     bool digits, bool symbols) {
  SVM_ACTION_SCOPE_DETAIL("CoreBridge", "generatePassword",
                          QStringLiteral("length=%1").arg(length));
  QJsonObject opts;
  opts.insert(QStringLiteral("length"), length);
  opts.insert(QStringLiteral("uppercase"), upper);
  opts.insert(QStringLiteral("lowercase"), lower);
  opts.insert(QStringLiteral("digits"), digits);
  opts.insert(QStringLiteral("symbols"), symbols);
  const QByteArray optsJson =
      QJsonDocument(opts).toJson(QJsonDocument::Compact);
  return qStr(svm::ffi::generate_password(rStr(optsJson)));
}

QJsonObject CoreBridge::evaluatePasswordStrength(const QString &password) {
  SVM_ACTION_SCOPE("CoreBridge", "evaluatePasswordStrength");
  const QByteArray pwdUtf8 = password.toUtf8();
  auto res = svm::ffi::password_strength(rStr(pwdUtf8));
  if (!res.success) {
    return {};
  }
  return QJsonDocument::fromJson(
             QByteArray(res.value.data(),
                        static_cast<qsizetype>(res.value.size())))
      .object();
}

/* ========================================================================= */
/* Nhập / Xuất Dữ liệu (Import / Export)                                      */
/* ========================================================================= */

int CoreBridge::importItems(int format, const QString &filePath,
                            const QString &password) {
  SVM_ACTION_SCOPE_DETAIL("CoreBridge", "importItems",
                          QStringLiteral("file=\"%1\"").arg(filePath));
  if (!m_context || filePath.isEmpty()) {
    return -1;
  }
  const QByteArray pathUtf8 = filePath.toUtf8();
  const QByteArray pwdUtf8 = password.toUtf8();
  auto prevRes = (*m_context)
                     ->import_preview(static_cast<int32_t>(format),
                                      rStr(pathUtf8), rStr(pwdUtf8));
  if (!prevRes.success) {
    emit errorOccurred(qStr(prevRes.error_message));
    return -1;
  }

  const QJsonObject previewObj =
      QJsonDocument::fromJson(
          QByteArray(prevRes.value.data(),
                     static_cast<qsizetype>(prevRes.value.size())))
          .object();
  const QByteArray previewIdUtf8 =
      previewObj.value(QStringLiteral("preview_id")).toString().toUtf8();
  if (previewIdUtf8.isEmpty()) {
    emit errorOccurred(tr("Invalid import preview identifier"));
    return -1;
  }

  auto execRes = (*m_context)->import_execute(rStr(previewIdUtf8));
  if (!execRes.success) {
    emit errorOccurred(qStr(execRes.error_message));
    return -1;
  }

  const QJsonObject summaryObj =
      QJsonDocument::fromJson(
          QByteArray(execRes.value.data(),
                     static_cast<qsizetype>(execRes.value.size())))
          .object();
  return summaryObj.value(QStringLiteral("imported")).toInt(0);
}

bool CoreBridge::exportItems(int format, const QString &filePath,
                             const QString &password) {
  SVM_ACTION_SCOPE_DETAIL("CoreBridge", "exportItems",
                          QStringLiteral("file=\"%1\"").arg(filePath));
  if (!m_context || filePath.isEmpty()) {
    return false;
  }
  const QByteArray pathUtf8 = filePath.toUtf8();
  const QByteArray pwdUtf8 = password.toUtf8();
  auto res = (*m_context)
                 ->export_vault(static_cast<int32_t>(format), rStr(pathUtf8),
                                rStr(pwdUtf8));
  if (!res.success) {
    emit errorOccurred(qStr(res.error_message));
    return false;
  }
  return true;
}

/* ========================================================================= */
/* SSH Agent, Browser Bridge & Cấu hình Hệ thống                              */
/* ========================================================================= */

bool CoreBridge::startSshAgent() {
  SVM_ACTION_SCOPE("CoreBridge", "startSshAgent");
  if (!m_context) {
    return false;
  }
  auto res = (*m_context)->start_ssh_agent();
  return res.success && res.value;
}

bool CoreBridge::stopSshAgent() {
  SVM_ACTION_SCOPE("CoreBridge", "stopSshAgent");
  if (!m_context) {
    return false;
  }
  auto res = (*m_context)->stop_ssh_agent();
  return res.success && res.value;
}

QJsonObject CoreBridge::sshAgentStatus() {
  if (!m_context) {
    return {};
  }
  auto res = (*m_context)->ssh_agent_status();
  if (!res.success) {
    return {};
  }
  return QJsonDocument::fromJson(
             QByteArray(res.value.data(),
                        static_cast<qsizetype>(res.value.size())))
      .object();
}

QJsonObject CoreBridge::generateSshKey(const QString &title,
                                       const QString &comment,
                                       const QString &vaultId) {
  if (!m_context) {
    return {};
  }
  const QByteArray vid = vaultId.toUtf8();
  const QByteArray tit = title.toUtf8();
  const QByteArray cmt = comment.toUtf8();
  auto res = (*m_context)->generate_ssh_key(rStr(vid), rStr(tit), rStr(cmt));
  if (!res.success) {
    return {};
  }
  QJsonObject ret;
  ret.insert(QStringLiteral("id"), qStr(res.value));
  return ret;
}

bool CoreBridge::startBridge() {
  if (!m_context) {
    return false;
  }
  auto res = (*m_context)->start_bridge();
  const bool ok = res.success && res.value;
  if (ok) {
    m_bridgeStarted = true;
  }
  return ok;
}

bool CoreBridge::stopBridge() {
  m_bridgeStarted = false;
  if (!m_context) {
    return false;
  }
  auto res = (*m_context)->stop_bridge();
  return res.success && res.value;
}

QJsonArray CoreBridge::listBridgeClients() {
  if (!m_context) {
    return {};
  }
  auto res = (*m_context)->bridge_clients_list();
  if (!res.success) {
    return {};
  }
  const QJsonDocument doc = QJsonDocument::fromJson(
      QByteArray(res.value.data(), static_cast<qsizetype>(res.value.size())));
  if (doc.isArray()) {
    return doc.array();
  }
  return doc.object().value(QStringLiteral("clients")).toArray();
}

bool CoreBridge::installNativeHostManifests() {
  if (!m_context) {
    return false;
  }
  auto res = (*m_context)->native_host_install();
  return res.success && res.value;
}

QJsonObject CoreBridge::getCoreSettings() {
  if (!m_context) {
    return {};
  }
  auto res = (*m_context)->get_settings();
  if (!res.success) {
    return {};
  }
  return QJsonDocument::fromJson(
             QByteArray(res.value.data(),
                        static_cast<qsizetype>(res.value.size())))
      .object();
}

bool CoreBridge::saveCoreSettings(const QJsonObject &settings) {
  if (!m_context) {
    return false;
  }
  const QByteArray jsonUtf8 =
      QJsonDocument(settings).toJson(QJsonDocument::Compact);
  auto res = (*m_context)->save_settings(rStr(jsonUtf8));
  return res.success && res.value;
}

bool CoreBridge::respondPrompt(const QString &requestId, bool approved) {
  if (!m_context || requestId.isEmpty()) {
    return false;
  }
  const QByteArray rid = requestId.toUtf8();
  auto res = (*m_context)->prompt_respond(rStr(rid), approved);
  return res.success && res.value;
}

QString CoreBridge::decodeQrImage(const QImage &image) {
  if (image.isNull()) {
    return QString();
  }
  const QImage rgba = image.convertToFormat(QImage::Format_RGBA8888);
  rust::Slice<const uint8_t> slice(rgba.constBits(),
                                   static_cast<size_t>(rgba.sizeInBytes()));
  auto res =
      svm::ffi::decode_qr_rgba(static_cast<uint32_t>(rgba.width()),
                               static_cast<uint32_t>(rgba.height()), slice);
  if (!res.success) {
    return QString();
  }
  return qStr(res.value);
}

QImage CoreBridge::fetchFavicon(const QString &domain) {
  if (!m_context || domain.isEmpty()) {
    return {};
  }
  const QByteArray domUtf8 = domain.toUtf8();
  auto res = (*m_context)->fetch_favicon(rStr(domUtf8));
  if (!res.success || res.data.empty()) {
    return {};
  }
  return QImage::fromData(reinterpret_cast<const uchar *>(res.data.data()),
                          static_cast<int>(res.data.size()));
}

QString CoreBridge::normalizeFaviconKey(const QString &urlOrDomain) {
  QString raw = urlOrDomain.trimmed();
  if (raw.isEmpty()) {
    return QString();
  }
  if (!raw.contains(QStringLiteral("://"))) {
    raw.prepend(QStringLiteral("https://"));
  }
  const QUrl url(raw);
  QString host = url.host().trimmed().toLower();
  while (host.endsWith(QLatin1Char('.'))) {
    host.chop(1);
  }
  if (host.isEmpty() || !host.contains(QLatin1Char('.'))) {
    return QString();
  }
  return host;
}

QImage CoreBridge::cachedFavicon(const QString &urlOrDomain) const {
  const QString key = normalizeFaviconKey(urlOrDomain);
  if (key.isEmpty()) {
    return {};
  }
  return m_faviconCache.value(key);
}

void CoreBridge::requestFaviconAsync(const QString &urlOrDomain) {
  if (m_shuttingDown.load() || !m_context ||
      !app::SettingsManager::instance().faviconFetchEnabled()) {
    return;
  }
  const QString key = normalizeFaviconKey(urlOrDomain);
  if (key.isEmpty() || m_faviconCache.contains(key) ||
      m_faviconInFlight.contains(key) || m_faviconFailed.contains(key)) {
    return;
  }

  m_faviconInFlight.insert(key);
  QPointer<CoreBridge> self(this);
  QThreadPool::globalInstance()->start([self, key]() {
    if (!self || self->m_shuttingDown.load()) {
      return;
    }
    const QImage img = self->fetchFavicon(key);
    if (!self || self->m_shuttingDown.load()) {
      return;
    }
    QMetaObject::invokeMethod(
        self,
        [self, key, img]() {
          if (!self || self->m_shuttingDown.load()) {
            return;
          }
          self->m_faviconInFlight.remove(key);
          if (!img.isNull()) {
            self->m_faviconCache.insert(key, img);
            emit self->faviconUpdated(key);
          } else if (app::SettingsManager::instance().faviconFetchEnabled()) {
            self->m_faviconFailed.insert(key);
          }
        },
        Qt::QueuedConnection);
  });
}

} // namespace svm::core
