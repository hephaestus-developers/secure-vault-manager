/**
 * @file ui/views/command_palette/command_palette_dialog.cpp
 * @brief Hiện thực CommandPaletteDialog nổi bật phong cách Spotlight.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#include "command_palette_dialog.h"
#include "app/clipboard_manager.h"
#include "app/theme_manager.h"
#include "views/item_list/item_card_delegate.h"

#include <QApplication>
#include <QEvent>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QIcon>
#include <QKeyEvent>
#include <QLabel>
#include <QVBoxLayout>

namespace svm::views {

CommandPaletteDialog::CommandPaletteDialog(ItemListModel *sourceModel,
                                           QWidget *parent)
    : QDialog(parent, Qt::FramelessWindowHint | Qt::Dialog) {
  setObjectName(QStringLiteral("CommandPaletteDialog"));
  setAttribute(Qt::WA_TranslucentBackground, true);
  setModal(true);
  setFixedSize(580, 420);

  m_proxyModel = new ItemSortFilterProxy(this);
  m_proxyModel->setSourceModel(sourceModel);
  m_proxyModel->setFilterType(SidebarFilterType::AllItems,
                              models::ItemCategory::Login);

  setupUI();

  // Bắt sự kiện phím từ Search Box để điều khiển danh sách
  m_searchEdit->installEventFilter(this);
}

void CommandPaletteDialog::setupUI() {
  auto *rootLayout = new QVBoxLayout(this);
  rootLayout->setContentsMargins(16, 16, 16, 16);

  auto *mainCard = new QWidget(this);
  mainCard->setObjectName(QStringLiteral("CommandPaletteCard"));

  auto *shadow = new QGraphicsDropShadowEffect(this);
  shadow->setBlurRadius(24);
  shadow->setColor(QColor(0, 0, 0, 160));
  shadow->setOffset(0, 8);
  mainCard->setGraphicsEffect(shadow);

  auto *layout = new QVBoxLayout(mainCard);
  layout->setContentsMargins(14, 14, 14, 12);
  layout->setSpacing(10);

  // 1. Ô tìm kiếm lớn
  m_searchEdit = new QLineEdit(mainCard);
  m_searchEdit->setObjectName(QStringLiteral("PaletteSearchInput"));
  m_searchEdit->setPlaceholderText(tr("Type a name or keyword to search..."));
  m_searchEdit->setClearButtonEnabled(true);
  m_searchEdit->addAction(QIcon(QStringLiteral(":/icons/search.svg")),
                          QLineEdit::LeadingPosition);
  connect(m_searchEdit, &QLineEdit::textChanged, this,
          &CommandPaletteDialog::onSearchTextChanged);
  layout->addWidget(m_searchEdit);

  // 2. Danh sách kết quả
  m_resultList = new QListView(mainCard);
  m_resultList->setObjectName(QStringLiteral("PaletteResultList"));
  m_resultList->setModel(m_proxyModel);
  m_resultList->setItemDelegate(new ItemCardDelegate(this));
  m_resultList->setUniformItemSizes(true);
  m_resultList->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
  connect(m_resultList, &QListView::activated, this,
          &CommandPaletteDialog::onActivated);
  layout->addWidget(m_resultList, 1);

  // 3. Footer hướng dẫn phím tắt
  auto *footer = new QWidget(mainCard);
  auto *footerLayout = new QHBoxLayout(footer);
  footerLayout->setContentsMargins(4, 4, 4, 0);

  auto *hintLabel = new QLabel(footer);
  hintLabel->setTextFormat(Qt::RichText);
  auto updateHint = [hintLabel]() {
    const bool isLight =
        (app::ThemeManager::instance().currentMode() == app::ThemeMode::Light);
    const QString accentColor =
        isLight ? QStringLiteral("#1E66F5") : QStringLiteral("#38BDF8");
    const QString subColor =
        isLight ? QStringLiteral("#6C6F85") : QStringLiteral("#94A3B8");
    hintLabel->setText(tr("<span style='color:%1;'><b>[Enter]</b></span> Copy "
                          "Password &nbsp;•&nbsp; "
                          "<span style='color:%1;'><b>[Tab]</b></span> Copy "
                          "Username &nbsp;•&nbsp; "
                          "<span style='color:%2;'><b>[Esc]</b> Close</span>")
                           .arg(accentColor, subColor));
  };
  updateHint();

  connect(&app::ThemeManager::instance(), &app::ThemeManager::themeChanged,
          this, [this, updateHint]() {
            updateHint();
            if (m_resultList && m_resultList->viewport()) {
              m_resultList->viewport()->update();
            }
          });

  footerLayout->addWidget(hintLabel);
  footerLayout->addStretch(1);
  layout->addWidget(footer);

  rootLayout->addWidget(mainCard);
}

void CommandPaletteDialog::openPalette() {
  m_searchEdit->clear();
  m_proxyModel->setSearchText(QString());

  if (m_proxyModel->rowCount() > 0) {
    m_resultList->setCurrentIndex(m_proxyModel->index(0, 0));
  }

  if (parentWidget()) {
    const QPoint center = parentWidget()->geometry().center();
    move(center.x() - width() / 2, center.y() - height() / 2);
  }

  show();
  m_searchEdit->setFocus();
}

void CommandPaletteDialog::onSearchTextChanged(const QString &text) {
  m_proxyModel->setSearchText(text);
  if (m_proxyModel->rowCount() > 0) {
    m_resultList->setCurrentIndex(m_proxyModel->index(0, 0));
  }
}

void CommandPaletteDialog::onActivated(const QModelIndex &index) {
  Q_UNUSED(index);
  copySelectedPassword();
}

void CommandPaletteDialog::copySelectedPassword() {
  const QModelIndex idx = m_resultList->currentIndex();
  if (!idx.isValid()) {
    return;
  }

  const auto itemVar = idx.data(ItemListModel::VaultItemRole);
  if (itemVar.canConvert<models::UiVaultItem>()) {
    const auto item = itemVar.value<models::UiVaultItem>();
    if (!item.password.isEmpty()) {
      app::ClipboardManager::instance().copySensitive(
          item.password, tr("Mật khẩu: %1").arg(item.title), 30);
    }
  }
  accept();
}

void CommandPaletteDialog::copySelectedUsername() {
  const QModelIndex idx = m_resultList->currentIndex();
  if (!idx.isValid()) {
    return;
  }

  const auto itemVar = idx.data(ItemListModel::VaultItemRole);
  if (itemVar.canConvert<models::UiVaultItem>()) {
    const auto item = itemVar.value<models::UiVaultItem>();
    if (!item.username.isEmpty()) {
      app::ClipboardManager::instance().copyPlainText(item.username);
    }
  }
  accept();
}

bool CommandPaletteDialog::eventFilter(QObject *watched, QEvent *event) {
  if (watched == m_searchEdit && event->type() == QEvent::KeyPress) {
    auto *keyEvent = static_cast<QKeyEvent *>(event);

    if (keyEvent->key() == Qt::Key_Down) {
      const int nextRow = m_resultList->currentIndex().row() + 1;
      if (nextRow < m_proxyModel->rowCount()) {
        m_resultList->setCurrentIndex(m_proxyModel->index(nextRow, 0));
      }
      return true;
    }

    if (keyEvent->key() == Qt::Key_Up) {
      const int prevRow = m_resultList->currentIndex().row() - 1;
      if (prevRow >= 0) {
        m_resultList->setCurrentIndex(m_proxyModel->index(prevRow, 0));
      }
      return true;
    }

    if (keyEvent->key() == Qt::Key_Tab) {
      copySelectedUsername();
      return true;
    }

    if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
      copySelectedPassword();
      return true;
    }

    if (keyEvent->key() == Qt::Key_Escape) {
      reject();
      return true;
    }
  }

  return QDialog::eventFilter(watched, event);
}

} // namespace svm::views
