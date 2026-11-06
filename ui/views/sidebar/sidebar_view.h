/**
 * @file ui/views/sidebar/sidebar_view.h
 * @brief View điều hướng Cột 1 (Sidebar Navigation) với bố cục hiện đại, liền
 * mạch và dải icon thu gọn.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#pragma once

#include "models/vault_item.h"
#include "sidebar_item_delegate.h"
#include "sidebar_types.h"

#include <QLabel>
#include <QListWidget>
#include <QMap>
#include <QMenu>
#include <QPushButton>
#include <QStackedWidget>
#include <QWidget>

namespace svm::views {

class SidebarView : public QWidget {
  Q_OBJECT

public:
  explicit SidebarView(QWidget *parent = nullptr);
  ~SidebarView() override = default;

  void updateCounts(const QMap<SidebarFilterType, int> &filterCounts,
                    const QMap<models::ItemCategory, int> &categoryCounts);

  void setCollapsed(bool collapsed);
  [[nodiscard]] bool isCollapsed() const { return m_isCollapsed; }

  void retranslateUi();

public slots:
  void toggleCollapsed();

signals:
  void filterSelected(SidebarFilterType type, models::ItemCategory category);
  void lockVaultRequested();
  void vaultChanged(const QString &vaultName);
  void collapsedChanged(bool collapsed);

protected:
  void changeEvent(QEvent *event) override;

private slots:
  void onListItemClicked(QListWidgetItem *item);
  void onCompactItemClicked(QListWidgetItem *item);
  void onVaultMenuActionTriggered(QAction *action);

private:
  void setupUI();
  void setupExpandedUI();
  void setupCollapsedUI();
  void populateNavItems();
  void populateVaultMenu();
  void updateThemeIcons();
  void syncSelectionToCompact(SidebarFilterType type,
                              models::ItemCategory category);
  void syncSelectionToList(SidebarFilterType type,
                           models::ItemCategory category);

  bool m_isCollapsed{false};
  QString m_currentVaultName{QStringLiteral("Personal Vault")};

  QStackedWidget *m_stackedWidget{nullptr};

  // Trang 0: Expanded View
  QWidget *m_expandedWidget{nullptr};
  QPushButton *m_vaultProfileBtn{nullptr};
  QLabel *m_vaultIconLabel{nullptr};
  QLabel *m_vaultTitleLabel{nullptr};
  QLabel *m_chevronLabel{nullptr};
  QMenu *m_vaultMenu{nullptr};
  QPushButton *m_collapseBtn{nullptr};
  QListWidget *m_navList{nullptr};
  SidebarItemDelegate *m_itemDelegate{nullptr};
  QPushButton *m_lockBtn{nullptr};
  QLabel *m_lockIconLabel{nullptr};

  QListWidgetItem *m_allItem{nullptr};
  QListWidgetItem *m_favItem{nullptr};
  QListWidgetItem *m_trashItem{nullptr};
  QListWidgetItem *m_categoriesHeaderItem{nullptr};
  QListWidgetItem *m_watchtowerHeaderItem{nullptr};
  QLabel *m_lockTextLabel{nullptr};
  QMap<models::ItemCategory, QListWidgetItem *> m_categoryItems;
  QListWidgetItem *m_weakPassItem{nullptr};
  QListWidgetItem *m_expiringItem{nullptr};
  QMap<SidebarFilterType, int> m_lastFilterCounts;
  QMap<models::ItemCategory, int> m_lastCategoryCounts;

  // Trang 1: Collapsed View (Icon Rail)
  QWidget *m_collapsedWidget{nullptr};
  QPushButton *m_expandBtn{nullptr};
  QPushButton *m_collapsedVaultBtn{nullptr};
  QListWidget *m_compactList{nullptr};
  CompactSidebarDelegate *m_compactDelegate{nullptr};
  QPushButton *m_collapsedLockBtn{nullptr};

  QMap<SidebarFilterType, QListWidgetItem *> m_compactFilterItems;
  QMap<models::ItemCategory, QListWidgetItem *> m_compactCategoryItems;
};

} // namespace svm::views
