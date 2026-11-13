/**
 * @file ui/app/clipboard_manager.cpp
 * @brief Hiện thực ClipboardManager với cơ chế tự động xóa dữ liệu nhạy cảm.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#include "clipboard_manager.h"
#include "app/settings_manager.h"

#include <QApplication>
#include <QClipboard>
#include <QCryptographicHash>
#include <QMimeData>

namespace svm::app {

ClipboardManager &ClipboardManager::instance() {
  static ClipboardManager s_instance;
  return s_instance;
}

ClipboardManager::ClipboardManager(QObject *parent) : QObject(parent) {
  m_timer.setInterval(1000);
  connect(&m_timer, &QTimer::timeout, this, [this]() {
    if (m_remainingSeconds > 0) {
      --m_remainingSeconds;
      emit countdownTick(m_remainingSeconds, m_totalSeconds);

      if (m_remainingSeconds <= 0) {
        clearClipboard();
      }
    }
  });
}

void ClipboardManager::copySensitive(const QString &text, const QString &label,
                                     int timeoutSeconds) {
  if (text.isEmpty()) {
    return;
  }

  QClipboard *clipboard = QApplication::clipboard();
  if (!clipboard) {
    return;
  }

  auto *mimeData = new QMimeData();
  mimeData->setText(text);
  // Cờ ẩn mật khẩu khỏi ứng dụng lịch sử Clipboard trên Linux/KDE (Klipper,
  // GPaste)
  mimeData->setData(QStringLiteral("x-kde-passwordManagerHint"),
                    QByteArrayLiteral("secret"));
  // Cờ ẩn mật khẩu khỏi Windows 10/11 Clipboard History (Win + V)
  mimeData->setData(
      QStringLiteral("ExcludeClipboardContentFromMonitorProcessing"),
      QByteArrayLiteral("1"));
  // Cờ ẩn mật khẩu khỏi macOS Pasteboard History
  mimeData->setData(QStringLiteral("org.nspasteboard.ConcealedType"),
                    QByteArrayLiteral("1"));

  clipboard->setMimeData(mimeData, QClipboard::Clipboard);

  // Tính hash để phát hiện nếu người dùng chép nội dung khác đè lên
  m_copiedHash = QString::fromUtf8(
      QCryptographicHash::hash(text.toUtf8(), QCryptographicHash::Sha256)
          .toHex());
  m_activeLabel = label;

  const int resolvedTimeout =
      (timeoutSeconds < 0) ? SettingsManager::instance().clipboardClearSeconds()
                           : timeoutSeconds;

  if (resolvedTimeout <= 0) {
    m_timer.stop();
    m_totalSeconds = 0;
    m_remainingSeconds = 0;
    emit sensitiveCopied(label, 0);
    return;
  }

  m_totalSeconds = resolvedTimeout;
  m_remainingSeconds = m_totalSeconds;

  m_timer.start();
  emit sensitiveCopied(label, m_totalSeconds);
  emit countdownTick(m_remainingSeconds, m_totalSeconds);
}

void ClipboardManager::copyPlainText(const QString &text) {
  QClipboard *clipboard = QApplication::clipboard();
  if (clipboard) {
    clipboard->setText(text, QClipboard::Clipboard);
  }
}

void ClipboardManager::clearClipboard() {
  m_timer.stop();
  m_remainingSeconds = 0;

  QClipboard *clipboard = QApplication::clipboard();
  if (clipboard) {
    const QString currentText = clipboard->text(QClipboard::Clipboard);
    const QString currentHash =
        QString::fromUtf8(QCryptographicHash::hash(currentText.toUtf8(),
                                                   QCryptographicHash::Sha256)
                              .toHex());

    // Chỉ xóa nếu clipboard hiện tại vẫn là dữ liệu bảo mật do ứng dụng ghi vào
    if (currentHash == m_copiedHash) {
      clipboard->clear(QClipboard::Clipboard);
    }
  }

  m_copiedHash.clear();
  m_activeLabel.clear();
  emit clipboardCleared();
}

} // namespace svm::app
