/**
 * @file ui/views/detail/detail_header_widget.cpp
 * @brief Hiện thực DetailHeaderWidget với thanh tiêu đề chi tiết chuẩn mực.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#include "detail_header_widget.h"
#include "app/settings_manager.h"
#include "app/theme_manager.h"
#include "app/vault_manager.h"
#include "components/combobox_popup_helper.h"
#include "components/in_window_combobox.h"
#include "core/core_bridge.h"

#include <QHBoxLayout>
#include <QIcon>
#include <QImage>
#include <QListView>
#include <QPixmap>
#include <QVBoxLayout>

namespace svm::views {

DetailHeaderWidget::DetailHeaderWidget(QWidget *parent) : QWidget(parent) {
  setObjectName(QStringLiteral("DetailHeaderCard"));
  setAttribute(Qt::WA_StyledBackground, true);
  setupUI();
}

void DetailHeaderWidget::setupUI() {
  auto *rootLayout = new QHBoxLayout(this);
  rootLayout->setContentsMargins(18, 14, 18, 14);
  rootLayout->setSpacing(14);
  rootLayout->setAlignment(Qt::AlignVCenter);

  // 1. Icon lớn (48x48)
  m_iconLabel = new QLabel(this);
  m_iconLabel->setObjectName(QStringLiteral("DetailHeaderIcon"));
  m_iconLabel->setFixedSize(48, 48);
  m_iconLabel->setAlignment(Qt::AlignCenter);
  rootLayout->addWidget(m_iconLabel, 0, Qt::AlignVCenter);

  // 2. Khu vực nội dung trung tâm (Hàng tiêu đề + Hàng phân loại/ngày cập nhật)
  auto *contentLayout = new QVBoxLayout();
  contentLayout->setContentsMargins(0, 0, 0, 0);
  contentLayout->setSpacing(8);
  contentLayout->setAlignment(Qt::AlignVCenter);

  // --- Hàng 1: Tiêu đề (Label / Input) + Các nút thao tác (Action Buttons) ---
  auto *titleRowLayout = new QHBoxLayout();
  titleRowLayout->setContentsMargins(0, 0, 0, 0);
  titleRowLayout->setSpacing(10);
  titleRowLayout->setAlignment(Qt::AlignVCenter);

  m_titleLabel = new components::ElidedLabel(tr("Untitled Item"), this);
  m_titleLabel->setObjectName(QStringLiteral("DetailHeaderTitle"));
  QFont titleFont = m_titleLabel->font();
  titleFont.setPointSize(16);
  titleFont.setBold(true);
  m_titleLabel->setFont(titleFont);
  titleRowLayout->addWidget(m_titleLabel, 1);

  m_titleInput = new QLineEdit(this);
  m_titleInput->setObjectName(QStringLiteral("DetailHeaderTitleInput"));
  m_titleInput->setFont(titleFont);
  m_titleInput->setFixedHeight(34);
  m_titleInput->setMinimumWidth(80);
  m_titleInput->setPlaceholderText(tr("Item Title..."));
  m_titleInput->hide();
  connect(m_titleInput, &QLineEdit::returnPressed, this,
          &DetailHeaderWidget::saveRequested);
  titleRowLayout->addWidget(m_titleInput, 1);

  // Cụm nút hành động bên phải Hàng 1
  auto *actionLayout = new QHBoxLayout();
  actionLayout->setContentsMargins(0, 0, 0, 0);
  actionLayout->setSpacing(8);
  actionLayout->setAlignment(Qt::AlignVCenter);

  m_favBtn = new QPushButton(this);
  m_favBtn->setObjectName(QStringLiteral("DetailHeaderFavBtn"));
  m_favBtn->setIcon(QIcon(QStringLiteral(":/icons/actions/star.svg")));
  m_favBtn->setIconSize(QSize(18, 18));
  m_favBtn->setFixedSize(34, 34);
  m_favBtn->setToolTip(tr("Mark as favorite"));
  m_favBtn->setCursor(Qt::PointingHandCursor);
  connect(m_favBtn, &QPushButton::clicked, this, [this]() {
    m_isFavorite = !m_isFavorite;
    m_favBtn->setIcon(
        QIcon(m_isFavorite ? QStringLiteral(":/icons/actions/star_filled.svg")
                           : QStringLiteral(":/icons/actions/star.svg")));
    emit favoriteToggled(m_isFavorite);
  });
  actionLayout->addWidget(m_favBtn);

  m_historyBtn = new QPushButton(this);
  m_historyBtn->setObjectName(QStringLiteral("DetailHeaderHistoryBtn"));
  m_historyBtn->setIcon(
      app::ThemeManager::instance().icon(QStringLiteral("sort_time")));
  m_historyBtn->setIconSize(QSize(16, 16));
  m_historyBtn->setFixedSize(34, 34);
  m_historyBtn->setToolTip(tr("Revision History"));
  m_historyBtn->setCursor(Qt::PointingHandCursor);
  connect(m_historyBtn, &QPushButton::clicked, this,
          &DetailHeaderWidget::historyRequested);
  actionLayout->addWidget(m_historyBtn);

  m_editBtn = new QPushButton(this);
  m_editBtn->setObjectName(QStringLiteral("DetailHeaderEditBtn"));
  m_editBtn->setIcon(QIcon(QStringLiteral(":/icons/actions/edit.svg")));
  m_editBtn->setText(tr("Edit"));
  m_editBtn->setFixedHeight(34);
  m_editBtn->setToolTip(tr("Edit item (Ctrl+E)"));
  m_editBtn->setCursor(Qt::PointingHandCursor);
  connect(m_editBtn, &QPushButton::clicked, this,
          &DetailHeaderWidget::editRequested);
  actionLayout->addWidget(m_editBtn);

  m_restoreBtn = new QPushButton(tr("Restore"), this);
  m_restoreBtn->setObjectName(QStringLiteral("DetailHeaderRestoreBtn"));
  m_restoreBtn->setFixedHeight(34);
  m_restoreBtn->setToolTip(tr("Restore item from Trash"));
  m_restoreBtn->setCursor(Qt::PointingHandCursor);
  m_restoreBtn->hide();
  connect(m_restoreBtn, &QPushButton::clicked, this,
          &DetailHeaderWidget::restoreRequested);
  actionLayout->addWidget(m_restoreBtn);

  m_deleteBtn = new QPushButton(this);
  m_deleteBtn->setObjectName(QStringLiteral("DetailHeaderDeleteBtn"));
  m_deleteBtn->setIcon(
      app::ThemeManager::instance().icon(QStringLiteral("trash")));
  m_deleteBtn->setIconSize(QSize(16, 16));
  m_deleteBtn->setFixedHeight(34);
  m_deleteBtn->setToolTip(tr("Move to Trash"));
  m_deleteBtn->setCursor(Qt::PointingHandCursor);
  connect(m_deleteBtn, &QPushButton::clicked, this,
          &DetailHeaderWidget::deleteRequested);
  actionLayout->addWidget(m_deleteBtn);

  m_cancelBtn = new QPushButton(this);
  m_cancelBtn->setObjectName(QStringLiteral("DetailHeaderCancelBtn"));
  m_cancelBtn->setText(tr("Cancel"));
  m_cancelBtn->setFixedHeight(34);
  m_cancelBtn->setToolTip(tr("Discard changes (Esc)"));
  m_cancelBtn->setCursor(Qt::PointingHandCursor);
  m_cancelBtn->hide();
  connect(m_cancelBtn, &QPushButton::clicked, this,
          &DetailHeaderWidget::cancelRequested);
  actionLayout->addWidget(m_cancelBtn);

  m_saveBtn = new QPushButton(this);
  m_saveBtn->setObjectName(QStringLiteral("DetailHeaderSaveBtn"));
  m_saveBtn->setText(tr("Save"));
  m_saveBtn->setFixedHeight(34);
  m_saveBtn->setToolTip(tr("Save changes (Ctrl+S)"));
  m_saveBtn->setCursor(Qt::PointingHandCursor);
  m_saveBtn->hide();
  connect(m_saveBtn, &QPushButton::clicked, this,
          &DetailHeaderWidget::saveRequested);
  actionLayout->addWidget(m_saveBtn);

  titleRowLayout->addLayout(actionLayout, 0);
  contentLayout->addLayout(titleRowLayout);

  // --- Hàng 2: Phân loại danh mục (Badge / ComboBox) + Thời gian cập nhật ---
  auto *metaRowLayout = new QHBoxLayout();
  metaRowLayout->setContentsMargins(0, 0, 0, 0);
  metaRowLayout->setSpacing(10);
  metaRowLayout->setAlignment(Qt::AlignVCenter);

  m_categoryBadge = new QLabel(this);
  m_categoryBadge->setObjectName(QStringLiteral("DetailCategoryBadge"));
  metaRowLayout->addWidget(m_categoryBadge);

  auto *catCombo = new svm::ui::InWindowComboBox(this);
  catCombo->setObjectName(QStringLiteral("DetailCategoryCombo"));
  catCombo->setFixedHeight(28);
  catCombo->setMinimumWidth(150);
  catCombo->setMaxVisibleItems(9);
  catCombo->setPopupMinWidth(190);
  m_categoryCombo = catCombo;

  const std::array<models::ItemCategory, 9> allCategories = {
      models::ItemCategory::Login,      models::ItemCategory::Passkey,
      models::ItemCategory::Card,       models::ItemCategory::Identity,
      models::ItemCategory::SecureNote, models::ItemCategory::SshKey,
      models::ItemCategory::Database,   models::ItemCategory::Server,
      models::ItemCategory::Wifi};

  for (auto cat : allCategories) {
    m_categoryCombo->addItem(QIcon(models::categoryIconPath(cat)),
                             models::categoryDisplayName(cat),
                             static_cast<int>(cat));
  }
  m_categoryCombo->hide();
  connect(m_categoryCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &DetailHeaderWidget::onCategoryIndexChanged);
  metaRowLayout->addWidget(m_categoryCombo);

  m_updatedLabel = new QLabel(this);
  m_updatedLabel->setObjectName(QStringLiteral("DetailUpdatedLabel"));
  metaRowLayout->addWidget(m_updatedLabel);

  metaRowLayout->addStretch(1);
  contentLayout->addLayout(metaRowLayout);

  rootLayout->addLayout(contentLayout, 1);

  connect(&app::ThemeManager::instance(), &app::ThemeManager::themeChanged,
          this, [this]() { updateThemeIcons(); });
  connect(&core::CoreBridge::instance(), &core::CoreBridge::faviconUpdated,
          this, [this](const QString &domainKey) {
            if (domainKey.isEmpty() || core::CoreBridge::normalizeFaviconKey(
                                           m_currentWebsiteUrl) == domainKey) {
              updateHeaderIcon();
            }
          });

  updateThemeIcons();
}

void DetailHeaderWidget::updateHeaderIcon() {
  if (!m_iconLabel) {
    return;
  }
  auto &bridge = core::CoreBridge::instance();
  if (!m_currentWebsiteUrl.isEmpty() &&
      app::SettingsManager::instance().faviconFetchEnabled() &&
      bridge.isInitialized()) {
    const QImage fav = bridge.cachedFavicon(m_currentWebsiteUrl);
    if (!fav.isNull()) {
      const QImage scaled =
          fav.scaled(32, 32, Qt::KeepAspectRatio, Qt::SmoothTransformation);
      m_iconLabel->setPixmap(QPixmap::fromImage(scaled));
      return;
    }
    bridge.requestFaviconAsync(m_currentWebsiteUrl);
  }
  const bool isLight =
      (app::ThemeManager::instance().currentMode() == app::ThemeMode::Light);
  m_iconLabel->setPixmap(
      QIcon(models::categoryIconPath(m_category, isLight)).pixmap(32, 32));
}

void DetailHeaderWidget::updateThemeIcons() {
  auto &tm = app::ThemeManager::instance();

  updateHeaderIcon();
  if (m_favBtn) {
    m_favBtn->setIcon(tm.icon(m_isFavorite ? QStringLiteral("star_filled")
                                           : QStringLiteral("star")));
  }
  if (m_editBtn) {
    m_editBtn->setIcon(tm.icon(QStringLiteral("edit")));
  }
  if (m_historyBtn) {
    m_historyBtn->setIcon(tm.icon(QStringLiteral("sort_time")));
  }
  if (m_deleteBtn) {
    m_deleteBtn->setIcon(tm.icon(QStringLiteral("trash")));
  }
  if (m_categoryCombo) {
    for (int i = 0; i < m_categoryCombo->count(); ++i) {
      const auto cat = static_cast<models::ItemCategory>(
          m_categoryCombo->itemData(i).toInt());
      m_categoryCombo->setItemIcon(i, tm.icon(models::categoryIconName(cat)));
    }
  }
}

void DetailHeaderWidget::setItem(const models::UiVaultItem &item) {
  m_isFavorite = item.isFavorite;
  m_isDeleted = item.isDeleted;
  m_currentWebsiteUrl = item.websiteUrl.trimmed();

  setCategory(item.category);
  setVaultId(item.vaultId);
  m_titleLabel->setText(item.title.isEmpty() ? tr("Untitled Item")
                                             : item.title);
  m_titleInput->setText(item.title);

  if (item.updatedAt.isValid()) {
    m_updatedLabel->setText(
        tr("Modified %1")
            .arg(item.updatedAt.toString(QStringLiteral("dd/MM/yyyy HH:mm"))));
  } else {
    m_updatedLabel->setText(tr("New record"));
  }

  m_favBtn->setIcon(app::ThemeManager::instance().icon(
      m_isFavorite ? QStringLiteral("star_filled") : QStringLiteral("star")));
  setEditMode(false);
  updateHeaderIcon();
}

void DetailHeaderWidget::setCategory(models::ItemCategory cat) {
  m_category = cat;
  updateHeaderIcon();
  m_categoryBadge->setText(models::categoryDisplayName(m_category));

  for (int i = 0; i < m_categoryCombo->count(); ++i) {
    if (m_categoryCombo->itemData(i).toInt() == static_cast<int>(cat)) {
      m_categoryCombo->blockSignals(true);
      m_categoryCombo->setCurrentIndex(i);
      m_categoryCombo->blockSignals(false);
      break;
    }
  }
}

void DetailHeaderWidget::onCategoryIndexChanged(int index) {
  if (index < 0)
    return;
  const auto newCat = static_cast<models::ItemCategory>(
      m_categoryCombo->itemData(index).toInt());
  if (newCat != m_category) {
    m_category = newCat;
    const bool isLight =
        (app::ThemeManager::instance().currentMode() == app::ThemeMode::Light);
    m_iconLabel->setPixmap(
        QIcon(models::categoryIconPath(m_category, isLight)).pixmap(32, 32));
    m_categoryBadge->setText(models::categoryDisplayName(m_category));
    emit categoryChanged(m_category);
  }
}

QString DetailHeaderWidget::title() const {
  if (m_isEditMode) {
    return m_titleInput->text().trimmed();
  }
  return m_titleLabel->fullText();
}

void DetailHeaderWidget::focusTitleInput() {
  m_titleInput->setFocus();
  m_titleInput->selectAll();
}

void DetailHeaderWidget::setVaultId(const QString &vaultId) {
  m_vaultId = vaultId;
}

QString DetailHeaderWidget::vaultId() const { return m_vaultId; }

void DetailHeaderWidget::setEditMode(bool edit) {
  m_isEditMode = edit;

  if (edit) {
    m_titleLabel->hide();
    m_titleInput->show();
    m_titleInput->setFocus();
    m_titleInput->selectAll();

    m_categoryBadge->hide();
    m_categoryCombo->show();
    m_updatedLabel->hide();

    m_favBtn->hide();
    if (m_historyBtn) {
      m_historyBtn->hide();
    }
    m_editBtn->hide();
    if (m_restoreBtn) {
      m_restoreBtn->hide();
    }
    if (m_deleteBtn) {
      m_deleteBtn->hide();
    }
    m_cancelBtn->show();
    m_saveBtn->show();
  } else {
    m_titleInput->hide();
    m_titleLabel->setText(m_titleInput->text().trimmed().isEmpty()
                              ? tr("Untitled Item")
                              : m_titleInput->text());
    m_titleLabel->show();

    m_categoryCombo->hide();
    m_categoryBadge->show();
    m_updatedLabel->show();

    m_cancelBtn->hide();
    m_saveBtn->hide();
    if (m_isDeleted) {
      m_favBtn->hide();
      if (m_historyBtn) {
        m_historyBtn->hide();
      }
      m_editBtn->hide();
      if (m_restoreBtn) {
        m_restoreBtn->show();
      }
      if (m_deleteBtn) {
        m_deleteBtn->setText(tr("Delete Permanently"));
        m_deleteBtn->setToolTip(tr("Permanently delete this item"));
        m_deleteBtn->show();
      }
    } else {
      m_editBtn->show();
      if (m_historyBtn) {
        m_historyBtn->show();
      }
      m_favBtn->show();
      if (m_restoreBtn) {
        m_restoreBtn->hide();
      }
      if (m_deleteBtn) {
        m_deleteBtn->setText(QString());
        m_deleteBtn->setToolTip(tr("Move to Trash"));
        m_deleteBtn->show();
      }
    }
  }
}

void DetailHeaderWidget::retranslateUi() {
  if (m_titleInput) {
    m_titleInput->setPlaceholderText(tr("Item Title..."));
  }
  if (m_favBtn) {
    m_favBtn->setToolTip(m_isFavorite ? tr("Remove from favorites")
                                      : tr("Mark as favorite"));
  }
  if (m_historyBtn) {
    m_historyBtn->setToolTip(tr("Revision History"));
  }
  if (m_editBtn) {
    m_editBtn->setText(tr("Edit"));
    m_editBtn->setToolTip(tr("Edit item (Ctrl+E)"));
  }
  if (m_restoreBtn) {
    m_restoreBtn->setText(tr("Restore"));
    m_restoreBtn->setToolTip(tr("Restore item from Trash"));
  }
  if (m_deleteBtn) {
    if (m_isDeleted) {
      m_deleteBtn->setText(tr("Delete Permanently"));
      m_deleteBtn->setToolTip(tr("Permanently delete this item"));
    } else {
      m_deleteBtn->setText(QString());
      m_deleteBtn->setToolTip(tr("Move to Trash"));
    }
  }
  if (m_cancelBtn) {
    m_cancelBtn->setText(tr("Cancel"));
    m_cancelBtn->setToolTip(tr("Discard changes (Esc)"));
  }
  if (m_saveBtn) {
    m_saveBtn->setText(tr("Save"));
    m_saveBtn->setToolTip(tr("Save changes (Ctrl+S)"));
  }

  if (m_categoryCombo) {
    m_categoryCombo->blockSignals(true);
    for (int i = 0; i < m_categoryCombo->count(); ++i) {
      const auto cat = static_cast<models::ItemCategory>(
          m_categoryCombo->itemData(i).toInt());
      m_categoryCombo->setItemText(i, models::categoryDisplayName(cat));
    }
    m_categoryCombo->blockSignals(false);
  }

  if (m_categoryBadge) {
    m_categoryBadge->setText(models::categoryDisplayName(m_category));
  }
}

void DetailHeaderWidget::changeEvent(QEvent *event) {
  if (event->type() == QEvent::LanguageChange) {
    retranslateUi();
  }
  QWidget::changeEvent(event);
}

} // namespace svm::views
