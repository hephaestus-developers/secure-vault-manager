/**
 * @file ui/views/command_palette/command_palette_dialog.h
 * @brief Hộp thoại tìm kiếm nhanh Spotlight Command Palette (Ctrl+K).
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#pragma once

#include "models/vault_item.h"
#include "views/item_list/item_list_model.h"
#include "views/item_list/item_sort_filter_proxy.h"

#include <QDialog>
#include <QLineEdit>
#include <QListView>

namespace svm::views {

class CommandPaletteDialog : public QDialog {
  Q_OBJECT

public:
  explicit CommandPaletteDialog(ItemListModel *sourceModel,
                                QWidget *parent = nullptr);
  ~CommandPaletteDialog() override = default;

  void openPalette();

protected:
  bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
  void onSearchTextChanged(const QString &text);
  void onActivated(const QModelIndex &index);

private:
  void setupUI();
  void copySelectedPassword();
  void copySelectedUsername();

  QLineEdit *m_searchEdit{nullptr};
  QListView *m_resultList{nullptr};
  ItemSortFilterProxy *m_proxyModel{nullptr};
};

} // namespace svm::views
