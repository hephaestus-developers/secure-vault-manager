/**
 * @file ui/views/detail/detail_header_widget.cpp
 * @brief Hiện thực DetailHeaderWidget với thanh tiêu đề chi tiết chuẩn mực.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#include "detail_header_widget.h"
#include "app/theme_manager.h"

#include <QHBoxLayout>
#include <QIcon>
#include <QListView>
#include <QVBoxLayout>

namespace svm::views {

DetailHeaderWidget::DetailHeaderWidget(QWidget *parent) : QWidget(parent) {
  setObjectName(QStringLiteral("DetailHeaderCard"));
  setAttribute(Qt::WA_StyledBackground, true);
  setupUI();
}

void DetailHeaderWidget::setupUI() {
  auto *layout = new QHBoxLayout(this);
  layout->setContentsMargins(18, 16, 18, 16);
  layout->setSpacing(14);

  // 1. Icon lớn (48x48)
  m_iconLabel = new QLabel(this);
  m_iconLabel->setObjectName(QStringLiteral("DetailHeaderIcon"));
  m_iconLabel->setFixedSize(48, 48);
  m_iconLabel->setAlignment(Qt::AlignCenter);
  layout->addWidget(m_iconLabel);

  // 2. Khu vực Tiêu đề + Category badge
  auto *metaLayout = new QVBoxLayout();
  metaLayout->setSpacing(4);

  m_titleLabel = new QLabel(tr("Untitled Item"), this);
  m_titleLabel->setObjectName(QStringLiteral("DetailHeaderTitle"));
  QFont titleFont = m_titleLabel->font();
  titleFont.setPointSize(16);
  titleFont.setBold(true);
  m_titleLabel->setFont(titleFont);
  metaLayout->addWidget(m_titleLabel);

  m_titleInput = new QLineEdit(this);
  m_titleInput->setObjectName(QStringLiteral("DetailHeaderTitleInput"));
  m_titleInput->setFont(titleFont);
  m_titleInput->setPlaceholderText(tr("Item Title..."));
  m_titleInput->hide();
  connect(m_titleInput, &QLineEdit::returnPressed, this,
          &DetailHeaderWidget::saveRequested);
  metaLayout->addWidget(m_titleInput);

  auto *subMetaLayout = new QHBoxLayout();
  subMetaLayout->setSpacing(8);

  m_categoryBadge = new QLabel(this);
  m_categoryBadge->setObjectName(QStringLiteral("DetailCategoryBadge"));
  subMetaLayout->addWidget(m_categoryBadge);

  m_categoryCombo = new QComboBox(this);
  m_categoryCombo->setObjectName(QStringLiteral("DetailCategoryCombo"));
  m_categoryCombo->setFixedHeight(26);
  m_categoryCombo->setView(new QListView(m_categoryCombo));
  m_categoryCombo->setCursor(Qt::PointingHandCursor);

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
  subMetaLayout->addWidget(m_categoryCombo);

  m_updatedLabel = new QLabel(this);
  m_updatedLabel->setObjectName(QStringLiteral("DetailUpdatedLabel"));
  subMetaLayout->addWidget(m_updatedLabel);

  subMetaLayout->addStretch(1);
  metaLayout->addLayout(subMetaLayout);

  layout->addLayout(metaLayout, 1);

  // 3. Nút Favorite Star + Action Buttons (Edit / Cancel / Save)
  m_favBtn = new QPushButton(this);
  m_favBtn->setObjectName(QStringLiteral("DetailHeaderFavBtn"));
  m_favBtn->setIcon(QIcon(QStringLiteral(":/icons/star.svg")));
  m_favBtn->setIconSize(QSize(18, 18));
  m_favBtn->setToolTip(tr("Mark as favorite"));
  m_favBtn->setCursor(Qt::PointingHandCursor);
  connect(m_favBtn, &QPushButton::clicked, this, [this]() {
    m_isFavorite = !m_isFavorite;
    m_favBtn->setIcon(QIcon(m_isFavorite
                                ? QStringLiteral(":/icons/star_filled.svg")
                                : QStringLiteral(":/icons/star.svg")));
    emit favoriteToggled(m_isFavorite);
  });
  layout->addWidget(m_favBtn);

  m_editBtn = new QPushButton(this);
  m_editBtn->setObjectName(QStringLiteral("DetailHeaderEditBtn"));
  m_editBtn->setIcon(QIcon(QStringLiteral(":/icons/edit.svg")));
  m_editBtn->setText(tr("Edit"));
  m_editBtn->setToolTip(tr("Edit item (Ctrl+E)"));
  m_editBtn->setCursor(Qt::PointingHandCursor);
  connect(m_editBtn, &QPushButton::clicked, this,
          &DetailHeaderWidget::editRequested);
  layout->addWidget(m_editBtn);

  m_cancelBtn = new QPushButton(this);
  m_cancelBtn->setObjectName(QStringLiteral("DetailHeaderCancelBtn"));
  m_cancelBtn->setText(tr("Cancel"));
  m_cancelBtn->setToolTip(tr("Discard changes (Esc)"));
  m_cancelBtn->setCursor(Qt::PointingHandCursor);
  m_cancelBtn->hide();
  connect(m_cancelBtn, &QPushButton::clicked, this,
          &DetailHeaderWidget::cancelRequested);
  layout->addWidget(m_cancelBtn);

  m_saveBtn = new QPushButton(this);
  m_saveBtn->setObjectName(QStringLiteral("DetailHeaderSaveBtn"));
  m_saveBtn->setText(tr("Save"));
  m_saveBtn->setToolTip(tr("Save changes (Ctrl+S)"));
  m_saveBtn->setCursor(Qt::PointingHandCursor);
  m_saveBtn->hide();
  connect(m_saveBtn, &QPushButton::clicked, this,
          &DetailHeaderWidget::saveRequested);
  layout->addWidget(m_saveBtn);

  connect(&app::ThemeManager::instance(), &app::ThemeManager::themeChanged,
          this, [this]() { updateThemeIcons(); });

  updateThemeIcons();
}

void DetailHeaderWidget::updateThemeIcons() {
  const bool isLight =
      (app::ThemeManager::instance().currentMode() == app::ThemeMode::Light);
  auto &tm = app::ThemeManager::instance();

  if (m_iconLabel) {
    m_iconLabel->setPixmap(
        QIcon(models::categoryIconPath(m_category, isLight)).pixmap(32, 32));
  }
  if (m_favBtn) {
    m_favBtn->setIcon(tm.icon(m_isFavorite ? QStringLiteral("star_filled")
                                           : QStringLiteral("star")));
  }
  if (m_editBtn) {
    m_editBtn->setIcon(tm.icon(QStringLiteral("edit")));
  }
  if (m_categoryCombo) {
    for (int i = 0; i < m_categoryCombo->count(); ++i) {
      const auto cat = static_cast<models::ItemCategory>(
          m_categoryCombo->itemData(i).toInt());
      m_categoryCombo->setItemIcon(
          i, QIcon(models::categoryIconPath(cat, isLight)));
    }
  }
}

void DetailHeaderWidget::setItem(const models::UiVaultItem &item) {
  m_isFavorite = item.isFavorite;

  setCategory(item.category);
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

  const bool isLight =
      (app::ThemeManager::instance().currentMode() == app::ThemeMode::Light);
  m_favBtn->setIcon(app::ThemeManager::instance().icon(
      m_isFavorite ? QStringLiteral("star_filled") : QStringLiteral("star")));
  setEditMode(false);
}

void DetailHeaderWidget::setCategory(models::ItemCategory cat) {
  m_category = cat;
  const bool isLight =
      (app::ThemeManager::instance().currentMode() == app::ThemeMode::Light);
  m_iconLabel->setPixmap(
      QIcon(models::categoryIconPath(m_category, isLight)).pixmap(32, 32));
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
  return m_titleLabel->text();
}

void DetailHeaderWidget::focusTitleInput() {
  m_titleInput->setFocus();
  m_titleInput->selectAll();
}

void DetailHeaderWidget::setEditMode(bool edit) {
  m_isEditMode = edit;

  if (edit) {
    m_titleLabel->hide();
    m_titleInput->show();
    m_titleInput->setFocus();
    m_titleInput->selectAll();

    m_categoryBadge->hide();
    m_categoryCombo->show();

    m_favBtn->hide();
    m_editBtn->hide();
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

    m_cancelBtn->hide();
    m_saveBtn->hide();
    m_editBtn->show();
    m_favBtn->show();
  }
}

} // namespace svm::views
