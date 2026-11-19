/**
 * @file LogCategories.cpp
 * @brief Instantiation of SVM subsystem logging categories and structured
 * message handler.
 */

#include "LogCategories.hpp"

#include <QDateTime>
#include <cstdio>

Q_LOGGING_CATEGORY(logUI, "svm.ui")
Q_LOGGING_CATEGORY(logWindow, "svm.window")
Q_LOGGING_CATEGORY(logVault, "svm.vault")
Q_LOGGING_CATEGORY(logFFI, "svm.ffi")
Q_LOGGING_CATEGORY(logIPC, "svm.ipc")
Q_LOGGING_CATEGORY(logSSH, "svm.ssh")

namespace svm::core {

namespace {

void customMessageHandler(QtMsgType type, const QMessageLogContext &context,
                          const QString &msg) {
  const char *color = "\033[0m";
  const char *levelStr = "INFO";

  switch (type) {
  case QtDebugMsg:
    color = "\033[36m"; // Cyan
    levelStr = "DEBUG";
    break;
  case QtInfoMsg:
    color = "\033[32m"; // Green
    levelStr = "INFO ";
    break;
  case QtWarningMsg:
    color = "\033[33m"; // Yellow
    levelStr = "WARN ";
    break;
  case QtCriticalMsg:
    color = "\033[31m"; // Red
    levelStr = "ERROR";
    break;
  case QtFatalMsg:
    color = "\033[35m"; // Magenta
    levelStr = "FATAL";
    break;
  }

  // Lọc bỏ cảnh báo nội bộ vô hại từ Qt 6 upstream
  if (msg.contains(
          QStringLiteral("The cached device pixel ratio value was stale"))) {
    return;
  }

  const QString timeStr =
      QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz");
  const char *category = context.category ? context.category : "default";

  fprintf(stderr, "%s[%s] [%s] [%s]\033[0m %s\n", color,
          timeStr.toUtf8().constData(), levelStr, category,
          msg.toUtf8().constData());
  fflush(stderr);

  if (type == QtFatalMsg) {
    abort();
  }
}

} // namespace

void installCustomMessageHandler() {
  qInstallMessageHandler(customMessageHandler);
}

} // namespace svm::core
