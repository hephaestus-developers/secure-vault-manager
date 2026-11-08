/**
 * @file ui/app/dev_logger.cpp
 * @brief Cài đặt bộ ghi log và đo độ trễ cho môi trường Dev / Debug.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#include "app/dev_logger.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QThread>
#include <algorithm>
#include <cstdio>
#include <iomanip>
#include <sstream>

#if defined(Q_OS_UNIX)
#include <unistd.h>
#elif defined(Q_OS_WIN)
#include <io.h>
#include <windows.h>
#endif

namespace svm::app {

namespace {

// ANSI Escape Codes cho console
constexpr const char *ANSI_RESET = "\033[0m";
constexpr const char *ANSI_BOLD = "\033[1m";
constexpr const char *ANSI_DIM = "\033[2m";
constexpr const char *ANSI_RED = "\033[31m";
constexpr const char *ANSI_GREEN = "\033[32m";
constexpr const char *ANSI_YELLOW = "\033[33m";
constexpr const char *ANSI_BLUE = "\033[34m";
constexpr const char *ANSI_MAGENTA = "\033[35m";
constexpr const char *ANSI_CYAN = "\033[36m";
constexpr const char *ANSI_WHITE = "\033[37m";
constexpr const char *ANSI_BOLD_RED = "\033[1;31m";
constexpr const char *ANSI_BOLD_GREEN = "\033[1;32m";
constexpr const char *ANSI_BOLD_YELLOW = "\033[1;33m";
constexpr const char *ANSI_BOLD_MAGENTA = "\033[1;35m";

bool isColorSupported() {
  static bool s_checked = false;
  static bool s_supported = false;
  if (!s_checked) {
#if defined(Q_OS_UNIX)
    s_supported = (isatty(fileno(stderr)) != 0);
#elif defined(Q_OS_WIN)
    s_supported = (_isatty(_fileno(stderr)) != 0);
    if (s_supported) {
      HANDLE hOut = GetStdHandle(STD_ERROR_HANDLE);
      DWORD dwMode = 0;
      if (GetConsoleMode(hOut, &dwMode)) {
        SetConsoleMode(hOut, dwMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
      }
    }
#else
    s_supported = false;
#endif
    // Cho phép ép buộc bật qua biến môi trường
    if (qEnvironmentVariableIsSet("SVM_COLOR_LOG")) {
      s_supported =
          (qEnvironmentVariable("SVM_COLOR_LOG") != QStringLiteral("0"));
    }
    s_checked = true;
  }
  return s_supported;
}

} // namespace

bool DevLogger::isEnabled() {
#if !defined(NDEBUG) || defined(QT_DEBUG)
  // Trong debug build, mặc định bật trừ khi đặt SVM_DEV_LOG=0
  if (qEnvironmentVariableIsSet("SVM_DEV_LOG")) {
    return qEnvironmentVariable("SVM_DEV_LOG") != QStringLiteral("0");
  }
  return true;
#else
  // Trong release build, chỉ bật nếu đặt SVM_DEV_LOG=1
  return qEnvironmentVariableIsSet("SVM_DEV_LOG") &&
         qEnvironmentVariable("SVM_DEV_LOG") != QStringLiteral("0");
#endif
}

void DevLogger::install() {
  std::lock_guard<std::mutex> lock(s_mutex);
  if (s_installed) {
    return;
  }

  qInstallMessageHandler(messageHandler);
  s_installed = true;

  const bool color = isColorSupported();
  std::fprintf(stderr,
               "%s============================================================="
               "===================%s\n",
               color ? ANSI_DIM : "", color ? ANSI_RESET : "");
  std::fprintf(stderr,
               "%s[SVM-DEV] Secure Vault Manager - Detailed Dev Logging & "
               "Latency Profiler Active%s\n",
               color ? ANSI_BOLD_MAGENTA : "", color ? ANSI_RESET : "");
  std::fprintf(stderr,
               "%s[SVM-DEV] High-resolution action latency tracking enabled "
               "for optimization%s\n",
               color ? ANSI_CYAN : "", color ? ANSI_RESET : "");
  std::fprintf(stderr,
               "%s============================================================="
               "===================%s\n",
               color ? ANSI_DIM : "", color ? ANSI_RESET : "");
  std::fflush(stderr);

  // Đăng ký dump bảng tổng kết khi ứng dụng thoát
  std::atexit([]() { DevLogger::dumpPerformanceSummary(); });
}

void DevLogger::messageHandler(QtMsgType type,
                               const QMessageLogContext &context,
                               const QString &msg) {
  const bool color = isColorSupported();
  const QString timestamp = QDateTime::currentDateTime().toString(
      QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"));

  const char *levelStr = "INFO ";
  const char *levelColor = ANSI_GREEN;

  switch (type) {
  case QtDebugMsg:
    levelStr = "DEBUG";
    levelColor = ANSI_CYAN;
    break;
  case QtInfoMsg:
    levelStr = "INFO ";
    levelColor = ANSI_GREEN;
    break;
  case QtWarningMsg:
    levelStr = "WARN ";
    levelColor = ANSI_BOLD_YELLOW;
    break;
  case QtCriticalMsg:
    levelStr = "CRIT ";
    levelColor = ANSI_BOLD_RED;
    break;
  case QtFatalMsg:
    levelStr = "FATAL";
    levelColor = ANSI_BOLD_RED;
    break;
  }

  // Tên tệp ngắn gọn thay vì đường dẫn đầy đủ
  QString fileInfo;
  if (context.file && context.line > 0) {
    const QString fullPath = QString::fromUtf8(context.file);
    const QString baseName = QFileInfo(fullPath).fileName();
    fileInfo = QStringLiteral(" (%1:%2)").arg(baseName).arg(context.line);
  }

  // Format log
  if (color) {
    std::fprintf(stderr, "%s[%s]%s %s[%s]%s %s%s%s%s\n", ANSI_DIM,
                 timestamp.toLocal8Bit().constData(), ANSI_RESET, levelColor,
                 levelStr, ANSI_RESET, msg.toLocal8Bit().constData(), ANSI_DIM,
                 fileInfo.toLocal8Bit().constData(), ANSI_RESET);
  } else {
    std::fprintf(stderr, "[%s] [%s] %s%s\n",
                 timestamp.toLocal8Bit().constData(), levelStr,
                 msg.toLocal8Bit().constData(),
                 fileInfo.toLocal8Bit().constData());
  }
  std::fflush(stderr);

  if (type == QtFatalMsg) {
    std::abort();
  }
}

void DevLogger::recordAction(const QString &category, const QString &action,
                             double elapsedMs, bool success,
                             const QString &details) {
  const QString key = QStringLiteral("%1::%2").arg(category, action);

  {
    std::lock_guard<std::mutex> lock(s_mutex);
    auto &st = s_stats[key];
    st.count++;
    st.totalMs += elapsedMs;
    if (elapsedMs < st.minMs) {
      st.minMs = elapsedMs;
    }
    if (elapsedMs > st.maxMs) {
      st.maxMs = elapsedMs;
    }
  }

  const bool color = isColorSupported();
  const QString timestamp = QDateTime::currentDateTime().toString(
      QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"));

  // Đánh giá latency: < 16ms (60fps): bình thường, 16-100ms: chú ý, >= 100ms:
  // chậm
  const char *latencyColor = ANSI_GREEN;
  const char *slowTag = "";
  if (elapsedMs >= 100.0) {
    latencyColor = ANSI_BOLD_RED;
    slowTag = color ? " \033[1;31m[SLOW]\033[0m" : " [SLOW]";
  } else if (elapsedMs >= 16.0) {
    latencyColor = ANSI_BOLD_YELLOW;
  }

  const QString statusStr =
      success ? QStringLiteral("OK") : QStringLiteral("FAILED");
  const char *statusColor = success ? ANSI_GREEN : ANSI_BOLD_RED;

  QString detailStr;
  if (!details.isEmpty()) {
    detailStr = QStringLiteral(" | ") + details;
  }

  if (color) {
    std::fprintf(stderr,
                 "%s[%s]%s %s[ACTION]%s %s[%s]%s %s %s%s ms%s [%s%s%s]%s%s\n",
                 ANSI_DIM, timestamp.toLocal8Bit().constData(), ANSI_RESET,
                 ANSI_BOLD_MAGENTA, ANSI_RESET, ANSI_CYAN,
                 category.toLocal8Bit().constData(), ANSI_RESET,
                 action.toLocal8Bit().constData(), latencyColor,
                 QString::number(elapsedMs, 'f', 2).toLocal8Bit().constData(),
                 ANSI_RESET, statusColor, statusStr.toLocal8Bit().constData(),
                 ANSI_RESET, slowTag, detailStr.toLocal8Bit().constData());
  } else {
    std::fprintf(stderr, "[%s] [ACTION] [%s] %s %s ms [%s]%s%s\n",
                 timestamp.toLocal8Bit().constData(),
                 category.toLocal8Bit().constData(),
                 action.toLocal8Bit().constData(),
                 QString::number(elapsedMs, 'f', 2).toLocal8Bit().constData(),
                 statusStr.toLocal8Bit().constData(), slowTag,
                 detailStr.toLocal8Bit().constData());
  }
  std::fflush(stderr);
}

void DevLogger::dumpPerformanceSummary() {
  std::lock_guard<std::mutex> lock(s_mutex);
  if (s_stats.isEmpty()) {
    return;
  }

  const bool color = isColorSupported();
  std::fprintf(stderr, "\n");
  std::fprintf(stderr,
               "%s============================================================="
               "=======================================%s\n",
               color ? ANSI_DIM : "", color ? ANSI_RESET : "");
  std::fprintf(stderr,
               "%s                      SECURE VAULT MANAGER - PERFORMANCE & "
               "LATENCY REPORT                         %s\n",
               color ? ANSI_BOLD_MAGENTA : "", color ? ANSI_RESET : "");
  std::fprintf(stderr,
               "%s============================================================="
               "=======================================%s\n",
               color ? ANSI_DIM : "", color ? ANSI_RESET : "");
  std::fprintf(stderr, "%-45s | %7s | %10s | %10s | %10s | %10s\n",
               "Action Name", "Calls", "Min (ms)", "Avg (ms)", "Max (ms)",
               "Total (ms)");
  std::fprintf(stderr,
               "----------------------------------------------+---------+------"
               "------+------------+------------+-----------\n");

  // Sắp xếp các hành động theo tổng thời gian thực thi giảm dần
  struct Item {
    QString key;
    ActionStats stats;
  };
  std::vector<Item> list;
  list.reserve(s_stats.size());
  for (auto it = s_stats.constBegin(); it != s_stats.constEnd(); ++it) {
    list.push_back({it.key(), it.value()});
  }

  std::sort(list.begin(), list.end(), [](const Item &a, const Item &b) {
    return a.stats.totalMs > b.stats.totalMs;
  });

  for (const auto &item : list) {
    const auto &st = item.stats;
    const double avg = st.avgMs();

    const char *rowColor = "";
    if (color) {
      if (avg >= 100.0) {
        rowColor = ANSI_BOLD_RED;
      } else if (avg >= 16.0) {
        rowColor = ANSI_YELLOW;
      }
    }

    std::fprintf(
        stderr, "%s%-45s%s | %7llu | %10.2f | %10.2f | %10.2f | %10.2f\n",
        rowColor, item.key.left(45).toLocal8Bit().constData(),
        color ? ANSI_RESET : "", static_cast<unsigned long long>(st.count),
        st.minMs, avg, st.maxMs, st.totalMs);
  }

  std::fprintf(stderr,
               "%s============================================================="
               "=======================================%s\n\n",
               color ? ANSI_DIM : "", color ? ANSI_RESET : "");
  std::fflush(stderr);
}

void DevLogger::clearStats() {
  std::lock_guard<std::mutex> lock(s_mutex);
  s_stats.clear();
}

/* ========================================================================= */
/* ScopedActionTimer Implementation                                          */
/* ========================================================================= */

ScopedActionTimer::ScopedActionTimer(QString category, QString action,
                                     QString details)
    : m_category(std::move(category)), m_action(std::move(action)),
      m_details(std::move(details)),
      m_startTime(std::chrono::steady_clock::now()) {}

ScopedActionTimer::~ScopedActionTimer() {
  if (!m_finished) {
    finish();
  }
}

ScopedActionTimer::ScopedActionTimer(ScopedActionTimer &&other) noexcept
    : m_category(std::move(other.m_category)),
      m_action(std::move(other.m_action)),
      m_details(std::move(other.m_details)), m_success(other.m_success),
      m_finished(other.m_finished), m_startTime(other.m_startTime) {
  other.m_finished = true;
}

ScopedActionTimer &
ScopedActionTimer::operator=(ScopedActionTimer &&other) noexcept {
  if (this != &other) {
    if (!m_finished) {
      finish();
    }
    m_category = std::move(other.m_category);
    m_action = std::move(other.m_action);
    m_details = std::move(other.m_details);
    m_success = other.m_success;
    m_finished = other.m_finished;
    m_startTime = other.m_startTime;
    other.m_finished = true;
  }
  return *this;
}

void ScopedActionTimer::appendDetail(const QString &detail) {
  if (detail.isEmpty()) {
    return;
  }
  if (!m_details.isEmpty()) {
    m_details.append(QStringLiteral(", "));
  }
  m_details.append(detail);
}

void ScopedActionTimer::finish() {
  if (m_finished) {
    return;
  }
  m_finished = true;
  const auto now = std::chrono::steady_clock::now();
  const double elapsedMs =
      std::chrono::duration<double, std::milli>(now - m_startTime).count();
  DevLogger::recordAction(m_category, m_action, elapsedMs, m_success,
                          m_details);
}

} // namespace svm::app
