/**
 * @file ui/components/in_window_combobox.h
 * @brief Thành phần ComboBox hiển thị danh sách lựa chọn dạng In-Window Child
 * Overlay, hoàn toàn không sử dụng cửa sổ popup của hệ điều hành (Qt::Popup /
 * OS Window).
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#pragma once

#include <QComboBox>
#include <QCoreApplication>
#include <QEvent>
#include <QFrame>
#include <QKeyEvent>
#include <QListView>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QVBoxLayout>
#include <QWidget>

#include "app/theme_manager.h"
#include "components/combobox_popup_helper.h"

namespace svm::ui {

/**
 * @brief Widget hiển thị danh sách mục nổi trực tiếp trên bề mặt cửa sổ
 * (In-Window Overlay). Tuyệt đối không sử dụng cờ Qt::Popup, đảm bảo không gặp
 * lỗi cửa sổ của Wayland/X11.
 */
class InWindowPopupOverlay : public QFrame {
  Q_OBJECT
public:
  explicit InWindowPopupOverlay(QWidget *parent = nullptr) : QFrame(parent) {
    setObjectName(QStringLiteral("InWindowPopupOverlay"));
    // Phải là Widget con thông thường, không tạo OS Window
    setWindowFlags(Qt::Widget);
    setAttribute(Qt::WA_StyledBackground, true);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(0);

    m_listView = new QListView(this);
    m_listView->setObjectName(QStringLiteral("InWindowPopupListView"));
    m_listView->setFrameShape(QFrame::NoFrame);
    m_listView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_listView->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_listView->setMouseTracking(true);
    m_listView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_listView->setItemDelegate(new ComboBoxPopupDelegate(m_listView));
    m_listView->setStyleSheet(
        QStringLiteral("background: transparent; border: none;"));

    if (m_listView->viewport()) {
      m_listView->viewport()->setMouseTracking(true);
      m_listView->viewport()->setStyleSheet(
          QStringLiteral("background: transparent;"));
    }

    layout->addWidget(m_listView);
  }

  [[nodiscard]] QListView *listView() const { return m_listView; }

protected:
  void paintEvent(QPaintEvent *event) override {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const bool isLight = (svm::app::ThemeManager::instance().currentMode() ==
                          svm::app::ThemeMode::Light);
    const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);

    QPainterPath path;
    path.addRoundedRect(r, 8, 8);

    const QColor bgColor =
        isLight ? QColor(0xFF, 0xFF, 0xFF) : QColor(0x18, 0x18, 0x25);
    const QColor borderColor =
        isLight ? QColor(0xBC, 0xC0, 0xCC) : QColor(0x45, 0x47, 0x5A);

    painter.fillPath(path, bgColor);
    painter.setPen(QPen(borderColor, 1));
    painter.drawPath(path);

    Q_UNUSED(event);
  }

private:
  QListView *m_listView{nullptr};
};

/**
 * @brief QComboBox tùy biến ghi đè phương thức showPopup() và hidePopup()
 * để hiển thị danh sách lựa chọn dạng In-Window Child Overlay thay vì OS
 * Window.
 */
class InWindowComboBox : public QComboBox {
  Q_OBJECT
public:
  explicit InWindowComboBox(QWidget *parent = nullptr) : QComboBox(parent) {
    setCursor(Qt::PointingHandCursor);
  }

  ~InWindowComboBox() override {
    if (m_isListeningFilter) {
      qApp->removeEventFilter(this);
    }
    if (m_overlay) {
      m_overlay->deleteLater();
      m_overlay = nullptr;
    }
  }

  void setPopupMinWidth(int minWidth) { m_popupMinWidth = minWidth; }

  void showPopup() override {
    if (count() == 0) {
      return;
    }

    ensureOverlayCreated();

    QWidget *topWindow = window();
    if (!topWindow) {
      return;
    }

    if (m_overlay->parentWidget() != topWindow) {
      m_overlay->setParent(topWindow);
    }

    // Đồng bộ model và selection
    m_overlay->listView()->setModel(model());
    const auto selIdx = model()->index(currentIndex(), 0);
    m_overlay->listView()->setCurrentIndex(selIdx);
    if (m_overlay->listView()->selectionModel()) {
      m_overlay->listView()->selectionModel()->select(
          selIdx, QItemSelectionModel::ClearAndSelect);
    }

    // Tính toán kích thước
    const int itemHeight = 34;
    const int maxVisible =
        std::min(count(), maxVisibleItems() > 0 ? maxVisibleItems() : 10);
    const int listHeight = maxVisible * itemHeight;
    const int totalHeight = listHeight + 10;

    int maxTextWidth = 0;
    const QFontMetrics fm = fontMetrics();
    for (int i = 0; i < count(); ++i) {
      const int w =
          fm.horizontalAdvance(itemText(i)) + 64; // padding + icon + checkmark
      if (w > maxTextWidth) {
        maxTextWidth = w;
      }
    }
    const int totalWidth =
        std::max({width(), m_popupMinWidth, maxTextWidth, 160});

    // Tính toán tọa độ hiển thị trong cửa sổ chính (window coordinates)
    const QPoint comboPosInWindow =
        topWindow->mapFromGlobal(mapToGlobal(QPoint(0, 0)));
    int targetX = comboPosInWindow.x();
    int targetY = comboPosInWindow.y() + height() + 4;

    // Tránh tràn ra đáy cửa sổ (lật lên phía trên nếu không đủ không gian)
    if (targetY + totalHeight > topWindow->height() - 8) {
      targetY = comboPosInWindow.y() - totalHeight - 4;
    }
    // Tránh tràn ra cạnh phải cửa sổ
    if (targetX + totalWidth > topWindow->width() - 8) {
      targetX = topWindow->width() - totalWidth - 8;
    }
    if (targetX < 8) {
      targetX = 8;
    }

    m_overlay->setGeometry(targetX, targetY, totalWidth, totalHeight);
    m_overlay->raise();
    m_overlay->show();

    if (!m_isListeningFilter) {
      qApp->installEventFilter(this);
      m_isListeningFilter = true;
    }

    m_overlay->listView()->setFocus();
  }

  void hidePopup() override {
    if (m_overlay && m_overlay->isVisible()) {
      m_overlay->hide();
    }
    if (m_isListeningFilter) {
      qApp->removeEventFilter(this);
      m_isListeningFilter = false;
    }
  }

protected:
  void mousePressEvent(QMouseEvent *e) override {
    if (m_overlay && m_overlay->isVisible()) {
      hidePopup();
      e->accept();
      return;
    }
    QComboBox::mousePressEvent(e);
  }

  bool eventFilter(QObject *watched, QEvent *event) override {
    if (!m_overlay || !m_overlay->isVisible()) {
      return QComboBox::eventFilter(watched, event);
    }

    if (event->type() == QEvent::MouseButtonPress) {
      auto *me = static_cast<QMouseEvent *>(event);
      const QPoint globalPos = me->globalPosition().toPoint();

      const bool clickInOverlay =
          m_overlay->rect().contains(m_overlay->mapFromGlobal(globalPos));
      const bool clickInCombo = rect().contains(mapFromGlobal(globalPos));

      if (!clickInOverlay && !clickInCombo) {
        hidePopup();
      }
    } else if (event->type() == QEvent::Wheel) {
      auto *we = static_cast<QWheelEvent *>(event);
      const QPoint globalPos = we->globalPosition().toPoint();
      if (!m_overlay->rect().contains(m_overlay->mapFromGlobal(globalPos))) {
        hidePopup();
      }
    } else if (event->type() == QEvent::KeyPress) {
      auto *ke = static_cast<QKeyEvent *>(event);
      if (ke->key() == Qt::Key_Escape) {
        hidePopup();
        return true;
      }
    } else if (event->type() == QEvent::Resize ||
               event->type() == QEvent::Move) {
      if (watched == window()) {
        hidePopup();
      }
    }

    return QComboBox::eventFilter(watched, event);
  }

private:
  void ensureOverlayCreated() {
    if (!m_overlay) {
      m_overlay = new InWindowPopupOverlay(window());
      m_overlay->hide();

      connect(m_overlay->listView(), &QAbstractItemView::clicked, this,
              [this](const QModelIndex &index) {
                if (index.isValid()) {
                  setCurrentIndex(index.row());
                }
                hidePopup();
              });

      connect(m_overlay->listView(), &QAbstractItemView::activated, this,
              [this](const QModelIndex &index) {
                if (index.isValid()) {
                  setCurrentIndex(index.row());
                }
                hidePopup();
              });
    }
  }

  InWindowPopupOverlay *m_overlay{nullptr};
  int m_popupMinWidth{160};
  bool m_isListeningFilter{false};
};

} // namespace svm::ui
