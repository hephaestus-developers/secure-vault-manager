/**
 * @file ui/views/detail/detail_inspector_view.h
 * @brief View chi tiết Cột 3 (Detail & Inspector Pane) hỗ trợ xem, copy và
 * inline editing.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#pragma once

#include "custom_fields_widget.h"
#include "detail_header_widget.h"
#include "empty_state_widget.h"
#include "field_row_widget.h"
#include "models/vault_item.h"
#include "totp_widget.h"

#include <QFrame>
#include <QScrollArea>
#include <QStackedWidget>
#include <QWidget>

namespace svm::views {

class DetailInspectorView : public QWidget {
  Q_OBJECT

public:
  explicit DetailInspectorView(QWidget *parent = nullptr);
  ~DetailInspectorView() override = default;

  void setItem(const models::UiVaultItem &item);
  void clearItem();
  void createNewItem(models::ItemCategory category);
  void setEditMode(bool editable);
  [[nodiscard]] bool isEditMode() const { return m_isEditMode; }
  void saveEdit();
  void cancelEdit();
  void showRevisionHistory();

  void retranslateUi();

signals:
  void itemSaved(const models::UiVaultItem &item);
  void itemDeleted(const QString &itemId);
  void itemTrashRequested(const QString &itemId);
  void itemRestoreRequested(const QString &itemId);
  void itemPurgeRequested(const QString &itemId);
  void historyRestored(const QString &itemId);
  void cancelRequested();

protected:
  void changeEvent(QEvent *event) override;

private slots:
  void onSaveRequested();
  void onCancelRequested();
  void onHistoryRequested();
  void onAddAttachmentClicked();

private:
  void setupUI();
  void refreshAttachments();
  void updateVisibilityForCategory(models::ItemCategory category);
  void refreshPrimaryCard(const QList<QWidget *> &visibleWidgets,
                          const QString &headerTitle);

  QStackedWidget *m_stackedWidget{nullptr};
  EmptyStateWidget *m_emptyState{nullptr};
  QScrollArea *m_scrollArea{nullptr};
  QWidget *m_contentContainer{nullptr};

  DetailHeaderWidget *m_headerWidget{nullptr};
  TotpWidget *m_totpWidget{nullptr};

  // 1. Tầng Primary Details thích ứng theo Category
  QWidget *m_primaryCard{nullptr};
  QVBoxLayout *m_primaryLayout{nullptr};
  QLabel *m_primaryHeader{nullptr};
  QWidget *m_addTotpRow{nullptr};
  QPushButton *m_addTotpBtn{nullptr};
  QWidget *m_genSshKeyRow{nullptr};
  QPushButton *m_genSshKeyBtn{nullptr};
  QList<FieldRowWidget *> m_allPrimaryFields;
  QList<QWidget *> m_allPrimaryWidgets;
  QList<QFrame *> m_primaryDividers;

  // 2. Tầng Custom Fields
  CustomFieldsWidget *m_customFieldsWidget{nullptr};

  // 3. Tầng Notes
  QWidget *m_notesCard{nullptr};
  QLabel *m_notesHeader{nullptr};
  FieldRowWidget *m_notesField{nullptr};

  // 4. Tầng Attachments
  QWidget *m_attachmentsCard{nullptr};
  QLabel *m_attachmentsHeader{nullptr};
  QVBoxLayout *m_attachmentsListLayout{nullptr};
  QLabel *m_noAttachmentsLabel{nullptr};
  QPushButton *m_addAttachmentBtn{nullptr};

  // Các trường nghiệp vụ
  FieldRowWidget *m_usernameField{nullptr};
  FieldRowWidget *m_passwordField{nullptr};
  FieldRowWidget *m_websiteField{nullptr};
  FieldRowWidget *m_cardNumberField{nullptr};
  FieldRowWidget *m_cardExpiryField{nullptr};
  FieldRowWidget *m_cardCvvField{nullptr};
  FieldRowWidget *m_wifiSsidField{nullptr};
  FieldRowWidget *m_wifiSecurityField{nullptr};
  FieldRowWidget *m_dbHostField{nullptr};
  FieldRowWidget *m_dbPortField{nullptr};
  FieldRowWidget *m_dbNameField{nullptr};
  FieldRowWidget *m_sshFingerprintField{nullptr};
  FieldRowWidget *m_sshKeyField{nullptr};
  FieldRowWidget *m_passkeyRpIdField{nullptr};
  FieldRowWidget *m_passkeyCredentialIdField{nullptr};
  FieldRowWidget *m_passkeyAlgorithmField{nullptr};
  FieldRowWidget *m_passkeyUserHandleField{nullptr};
  FieldRowWidget *m_passkeySignCountField{nullptr};

  models::UiVaultItem m_currentItem;
  bool m_isEditMode{false};
  bool m_isNewItem{false};
  bool m_isTotpActive{false};
};

} // namespace svm::views
