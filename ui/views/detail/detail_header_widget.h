/**
 * @file ui/views/detail/detail_header_widget.h
 * @brief Header của DetailInspectorView với Icon lớn, Title, Category Badge và
 * nút Edit/Save.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#pragma once

#include "components/elided_label.h"
#include "models/vault_item.h"

#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QWidget>

namespace svm::views {

class DetailHeaderWidget : public QWidget {
  Q_OBJECT

public:
  explicit DetailHeaderWidget(QWidget *parent = nullptr);
  ~DetailHeaderWidget() override = default;

  void setItem(const models::UiVaultItem &item);
  [[nodiscard]] QString title() const;
  [[nodiscard]] models::ItemCategory category() const { return m_category; }
  void setCategory(models::ItemCategory cat);
  [[nodiscard]] QString vaultId() const;
  void setVaultId(const QString &vaultId);
  void setEditMode(bool edit);
  [[nodiscard]] bool isEditMode() const { return m_isEditMode; }
  void focusTitleInput();

  void retranslateUi();

signals:
  void favoriteToggled(bool isFavorite);
  void editRequested();
  void saveRequested();
  void cancelRequested();
  void categoryChanged(models::ItemCategory category);

protected:
  void changeEvent(QEvent *event) override;

private slots:
  void onCategoryIndexChanged(int index);

private:
  void setupUI();
  void updateThemeIcons();

  bool m_isEditMode{false};
  bool m_isFavorite{false};
  models::ItemCategory m_category{models::ItemCategory::Login};

  QLabel *m_iconLabel{nullptr};
  components::ElidedLabel *m_titleLabel{nullptr};
  QLineEdit *m_titleInput{nullptr};
  QLabel *m_categoryBadge{nullptr};
  QComboBox *m_categoryCombo{nullptr};
  QLabel *m_vaultBadge{nullptr};
  QComboBox *m_vaultCombo{nullptr};
  QLabel *m_updatedLabel{nullptr};
  QString m_vaultId{QStringLiteral("personal")};
  QPushButton *m_favBtn{nullptr};
  QPushButton *m_editBtn{nullptr};
  QPushButton *m_cancelBtn{nullptr};
  QPushButton *m_saveBtn{nullptr};
};

} // namespace svm::views
