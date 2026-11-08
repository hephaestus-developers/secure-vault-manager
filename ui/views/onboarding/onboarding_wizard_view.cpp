/**
 * @file ui/views/onboarding/onboarding_wizard_view.cpp
 * @brief Hiện thực giao diện OnboardingWizardView dạng 2-Panel Split Card hiện
 * đại.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#include "onboarding_wizard_view.h"
#include "app/clipboard_manager.h"
#include "app/dev_logger.h"
#include "app/theme_manager.h"
#include "app/vault_manager.h"

#include <QDateTime>
#include <QEvent>
#include <QFile>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QIcon>
#include <QStandardPaths>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>

namespace svm::views {

OnboardingWizardView::OnboardingWizardView(QWidget *parent) : QWidget(parent) {
  setObjectName(QStringLiteral("OnboardingWizardView"));
  setAttribute(Qt::WA_StyledBackground, true);
  setupUI();
  resetWizard();

  connect(&app::ThemeManager::instance(), &app::ThemeManager::themeChanged,
          this, [this]() { updateThemeIcons(); });
  updateThemeIcons();
}

void OnboardingWizardView::setupUI() {
  auto *rootLayout = new QVBoxLayout(this);
  rootLayout->setAlignment(Qt::AlignCenter);
  rootLayout->setContentsMargins(20, 20, 20, 20);

  // Thẻ Card chính ở giữa màn hình (Split 2 Panel: 270px Sidebar + 570px
  // Content)
  m_cardWidget = new QWidget(this);
  m_cardWidget->setObjectName(QStringLiteral("OnboardingCard"));
  m_cardWidget->setFixedSize(840, 520);

  auto *cardLayout = new QHBoxLayout(m_cardWidget);
  cardLayout->setContentsMargins(0, 0, 0, 0);
  cardLayout->setSpacing(0);

  // 1. Cột Sidebar bên trái
  m_sidebarWidget = createSidebarPanel();
  cardLayout->addWidget(m_sidebarWidget);

  // 2. Vùng Content bên phải (Stack chứa các bước)
  auto *contentContainer = new QWidget(m_cardWidget);
  contentContainer->setObjectName(QStringLiteral("OnboardingContentPanel"));
  auto *contentLayout = new QVBoxLayout(contentContainer);
  contentLayout->setContentsMargins(32, 28, 32, 28);
  contentLayout->setSpacing(0);

  m_stack = new QStackedWidget(contentContainer);
  m_stack->addWidget(createStepWelcome());
  m_stack->addWidget(createStepPassword());
  m_stack->addWidget(createStepRecoveryKey());
  m_stack->addWidget(createStepVaultSetup());
  contentLayout->addWidget(m_stack, 1);

  cardLayout->addWidget(contentContainer, 1);
  rootLayout->addWidget(m_cardWidget);
}

QWidget *OnboardingWizardView::createSidebarPanel() {
  auto *sidebar = new QWidget(m_cardWidget);
  sidebar->setObjectName(QStringLiteral("OnboardingSidebar"));
  sidebar->setFixedWidth(280);

  auto *layout = new QVBoxLayout(sidebar);
  layout->setContentsMargins(22, 28, 18, 24);
  layout->setSpacing(16);

  // Logo & Branding
  auto *brandRow = new QHBoxLayout();
  brandRow->setSpacing(12);

  auto *logo = new QLabel(sidebar);
  logo->setPixmap(QIcon(QStringLiteral(":/icons/app_logo.svg")).pixmap(36, 36));
  brandRow->addWidget(logo);

  auto *brandTextLayout = new QVBoxLayout();
  brandTextLayout->setSpacing(2);

  m_sidebarBrandTitle = new QLabel(tr("Secure Vault"), sidebar);
  m_sidebarBrandTitle->setObjectName(QStringLiteral("OnboardingSidebarTitle"));
  QFont brandFont = m_sidebarBrandTitle->font();
  brandFont.setPointSize(14);
  brandFont.setBold(true);
  m_sidebarBrandTitle->setFont(brandFont);
  brandTextLayout->addWidget(m_sidebarBrandTitle);

  m_sidebarBrandSub = new QLabel(tr("PRIVATE & SECURE VAULT"), sidebar);
  m_sidebarBrandSub->setObjectName(QStringLiteral("OnboardingSidebarSub"));
  QFont subFont = m_sidebarBrandSub->font();
  subFont.setPointSize(9);
  subFont.setBold(true);
  m_sidebarBrandSub->setFont(subFont);
  brandTextLayout->addWidget(m_sidebarBrandSub);

  brandRow->addLayout(brandTextLayout);
  brandRow->addStretch(1);
  layout->addLayout(brandRow);

  // Đường kẻ phân cách
  auto *line = new QWidget(sidebar);
  line->setObjectName(QStringLiteral("OnboardingDivider"));
  line->setFixedHeight(1);
  layout->addWidget(line);

  layout->addSpacing(8);

  // Danh sách 4 bước tiến trình dạng Vertical Stepper
  const QString stepTitles[4] = {
      tr("Welcome"),
      tr("Master Password"),
      tr("Recovery Key"),
      tr("Vault Setup"),
  };
  const QString stepSubs[4] = {
      tr("Overview & Security"),
      tr("Your Master Key"),
      tr("Emergency Backup"),
      tr("Preferences & Finalize"),
  };

  for (int i = 0; i < 4; ++i) {
    auto *stepWidget = new QWidget(sidebar);
    stepWidget->setObjectName(QStringLiteral("OnboardingStepItem"));
    stepWidget->setCursor(Qt::PointingHandCursor);
    stepWidget->installEventFilter(this);

    auto *row = new QHBoxLayout(stepWidget);
    row->setContentsMargins(8, 6, 8, 6);
    row->setSpacing(12);

    auto *badge = new QLabel(stepWidget);
    badge->setFixedSize(24, 24);
    badge->setAlignment(Qt::AlignCenter);
    badge->setText(QString::number(i + 1));
    row->addWidget(badge);

    auto *col = new QVBoxLayout();
    col->setSpacing(2);

    auto *tLabel = new QLabel(stepTitles[i], stepWidget);
    QFont tf = tLabel->font();
    tf.setPointSize(10);
    tf.setBold(true);
    tLabel->setFont(tf);
    col->addWidget(tLabel);

    auto *sLabel = new QLabel(stepSubs[i], stepWidget);
    QFont sf = sLabel->font();
    sf.setPointSize(9.5);
    sLabel->setFont(sf);
    col->addWidget(sLabel);

    row->addLayout(col, 1);
    layout->addWidget(stepWidget);

    m_stepItems[i].container = stepWidget;
    m_stepItems[i].badge = badge;
    m_stepItems[i].title = tLabel;
    m_stepItems[i].subtitle = sLabel;
  }

  layout->addStretch(1);

  // Khối bảo chứng an ninh (Trust Badge) ở đáy cột trái
  auto *trustCard = new QWidget(sidebar);
  trustCard->setObjectName(QStringLiteral("OnboardingTrustBadge"));
  auto *trustLayout = new QHBoxLayout(trustCard);
  trustLayout->setContentsMargins(10, 10, 10, 10);
  trustLayout->setSpacing(10);

  m_trustIconLabel = new QLabel(trustCard);
  m_trustIconLabel->setPixmap(
      QIcon(QStringLiteral(":/icons/shield_check.svg")).pixmap(22, 22));
  trustLayout->addWidget(m_trustIconLabel);

  auto *trustTexts = new QVBoxLayout();
  trustTexts->setSpacing(1);

  m_trustTitle = new QLabel(tr("Completely Private"), trustCard);
  m_trustTitle->setObjectName(QStringLiteral("OnboardingTrustTitle"));
  QFont trf = m_trustTitle->font();
  trf.setPointSize(10);
  trf.setBold(true);
  m_trustTitle->setFont(trf);
  trustTexts->addWidget(m_trustTitle);

  m_trustSub = new QLabel(tr("Stored locally on device"), trustCard);
  m_trustSub->setObjectName(QStringLiteral("OnboardingTrustSub"));
  QFont trsf = m_trustSub->font();
  trsf.setPointSize(9);
  m_trustSub->setFont(trsf);
  trustTexts->addWidget(m_trustSub);

  trustLayout->addLayout(trustTexts, 1);
  layout->addWidget(trustCard);

  return sidebar;
}

QWidget *OnboardingWizardView::createStepWelcome() {
  auto *page = new QWidget(m_stack);
  auto *layout = new QVBoxLayout(page);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(12);

  // Badge bước
  m_welcomeBadge = new QLabel(tr("STEP 1 OF 4 • WELCOME"), page);
  m_welcomeBadge->setObjectName(QStringLiteral("OnboardingBadge"));
  layout->addWidget(m_welcomeBadge);

  m_welcomeTitle = new QLabel(tr("Welcome to Secure Vault Manager"), page);
  m_welcomeTitle->setObjectName(QStringLiteral("OnboardingTitle"));
  QFont tf = m_welcomeTitle->font();
  tf.setPointSize(16);
  tf.setBold(true);
  m_welcomeTitle->setFont(tf);
  layout->addWidget(m_welcomeTitle);

  m_welcomeDesc = new QLabel(
      tr("A fast, private vault designed to keep your passwords, "
         "secrets, and personal credentials safe and accessible only to you."),
      page);
  m_welcomeDesc->setObjectName(QStringLiteral("OnboardingDesc"));
  m_welcomeDesc->setWordWrap(true);
  QFont df = m_welcomeDesc->font();
  df.setPointSize(11);
  m_welcomeDesc->setFont(df);
  layout->addWidget(m_welcomeDesc);

  layout->addSpacing(4);

  // 3 Thẻ tính năng bảo mật nổi bật (Feature Cards)
  auto createFeatureCard = [page](const QString &iconName, const QString &title,
                                  const QString &desc, QLabel *&outTitle,
                                  QLabel *&outDesc,
                                  QLabel *&outIcon) -> QWidget * {
    auto *card = new QWidget(page);
    card->setObjectName(QStringLiteral("OnboardingFeatureCard"));
    auto *row = new QHBoxLayout(card);
    row->setContentsMargins(12, 10, 14, 10);
    row->setSpacing(12);

    auto *iconLbl = new QLabel(card);
    iconLbl->setPixmap(
        app::ThemeManager::instance().icon(iconName).pixmap(24, 24));
    row->addWidget(iconLbl);
    outIcon = iconLbl;

    auto *texts = new QVBoxLayout();
    texts->setSpacing(2);

    outTitle = new QLabel(title, card);
    outTitle->setObjectName(QStringLiteral("OnboardingFeatureTitle"));
    QFont f = outTitle->font();
    f.setPointSize(10.5);
    f.setBold(true);
    outTitle->setFont(f);
    texts->addWidget(outTitle);

    outDesc = new QLabel(desc, card);
    outDesc->setObjectName(QStringLiteral("OnboardingFeatureDesc"));
    outDesc->setWordWrap(true);
    QFont odf = outDesc->font();
    odf.setPointSize(10);
    outDesc->setFont(odf);
    texts->addWidget(outDesc);

    row->addLayout(texts, 1);
    return card;
  };

  layout->addWidget(createFeatureCard(
      QStringLiteral("shield_check"), tr("Private Local Storage"),
      tr("Your data is strongly encrypted and stored only on your computer. "
         "Nothing is ever sent to external servers."),
      m_featTitle1, m_featDesc1, m_featIcon1));

  layout->addWidget(createFeatureCard(
      QStringLiteral("lock_neutral"), tr("Single Master Key"),
      tr("One master password unlocks all your credentials. Simple, secure, "
         "and completely in your control."),
      m_featTitle2, m_featDesc2, m_featIcon2));

  layout->addWidget(createFeatureCard(
      QStringLiteral("category_passkey"), tr("Emergency Backup Key"),
      tr("A generated recovery key guarantees you can restore access "
         "if you ever forget your master password."),
      m_featTitle3, m_featDesc3, m_featIcon3));

  layout->addStretch(1);

  // Nút hành động chính
  m_createVaultBtn = new QPushButton(tr("Create a New Vault →"), page);
  m_createVaultBtn->setObjectName(QStringLiteral("OnboardingPrimaryBtn"));
  m_createVaultBtn->setFixedHeight(42);
  m_createVaultBtn->setCursor(Qt::PointingHandCursor);
  connect(m_createVaultBtn, &QPushButton::clicked, this,
          [this]() { goToStep(1); });
  layout->addWidget(m_createVaultBtn);

  m_welcomeFootnote = new QLabel(tr("Strictly offline & private • Your "
                                    "credentials never leave your device"),
                                 page);
  m_welcomeFootnote->setObjectName(QStringLiteral("OnboardingDesc"));
  m_welcomeFootnote->setAlignment(Qt::AlignCenter);
  QFont fnf = m_welcomeFootnote->font();
  fnf.setPointSize(10);
  m_welcomeFootnote->setFont(fnf);
  layout->addWidget(m_welcomeFootnote);

  return page;
}

QWidget *OnboardingWizardView::createStepPassword() {
  auto *page = new QWidget(m_stack);
  auto *layout = new QVBoxLayout(page);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(10);

  m_pwdBadge = new QLabel(tr("STEP 2 OF 4 • SECURITY"), page);
  m_pwdBadge->setObjectName(QStringLiteral("OnboardingBadge"));
  layout->addWidget(m_pwdBadge);

  m_pwdTitle = new QLabel(tr("Create Your Master Password"), page);
  m_pwdTitle->setObjectName(QStringLiteral("OnboardingTitle"));
  QFont tf = m_pwdTitle->font();
  tf.setPointSize(16);
  tf.setBold(true);
  m_pwdTitle->setFont(tf);
  layout->addWidget(m_pwdTitle);

  m_pwdDesc = new QLabel(
      tr("This master password protects your vault. Choose a strong, memorable "
         "passphrase.\n"
         "For your privacy, forgotten passwords cannot be reset without your "
         "emergency key."),
      page);
  m_pwdDesc->setObjectName(QStringLiteral("OnboardingDesc"));
  m_pwdDesc->setWordWrap(true);
  QFont df = m_pwdDesc->font();
  df.setPointSize(11);
  m_pwdDesc->setFont(df);
  layout->addWidget(m_pwdDesc);

  layout->addSpacing(4);

  // Trường Master Password với Nút Eye Toggle
  m_pwdLabel = new QLabel(tr("Master Password"), page);
  m_pwdLabel->setObjectName(QStringLiteral("OnboardingLabel"));
  QFont lf = m_pwdLabel->font();
  lf.setPointSize(10.5);
  lf.setBold(true);
  m_pwdLabel->setFont(lf);
  layout->addWidget(m_pwdLabel);

  auto *pwdBox = new QWidget(page);
  pwdBox->setObjectName(QStringLiteral("OnboardingInputContainer"));
  pwdBox->setFixedHeight(38);
  auto *pwdBoxLayout = new QHBoxLayout(pwdBox);
  pwdBoxLayout->setContentsMargins(10, 0, 6, 0);
  pwdBoxLayout->setSpacing(6);

  m_pwdEdit = new QLineEdit(pwdBox);
  m_pwdEdit->setObjectName(QStringLiteral("OnboardingInput"));
  m_pwdEdit->setEchoMode(QLineEdit::Password);
  m_pwdEdit->setPlaceholderText(tr("Enter master password..."));
  pwdBoxLayout->addWidget(m_pwdEdit, 1);

  m_pwdEyeBtn = new QPushButton(pwdBox);
  m_pwdEyeBtn->setObjectName(QStringLiteral("OnboardingEyeBtn"));
  m_pwdEyeBtn->setFixedSize(28, 28);
  m_pwdEyeBtn->setIconSize(QSize(16, 16));
  m_pwdEyeBtn->setCursor(Qt::PointingHandCursor);
  m_pwdEyeBtn->setToolTip(tr("Show/hide password"));
  connect(m_pwdEyeBtn, &QPushButton::clicked, this, [this]() {
    m_isPwdRevealed = !m_isPwdRevealed;
    m_pwdEdit->setEchoMode(m_isPwdRevealed ? QLineEdit::Normal
                                           : QLineEdit::Password);
    m_pwdEyeBtn->setIcon(app::ThemeManager::instance().icon(
        m_isPwdRevealed ? QStringLiteral("eye_off") : QStringLiteral("eye")));
  });
  pwdBoxLayout->addWidget(m_pwdEyeBtn);
  layout->addWidget(pwdBox);

  // Đồng hồ đo độ mạnh Entropy
  m_entropyMeter = new PasswordEntropyMeter(page);
  layout->addWidget(m_entropyMeter);

  connect(m_pwdEdit, &QLineEdit::textChanged, this,
          [this](const QString &text) {
            m_entropyMeter->setPassword(text);
            m_pwdMismatchLabel->clear();
          });

  // Trường Xác nhận Master Password với Nút Eye Toggle
  m_confirmPwdLabel = new QLabel(tr("Confirm Master Password"), page);
  m_confirmPwdLabel->setObjectName(QStringLiteral("OnboardingLabel"));
  m_confirmPwdLabel->setFont(lf);
  layout->addWidget(m_confirmPwdLabel);

  auto *confirmBox = new QWidget(page);
  confirmBox->setObjectName(QStringLiteral("OnboardingInputContainer"));
  confirmBox->setFixedHeight(38);
  auto *confirmBoxLayout = new QHBoxLayout(confirmBox);
  confirmBoxLayout->setContentsMargins(10, 0, 6, 0);
  confirmBoxLayout->setSpacing(6);

  m_confirmPwdEdit = new QLineEdit(confirmBox);
  m_confirmPwdEdit->setObjectName(QStringLiteral("OnboardingInput"));
  m_confirmPwdEdit->setEchoMode(QLineEdit::Password);
  m_confirmPwdEdit->setPlaceholderText(tr("Repeat master password..."));
  confirmBoxLayout->addWidget(m_confirmPwdEdit, 1);

  m_confirmEyeBtn = new QPushButton(confirmBox);
  m_confirmEyeBtn->setObjectName(QStringLiteral("OnboardingEyeBtn"));
  m_confirmEyeBtn->setFixedSize(28, 28);
  m_confirmEyeBtn->setIconSize(QSize(16, 16));
  m_confirmEyeBtn->setCursor(Qt::PointingHandCursor);
  m_confirmEyeBtn->setToolTip(tr("Show/hide password"));
  connect(m_confirmEyeBtn, &QPushButton::clicked, this, [this]() {
    m_isConfirmRevealed = !m_isConfirmRevealed;
    m_confirmPwdEdit->setEchoMode(m_isConfirmRevealed ? QLineEdit::Normal
                                                      : QLineEdit::Password);
    m_confirmEyeBtn->setIcon(app::ThemeManager::instance().icon(
        m_isConfirmRevealed ? QStringLiteral("eye_off")
                            : QStringLiteral("eye")));
  });
  confirmBoxLayout->addWidget(m_confirmEyeBtn);
  layout->addWidget(confirmBox);

  connect(m_confirmPwdEdit, &QLineEdit::textChanged, this,
          [this]() { m_pwdMismatchLabel->clear(); });

  m_pwdMismatchLabel = new QLabel(page);
  m_pwdMismatchLabel->setStyleSheet(
      QStringLiteral("color: #f38ba8; font-size: 11px; font-weight: 500;"));
  layout->addWidget(m_pwdMismatchLabel);

  layout->addStretch(1);

  // Thanh nút điều hướng đáy
  auto *btnLayout = new QHBoxLayout();
  m_pwdBackBtn = new QPushButton(tr("← Back"), page);
  m_pwdBackBtn->setObjectName(QStringLiteral("OnboardingSecondaryBtn"));
  m_pwdBackBtn->setFixedHeight(38);
  m_pwdBackBtn->setCursor(Qt::PointingHandCursor);
  connect(m_pwdBackBtn, &QPushButton::clicked, this, [this]() { goToStep(0); });
  btnLayout->addWidget(m_pwdBackBtn);

  btnLayout->addStretch(1);

  m_pwdNextBtn = new QPushButton(tr("Continue →"), page);
  m_pwdNextBtn->setObjectName(QStringLiteral("OnboardingPrimaryBtn"));
  m_pwdNextBtn->setFixedHeight(38);
  m_pwdNextBtn->setCursor(Qt::PointingHandCursor);
  connect(m_pwdNextBtn, &QPushButton::clicked, this, [this]() {
    const QString p1 = m_pwdEdit->text();
    const QString p2 = m_confirmPwdEdit->text();
    if (p1.isEmpty()) {
      m_pwdMismatchLabel->setText(tr("Master password cannot be empty."));
      return;
    }
    if (p1.length() < 8) {
      m_pwdMismatchLabel->setText(
          tr("Master password must be at least 8 characters."));
      return;
    }
    if (p1 != p2) {
      m_pwdMismatchLabel->setText(tr("Passwords do not match."));
      return;
    }
    m_pwdMismatchLabel->clear();
    goToStep(2);
  });
  btnLayout->addWidget(m_pwdNextBtn);

  layout->addLayout(btnLayout);
  return page;
}

QWidget *OnboardingWizardView::createStepRecoveryKey() {
  auto *page = new QWidget(m_stack);
  auto *layout = new QVBoxLayout(page);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(10);

  m_recBadge = new QLabel(tr("STEP 3 OF 4 • EMERGENCY BACKUP"), page);
  m_recBadge->setObjectName(QStringLiteral("OnboardingBadge"));
  layout->addWidget(m_recBadge);

  m_recTitle = new QLabel(tr("Save Your Emergency Recovery Key"), page);
  m_recTitle->setObjectName(QStringLiteral("OnboardingTitle"));
  QFont tf = m_recTitle->font();
  tf.setPointSize(16);
  tf.setBold(true);
  m_recTitle->setFont(tf);
  layout->addWidget(m_recTitle);

  m_recDesc = new QLabel(
      tr("This emergency key is the only way to regain access to your vault "
         "if you ever forget your master password."),
      page);
  m_recDesc->setObjectName(QStringLiteral("OnboardingDesc"));
  m_recDesc->setWordWrap(true);
  QFont df = m_recDesc->font();
  df.setPointSize(11);
  m_recDesc->setFont(df);
  layout->addWidget(m_recDesc);

  layout->addSpacing(4);

  // Hộp hiển thị mã Recovery Key
  auto *box = new QWidget(page);
  box->setObjectName(QStringLiteral("RecoveryBox"));
  auto *boxLayout = new QVBoxLayout(box);
  boxLayout->setContentsMargins(16, 14, 16, 14);
  boxLayout->setSpacing(12);

  m_recKeyLabel = new QLabel(box);
  m_recKeyLabel->setObjectName(QStringLiteral("RecoveryKeyDisplay"));
  m_recKeyLabel->setAlignment(Qt::AlignCenter);
  m_recKeyLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
  boxLayout->addWidget(m_recKeyLabel);

  auto *boxBtns = new QHBoxLayout();
  boxBtns->setSpacing(10);

  m_copyRecBtn = new QPushButton(tr("Copy Key"), box);
  m_copyRecBtn->setObjectName(QStringLiteral("RecoveryActionBtn"));
  m_copyRecBtn->setIcon(
      app::ThemeManager::instance().icon(QStringLiteral("copy")));
  m_copyRecBtn->setFixedHeight(34);
  m_copyRecBtn->setCursor(Qt::PointingHandCursor);
  connect(m_copyRecBtn, &QPushButton::clicked, this, [this]() {
    app::ClipboardManager::instance().copySensitive(
        m_generatedRecoveryKey, tr("Emergency Recovery Key"));
    m_copyRecBtn->setText(tr("Copied!"));
    QTimer::singleShot(2500, this, [this]() {
      if (m_copyRecBtn) {
        m_copyRecBtn->setText(tr("Copy Key"));
      }
    });
  });
  boxBtns->addWidget(m_copyRecBtn);

  m_saveRecBtn = new QPushButton(tr("Save to File..."), box);
  m_saveRecBtn->setObjectName(QStringLiteral("RecoveryActionBtn"));
  m_saveRecBtn->setFixedHeight(34);
  m_saveRecBtn->setCursor(Qt::PointingHandCursor);
  connect(m_saveRecBtn, &QPushButton::clicked, this, [this]() {
    const QString filePath = QFileDialog::getSaveFileName(
        this, tr("Save Emergency Recovery Key"),
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) +
            QStringLiteral("/SVM_Recovery_Key.txt"),
        tr("Text Files (*.txt);;All Files (*)"));
    if (!filePath.isEmpty()) {
      QFile file(filePath);
      if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        file.write(
            QStringLiteral("SECURE VAULT MANAGER - EMERGENCY RECOVERY KEY\n"
                           "Generated: %1\n\n"
                           "Recovery Key: %2\n\n"
                           "Keep this key confidential. Anyone with this key "
                           "can reset your master password.\n")
                .arg(QDateTime::currentDateTime().toString(Qt::ISODate),
                     m_generatedRecoveryKey)
                .toUtf8());
        file.close();
        m_saveRecBtn->setText(tr("Saved!"));
        QTimer::singleShot(2500, this, [this]() {
          if (m_saveRecBtn) {
            m_saveRecBtn->setText(tr("Save to File..."));
          }
        });
      }
    }
  });
  boxBtns->addWidget(m_saveRecBtn);

  boxLayout->addLayout(boxBtns);
  layout->addWidget(box);

  // Hộp cảnh báo màu hổ phách mềm mại
  auto *warnBox = new QWidget(page);
  warnBox->setObjectName(QStringLiteral("OnboardingWarningBox"));
  auto *warnLayout = new QHBoxLayout(warnBox);
  warnLayout->setContentsMargins(10, 8, 10, 8);
  warnLayout->setSpacing(8);

  m_recWarnText = new QLabel(
      tr("Important: Store this key safely offline. Anyone with this key "
         "can access your vault."),
      warnBox);
  m_recWarnText->setWordWrap(true);
  QFont wf = m_recWarnText->font();
  wf.setPointSize(10);
  m_recWarnText->setFont(wf);
  warnLayout->addWidget(m_recWarnText);
  layout->addWidget(warnBox);

  // Checkbox xác nhận bắt buộc
  m_recConfirmBox = new QCheckBox(
      tr("I have saved this recovery key in a secure offline location."), page);
  m_recConfirmBox->setObjectName(QStringLiteral("OnboardingLabel"));
  QFont cbf = m_recConfirmBox->font();
  cbf.setPointSize(10.5);
  cbf.setBold(true);
  m_recConfirmBox->setFont(cbf);
  m_recConfirmBox->setCursor(Qt::PointingHandCursor);
  connect(m_recConfirmBox, &QCheckBox::toggled, this,
          [this](bool checked) { m_recNextBtn->setEnabled(checked); });
  layout->addWidget(m_recConfirmBox);

  layout->addStretch(1);

  // Bottom action bar
  auto *btnLayout = new QHBoxLayout();
  m_recBackBtn = new QPushButton(tr("← Back"), page);
  m_recBackBtn->setObjectName(QStringLiteral("OnboardingSecondaryBtn"));
  m_recBackBtn->setFixedHeight(38);
  m_recBackBtn->setCursor(Qt::PointingHandCursor);
  connect(m_recBackBtn, &QPushButton::clicked, this, [this]() { goToStep(1); });
  btnLayout->addWidget(m_recBackBtn);

  btnLayout->addStretch(1);

  m_recNextBtn = new QPushButton(tr("Continue →"), page);
  m_recNextBtn->setObjectName(QStringLiteral("OnboardingPrimaryBtn"));
  m_recNextBtn->setFixedHeight(38);
  m_recNextBtn->setCursor(Qt::PointingHandCursor);
  m_recNextBtn->setEnabled(false); // Bắt buộc tick checkbox
  connect(m_recNextBtn, &QPushButton::clicked, this, [this]() { goToStep(3); });
  btnLayout->addWidget(m_recNextBtn);

  layout->addLayout(btnLayout);
  return page;
}

QWidget *OnboardingWizardView::createStepVaultSetup() {
  auto *page = new QWidget(m_stack);
  auto *layout = new QVBoxLayout(page);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(10);

  m_vaultBadge = new QLabel(tr("STEP 4 OF 4 • FINALIZE"), page);
  m_vaultBadge->setObjectName(QStringLiteral("OnboardingBadge"));
  layout->addWidget(m_vaultBadge);

  m_vaultTitle = new QLabel(tr("Personalize Your Vault"), page);
  m_vaultTitle->setObjectName(QStringLiteral("OnboardingTitle"));
  QFont tf = m_vaultTitle->font();
  tf.setPointSize(16);
  tf.setBold(true);
  m_vaultTitle->setFont(tf);
  layout->addWidget(m_vaultTitle);

  m_vaultDesc = new QLabel(
      tr("Vaults help organize passwords, keys, and cards. You can create more "
         "vaults (e.g. Work, Family) anytime from the sidebar."),
      page);
  m_vaultDesc->setObjectName(QStringLiteral("OnboardingDesc"));
  m_vaultDesc->setWordWrap(true);
  QFont df = m_vaultDesc->font();
  df.setPointSize(11);
  m_vaultDesc->setFont(df);
  layout->addWidget(m_vaultDesc);

  layout->addSpacing(4);

  // Tên Vault ban đầu
  m_vaultNameLabel = new QLabel(tr("Vault Name"), page);
  m_vaultNameLabel->setObjectName(QStringLiteral("OnboardingLabel"));
  QFont lf = m_vaultNameLabel->font();
  lf.setPointSize(10.5);
  lf.setBold(true);
  m_vaultNameLabel->setFont(lf);
  layout->addWidget(m_vaultNameLabel);

  auto *vaultBox = new QWidget(page);
  vaultBox->setObjectName(QStringLiteral("OnboardingInputContainer"));
  vaultBox->setFixedHeight(38);
  auto *vaultBoxLayout = new QHBoxLayout(vaultBox);
  vaultBoxLayout->setContentsMargins(10, 0, 6, 0);
  vaultBoxLayout->setSpacing(8);

  auto *vIcon = new QLabel(vaultBox);
  vIcon->setPixmap(
      QIcon(QStringLiteral(":/icons/folder_empty.svg")).pixmap(18, 18));
  vaultBoxLayout->addWidget(vIcon);

  m_vaultNameEdit = new QLineEdit(vaultBox);
  m_vaultNameEdit->setObjectName(QStringLiteral("OnboardingInput"));
  m_vaultNameEdit->setText(tr("Personal Vault"));
  vaultBoxLayout->addWidget(m_vaultNameEdit, 1);
  layout->addWidget(vaultBox);

  // Thẻ Cấu hình Sinh trắc học (Biometric Card)
  auto *bioCard = new QWidget(page);
  bioCard->setObjectName(QStringLiteral("OnboardingBiometricCard"));
  auto *bioLayout = new QHBoxLayout(bioCard);
  bioLayout->setContentsMargins(12, 10, 12, 10);
  bioLayout->setSpacing(12);

  auto *bioIcon = new QLabel(bioCard);
  bioIcon->setPixmap(QIcon(QStringLiteral(":/icons/lock.svg")).pixmap(24, 24));
  bioLayout->addWidget(bioIcon);

  auto *bioTexts = new QVBoxLayout();
  bioTexts->setSpacing(2);

  m_bioTitle = new QLabel(tr("Biometric Quick Unlock"), bioCard);
  m_bioTitle->setObjectName(QStringLiteral("OnboardingFeatureTitle"));
  QFont bf = m_bioTitle->font();
  bf.setPointSize(10.5);
  bf.setBold(true);
  m_bioTitle->setFont(bf);
  bioTexts->addWidget(m_bioTitle);

  m_bioDesc = new QLabel(
      tr("Unlock quickly with Touch ID, Windows Hello, or Fingerprint "
         "without typing your master password."),
      bioCard);
  m_bioDesc->setObjectName(QStringLiteral("OnboardingFeatureDesc"));
  m_bioDesc->setWordWrap(true);
  QFont bdf = m_bioDesc->font();
  bdf.setPointSize(10);
  m_bioDesc->setFont(bdf);
  bioTexts->addWidget(m_bioDesc);

  bioLayout->addLayout(bioTexts, 1);

  m_biometricCheck = new QCheckBox(bioCard);
  m_biometricCheck->setChecked(true);
  m_biometricCheck->setCursor(Qt::PointingHandCursor);
  bioLayout->addWidget(m_biometricCheck);
  layout->addWidget(bioCard);

  // Tóm tắt trạng thái sẵn sàng
  auto *summaryBox = new QWidget(page);
  summaryBox->setObjectName(QStringLiteral("OnboardingSummaryBox"));
  auto *sumLayout = new QVBoxLayout(summaryBox);
  sumLayout->setContentsMargins(12, 8, 12, 8);
  sumLayout->setSpacing(3);

  m_summaryItem1 = new QLabel(tr("Encrypted vault storage ready"), summaryBox);
  m_summaryItem1->setObjectName(QStringLiteral("OnboardingSummaryItem"));
  sumLayout->addWidget(m_summaryItem1);

  m_summaryItem2 =
      new QLabel(tr("Master password protection active"), summaryBox);
  m_summaryItem2->setObjectName(QStringLiteral("OnboardingSummaryItem"));
  sumLayout->addWidget(m_summaryItem2);

  m_summaryItem3 =
      new QLabel(tr("Emergency recovery key generated"), summaryBox);
  m_summaryItem3->setObjectName(QStringLiteral("OnboardingSummaryItem"));
  sumLayout->addWidget(m_summaryItem3);

  layout->addWidget(summaryBox);
  layout->addStretch(1);

  // Bottom action bar
  auto *btnLayout = new QHBoxLayout();
  m_vaultBackBtn = new QPushButton(tr("← Back"), page);
  m_vaultBackBtn->setObjectName(QStringLiteral("OnboardingSecondaryBtn"));
  m_vaultBackBtn->setFixedHeight(38);
  m_vaultBackBtn->setCursor(Qt::PointingHandCursor);
  connect(m_vaultBackBtn, &QPushButton::clicked, this,
          [this]() { goToStep(2); });
  btnLayout->addWidget(m_vaultBackBtn);

  btnLayout->addStretch(1);

  m_finishBtn = new QPushButton(tr("Complete Setup && Open Vault →"), page);
  m_finishBtn->setObjectName(QStringLiteral("OnboardingPrimaryBtn"));
  m_finishBtn->setFixedHeight(38);
  m_finishBtn->setCursor(Qt::PointingHandCursor);
  connect(m_finishBtn, &QPushButton::clicked, this, [this]() {
    const QString pwd = m_pwdEdit->text();
    const QString vName = m_vaultNameEdit->text().trimmed().isEmpty()
                              ? tr("Personal Vault")
                              : m_vaultNameEdit->text().trimmed();
    const bool success =
        app::VaultManager::instance().setupInitialMasterPassword(
            pwd, m_generatedRecoveryKey, vName);
    if (success) {
      emit onboardingFinished();
    }
  });
  btnLayout->addWidget(m_finishBtn);

  layout->addLayout(btnLayout);
  return page;
}

void OnboardingWizardView::goToStep(int stepIndex) {
  SVM_ACTION_SCOPE_DETAIL("Onboarding", "goToStep",
                          QStringLiteral("step=%1").arg(stepIndex + 1));
  m_currentStep = qBound(0, stepIndex, 3);
  m_stack->setCurrentIndex(m_currentStep);
  updateStepIndicator();
}

void OnboardingWizardView::updateStepIndicator() {
  const bool isLight =
      (app::ThemeManager::instance().currentMode() == app::ThemeMode::Light);

  for (int i = 0; i < 4; ++i) {
    auto &item = m_stepItems[i];
    if (!item.container || !item.badge) {
      continue;
    }

    if (i < m_currentStep) {
      // Đã hoàn thành (Completed)
      item.container->setObjectName(QStringLiteral("OnboardingStepItem"));
      item.badge->setText(QString());
      item.badge->setPixmap(
          QIcon(isLight ? QStringLiteral(":/icons/checkbox_check_light.svg")
                        : QStringLiteral(":/icons/checkbox_check_dark.svg"))
              .pixmap(12, 12));
      item.badge->setStyleSheet(
          isLight ? QStringLiteral("background-color: #1e1e2e; "
                                   "border-radius: 12px;")
                  : QStringLiteral("background-color: #ffffff; "
                                   "border-radius: 12px;"));
      item.title->setStyleSheet(
          isLight ? QStringLiteral("color: #4c4f69; font-weight: 600;")
                  : QStringLiteral("color: #cdd6f4; font-weight: 600;"));
      item.subtitle->setStyleSheet(
          isLight ? QStringLiteral("color: #5c5f77; font-weight: 500;")
                  : QStringLiteral("color: #a6adc8; font-weight: 500;"));
    } else if (i == m_currentStep) {
      // Đang active
      item.container->setObjectName(QStringLiteral("OnboardingStepItemActive"));
      item.badge->setPixmap(QPixmap());
      item.badge->setText(QString::number(i + 1));
      item.badge->setStyleSheet(
          isLight ? QStringLiteral("background-color: #1e1e2e; color: #ffffff; "
                                   "border-radius: 12px; font-weight: 700; "
                                   "font-size: 11px;")
                  : QStringLiteral("background-color: #ffffff; color: #11111b; "
                                   "border-radius: 12px; font-weight: 700; "
                                   "font-size: 11px;"));
      item.title->setStyleSheet(
          isLight ? QStringLiteral("color: #1e1e2e; font-weight: 700;")
                  : QStringLiteral("color: #ffffff; font-weight: 700;"));
      item.subtitle->setStyleSheet(
          isLight ? QStringLiteral("color: #5c5f77; font-weight: 500;")
                  : QStringLiteral("color: #a6adc8; font-weight: 500;"));
    } else {
      // Chưa tới (Upcoming)
      item.container->setObjectName(QStringLiteral("OnboardingStepItem"));
      item.badge->setPixmap(QPixmap());
      item.badge->setText(QString::number(i + 1));
      item.badge->setStyleSheet(
          isLight ? QStringLiteral(
                        "background-color: #e6e9ef; color: #8c8fa1; "
                        "border: 1px solid #ccd0da; border-radius: 12px; "
                        "font-weight: 600; font-size: 11px;")
                  : QStringLiteral(
                        "background-color: #1e1e2e; color: #6c7086; "
                        "border: 1px solid #313244; border-radius: 12px; "
                        "font-weight: 600; font-size: 11px;"));
      item.title->setStyleSheet(
          isLight ? QStringLiteral("color: #8c8fa1; font-weight: 500;")
                  : QStringLiteral("color: #6c7086; font-weight: 500;"));
      item.subtitle->setStyleSheet(isLight ? QStringLiteral("color: #bcc0cc;")
                                           : QStringLiteral("color: #45475a;"));
    }

    item.container->style()->unpolish(item.container);
    item.container->style()->polish(item.container);
  }
}

void OnboardingWizardView::updateThemeIcons() {
  auto &tm = app::ThemeManager::instance();
  if (m_copyRecBtn) {
    m_copyRecBtn->setIcon(tm.icon(QStringLiteral("copy")));
  }
  if (m_pwdEyeBtn) {
    m_pwdEyeBtn->setIcon(tm.icon(m_isPwdRevealed ? QStringLiteral("eye_off")
                                                 : QStringLiteral("eye")));
  }
  if (m_confirmEyeBtn) {
    m_confirmEyeBtn->setIcon(tm.icon(m_isConfirmRevealed
                                         ? QStringLiteral("eye_off")
                                         : QStringLiteral("eye")));
  }
  if (m_trustIconLabel) {
    m_trustIconLabel->setPixmap(
        tm.icon(QStringLiteral("shield_check")).pixmap(22, 22));
  }
  if (m_featIcon1) {
    m_featIcon1->setPixmap(
        tm.icon(QStringLiteral("shield_check")).pixmap(24, 24));
  }
  if (m_featIcon2) {
    m_featIcon2->setPixmap(
        tm.icon(QStringLiteral("lock_neutral")).pixmap(24, 24));
  }
  if (m_featIcon3) {
    m_featIcon3->setPixmap(
        tm.icon(QStringLiteral("category_passkey")).pixmap(24, 24));
  }
  updateStepIndicator();
}

bool OnboardingWizardView::eventFilter(QObject *watched, QEvent *event) {
  if (event->type() == QEvent::MouseButtonPress) {
    for (int i = 0; i < 4; ++i) {
      if (m_stepItems[i].container == watched) {
        if (i <= m_currentStep) {
          goToStep(i);
        }
        return true;
      }
    }
  }
  return QWidget::eventFilter(watched, event);
}

void OnboardingWizardView::resetWizard() {
  m_generatedRecoveryKey = app::VaultManager::generateRecoveryKey();
  if (m_recKeyLabel) {
    m_recKeyLabel->setText(m_generatedRecoveryKey);
  }
  if (m_pwdEdit) {
    m_pwdEdit->clear();
  }
  if (m_confirmPwdEdit) {
    m_confirmPwdEdit->clear();
  }
  if (m_entropyMeter) {
    m_entropyMeter->setPassword(QString());
  }
  if (m_pwdMismatchLabel) {
    m_pwdMismatchLabel->clear();
  }
  if (m_recConfirmBox) {
    m_recConfirmBox->setChecked(false);
  }
  if (m_copyRecBtn) {
    m_copyRecBtn->setText(tr("Copy Key"));
  }
  if (m_saveRecBtn) {
    m_saveRecBtn->setText(tr("Save to File..."));
  }
  m_isPwdRevealed = false;
  m_isConfirmRevealed = false;
  if (m_pwdEdit) {
    m_pwdEdit->setEchoMode(QLineEdit::Password);
  }
  if (m_confirmPwdEdit) {
    m_confirmPwdEdit->setEchoMode(QLineEdit::Password);
  }
  goToStep(0);
}

void OnboardingWizardView::retranslateUi() {
  updateStepIndicator();

  if (m_sidebarBrandTitle) {
    m_sidebarBrandTitle->setText(tr("Secure Vault"));
  }
  if (m_sidebarBrandSub) {
    m_sidebarBrandSub->setText(tr("PRIVATE & SECURE VAULT"));
  }
  if (m_trustTitle) {
    m_trustTitle->setText(tr("Completely Private"));
  }
  if (m_trustSub) {
    m_trustSub->setText(tr("Stored locally on device"));
  }

  // Step 1
  if (m_welcomeBadge) {
    m_welcomeBadge->setText(tr("STEP 1 OF 4 • WELCOME"));
  }
  if (m_welcomeTitle) {
    m_welcomeTitle->setText(tr("Welcome to Secure Vault Manager"));
  }
  if (m_welcomeDesc) {
    m_welcomeDesc->setText(tr(
        "A fast, private vault designed to keep your passwords, "
        "secrets, and personal credentials safe and accessible only to you."));
  }
  if (m_featTitle1) {
    m_featTitle1->setText(tr("Private Local Storage"));
  }
  if (m_featDesc1) {
    m_featDesc1->setText(
        tr("Your data is strongly encrypted and stored only on your computer. "
           "Nothing is ever sent to external servers."));
  }
  if (m_featTitle2) {
    m_featTitle2->setText(tr("Single Master Key"));
  }
  if (m_featDesc2) {
    m_featDesc2->setText(
        tr("One master password unlocks all your credentials. Simple, secure, "
           "and completely in your control."));
  }
  if (m_featTitle3) {
    m_featTitle3->setText(tr("Emergency Backup Key"));
  }
  if (m_featDesc3) {
    m_featDesc3->setText(
        tr("A generated recovery key guarantees you can restore access "
           "if you ever forget your master password."));
  }
  if (m_createVaultBtn) {
    m_createVaultBtn->setText(tr("Create a New Vault →"));
  }
  if (m_welcomeFootnote) {
    m_welcomeFootnote->setText(tr("Strictly offline & private • Your "
                                  "credentials never leave your device"));
  }

  // Step 2
  if (m_pwdBadge) {
    m_pwdBadge->setText(tr("STEP 2 OF 4 • SECURITY"));
  }
  if (m_pwdTitle) {
    m_pwdTitle->setText(tr("Create Your Master Password"));
  }
  if (m_pwdDesc) {
    m_pwdDesc->setText(tr(
        "This master password protects your vault. Choose a strong, memorable "
        "passphrase.\n"
        "For your privacy, forgotten passwords cannot be reset without your "
        "emergency key."));
  }
  if (m_pwdLabel) {
    m_pwdLabel->setText(tr("Master Password"));
  }
  if (m_pwdEdit) {
    m_pwdEdit->setPlaceholderText(tr("Enter master password..."));
  }
  if (m_confirmPwdLabel) {
    m_confirmPwdLabel->setText(tr("Confirm Master Password"));
  }
  if (m_confirmPwdEdit) {
    m_confirmPwdEdit->setPlaceholderText(tr("Repeat master password..."));
  }
  if (m_pwdBackBtn) {
    m_pwdBackBtn->setText(tr("← Back"));
  }
  if (m_pwdNextBtn) {
    m_pwdNextBtn->setText(tr("Continue →"));
  }

  // Step 3
  if (m_recBadge) {
    m_recBadge->setText(tr("STEP 3 OF 4 • EMERGENCY BACKUP"));
  }
  if (m_recTitle) {
    m_recTitle->setText(tr("Save Your Emergency Recovery Key"));
  }
  if (m_recDesc) {
    m_recDesc->setText(
        tr("This emergency key is the only way to regain access to your vault "
           "if you ever forget your master password."));
  }
  if (m_copyRecBtn) {
    m_copyRecBtn->setText(tr("Copy Key"));
  }
  if (m_saveRecBtn) {
    m_saveRecBtn->setText(tr("Save to File..."));
  }
  if (m_recWarnText) {
    m_recWarnText->setText(
        tr("Important: Store this key safely offline. Anyone with this key "
           "can access your vault."));
  }
  if (m_recConfirmBox) {
    m_recConfirmBox->setText(
        tr("I have saved this recovery key in a secure offline location."));
  }
  if (m_recBackBtn) {
    m_recBackBtn->setText(tr("← Back"));
  }
  if (m_recNextBtn) {
    m_recNextBtn->setText(tr("Continue →"));
  }

  // Step 4
  if (m_vaultBadge) {
    m_vaultBadge->setText(tr("STEP 4 OF 4 • FINALIZE"));
  }
  if (m_vaultTitle) {
    m_vaultTitle->setText(tr("Personalize Your Vault"));
  }
  if (m_vaultDesc) {
    m_vaultDesc->setText(tr(
        "Vaults help organize passwords, keys, and cards. You can create more "
        "vaults (e.g. Work, Family) anytime from the sidebar."));
  }
  if (m_vaultNameLabel) {
    m_vaultNameLabel->setText(tr("Vault Name"));
  }
  if (m_vaultNameEdit && m_vaultNameEdit->text().isEmpty()) {
    m_vaultNameEdit->setText(tr("Personal Vault"));
  }
  if (m_bioTitle) {
    m_bioTitle->setText(tr("Biometric Quick Unlock"));
  }
  if (m_bioDesc) {
    m_bioDesc->setText(
        tr("Unlock quickly with Touch ID, Windows Hello, or Fingerprint "
           "without typing your master password."));
  }
  if (m_summaryItem1) {
    m_summaryItem1->setText(tr("Encrypted vault storage ready"));
  }
  if (m_summaryItem2) {
    m_summaryItem2->setText(tr("Master password protection active"));
  }
  if (m_summaryItem3) {
    m_summaryItem3->setText(tr("Emergency recovery key generated"));
  }
  if (m_vaultBackBtn) {
    m_vaultBackBtn->setText(tr("← Back"));
  }
  if (m_finishBtn) {
    m_finishBtn->setText(tr("Complete Setup && Open Vault →"));
  }
}

void OnboardingWizardView::changeEvent(QEvent *event) {
  if (event->type() == QEvent::LanguageChange) {
    retranslateUi();
  }
  QWidget::changeEvent(event);
}

} // namespace svm::views
