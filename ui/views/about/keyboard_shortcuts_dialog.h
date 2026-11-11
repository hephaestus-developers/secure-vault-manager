/**
 * @file ui/views/about/keyboard_shortcuts_dialog.h
 * @brief Hộp thoại tra cứu phím tắt (KeyboardShortcutsDialog) theo chuẩn
 * Frameless Dialog của Secure Vault Manager.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#pragma once

#include <QDialog>
#include <QLabel>
#include <QList>
#include <QPushButton>

class QVBoxLayout;

namespace svm::views {

class KeyboardShortcutsDialog : public QDialog {
  Q_OBJECT

public:
  explicit KeyboardShortcutsDialog(QWidget *parent = nullptr);
  ~KeyboardShortcutsDialog() override = default;

  void retranslateUi();

protected:
  void changeEvent(QEvent *event) override;
  void keyPressEvent(QKeyEvent *event) override;

private:
  struct ShortcutRowRef {
    QLabel *label{nullptr};
    const char *sourceText{nullptr};
  };

  void setupUI();
  QWidget *createSectionCard(const char *titleSource,
                             const QList<QPair<const char *, QString>> &rows,
                             QWidget *parent);

  QWidget *m_cardWidget{nullptr};
  QWidget *m_headerWidget{nullptr};
  QLabel *m_headerTitle{nullptr};
  QPushButton *m_closeIconBtn{nullptr};
  QPushButton *m_bottomCloseBtn{nullptr};

  QList<ShortcutRowRef> m_translatableLabels;
};

} // namespace svm::views
