/**
 * @file ui/views/detail/revision_history_dialog.h
 * @brief Hộp thoại xem lại và khôi phục các bản chụp lịch sử sửa đổi của bản
 * ghi theo chuẩn giao diện Frameless hiện đại.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#pragma once

#include "models/vault_item.h"

#include <QDialog>
#include <QLabel>
#include <QList>
#include <QListWidget>
#include <QPushButton>
#include <QWidget>

namespace svm::views {

class RevisionHistoryDialog : public QDialog {
  Q_OBJECT

public:
  explicit RevisionHistoryDialog(const QList<models::UiItemRevision> &revisions,
                                 QWidget *parent = nullptr);
  ~RevisionHistoryDialog() override = default;

  [[nodiscard]] int selectedRevision() const { return m_selectedRevision; }

  /**
   * @brief Hiển thị hộp thoại lịch sử sửa đổi và trả về số phiên bản người
   * dùng chọn khôi phục (hoặc `-1` nếu hủy).
   */
  static int chooseRevision(QWidget *parent,
                            const QList<models::UiItemRevision> &revisions);

protected:
  void keyPressEvent(QKeyEvent *event) override;

private:
  void setupUI();
  void populateRevisions();
  void updatePreview(int row);
  void updateThemeIcons();
  void acceptSelection();

  QList<models::UiItemRevision> m_revisions;
  int m_selectedRevision{-1};

  QWidget *m_cardWidget{nullptr};
  QWidget *m_headerWidget{nullptr};
  QLabel *m_headerIconLabel{nullptr};
  QLabel *m_titleLabel{nullptr};
  QPushButton *m_closeHeaderBtn{nullptr};

  QLabel *m_subtitleLabel{nullptr};
  QListWidget *m_listWidget{nullptr};
  QWidget *m_emptyBox{nullptr};
  QLabel *m_emptyIconLabel{nullptr};
  QLabel *m_emptyDescLabel{nullptr};

  QWidget *m_previewCard{nullptr};
  QLabel *m_previewHeaderLabel{nullptr};
  QLabel *m_previewTitleLabel{nullptr};
  QLabel *m_previewMetaLabel{nullptr};

  QPushButton *m_closeBtn{nullptr};
  QPushButton *m_restoreBtn{nullptr};
};

} // namespace svm::views
