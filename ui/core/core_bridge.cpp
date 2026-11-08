/**
 * @file ui/core/core_bridge.cpp
 * @brief Hiện thực lớp CoreBridge kết nối với Rust Core qua FFI C-ABI.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#include "core_bridge.h"

#include "app/dev_logger.h"

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaObject>

namespace svm::core {

namespace {

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
  // Tự động gia cố bảo vệ tiến trình ngay khi khởi tạo
  SvmStatus status = svm_harden_process();
  m_hardened = (status == SVM_OK);
  if (!m_hardened) {
    qWarning() << "[CoreBridge] svm_harden_process failed with code:" << status;
  } else {
    qInfo() << "[CoreBridge] Process memory hardened (PR_SET_DUMPABLE=0, "
               "anti-debug).";
  }
}

CoreBridge::~CoreBridge() {
  if (m_context) {
    svm_context_free(m_context);
    m_context = nullptr;
  }
}

void CoreBridge::onRustEventCallback(int32_t kind, const char *payloadJson,
                                     void *userData) {
  auto *self = static_cast<CoreBridge *>(userData);
  if (!self) {
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
  }
}

bool CoreBridge::initialize(const QString &dataDir) {
  SVM_ACTION_SCOPE_DETAIL("CoreBridge", "initialize",
                          dataDir.isEmpty() ? QStringLiteral("default")
                                            : dataDir);
  if (m_context) {
    return true;
  }

  QByteArray dirUtf8;
  const char *cDataDir = nullptr;
  if (!dataDir.isEmpty()) {
    dirUtf8 = dataDir.toUtf8();
    cDataDir = dirUtf8.constData();
  }

  m_context = svm_context_new(cDataDir);
  if (!m_context) {
    const QString err = lastError();
    qCritical() << "[CoreBridge] Failed to create SvmContext:" << err;
    emit errorOccurred(
        err.isEmpty()
            ? QStringLiteral("Failed to initialize Rust Core context.")
            : err);
    return false;
  }

  svm_set_event_callback(m_context, &CoreBridge::onRustEventCallback, this);

  qInfo() << "[CoreBridge] SvmContext initialized successfully. Core version:"
          << coreVersion();
  emit initializedChanged(true);
  return true;
}

bool CoreBridge::resetStorage() {
  SVM_ACTION_SCOPE("CoreBridge", "resetStorage");
  if (m_context) {
    svm_vault_lock(m_context);
    svm_context_free(m_context);
    m_context = nullptr;
  }

  QStringList candidateDirs;
  const QString envDir = qEnvironmentVariable("SVM_DATA_DIR");
  if (!envDir.isEmpty()) {
    candidateDirs << envDir;
  }
  candidateDirs << QDir::homePath() +
                       QStringLiteral(
                           "/.local/share/hephaestus/secure-vault-manager");
  candidateDirs << QStringLiteral("/tmp/hephaestus-svm-data");

  for (const QString &dPath : candidateDirs) {
    QDir dir(dPath);
    if (dir.exists()) {
      dir.removeRecursively();
    }
  }

  return initialize();
}

bool CoreBridge::isInitialized() const { return m_context != nullptr; }

QString CoreBridge::coreVersion() const {
  const char *ver = svm_core_version();
  return ver ? QString::fromUtf8(ver) : QStringLiteral("unknown");
}

QString CoreBridge::lastError() const {
  const char *err = svm_last_error();
  return err ? QString::fromUtf8(err) : QString();
}

SvmContext *CoreBridge::rawContext() const { return m_context; }

/* ========================================================================= */
/* Quản lý Kho (Multi-Vault & Vòng đời Khóa/Mở khóa)                          */
/* ========================================================================= */

QString CoreBridge::listVaults() {
  if (!m_context) {
    return QStringLiteral("{\"vaults\":[]}");
  }

  char *outJson = nullptr;
  SvmStatus status = svm_vault_list(m_context, &outJson);
  if (status != SVM_OK || !outJson) {
    emit errorOccurred(lastError());
    return QStringLiteral("{\"vaults\":[]}");
  }

  QString result = QString::fromUtf8(outJson);
  svm_free_string(outJson);
  return result;
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
                     .toString(QStringLiteral("folder_open"));
    v.accentColor = obj.value(QStringLiteral("accentColor"))
                        .toString(QStringLiteral("#64748B"));
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
  meta.insert(QStringLiteral("accentColor"), vault.accentColor);
  meta.insert(QStringLiteral("description"), vault.description);
  meta.insert(QStringLiteral("isDefault"), vault.isDefault);
  meta.insert(QStringLiteral("initialSetup"), initialSetup);
  if (!recoveryKey.isEmpty()) {
    meta.insert(QStringLiteral("recoveryKey"), recoveryKey);
  }

  const QByteArray metaJson =
      QJsonDocument(meta).toJson(QJsonDocument::Compact);
  const QByteArray pwdUtf8 = password.toUtf8();

  char *outId = nullptr;
  SvmStatus st = svm_vault_create(m_context, metaJson.constData(),
                                  pwdUtf8.constData(), &outId);
  if (st != SVM_OK) {
    emit errorOccurred(lastError());
    return false;
  }
  if (outId) {
    if (outVaultId) {
      *outVaultId = QString::fromUtf8(outId);
    }
    svm_free_string(outId);
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
  meta.insert(QStringLiteral("accentColor"), vault.accentColor);
  meta.insert(QStringLiteral("description"), vault.description);

  const QByteArray vidUtf8 = vault.id.toUtf8();
  const QByteArray metaJson =
      QJsonDocument(meta).toJson(QJsonDocument::Compact);
  SvmStatus st =
      svm_vault_rename(m_context, vidUtf8.constData(), metaJson.constData());
  if (st != SVM_OK) {
    emit errorOccurred(lastError());
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
  SvmStatus st =
      svm_vault_delete(m_context, vidUtf8.constData(), pwdUtf8.constData());
  if (st != SVM_OK) {
    emit errorOccurred(lastError());
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
  SvmStatus st = svm_vault_set_default(m_context, vidUtf8.constData());
  if (st != SVM_OK) {
    emit errorOccurred(lastError());
    return false;
  }
  return true;
}

bool CoreBridge::unlockVault(const QString &vaultId, const QString &password) {
  SVM_ACTION_SCOPE_DETAIL("CoreBridge", "unlockVault",
                          QStringLiteral("id=\"%1\"").arg(vaultId));
  if (!m_context) {
    return false;
  }
  const QByteArray vidUtf8 =
      (vaultId.isEmpty() ? QStringLiteral("personal") : vaultId).toUtf8();
  const QByteArray pwdUtf8 = password.toUtf8();
  SvmStatus st =
      svm_vault_unlock(m_context, vidUtf8.constData(), pwdUtf8.constData());
  if (st != SVM_OK) {
    return false;
  }
  return true;
}

bool CoreBridge::unlockWithRecoveryKey(const QString &recoveryKey,
                                       const QString &newPassword) {
  SVM_ACTION_SCOPE("CoreBridge", "unlockWithRecoveryKey");
  if (!m_context) {
    return false;
  }
  QJsonObject obj;
  obj.insert(QStringLiteral("recoveryKey"), recoveryKey.trimmed().toUpper());
  if (!newPassword.isEmpty()) {
    obj.insert(QStringLiteral("newPassword"), newPassword);
  }
  const QByteArray payload = QJsonDocument(obj).toJson(QJsonDocument::Compact);
  SvmStatus st =
      svm_vault_unlock(m_context, "__recovery__", payload.constData());
  return st == SVM_OK;
}

bool CoreBridge::unlockWithBiometrics(const QString &vaultId) {
  SVM_ACTION_SCOPE_DETAIL("CoreBridge", "unlockWithBiometrics",
                          QStringLiteral("id=\"%1\"").arg(vaultId));
  if (!m_context) {
    return false;
  }
  const QByteArray vidUtf8 = vaultId.toUtf8();
  SvmStatus st = svm_vault_unlock_biometric(m_context, vidUtf8.constData());
  return st == SVM_OK;
}

bool CoreBridge::lockVault() {
  SVM_ACTION_SCOPE("CoreBridge", "lockVault");
  if (!m_context) {
    return false;
  }
  return svm_vault_lock(m_context) == SVM_OK;
}

bool CoreBridge::isVaultUnlocked() const {
  if (!m_context) {
    return false;
  }
  return svm_vault_is_unlocked(m_context);
}

QString CoreBridge::recoveryKey() const {
  if (!m_context) {
    return QString();
  }
  char *out = nullptr;
  SvmStatus st = svm_vault_get_recovery_key(m_context, &out);
  if (st != SVM_OK || !out) {
    return QString();
  }
  QString key = QString::fromUtf8(out);
  svm_free_secret(out);
  return key;
}

QString CoreBridge::rotateRecoveryKey(const QString &masterPassword) {
  SVM_ACTION_SCOPE("CoreBridge", "rotateRecoveryKey");
  if (!m_context) {
    return QString();
  }
  char *out = nullptr;
  const QByteArray pwUtf8 = masterPassword.toUtf8();
  SvmStatus st =
      svm_vault_rotate_recovery_key(m_context, pwUtf8.constData(), &out);
  if (st != SVM_OK || !out) {
    emit errorOccurred(lastError());
    return QString();
  }
  QString newKey = QString::fromUtf8(out);
  svm_free_secret(out);
  return newKey;
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
  SvmStatus st = svm_vault_change_password(m_context, oldUtf8.constData(),
                                           newUtf8.constData());
  if (st != SVM_OK) {
    emit errorOccurred(lastError());
    return false;
  }
  return true;
}

bool CoreBridge::enableBiometric(const QString &vaultId,
                                 const QString &password) {
  if (!m_context) {
    return false;
  }
  const QByteArray vidUtf8 = vaultId.toUtf8();
  const QByteArray pwdUtf8 = password.toUtf8();
  return svm_vault_biometric_enable(m_context, vidUtf8.constData(),
                                    pwdUtf8.constData()) == SVM_OK;
}

bool CoreBridge::disableBiometric(const QString &vaultId) {
  if (!m_context) {
    return false;
  }
  const QByteArray vidUtf8 = vaultId.toUtf8();
  return svm_vault_biometric_disable(m_context, vidUtf8.constData()) == SVM_OK;
}

void CoreBridge::reportIdle(quint64 idleMs) {
  if (m_context) {
    svm_vault_report_idle(m_context, idleMs);
  }
}

/* ========================================================================= */
/* Thao tác Bản ghi (Vault Items & Trash)                                     */
/* ========================================================================= */

models::UiVaultItem CoreBridge::itemFromJson(const QJsonObject &obj) {
  models::UiVaultItem item;
  item.id = obj.value(QStringLiteral("id")).toString();
  item.vaultId =
      obj.value(QStringLiteral("vaultId")).toString(QStringLiteral("personal"));
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
  obj.insert(QStringLiteral("vaultId"), item.vaultId.isEmpty()
                                            ? QStringLiteral("personal")
                                            : item.vaultId);
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
  SVM_ACTION_SCOPE_DETAIL("CoreBridge", "listItems",
                          filterJson.isEmpty() ? QStringLiteral("all")
                                               : filterJson);
  QList<models::UiVaultItem> items;
  if (!m_context) {
    return items;
  }

  QByteArray filterUtf8;
  const char *cFilter = nullptr;
  if (!filterJson.isEmpty()) {
    filterUtf8 = filterJson.toUtf8();
    cFilter = filterUtf8.constData();
  }

  char *outJson = nullptr;
  SvmStatus st = svm_items_list(m_context, cFilter, &outJson);
  if (st != SVM_OK || !outJson) {
    return items;
  }

  const QByteArray raw = QByteArray(outJson);
  svm_free_string(outJson);

  QJsonDocument doc = QJsonDocument::fromJson(raw);
  if (!doc.isObject()) {
    return items;
  }
  const QJsonArray arr = doc.object().value(QStringLiteral("items")).toArray();
  items.reserve(arr.size());
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
  char *outJson = nullptr;
  SvmStatus st = svm_item_get(m_context, idUtf8.constData(), &outJson);
  if (st != SVM_OK || !outJson) {
    return std::nullopt;
  }
  const QByteArray raw(outJson);
  svm_free_string(outJson);

  QJsonDocument doc = QJsonDocument::fromJson(raw);
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

  char *outId = nullptr;
  SvmStatus st = svm_item_upsert(m_context, jsonUtf8.constData(), &outId);
  if (st != SVM_OK || !outId) {
    emit errorOccurred(lastError());
    return QString();
  }
  QString savedId = QString::fromUtf8(outId);
  svm_free_string(outId);
  return savedId;
}

bool CoreBridge::trashItem(const QString &itemId) {
  SVM_ACTION_SCOPE_DETAIL("CoreBridge", "trashItem",
                          QStringLiteral("id=\"%1\"").arg(itemId));
  if (!m_context || itemId.isEmpty()) {
    return false;
  }
  const QByteArray idUtf8 = itemId.toUtf8();
  return svm_item_trash(m_context, idUtf8.constData()) == SVM_OK;
}

bool CoreBridge::restoreItem(const QString &itemId) {
  SVM_ACTION_SCOPE_DETAIL("CoreBridge", "restoreItem",
                          QStringLiteral("id=\"%1\"").arg(itemId));
  if (!m_context || itemId.isEmpty()) {
    return false;
  }
  const QByteArray idUtf8 = itemId.toUtf8();
  return svm_item_restore(m_context, idUtf8.constData()) == SVM_OK;
}

bool CoreBridge::purgeItem(const QString &itemId) {
  SVM_ACTION_SCOPE_DETAIL("CoreBridge", "purgeItem",
                          QStringLiteral("id=\"%1\"").arg(itemId));
  if (!m_context || itemId.isEmpty()) {
    return false;
  }
  const QByteArray idUtf8 = itemId.toUtf8();
  return svm_item_purge(m_context, idUtf8.constData()) == SVM_OK;
}

bool CoreBridge::emptyTrash() {
  SVM_ACTION_SCOPE("CoreBridge", "emptyTrash");
  if (!m_context) {
    return false;
  }
  return svm_item_purge(m_context, "__all_trash__") == SVM_OK;
}

QString CoreBridge::revealField(const QString &itemId, const QString &fieldId) {
  SVM_ACTION_SCOPE_DETAIL(
      "CoreBridge", "revealField",
      QStringLiteral("item=\"%1\", field=\"%2\"").arg(itemId, fieldId));
  if (!m_context || itemId.isEmpty() || fieldId.isEmpty()) {
    return QString();
  }
  const QByteArray idUtf8 = itemId.toUtf8();
  const QByteArray fidUtf8 = fieldId.toUtf8();
  char *outPlain = nullptr;
  SvmStatus st = svm_item_reveal_field(m_context, idUtf8.constData(),
                                       fidUtf8.constData(), &outPlain);
  if (st != SVM_OK || !outPlain) {
    return QString();
  }
  QString val = QString::fromUtf8(outPlain);
  svm_free_secret(outPlain);
  return val;
}

/* ========================================================================= */
/* Tiện ích Mật mã & TOTP (RFC 6238)                                          */
/* ========================================================================= */

QPair<QString, int> CoreBridge::generateTotp(const QString &itemIdOrSecret,
                                             quint64 unixTs) {
  SVM_ACTION_SCOPE("CoreBridge", "generateTotp");
  if (!m_context || itemIdOrSecret.trimmed().isEmpty()) {
    return {QStringLiteral("--- ---"), 30};
  }
  const QByteArray secretUtf8 = itemIdOrSecret.trimmed().toUtf8();
  char *outCode = nullptr;
  uint32_t outRemaining = 30;
  SvmStatus st = svm_totp_generate(m_context, secretUtf8.constData(), unixTs,
                                   &outCode, &outRemaining);
  if (st != SVM_OK || !outCode) {
    return {QStringLiteral("--- ---"), 30};
  }
  QString code = QString::fromUtf8(outCode);
  svm_free_string(outCode);
  return {code, static_cast<int>(outRemaining)};
}

QJsonObject CoreBridge::parseOtpAuth(const QString &uri) {
  const QByteArray uriUtf8 = uri.trimmed().toUtf8();
  char *outJson = nullptr;
  if (svm_otpauth_parse(uriUtf8.constData(), &outJson) != SVM_OK || !outJson) {
    return {};
  }
  const QByteArray raw(outJson);
  svm_free_string(outJson);
  return QJsonDocument::fromJson(raw).object();
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

  char *outPwd = nullptr;
  if (svm_password_generate(optsJson.constData(), &outPwd) != SVM_OK ||
      !outPwd) {
    return QString();
  }
  QString pwd = QString::fromUtf8(outPwd);
  svm_free_secret(outPwd);
  return pwd;
}

QJsonObject CoreBridge::evaluatePasswordStrength(const QString &password) {
  SVM_ACTION_SCOPE("CoreBridge", "evaluatePasswordStrength");
  const QByteArray pwdUtf8 = password.toUtf8();
  char *outJson = nullptr;
  if (svm_password_strength(pwdUtf8.constData(), &outJson) != SVM_OK ||
      !outJson) {
    return {};
  }
  const QByteArray raw(outJson);
  svm_free_string(outJson);
  return QJsonDocument::fromJson(raw).object();
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
  const char *cPwd = password.isEmpty() ? nullptr : pwdUtf8.constData();

  char *previewJson = nullptr;
  SvmStatus st = svm_import_preview(m_context, static_cast<int32_t>(format),
                                    pathUtf8.constData(), cPwd, &previewJson);
  if (st != SVM_OK || !previewJson) {
    emit errorOccurred(lastError());
    return -1;
  }

  const QByteArray rawPreview(previewJson);
  svm_free_string(previewJson);

  char *summaryJson = nullptr;
  st =
      svm_import_execute(m_context, rawPreview.constData(), "{}", &summaryJson);
  if (summaryJson) {
    svm_free_string(summaryJson);
  }
  if (st != SVM_OK) {
    emit errorOccurred(lastError());
    return -1;
  }

  const QJsonObject previewObj = QJsonDocument::fromJson(rawPreview).object();
  return previewObj.value(QStringLiteral("valid_count")).toInt(0);
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
  const char *cPwd = password.isEmpty() ? nullptr : pwdUtf8.constData();

  SvmStatus st = svm_export(m_context, static_cast<int32_t>(format),
                            pathUtf8.constData(), cPwd);
  if (st != SVM_OK) {
    emit errorOccurred(lastError());
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
  return svm_ssh_agent_start(m_context) == SVM_OK;
}

bool CoreBridge::stopSshAgent() {
  SVM_ACTION_SCOPE("CoreBridge", "stopSshAgent");
  if (!m_context) {
    return false;
  }
  return svm_ssh_agent_stop(m_context) == SVM_OK;
}

QJsonObject CoreBridge::sshAgentStatus() {
  if (!m_context) {
    return {};
  }
  char *outJson = nullptr;
  if (svm_ssh_agent_status(m_context, &outJson) != SVM_OK || !outJson) {
    return {};
  }
  const QByteArray raw(outJson);
  svm_free_string(outJson);
  return QJsonDocument::fromJson(raw).object();
}

QJsonObject CoreBridge::generateSshKey(const QString &title,
                                       const QString &comment,
                                       const QString &vaultId) {
  if (!m_context) {
    return {};
  }
  QJsonObject opts;
  opts.insert(QStringLiteral("title"), title);
  opts.insert(QStringLiteral("comment"), comment);
  opts.insert(QStringLiteral("vaultId"), vaultId);
  const QByteArray optsJson =
      QJsonDocument(opts).toJson(QJsonDocument::Compact);

  char *outId = nullptr;
  if (svm_ssh_key_generate(m_context, optsJson.constData(), &outId) != SVM_OK ||
      !outId) {
    return {};
  }
  const QString itemId = QString::fromUtf8(outId);
  svm_free_string(outId);
  QJsonObject res;
  res.insert(QStringLiteral("id"), itemId);
  return res;
}

bool CoreBridge::startBridge() {
  if (!m_context) {
    return false;
  }
  return svm_bridge_start(m_context) == SVM_OK;
}

bool CoreBridge::stopBridge() {
  if (!m_context) {
    return false;
  }
  return svm_bridge_stop(m_context) == SVM_OK;
}

QString CoreBridge::beginBridgePairing() {
  if (!m_context) {
    return QString();
  }
  char *outCode = nullptr;
  if (svm_bridge_pairing_begin(m_context, &outCode) != SVM_OK || !outCode) {
    return QString();
  }
  QString code = QString::fromUtf8(outCode);
  svm_free_string(outCode);
  return code;
}

QJsonArray CoreBridge::listBridgeClients() {
  if (!m_context) {
    return {};
  }
  char *outJson = nullptr;
  if (svm_bridge_clients_list(m_context, &outJson) != SVM_OK || !outJson) {
    return {};
  }
  const QByteArray raw(outJson);
  svm_free_string(outJson);
  const QJsonObject obj = QJsonDocument::fromJson(raw).object();
  return obj.value(QStringLiteral("clients")).toArray();
}

bool CoreBridge::revokeBridgeClient(const QString &clientId) {
  if (!m_context || clientId.isEmpty()) {
    return false;
  }
  const QByteArray cidUtf8 = clientId.toUtf8();
  return svm_bridge_client_revoke(m_context, cidUtf8.constData()) == SVM_OK;
}

bool CoreBridge::installNativeHostManifests() {
  if (!m_context) {
    return false;
  }
  return svm_native_host_install(
             m_context, "[\"chrome\",\"firefox\",\"brave\",\"edge\"]") ==
         SVM_OK;
}

QJsonObject CoreBridge::getCoreSettings() {
  if (!m_context) {
    return {};
  }
  char *outJson = nullptr;
  if (svm_settings_get(m_context, &outJson) != SVM_OK || !outJson) {
    return {};
  }
  const QByteArray raw(outJson);
  svm_free_string(outJson);
  return QJsonDocument::fromJson(raw).object();
}

bool CoreBridge::saveCoreSettings(const QJsonObject &settings) {
  if (!m_context) {
    return false;
  }
  const QByteArray jsonUtf8 =
      QJsonDocument(settings).toJson(QJsonDocument::Compact);
  return svm_settings_save(m_context, jsonUtf8.constData()) == SVM_OK;
}

} // namespace svm::core
