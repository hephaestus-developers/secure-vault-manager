/**
 * @file ui/components/dialog_drag_helper.h
 * @brief Helper hỗ trợ di chuyển (drag-to-move) cho các QDialog frameless.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#pragma once

#include <QAbstractButton>
#include <QAbstractItemView>
#include <QAbstractScrollArea>
#include <QAbstractSlider>
#include <QComboBox>
#include <QDialog>
#include <QEvent>
#include <QEventLoop>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QMouseEvent>
#include <QObject>
#include <QPoint>
#include <QPropertyAnimation>
#include <QScrollBar>
#include <QTabBar>
#include <QWidget>
#include <QWindow>

namespace svm::components {

/**
 * @brief Biến bất kỳ widget nào (thường là Header Bar hoặc Card) thành tay nắm
 * kéo thả cửa sổ Dialog độc lập trên cả Wayland và X11.
 */
class DialogDragHelper : public QObject {
public:
  /**
   * @brief Kích hoạt khả năng kéo thả độc lập cho Dialog thông qua
   * handleWidget.
   * @param handleWidget Widget đóng vai trò thanh tiêu đề / vùng kéo thả.
   * @param dialog Cửa sổ QDialog cần di chuyển.
   */
  static void makeDraggable(QWidget *handleWidget, QWidget *dialog) {
    if (!handleWidget || !dialog) {
      return;
    }
    // Đảm bảo không kích hoạt xdg_dialog_v1.set_modal của Mutter
    // (attach-modal-dialogs) để Dialog có thể di chuyển độc lập với MainWindow,
    // trong khi tính Modal được kiểm soát nghiêm ngặt bởi
    // MainWindow::eventFilter.
    if (auto *qDlg = qobject_cast<QDialog *>(dialog)) {
      qDlg->setModal(false);
    }
    dialog->setWindowModality(Qt::NonModal);
    dialog->setProperty("svmModalDialog", true);

    auto *helper = new DialogDragHelper(dialog, handleWidget);
    handleWidget->installEventFilter(helper);

    const auto children = handleWidget->findChildren<QWidget *>();
    for (auto *child : children) {
      if (!isInteractiveWidget(child, handleWidget)) {
        child->installEventFilter(helper);
      }
    }
  }

  inline static QList<QDialog *> s_activeDialogs;

  /**
   * @brief Lấy QDialog đang chạy ở tầng cao nhất (topmost modal dialog).
   */
  static QDialog *activeModalDialog() {
    for (int i = s_activeDialogs.size() - 1; i >= 0; --i) {
      if (s_activeDialogs[i] && s_activeDialogs[i]->isVisible()) {
        return s_activeDialogs[i];
      }
    }
    return nullptr;
  }

  /**
   * @brief Kích hoạt hiệu ứng rung nhẹ (Shake Animation) trên dialog được chỉ
   * định (thường là dialog con nhỏ nhất ở tầng trên cùng) khi người dùng click
   * bên ngoài.
   */
  static void shakeDialog(QWidget *dlg) {
    if (!dlg || !dlg->isVisible()) {
      return;
    }
    if (dlg->property("_svm_is_shaking").toBool()) {
      return;
    }
    dlg->setProperty("_svm_is_shaking", true);

    QWidget *target = dlg;
    QLayout *dlgLayout = dlg->layout();
    if (dlgLayout && dlgLayout->count() > 0 && dlgLayout->itemAt(0) &&
        dlgLayout->itemAt(0)->widget()) {
      target = dlgLayout->itemAt(0)->widget();
    }

    const QPoint origPos = target->pos();
    if (dlgLayout && target != dlg) {
      dlgLayout->setEnabled(false);
    }

    auto *anim = new QPropertyAnimation(target, "pos", dlg);
    anim->setDuration(280);
    anim->setKeyValueAt(0.00, origPos);
    anim->setKeyValueAt(0.12, origPos + QPoint(-10, 0));
    anim->setKeyValueAt(0.28, origPos + QPoint(10, 0));
    anim->setKeyValueAt(0.44, origPos + QPoint(-7, 0));
    anim->setKeyValueAt(0.60, origPos + QPoint(7, 0));
    anim->setKeyValueAt(0.76, origPos + QPoint(-4, 0));
    anim->setKeyValueAt(0.88, origPos + QPoint(4, 0));
    anim->setKeyValueAt(1.00, origPos);

    QObject::connect(anim, &QPropertyAnimation::finished, dlg,
                     [dlg, target, dlgLayout, origPos]() {
                       target->move(origPos);
                       if (dlgLayout && target != dlg) {
                         dlgLayout->setEnabled(true);
                         dlgLayout->activate();
                       }
                       dlg->setProperty("_svm_is_shaking", false);
                     });

    anim->start(QAbstractAnimation::DeleteWhenStopped);
  }

  /**
   * @brief Thực thi QDialog đồng bộ (tương đương QDialog::exec()) nhưng giữ
   * modality ở mức NonModal đối với Window Manager để cho phép kéo thả độc lập
   * và bắt sự kiện click bên ngoài tạo hiệu ứng rung.
   */
  static int execDialog(QDialog &dlg) {
    dlg.setModal(false);
    dlg.setWindowModality(Qt::NonModal);
    dlg.setProperty("svmModalDialog", true);
    dlg.setResult(QDialog::Rejected);

    if (QWidget *p = dlg.parentWidget()) {
      if (QWidget *top = p->window()) {
        const QPoint center = top->geometry().center();
        dlg.move(center.x() - dlg.width() / 2, center.y() - dlg.height() / 2);
      }
    }

    if (!s_activeDialogs.contains(&dlg)) {
      s_activeDialogs.append(&dlg);
    }

    QEventLoop loop;
    auto conn =
        QObject::connect(&dlg, &QDialog::finished, &loop, &QEventLoop::exit);
    dlg.show();
    dlg.raise();
    dlg.activateWindow();
    const int result = loop.exec(QEventLoop::DialogExec);

    QObject::disconnect(conn);
    s_activeDialogs.removeOne(&dlg);

    // Khôi phục focus và trạng thái active cho dialog cha (nếu có)
    if (!s_activeDialogs.isEmpty() && s_activeDialogs.last() &&
        s_activeDialogs.last()->isVisible()) {
      auto *parentDlg = s_activeDialogs.last();
      parentDlg->raise();
      parentDlg->activateWindow();
      if (QWidget *fw = parentDlg->focusWidget()) {
        fw->setFocus();
      } else {
        parentDlg->setFocus();
      }
    } else if (QWidget *p = dlg.parentWidget()) {
      if (QWidget *top = p->window()) {
        top->raise();
        top->activateWindow();
        if (QWidget *fw = top->focusWidget()) {
          fw->setFocus();
        }
      }
    }

    return result;
  }

  explicit DialogDragHelper(QWidget *dialog, QObject *parent = nullptr)
      : QObject(parent), m_dialog(dialog) {}

protected:
  static bool isInteractiveWidget(QObject *obj, QWidget *rootHandle) {
    auto *w = qobject_cast<QWidget *>(obj);
    while (w && w != rootHandle) {
      if (qobject_cast<QAbstractButton *>(w) || qobject_cast<QLineEdit *>(w) ||
          qobject_cast<QAbstractScrollArea *>(w) ||
          qobject_cast<QAbstractItemView *>(w) ||
          qobject_cast<QScrollBar *>(w) || qobject_cast<QComboBox *>(w) ||
          qobject_cast<QAbstractSlider *>(w) || qobject_cast<QTabBar *>(w)) {
        return true;
      }
      if (auto *lbl = qobject_cast<QLabel *>(w)) {
        if (lbl->textInteractionFlags() &
            (Qt::TextSelectableByMouse | Qt::LinksAccessibleByMouse)) {
          return true;
        }
      }
      w = w->parentWidget();
    }
    return false;
  }

  bool eventFilter(QObject *obj, QEvent *event) override {
    auto *rootHandle = qobject_cast<QWidget *>(parent());
    if (isInteractiveWidget(obj, rootHandle)) {
      return false;
    }

    if (event->type() == QEvent::ChildAdded) {
      auto *ce = static_cast<QChildEvent *>(event);
      if (auto *childWidget = qobject_cast<QWidget *>(ce->child())) {
        if (!isInteractiveWidget(childWidget, rootHandle)) {
          childWidget->installEventFilter(this);
        }
      }
      return false;
    }

    if (event->type() == QEvent::MouseButtonPress) {
      auto *me = static_cast<QMouseEvent *>(event);
      if (me->button() == Qt::LeftButton && m_dialog) {
        if (QWindow *handle = m_dialog->windowHandle()) {
          if (handle->startSystemMove()) {
            event->accept();
            return true;
          }
        }
        m_isDragging = true;
        m_dragOffset = me->globalPosition().toPoint() -
                       m_dialog->frameGeometry().topLeft();
        event->accept();
        return true;
      }
    } else if (event->type() == QEvent::MouseMove) {
      auto *me = static_cast<QMouseEvent *>(event);
      if (m_isDragging && (me->buttons() & Qt::LeftButton) && m_dialog) {
        m_dialog->move(me->globalPosition().toPoint() - m_dragOffset);
        event->accept();
        return true;
      }
    } else if (event->type() == QEvent::MouseButtonRelease) {
      if (m_isDragging) {
        m_isDragging = false;
        event->accept();
        return true;
      }
    }
    return false;
  }

private:
  QWidget *m_dialog{nullptr};
  bool m_isDragging{false};
  QPoint m_dragOffset;
};

} // namespace svm::components
