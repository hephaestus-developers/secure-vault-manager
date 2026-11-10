/**
 * @file ui/app/single_instance_guard.h
 * @brief Cơ chế đảm bảo chỉ có một phiên bản ứng dụng Desktop chạy tại một
 * thời điểm (Single Instance) dùng QLockFile và QLocalServer.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#pragma once

#include <QLocalServer>
#include <QLockFile>
#include <QObject>
#include <QString>

#include <memory>

namespace svm::app {

/**
 * @brief Bảo vệ tiến trình đơn (Single Instance) theo từng người dùng hệ điều
 * hành và chuyển tiếp yêu cầu đánh thức cửa sổ từ tiến trình thứ hai.
 */
class SingleInstanceGuard : public QObject {
  Q_OBJECT

public:
  explicit SingleInstanceGuard(const QString &appKey,
                               QObject *parent = nullptr);
  ~SingleInstanceGuard() override;

  SingleInstanceGuard(const SingleInstanceGuard &) = delete;
  SingleInstanceGuard &operator=(const SingleInstanceGuard &) = delete;

  /**
   * @brief Cố gắng chiếm quyền chạy duy nhất.
   * @param command Lệnh gửi tới instance đang chạy nếu không chiếm được quyền
   *                (ví dụ: "SHOW", "SETTINGS").
   * @return true nếu đây là instance duy nhất; false nếu đã có instance khác
   *         đang chạy và đã gửi tín hiệu đánh thức thành công.
   */
  bool tryAcquire(const QString &command = QStringLiteral("SHOW"));

signals:
  void activateRequested(const QString &command);

private slots:
  void onNewConnection();

private:
  QString m_serverName;
  QString m_lockFilePath;
  std::unique_ptr<QLockFile> m_lockFile;
  QLocalServer *m_server{nullptr};
};

} // namespace svm::app
