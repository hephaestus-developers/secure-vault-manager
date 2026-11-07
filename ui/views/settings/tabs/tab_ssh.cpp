/**
 * @file ui/views/settings/tabs/tab_ssh.cpp
 * @brief Hiện thực giao diện Tích hợp SSH Agent daemon.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#include "tab_ssh.h"
#include "app/clipboard_manager.h"
#include "app/settings_manager.h"
#include "components/in_window_combobox.h"
#include "core/core_bridge.h"

#include <QDir>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

namespace svm::views {

TabSsh::TabSsh(QWidget *parent) : QWidget(parent) {
  setAttribute(Qt::WA_StyledBackground, true);
  setupUI();
}

void TabSsh::setupUI() {
  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(16);

  auto &settings = app::SettingsManager::instance();

  // 1. Agent Card
  m_agentCard = new SettingSectionCard(tr("SSH Agent Daemon"), this);

  m_enableCheck = new QCheckBox(m_agentCard);
  m_enableCheck->setChecked(settings.sshAgentEnabled());
  if (settings.sshAgentEnabled()) {
    core::CoreBridge::instance().startSshAgent();
  }
  connect(m_enableCheck, &QCheckBox::toggled, this, [](bool checked) {
    app::SettingsManager::instance().setSshAgentEnabled(checked);
    if (checked) {
      core::CoreBridge::instance().startSshAgent();
    } else {
      core::CoreBridge::instance().stopSshAgent();
    }
  });
  m_enableRow = new SettingRowWidget(
      tr("Enable Secure SSH Agent"),
      tr("Seamlessly provide SSH authentication keys stored in your vault to "
         "Git, SSH, and developer tools."),
      m_enableCheck, m_agentCard);
  m_agentCard->addRow(m_enableRow);

  QString sockPath = QDir::homePath() + QStringLiteral("/.svm/ssh-agent.sock");
  const auto statusObj = core::CoreBridge::instance().sshAgentStatus();
  const QString coreSock =
      statusObj.value(QStringLiteral("socket_path")).toString();
  if (!coreSock.isEmpty()) {
    sockPath = coreSock;
  }
  m_copySocketBtn = new QPushButton(tr("Copy Path"), m_agentCard);
  m_copySocketBtn->setFixedHeight(30);
  m_copySocketBtn->setCursor(Qt::PointingHandCursor);
  connect(m_copySocketBtn, &QPushButton::clicked, this, [this, sockPath]() {
    app::ClipboardManager::instance().copyPlainText(sockPath);
    m_copySocketBtn->setText(tr("Copied!"));
  });
  m_socketRow = new SettingRowWidget(tr("SSH Auth Socket"), sockPath,
                                     m_copySocketBtn, m_agentCard);
  m_agentCard->addRow(m_socketRow);

  // Snippet box
  auto *snipBox = new QWidget(m_agentCard);
  snipBox->setObjectName(QStringLiteral("SshSnippetBox"));
  snipBox->setAttribute(Qt::WA_StyledBackground, true);
  auto *snipLayout = new QVBoxLayout(snipBox);
  snipLayout->setContentsMargins(10, 8, 10, 8);

  auto *snipHint = new QLabel(
      tr("Add this line to your ~/.bashrc or ~/.zshrc file:"), snipBox);
  snipHint->setObjectName(QStringLiteral("SshSnippetHint"));
  snipLayout->addWidget(snipHint);

  auto *snipCode = new QLabel(
      QStringLiteral("export SSH_AUTH_SOCK=\"$HOME/.svm/ssh-agent.sock\""),
      snipBox);
  snipCode->setTextInteractionFlags(Qt::TextSelectableByMouse);
  snipCode->setObjectName(QStringLiteral("SshSnippetCode"));
  snipLayout->addWidget(snipCode);

  m_agentCard->addWidget(snipBox);
  layout->addWidget(m_agentCard);

  // 2. Authorization Policy Card
  m_policyCard = new SettingSectionCard(tr("Authorization Policy"), this);

  m_policyCombo = new svm::ui::InWindowComboBox(m_policyCard);
  m_policyCombo->addItem(tr("Prompt to approve each request (Recommended)"),
                         true);
  m_policyCombo->addItem(tr("Automatically allow when vault is unlocked"),
                         false);
  m_policyCombo->setCurrentIndex(settings.sshPromptApproval() ? 0 : 1);
  m_policyCombo->setFixedWidth(280);
  m_policyCombo->setFixedHeight(32);
  m_policyCombo->setCursor(Qt::PointingHandCursor);
  connect(m_policyCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, [this](int index) {
            const bool prompt = m_policyCombo->itemData(index).toBool();
            app::SettingsManager::instance().setSshPromptApproval(prompt);
          });

  m_policyRow =
      new SettingRowWidget(tr("Signing Requests"),
                           tr("Decide whether SVM should ask for explicit user "
                              "consent before signing SSH requests."),
                           m_policyCombo, m_policyCard);
  m_policyCard->addRow(m_policyRow);

  layout->addWidget(m_policyCard);
  layout->addStretch(1);
}

void TabSsh::retranslateUi() {
  if (m_agentCard) {
    m_agentCard->setTitle(tr("SSH Agent Daemon"));
  }
  if (m_enableRow) {
    m_enableRow->updateText(
        tr("Enable Secure SSH Agent"),
        tr("Seamlessly provide SSH authentication keys stored in your vault to "
           "Git, SSH, and developer tools."));
  }
  if (m_copySocketBtn) {
    m_copySocketBtn->setText(tr("Copy Path"));
  }
  if (m_policyCard) {
    m_policyCard->setTitle(tr("Authorization Policy"));
  }
  if (m_policyRow) {
    m_policyRow->updateText(tr("Signing Requests"),
                            tr("Decide whether SVM should ask for explicit "
                               "user consent before signing SSH requests."));
  }
}

} // namespace svm::views
