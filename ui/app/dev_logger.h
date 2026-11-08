/**
 * @file ui/app/dev_logger.h
 * @brief Bộ ghi nhật ký chi tiết và đo lường độ trễ (latency tracking) cho môi
 * trường Dev / Debug.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#pragma once

#include <QMap>
#include <QString>
#include <QStringList>
#include <QtLogging>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>

namespace svm::app {

/**
 * @brief Thống kê hiệu năng tổng hợp của một loại hành động.
 */
struct ActionStats {
  quint64 count{0};
  double totalMs{0.0};
  double minMs{1e9};
  double maxMs{0.0};

  [[nodiscard]] double avgMs() const {
    return count > 0 ? (totalMs / static_cast<double>(count)) : 0.0;
  }
};

/**
 * @brief Quản lý logging nâng cao và đo lường latency của mọi thao tác trong
 * app.
 */
class DevLogger {
public:
  /**
   * @brief Kiểm tra xem chế độ ghi log chi tiết Dev/Debug có được kích hoạt
   * không.
   */
  static bool isEnabled();

  /**
   * @brief Cài đặt bộ xử lý thông điệp Qt (qInstallMessageHandler) tùy biến có
   * màu sắc và timestamp.
   */
  static void install();

  /**
   * @brief Ghi lại kết quả và thời gian thực thi của một hành động.
   */
  static void recordAction(const QString &category, const QString &action,
                           double elapsedMs, bool success = true,
                           const QString &details = QString());

  /**
   * @brief Xuất báo cáo tổng kết hiệu năng và độ trễ ra console.
   */
  static void dumpPerformanceSummary();

  /**
   * @brief Xóa toàn bộ dữ liệu thống kê hiệu năng đã lưu.
   */
  static void clearStats();

private:
  static void messageHandler(QtMsgType type, const QMessageLogContext &context,
                             const QString &msg);

  static inline std::mutex s_mutex;
  static inline QMap<QString, ActionStats> s_stats;
  static inline bool s_installed{false};
};

/**
 * @brief Bộ đếm thời gian RAII tự động đo và ghi lại latency khi rời khỏi
 * scope.
 */
class ScopedActionTimer {
public:
  explicit ScopedActionTimer(QString category, QString action,
                             QString details = QString());
  ~ScopedActionTimer();

  // Không cho phép copy
  ScopedActionTimer(const ScopedActionTimer &) = delete;
  ScopedActionTimer &operator=(const ScopedActionTimer &) = delete;

  // Cho phép move
  ScopedActionTimer(ScopedActionTimer &&) noexcept;
  ScopedActionTimer &operator=(ScopedActionTimer &&) noexcept;

  void setSuccess(bool success) { m_success = success; }
  void setDetails(const QString &details) { m_details = details; }
  void appendDetail(const QString &detail);
  void finish();

private:
  QString m_category;
  QString m_action;
  QString m_details;
  bool m_success{true};
  bool m_finished{false};
  std::chrono::steady_clock::time_point m_startTime;
};

} // namespace svm::app

#if !defined(NDEBUG) || defined(QT_DEBUG)
#define SVM_ACTION_SCOPE(category, action)                                     \
  ::svm::app::ScopedActionTimer _svm_perf_scope(QStringLiteral(category),      \
                                                QStringLiteral(action))

#define SVM_ACTION_SCOPE_DETAIL(category, action, details)                     \
  ::svm::app::ScopedActionTimer _svm_perf_scope(                               \
      QStringLiteral(category), QStringLiteral(action), details)
#else
#define SVM_ACTION_SCOPE(category, action)                                     \
  do {                                                                         \
    if (::svm::app::DevLogger::isEnabled()) {                                  \
      ::svm::app::ScopedActionTimer _svm_perf_scope(QStringLiteral(category),  \
                                                    QStringLiteral(action));   \
    }                                                                          \
  } while (0)

#define SVM_ACTION_SCOPE_DETAIL(category, action, details)                     \
  do {                                                                         \
    if (::svm::app::DevLogger::isEnabled()) {                                  \
      ::svm::app::ScopedActionTimer _svm_perf_scope(                           \
          QStringLiteral(category), QStringLiteral(action), details);          \
    }                                                                          \
  } while (0)
#endif
