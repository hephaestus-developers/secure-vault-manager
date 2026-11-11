/**
 * @file ui/views/about/keyboard_shortcuts_dialog.cpp
 * @brief Hiện thực hộp thoại tra cứu phím tắt KeyboardShortcutsDialog.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#include "keyboard_shortcuts_dialog.h"
#include "app/theme_manager.h"
#include "components/window_resize_handler.h"

#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QKeyEvent>
#include <QScrollArea>
#include <QVBoxLayout>

namespace svm::views {

KeyboardShortcutsDialog::KeyboardShortcutsDialog(QWidget *parent)
    : QDialog(parent, Qt::FramelessWindowHint | Qt::Dialog) {
  setObjectName(QStringLiteral("KeyboardShortcutsDialog"));
  setAttribute(Qt::WA_TranslucentBackground, false);
  setModal(false);
  setWindowModality(Qt::NonModal);
  setMinimumSize(460, 400);
  resize(520, 470);

  setupUI();

  new components::WindowResizeHandler(this);
}

void KeyboardShortcutsDialog::setupUI() {
  auto *rootLayout = new QVBoxLayout(this);
  rootLayout->setContentsMargins(0, 0, 0, 0);

  m_cardWidget = new QWidget(this);
  m_cardWidget->setObjectName(QStringLiteral("KeyboardShortcutsDialogCard"));
  m_cardWidget->setAttribute(Qt::WA_StyledBackground, true);

  auto *cardLayout = new QVBoxLayout(m_cardWidget);
  cardLayout->setContentsMargins(0, 0, 0, 14);
  cardLayout->setSpacing(0);

  // 1. Header Bar đồng bộ AboutDialog
  m_headerWidget = new QWidget(m_cardWidget);
  m_headerWidget->setObjectName(
      QStringLiteral("KeyboardShortcutsDialogHeader"));
  m_headerWidget->setAttribute(Qt::WA_StyledBackground, true);
  m_headerWidget->setFixedHeight(38);

  auto *headerLayout = new QHBoxLayout(m_headerWidget);
  headerLayout->setContentsMargins(14, 0, 0, 0);
  headerLayout->setSpacing(8);

  auto *iconLabel = new QLabel(m_headerWidget);
  iconLabel->setPixmap(
      QIcon(QStringLiteral(":/icons/brand/app_logo.svg")).pixmap(18, 18));
  headerLayout->addWidget(iconLabel);

  m_headerTitle = new QLabel(tr("Keyboard Shortcuts"), m_headerWidget);
  m_headerTitle->setObjectName(QStringLiteral("KeyboardShortcutsDialogTitle"));
  headerLayout->addWidget(m_headerTitle);

  headerLayout->addStretch(1);

  m_closeIconBtn = new QPushButton(m_headerWidget);
  m_closeIconBtn->setObjectName(QStringLiteral("WinCloseBtn"));
  m_closeIconBtn->setFixedSize(46, 38);
  m_closeIconBtn->setIconSize(QSize(12, 12));
  m_closeIconBtn->setFlat(true);
  m_closeIconBtn->setFocusPolicy(Qt::NoFocus);
  m_closeIconBtn->setCursor(Qt::PointingHandCursor);
  m_closeIconBtn->setIcon(
      app::ThemeManager::instance().icon(QStringLiteral("win_close")));
  connect(m_closeIconBtn, &QPushButton::clicked, this, &QDialog::reject);
  connect(&app::ThemeManager::instance(), &app::ThemeManager::themeChanged,
          this, [this]() {
            if (m_closeIconBtn) {
              m_closeIconBtn->setIcon(app::ThemeManager::instance().icon(
                  QStringLiteral("win_close")));
            }
          });
  headerLayout->addWidget(m_closeIconBtn);

  cardLayout->addWidget(m_headerWidget);

  // 2. Vùng cuộn chứa các nhóm phím tắt
  auto *scrollArea = new QScrollArea(m_cardWidget);
  scrollArea->setObjectName(QStringLiteral("KeyboardShortcutsScrollArea"));
  scrollArea->setWidgetResizable(true);
  scrollArea->setFrameShape(QFrame::NoFrame);
  scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

  auto *contentWidget = new QWidget(scrollArea);
  contentWidget->setObjectName(
      QStringLiteral("KeyboardShortcutsScrollContent"));
  contentWidget->setAttribute(Qt::WA_StyledBackground, true);
  auto *contentLayout = new QVBoxLayout(contentWidget);
  contentLayout->setContentsMargins(16, 14, 16, 10);
  contentLayout->setSpacing(12);

  contentLayout->addWidget(createSectionCard(
      QT_TR_NOOP("Items & Editing"),
      {
          {QT_TR_NOOP("Create new item"), QStringLiteral("Ctrl+N")},
          {QT_TR_NOOP("Edit selected item"), QStringLiteral("Ctrl+E")},
          {QT_TR_NOOP("Save changes"), QStringLiteral("Ctrl+S")},
          {QT_TR_NOOP("Cancel editing"), QStringLiteral("Esc")},
      },
      contentWidget));

  contentLayout->addWidget(createSectionCard(
      QT_TR_NOOP("Navigation & View"),
      {
          {QT_TR_NOOP("Search vault"), QStringLiteral("Ctrl+F")},
          {QT_TR_NOOP("Toggle sidebar"), QStringLiteral("Ctrl+B")},
          {QT_TR_NOOP("Switch Dark / Light theme"), QStringLiteral("Ctrl+T")},
      },
      contentWidget));

  contentLayout->addWidget(createSectionCard(
      QT_TR_NOOP("Vault & Application"),
      {
          {QT_TR_NOOP("Lock vault"), QStringLiteral("Ctrl+L")},
          {QT_TR_NOOP("Open settings"), QStringLiteral("Ctrl+,")},
          {QT_TR_NOOP("Keyboard shortcuts"), QStringLiteral("F1")},
          {QT_TR_NOOP("Quit application"), QStringLiteral("Ctrl+Q")},
      },
      contentWidget));

  contentLayout->addStretch(1);
  scrollArea->setWidget(contentWidget);
  cardLayout->addWidget(scrollArea, 1);

  // 3. Chân hộp thoại
  auto *footerLayout = new QHBoxLayout();
  footerLayout->setContentsMargins(16, 8, 16, 0);
  footerLayout->addStretch(1);

  m_bottomCloseBtn = new QPushButton(tr("Close"), m_cardWidget);
  m_bottomCloseBtn->setObjectName(QStringLiteral("ShortcutDialogCloseBtn"));
  m_bottomCloseBtn->setFixedHeight(32);
  m_bottomCloseBtn->setMinimumWidth(92);
  m_bottomCloseBtn->setCursor(Qt::PointingHandCursor);
  connect(m_bottomCloseBtn, &QPushButton::clicked, this, &QDialog::reject);
  footerLayout->addWidget(m_bottomCloseBtn);

  cardLayout->addLayout(footerLayout);
  rootLayout->addWidget(m_cardWidget);
}

QWidget *KeyboardShortcutsDialog::createSectionCard(
    const char *titleSource, const QList<QPair<const char *, QString>> &rows,
    QWidget *parent) {
  auto *card = new QWidget(parent);
  card->setObjectName(QStringLiteral("ShortcutSectionCard"));
  card->setAttribute(Qt::WA_StyledBackground, true);

  auto *layout = new QVBoxLayout(card);
  layout->setContentsMargins(14, 10, 14, 10);
  layout->setSpacing(8);

  auto *titleLabel = new QLabel(tr(titleSource), card);
  titleLabel->setObjectName(QStringLiteral("ShortcutSectionTitle"));
  layout->addWidget(titleLabel);
  m_translatableLabels.append({titleLabel, titleSource});

  for (int i = 0; i < rows.size(); ++i) {
    if (i > 0) {
      auto *divider = new QFrame(card);
      divider->setObjectName(QStringLiteral("ShortcutRowDivider"));
      divider->setFrameShape(QFrame::HLine);
      divider->setFixedHeight(1);
      layout->addWidget(divider);
    }

    auto *rowWidget = new QWidget(card);
    auto *rowLayout = new QHBoxLayout(rowWidget);
    rowLayout->setContentsMargins(0, 2, 0, 2);
    rowLayout->setSpacing(12);

    auto *actionLabel = new QLabel(tr(rows[i].first), rowWidget);
    actionLabel->setObjectName(QStringLiteral("ShortcutActionLabel"));
    rowLayout->addWidget(actionLabel, 1);
    m_translatableLabels.append({actionLabel, rows[i].first});

    auto *keyBadge = new QLabel(rows[i].second, rowWidget);
    keyBadge->setObjectName(QStringLiteral("ShortcutKeyBadge"));
    keyBadge->setAlignment(Qt::AlignCenter);
    rowLayout->addWidget(keyBadge, 0, Qt::AlignRight | Qt::AlignVCenter);

    layout->addWidget(rowWidget);
  }

  return card;
}

void KeyboardShortcutsDialog::retranslateUi() {
  if (m_headerTitle) {
    m_headerTitle->setText(tr("Keyboard Shortcuts"));
  }
  if (m_bottomCloseBtn) {
    m_bottomCloseBtn->setText(tr("Close"));
  }
  for (const auto &ref : m_translatableLabels) {
    if (ref.label && ref.sourceText) {
      ref.label->setText(tr(ref.sourceText));
    }
  }
}

void KeyboardShortcutsDialog::changeEvent(QEvent *event) {
  if (event->type() == QEvent::LanguageChange) {
    retranslateUi();
  }
  QDialog::changeEvent(event);
}

void KeyboardShortcutsDialog::keyPressEvent(QKeyEvent *event) {
  if (event->key() == Qt::Key_Escape) {
    reject();
    return;
  }
  QDialog::keyPressEvent(event);
}

} // namespace svm::views
