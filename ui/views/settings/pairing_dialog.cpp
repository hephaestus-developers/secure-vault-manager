/**
 * @file ui/views/settings/pairing_dialog.cpp
 * @brief Hiện thực hộp thoại hiển thị mã PIN ghép nối trình duyệt.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#include "pairing_dialog.h"
#include "app/theme_manager.h"
#include "components/dialog_drag_helper.h"
#include "core/core_bridge.h"

#include <QApplication>
#include <QClipboard>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QVBoxLayout>

namespace svm::views {

PairingDialog::PairingDialog(QWidget *parent)
    : QDialog(parent ? parent->window() : nullptr,
              Qt::Dialog | Qt::FramelessWindowHint) {
  setObjectName(QStringLiteral("PairingDialog"));
  setAttribute(Qt::WA_TranslucentBackground, true);
  setModal(false);
  setWindowModality(Qt::NonModal);
  setProperty("svmModalDialog", true);
  setFixedSize(420, 290);

  m_timer = new QTimer(this);
  connect(m_timer, &QTimer::timeout, this, &PairingDialog::onTick);

  connect(&core::CoreBridge::instance(),
          &core::CoreBridge::bridgeClientsChangedEvent, this,
          &PairingDialog::onClientPaired);

  setupUI();

  components::DialogDragHelper::makeDraggable(m_cardWidget, this);
}

int PairingDialog::exec() {
  return components::DialogDragHelper::execDialog(*this);
}

static bool s_isShowingPairingDialog = false;

bool PairingDialog::isPairingDialogOpen() {
  if (s_isShowingPairingDialog) {
    return true;
  }
  for (auto *dlg : components::DialogDragHelper::s_activeDialogs) {
    if (qobject_cast<PairingDialog *>(dlg) && dlg->isVisible()) {
      return true;
    }
  }
  for (auto *widget : QApplication::allWidgets()) {
    if (auto *pd = qobject_cast<PairingDialog *>(widget)) {
      if (pd->isVisible()) {
        return true;
      }
    }
  }
  return false;
}

void PairingDialog::showPairingDialog(const QString &pin, int expiresInSecs) {
  if (pin.trimmed().isEmpty() || s_isShowingPairingDialog) {
    return;
  }
  s_isShowingPairingDialog = true;

  QWidget *parent = components::DialogDragHelper::activeModalDialog();
  if (!parent) {
    parent = QApplication::activeWindow();
  }
  PairingDialog dlg(parent);
  dlg.startPairing(pin, expiresInSecs);
  dlg.exec();

  s_isShowingPairingDialog = false;
}

void PairingDialog::setupUI() {
  auto *rootLayout = new QVBoxLayout(this);
  rootLayout->setContentsMargins(12, 12, 12, 12);

  m_cardWidget = new QWidget(this);
  m_cardWidget->setObjectName(QStringLiteral("PairingDialogCard"));
  m_cardWidget->setAttribute(Qt::WA_StyledBackground, true);

  auto *cardLayout = new QVBoxLayout(m_cardWidget);
  cardLayout->setContentsMargins(24, 20, 24, 20);
  cardLayout->setSpacing(12);

  // 1. Tiêu đề
  m_titleLabel = new QLabel(tr("Ghép nối Tiện ích Trình duyệt"), m_cardWidget);
  m_titleLabel->setObjectName(QStringLiteral("PairingDialogTitle"));
  m_titleLabel->setAlignment(Qt::AlignCenter);
  cardLayout->addWidget(m_titleLabel);

  // 2. Mô tả
  m_descLabel = new QLabel(
      tr("Nhập mã PIN 6 số dưới đây vào tiện ích mở rộng Secure Vault Manager "
         "trên trình duyệt của bạn:"),
      m_cardWidget);
  m_descLabel->setObjectName(QStringLiteral("PairingDialogDesc"));
  m_descLabel->setAlignment(Qt::AlignCenter);
  m_descLabel->setWordWrap(true);
  cardLayout->addWidget(m_descLabel);

  // 3. Khung hiển thị PIN
  auto *pinBox = new QWidget(m_cardWidget);
  pinBox->setObjectName(QStringLiteral("PairingPinBox"));
  pinBox->setAttribute(Qt::WA_StyledBackground, true);
  auto *pinBoxLayout = new QHBoxLayout(pinBox);
  pinBoxLayout->setContentsMargins(16, 10, 16, 10);
  pinBoxLayout->setAlignment(Qt::AlignCenter);

  m_pinLabel = new QLabel(QStringLiteral("------"), pinBox);
  m_pinLabel->setObjectName(QStringLiteral("PairingPinLabel"));
  m_pinLabel->setAlignment(Qt::AlignCenter);
  pinBoxLayout->addWidget(m_pinLabel);

  cardLayout->addWidget(pinBox);

  // 4. Đồng hồ đếm ngược
  m_timerLabel = new QLabel(m_cardWidget);
  m_timerLabel->setObjectName(QStringLiteral("PairingTimerLabel"));
  m_timerLabel->setAlignment(Qt::AlignCenter);
  cardLayout->addWidget(m_timerLabel);

  // 5. Nút Thao tác
  auto *btnLayout = new QHBoxLayout();
  btnLayout->setContentsMargins(0, 4, 0, 0);
  btnLayout->setSpacing(12);

  m_copyBtn = new QPushButton(tr("Sao chép mã PIN"), m_cardWidget);
  m_copyBtn->setObjectName(QStringLiteral("PairingCopyBtn"));
  m_copyBtn->setFixedHeight(32);
  m_copyBtn->setCursor(Qt::PointingHandCursor);
  connect(m_copyBtn, &QPushButton::clicked, this, [this]() {
    if (!m_pin.isEmpty()) {
      QClipboard *cb = QApplication::clipboard();
      cb->setText(m_pin);
      m_copyBtn->setText(tr("✓ Đã sao chép"));
      QTimer::singleShot(
          2000, this, [this]() { m_copyBtn->setText(tr("Sao chép mã PIN")); });
    }
  });
  btnLayout->addWidget(m_copyBtn);

  m_closeBtn = new QPushButton(tr("Đóng"), m_cardWidget);
  m_closeBtn->setObjectName(QStringLiteral("PairingCloseBtn"));
  m_closeBtn->setFixedHeight(32);
  m_closeBtn->setCursor(Qt::PointingHandCursor);
  connect(m_closeBtn, &QPushButton::clicked, this, &QDialog::reject);
  btnLayout->addWidget(m_closeBtn);

  cardLayout->addLayout(btnLayout);
  rootLayout->addWidget(m_cardWidget);
}

void PairingDialog::startPairing(const QString &pin, int expiresInSecs) {
  m_pin = pin.trimmed();
  m_remainingSeconds = expiresInSecs > 0 ? expiresInSecs : 60;
  m_pairedSuccessfully = false;

  QString formattedPin = m_pin;
  if (formattedPin.length() == 6) {
    formattedPin =
        formattedPin.left(3) + QStringLiteral(" ") + formattedPin.mid(3);
  }
  m_pinLabel->setText(formattedPin);
  m_timerLabel->setText(
      tr("Mã có hiệu lực trong %1 giây...").arg(m_remainingSeconds));

  m_timer->start(1000);
}

void PairingDialog::onTick() {
  if (m_pairedSuccessfully) {
    return;
  }
  m_remainingSeconds--;
  if (m_remainingSeconds <= 0) {
    m_timer->stop();
    m_timerLabel->setText(tr("Mã PIN đã hết hạn. Vui lòng thử lại."));
    m_timerLabel->setStyleSheet(
        QStringLiteral("color: #f38ba8; font-weight: 600;"));
    m_pinLabel->setText(QStringLiteral("EXPIRED"));
    m_copyBtn->setEnabled(false);
  } else {
    m_timerLabel->setText(
        tr("Mã có hiệu lực trong %1 giây...").arg(m_remainingSeconds));
  }
}

void PairingDialog::onClientPaired() {
  m_pairedSuccessfully = true;
  m_timer->stop();
  m_pinLabel->setText(tr("✓ THÀNH CÔNG"));
  m_pinLabel->setStyleSheet(
      QStringLiteral("color: #a6e3a1; font-size: 24px; font-weight: 800;"));
  m_timerLabel->setText(tr("Tiện ích trình duyệt đã kết nối thành công!"));
  m_timerLabel->setStyleSheet(
      QStringLiteral("color: #a6e3a1; font-weight: 600;"));
  m_copyBtn->setVisible(false);
  m_closeBtn->setText(tr("Hoàn tất"));
  QTimer::singleShot(1200, this, &QDialog::accept);
}

void PairingDialog::keyPressEvent(QKeyEvent *event) {
  if (event->key() == Qt::Key_Escape) {
    reject();
    return;
  }
  QDialog::keyPressEvent(event);
}

} // namespace svm::views
