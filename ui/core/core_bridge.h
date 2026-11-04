/**
 * @file ui/core/core_bridge.h
 * @brief Cầu nối C++ bọc thư viện lõi Rust C-ABI (svm_core).
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#pragma once

#include <QObject>
#include <QString>
#include <memory>
#include <vector>

extern "C" {
#include "svm_core.h"
}

namespace svm::core {

/**
 * @brief Lớp Singleton / Facade bọc các lệnh gọi FFI sang Rust lõi.
 */
class CoreBridge : public QObject {
  Q_OBJECT

public:
  static CoreBridge &instance();

  ~CoreBridge() override;

  // Không cho phép copy hoặc gán
  CoreBridge(const CoreBridge &) = delete;
  CoreBridge &operator=(const CoreBridge &) = delete;

  /**
   * @brief Khởi tạo Context và tiến trình bảo mật.
   */
  bool initialize(const QString &dataDir = QString());

  /**
   * @brief Kiểm tra xem Context đã khởi tạo thành công chưa.
   */
  [[nodiscard]] bool isInitialized() const;

  /**
   * @brief Lấy phiên bản Rust Core.
   */
  [[nodiscard]] QString coreVersion() const;

  /**
   * @brief Lấy thông báo lỗi gần nhất từ TLS của Rust.
   */
  [[nodiscard]] QString lastError() const;

  /**
   * @brief Lấy con trỏ thô tới SvmContext (cho các thao tác FFI nâng cao).
   */
  [[nodiscard]] SvmContext *rawContext() const;

  /**
   * @brief Lấy danh sách Vault (dưới dạng chuỗi JSON thô).
   */
  [[nodiscard]] QString listVaults();

signals:
  void initializedChanged(bool initialized);
  void errorOccurred(const QString &errorMessage);

private:
  CoreBridge();

  SvmContext *m_context{nullptr};
  bool m_hardened{false};
};

} // namespace svm::core
