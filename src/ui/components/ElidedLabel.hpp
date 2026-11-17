/**
 * @file ui/components/elided_label.h
 * @brief Nhãn QLabel tự động rút gọn văn bản bằng dấu ba chấm ("...") khi không
 * đủ không gian hiển thị.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#pragma once

#include <QEvent>
#include <QFontMetrics>
#include <QLabel>
#include <QResizeEvent>
#include <QSizePolicy>

namespace svm::components {

/**
 * @brief QLabel tự động thêm "..." ở cuối khi văn bản vượt quá chiều rộng cho
 * phép, đồng thời gán tooltip chứa toàn bộ nội dung để người dùng có thể đọc
 * trọn vẹn khi rê chuột.
 */
class ElidedLabel : public QLabel {
  Q_OBJECT

public:
  explicit ElidedLabel(QWidget *parent = nullptr,
                       Qt::WindowFlags f = Qt::WindowFlags())
      : QLabel(parent, f) {
    setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
  }

  explicit ElidedLabel(const QString &text, QWidget *parent = nullptr,
                       Qt::WindowFlags f = Qt::WindowFlags())
      : QLabel(text, parent, f), m_fullText(text) {
    setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
  }

  void setText(const QString &text) {
    m_fullText = text;
    QLabel::setText(text);
    updateElidedText();
  }

  [[nodiscard]] const QString &fullText() const { return m_fullText; }

  [[nodiscard]] QSize minimumSizeHint() const override {
    const QFontMetrics fm(font());
    return {30, fm.height()};
  }

protected:
  void resizeEvent(QResizeEvent *event) override {
    QLabel::resizeEvent(event);
    updateElidedText();
  }

  void changeEvent(QEvent *event) override {
    QLabel::changeEvent(event);
    if (event->type() == QEvent::FontChange) {
      updateElidedText();
    }
  }

private:
  void updateElidedText() {
    if (m_fullText.isEmpty()) {
      QLabel::setText(QString());
      setToolTip(QString());
      return;
    }

    const int availWidth = contentsRect().width();
    if (availWidth <= 0) {
      return;
    }

    const QFontMetrics fm(font());
    const QString elided =
        fm.elidedText(m_fullText, Qt::ElideRight, availWidth);
    QLabel::setText(elided);

    if (elided != m_fullText) {
      setToolTip(m_fullText);
    } else {
      setToolTip(QString());
    }
  }

  QString m_fullText;
};

} // namespace svm::components
