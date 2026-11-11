/**
 * @file ui/app/clipboard_manager.h
 * @brief Quản lý sao chép dữ liệu bảo mật vào Clipboard với cơ chế tự động xóa
 * sau 30 giây.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#pragma once

#include <QObject>
#include <QString>
#include <QTimer>

namespace svm::app {

class ClipboardManager : public QObject {
  Q_OBJECT

public:
  static ClipboardManager &instance();

  void copySensitive(const QString &text,
                     const QString &label = QStringLiteral("Data"),
                     int timeoutSeconds = -1);
  void copyPlainText(const QString &text);
  void clearClipboard();

  [[nodiscard]] bool isSensitiveActive() const {
    return m_remainingSeconds > 0;
  }
  [[nodiscard]] int remainingSeconds() const { return m_remainingSeconds; }
  [[nodiscard]] QString activeLabel() const { return m_activeLabel; }

signals:
  void sensitiveCopied(const QString &label, int timeoutSeconds);
  void countdownTick(int remainingSeconds, int totalSeconds);
  void clipboardCleared();

private:
  explicit ClipboardManager(QObject *parent = nullptr);
  ~ClipboardManager() override = default;

  Q_DISABLE_COPY(ClipboardManager)

  QTimer m_timer;
  int m_remainingSeconds{0};
  int m_totalSeconds{30};
  QString m_activeLabel;
  QString m_copiedHash;
};

} // namespace svm::app
