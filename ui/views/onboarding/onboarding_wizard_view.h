/**
 * @file ui/views/onboarding/onboarding_wizard_view.h
 * @brief Giao diện thiết lập lần đầu OnboardingWizardView phong cách Centered
 * Card.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#pragma once

#include "password_entropy_meter.h"

#include <QCheckBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStackedWidget>
#include <QWidget>

namespace svm::views {

class OnboardingWizardView : public QWidget {
  Q_OBJECT

public:
  explicit OnboardingWizardView(QWidget *parent = nullptr);
  ~OnboardingWizardView() override = default;

  void resetWizard();
  void retranslateUi();

signals:
  void onboardingFinished();

protected:
  void changeEvent(QEvent *event) override;

private:
  void setupUI();
  QWidget *createStepWelcome();
  QWidget *createStepPassword();
  QWidget *createStepRecoveryKey();
  QWidget *createStepVaultSetup();

  void updateStepIndicator();
  void goToStep(int stepIndex);

  int m_currentStep{0};
  QString m_generatedRecoveryKey;

  // Header / Step indicator
  QWidget *m_cardWidget{nullptr};
  QLabel *m_stepDots[4]{nullptr};
  QLabel *m_stepTitleLabel{nullptr};

  QStackedWidget *m_stack{nullptr};

  // Step 1: Welcome
  QLabel *m_welcomeTitle{nullptr};
  QLabel *m_welcomeDesc{nullptr};
  QPushButton *m_createVaultBtn{nullptr};

  // Step 2: Master Password
  QLabel *m_pwdTitle{nullptr};
  QLabel *m_pwdDesc{nullptr};
  QLineEdit *m_pwdEdit{nullptr};
  QLineEdit *m_confirmPwdEdit{nullptr};
  PasswordEntropyMeter *m_entropyMeter{nullptr};
  QLabel *m_pwdMismatchLabel{nullptr};
  QPushButton *m_pwdBackBtn{nullptr};
  QPushButton *m_pwdNextBtn{nullptr};

  // Step 3: Recovery Key
  QLabel *m_recTitle{nullptr};
  QLabel *m_recDesc{nullptr};
  QLabel *m_recKeyLabel{nullptr};
  QPushButton *m_copyRecBtn{nullptr};
  QPushButton *m_saveRecBtn{nullptr};
  QCheckBox *m_recConfirmBox{nullptr};
  QPushButton *m_recBackBtn{nullptr};
  QPushButton *m_recNextBtn{nullptr};

  // Step 4: Initial Vault & Biometrics
  QLabel *m_vaultTitle{nullptr};
  QLabel *m_vaultDesc{nullptr};
  QLineEdit *m_vaultNameEdit{nullptr};
  QCheckBox *m_biometricCheck{nullptr};
  QPushButton *m_vaultBackBtn{nullptr};
  QPushButton *m_finishBtn{nullptr};
};

} // namespace svm::views
