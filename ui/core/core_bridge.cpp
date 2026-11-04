/**
 * @file ui/core/core_bridge.cpp
 * @brief Hiện thực lớp CoreBridge kết nối với Rust Core qua FFI C-ABI.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#include "core_bridge.h"
#include <QDebug>

namespace svm::core {

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

bool CoreBridge::initialize(const QString &dataDir) {
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
    emit errorOccurred(err.isEmpty() ? "Failed to initialize Rust Core context."
                                     : err);
    return false;
  }

  qInfo() << "[CoreBridge] SvmContext initialized successfully. Core version:"
          << coreVersion();
  emit initializedChanged(true);
  return true;
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

} // namespace svm::core
