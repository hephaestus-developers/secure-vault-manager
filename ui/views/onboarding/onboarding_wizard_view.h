/**
 * @file ui/views/onboarding/onboarding_wizard_view.h
 * @brief Giao diện thiết lập lần đầu OnboardingWizardView phong cách 2-Panel
 * Split Card hiện đại.
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
#include <array>

namespace svm::views {

class OnboardingWizardView : public QWidget {
  Q_OBJECT

public:
  explicit OnboardingWizardView(QWidget *parent = nullptr);
  ~OnboardingWizardView() override = default;

  void resetWizard();
  void retranslateUi();
  void goToStep(int stepIndex);

signals:
  void onboardingFinished();

protected:
  void changeEvent(QEvent *event) override;
  bool eventFilter(QObject *watched, QEvent *event) override;

private:
  void setupUI();
  QWidget *createSidebarPanel();
  QWidget *createStepWelcome();
  QWidget *createStepPassword();
  QWidget *createStepRecoveryKey();
  QWidget *createStepVaultSetup();

  void updateStepIndicator();
  void updateThemeIcons();

  int m_currentStep{0};
  QString m_generatedRecoveryKey;

  // Main Card & Layout
  QWidget *m_cardWidget{nullptr};
  QWidget *m_sidebarWidget{nullptr};

  // Left Sidebar Steps
  struct StepItemWidget {
    QWidget *container{nullptr};
    QLabel *badge{nullptr};
    QLabel *title{nullptr};
    QLabel *subtitle{nullptr};
  };
  std::array<StepItemWidget, 4> m_stepItems;

  QLabel *m_sidebarBrandTitle{nullptr};
  QLabel *m_sidebarBrandSub{nullptr};
  QLabel *m_trustTitle{nullptr};
  QLabel *m_trustSub{nullptr};
  QLabel *m_trustIconLabel{nullptr};

  // Right Stack
  QStackedWidget *m_stack{nullptr};

  // Step 1: Welcome
  QLabel *m_welcomeBadge{nullptr};
  QLabel *m_welcomeTitle{nullptr};
  QLabel *m_welcomeDesc{nullptr};
  QLabel *m_featTitle1{nullptr};
  QLabel *m_featDesc1{nullptr};
  QLabel *m_featIcon1{nullptr};
  QLabel *m_featTitle2{nullptr};
  QLabel *m_featDesc2{nullptr};
  QLabel *m_featIcon2{nullptr};
  QLabel *m_featTitle3{nullptr};
  QLabel *m_featDesc3{nullptr};
  QLabel *m_featIcon3{nullptr};
  QPushButton *m_createVaultBtn{nullptr};
  QLabel *m_welcomeFootnote{nullptr};

  // Step 2: Master Password
  QLabel *m_pwdBadge{nullptr};
  QLabel *m_pwdTitle{nullptr};
  QLabel *m_pwdDesc{nullptr};
  QLabel *m_pwdLabel{nullptr};
  QLineEdit *m_pwdEdit{nullptr};
  QPushButton *m_pwdEyeBtn{nullptr};
  PasswordEntropyMeter *m_entropyMeter{nullptr};
  QLabel *m_confirmPwdLabel{nullptr};
  QLineEdit *m_confirmPwdEdit{nullptr};
  QPushButton *m_confirmEyeBtn{nullptr};
  QLabel *m_pwdMismatchLabel{nullptr};
  QPushButton *m_pwdBackBtn{nullptr};
  QPushButton *m_pwdNextBtn{nullptr};
  bool m_isPwdRevealed{false};
  bool m_isConfirmRevealed{false};

  // Step 3: Recovery Key
  QLabel *m_recBadge{nullptr};
  QLabel *m_recTitle{nullptr};
  QLabel *m_recDesc{nullptr};
  QLabel *m_recKeyLabel{nullptr};
  QPushButton *m_copyRecBtn{nullptr};
  QPushButton *m_saveRecBtn{nullptr};
  QLabel *m_recWarnText{nullptr};
  QCheckBox *m_recConfirmBox{nullptr};
  QPushButton *m_recBackBtn{nullptr};
  QPushButton *m_recNextBtn{nullptr};

  // Step 4: Initial Vault & Biometrics
  QLabel *m_vaultBadge{nullptr};
  QLabel *m_vaultTitle{nullptr};
  QLabel *m_vaultDesc{nullptr};
  QLabel *m_vaultNameLabel{nullptr};
  QLineEdit *m_vaultNameEdit{nullptr};
  QLabel *m_bioTitle{nullptr};
  QLabel *m_bioDesc{nullptr};
  QCheckBox *m_biometricCheck{nullptr};
  QLabel *m_summaryItem1{nullptr};
  QLabel *m_summaryItem2{nullptr};
  QLabel *m_summaryItem3{nullptr};
  QPushButton *m_vaultBackBtn{nullptr};
  QPushButton *m_finishBtn{nullptr};
};

} // namespace svm::views
