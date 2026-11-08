/**
 * @file ui/views/settings/pairing_dialog.h
 * @brief Hộp thoại hiển thị mã PIN 6 số để ghép nối tiện ích trình duyệt
 * (WebExtension).
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#pragma once

#include <QDialog>
#include <QLabel>
#include <QPushButton>
#include <QTimer>

namespace svm::views {

class PairingDialog : public QDialog {
  Q_OBJECT

public:
  explicit PairingDialog(QWidget *parent = nullptr);
  ~PairingDialog() override = default;

  void startPairing(const QString &pin, int expiresInSecs = 60);
  int exec() override;

  static void showPairingDialog(const QString &pin, int expiresInSecs = 60);
  static bool isPairingDialogOpen();

protected:
  void keyPressEvent(QKeyEvent *event) override;

private:
  void setupUI();
  void onTick();
  void onClientPaired();

  QWidget *m_cardWidget{nullptr};
  QLabel *m_titleLabel{nullptr};
  QLabel *m_descLabel{nullptr};
  QLabel *m_pinLabel{nullptr};
  QLabel *m_timerLabel{nullptr};
  QPushButton *m_copyBtn{nullptr};
  QPushButton *m_closeBtn{nullptr};

  QTimer *m_timer{nullptr};
  int m_remainingSeconds{60};
  QString m_pin;
  bool m_pairedSuccessfully{false};
};

} // namespace svm::views
