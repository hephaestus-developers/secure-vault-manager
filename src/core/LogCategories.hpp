/**
 * @file LogCategories.hpp
 * @brief Structured QLoggingCategory definitions for Secure Vault Manager
 * subsystems.
 */

#pragma once

#include <QLoggingCategory>

/**
 * @brief Logging category for UI events, navigation, theme changes, and view
 * interactions.
 */
Q_DECLARE_LOGGING_CATEGORY(logUI)

/**
 * @brief Logging category for window lifecycle, hierarchy, modal management,
 * and single-instance handling.
 */
Q_DECLARE_LOGGING_CATEGORY(logWindow)

/**
 * @brief Logging category for vault lifecycle, cryptographic storage,
 * locking/unlocking, and items CRUD.
 */
Q_DECLARE_LOGGING_CATEGORY(logVault)

/**
 * @brief Logging category for FFI bridge operations, serialization, and native
 * cross-boundary calls.
 */
Q_DECLARE_LOGGING_CATEGORY(logFFI)

/**
 * @brief Logging category for Browser extension bridge, IPC listener, and
 * native messaging communications.
 */
Q_DECLARE_LOGGING_CATEGORY(logIPC)

/**
 * @brief Logging category for SSH Agent server, key synchronization, and sign
 * prompt approvals.
 */
Q_DECLARE_LOGGING_CATEGORY(logSSH)

#define SVM_ACTION_SCOPE(category, action)                                     \
  qCDebug(logUI) << "[" << category << "] " << action

#define SVM_ACTION_SCOPE_DETAIL(category, action, detail)                      \
  qCDebug(logUI) << "[" << category << "] " << action << "(" << (detail) << ")"

namespace svm::core {
void installCustomMessageHandler();
} // namespace svm::core
