/**
 * @file ui/views/detail/totp_widget.h
 * @brief Widget hiển thị mã xác thực 2FA/TOTP hoặc chỉnh sửa Secret Key
 * lồng trong Primary Credentials.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#pragma once

#include <QLabel>
#include <QLineEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QTimer>
#include <QWidget>

namespace svm::views {

class TotpWidget : public QWidget {
  Q_OBJECT

public:
  explicit TotpWidget(QWidget *parent = nullptr);
  ~TotpWidget() override = default;

  void setTotpSecret(const QString &secret);
  [[nodiscard]] QString totpSecret() const;
  void clearTotp();
  void focusSecretInput();

  void setEditMode(bool editable);
  [[nodiscard]] bool isEditMode() const { return m_isEditMode; }

signals:
  void copyRequested(const QString &code);
  void removeRequested();

private slots:
  void onTick();
  void onToggleRevealSecret();

private:
  void setupUI();
  void updateCodeAndTimer();
  void updateThemeIcons();

  bool m_isEditMode{false};
  bool m_isSecretRevealed{false};
  QString m_secret;
  int m_remainingSeconds{30};
  QString m_currentCode;
  QTimer m_timer;

  // View Mode widgets
  QWidget *m_viewContainer{nullptr};
  QLabel *m_codeLabel{nullptr};
  QLabel *m_timerLabel{nullptr};
  QProgressBar *m_progressBar{nullptr};
  QPushButton *m_copyBtn{nullptr};

  // Edit Mode widgets
  QWidget *m_editContainer{nullptr};
  QLineEdit *m_secretInput{nullptr};
  QPushButton *m_revealSecretBtn{nullptr};
  QPushButton *m_removeBtn{nullptr};
};

} // namespace svm::views
