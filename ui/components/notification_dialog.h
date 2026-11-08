/**
 * @file ui/components/notification_dialog.h
 * @brief Hộp thoại thông báo và xác nhận hiện đại chuẩn Frameless với hiệu ứng
 * rung.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#pragma once

#include <QDialog>
#include <QLabel>
#include <QPushButton>
#include <QString>

namespace svm::components {

enum class NotificationType { Information, Success, Warning, Question };

class NotificationDialog : public QDialog {
  Q_OBJECT

public:
  explicit NotificationDialog(QWidget *parent = nullptr);
  ~NotificationDialog() override = default;

  void setType(NotificationType type);
  void setTitle(const QString &title);
  void setMessage(const QString &message);
  void setButtons(const QString &confirmText,
                  const QString &cancelText = QString(),
                  bool destructive = false);

  // Static convenience methods
  static void information(QWidget *parent, const QString &title,
                          const QString &message);
  static void success(QWidget *parent, const QString &title,
                      const QString &message);
  static void warning(QWidget *parent, const QString &title,
                      const QString &message);
  static bool question(QWidget *parent, const QString &title,
                       const QString &message,
                       const QString &confirmText = QString(),
                       const QString &cancelText = QString(),
                       bool destructive = false);

protected:
  void keyPressEvent(QKeyEvent *event) override;

private:
  void setupUI();
  void updateThemeIcon();

  NotificationType m_type{NotificationType::Information};
  QWidget *m_cardWidget{nullptr};
  QWidget *m_headerWidget{nullptr};
  QLabel *m_headerIconLabel{nullptr};
  QLabel *m_titleLabel{nullptr};
  QPushButton *m_closeBtn{nullptr};

  QWidget *m_iconContainer{nullptr};
  QLabel *m_iconLabel{nullptr};
  QLabel *m_messageLabel{nullptr};

  QWidget *m_btnContainer{nullptr};
  QPushButton *m_cancelBtn{nullptr};
  QPushButton *m_confirmBtn{nullptr};
};

} // namespace svm::components
