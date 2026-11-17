/**
 * @file ui/views/lock/lock_screen_view.h
 * @brief Màn hình khóa bảo mật tập trung LockScreenView.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#pragma once

#include <QLabel>
#include <QLineEdit>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QWidget>

namespace svm::views {

class LockScreenView : public QWidget {
  Q_OBJECT

public:
  explicit LockScreenView(QWidget *parent = nullptr);
  ~LockScreenView() override = default;

  void resetLockScreen();
  void focusPasswordInput();
  void retranslateUi();

signals:
  void unlocked();

protected:
  void changeEvent(QEvent *event) override;
  void keyPressEvent(QKeyEvent *event) override;
  void showEvent(QShowEvent *event) override;

private:
  void setupUI();
  void attemptUnlock();
  void attemptBiometricUnlock();
  void openRecoveryDialog();
  void triggerShakeAnimation();
  void checkCapsLockState();

  QWidget *m_cardWidget{nullptr};
  QLabel *m_logoLabel{nullptr};
  QLabel *m_titleLabel{nullptr};
  QLabel *m_subtitleLabel{nullptr};

  QLineEdit *m_passwordEdit{nullptr};
  QPushButton *m_revealBtn{nullptr};
  QPushButton *m_unlockBtn{nullptr};
  QPushButton *m_biometricBtn{nullptr};

  QLabel *m_capsLockLabel{nullptr};
  QLabel *m_errorLabel{nullptr};
  QPushButton *m_forgotBtn{nullptr};

  bool m_isRevealed{false};
};

} // namespace svm::views
