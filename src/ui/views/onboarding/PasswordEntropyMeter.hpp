/**
 * @file ui/views/onboarding/password_entropy_meter.h
 * @brief Widget trực quan đánh giá độ mạnh của Master Password.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#pragma once

#include <QLabel>
#include <QProgressBar>
#include <QWidget>

namespace svm::views {

class PasswordEntropyMeter : public QWidget {
  Q_OBJECT

public:
  enum class StrengthLevel { Empty, Weak, Fair, Strong, Excellent };

  explicit PasswordEntropyMeter(QWidget *parent = nullptr);
  ~PasswordEntropyMeter() override = default;

  void setPassword(const QString &password);
  [[nodiscard]] StrengthLevel level() const { return m_level; }

  void retranslateUi();

protected:
  void changeEvent(QEvent *event) override;

private:
  void setupUI();
  void updateDisplay();
  [[nodiscard]] StrengthLevel calculateStrength(const QString &pwd) const;

  StrengthLevel m_level{StrengthLevel::Empty};
  QWidget *m_segments[4]{nullptr};
  QLabel *m_label{nullptr};
};

} // namespace svm::views
