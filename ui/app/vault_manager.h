/**
 * @file ui/app/vault_manager.h
 * @brief Lớp quản lý trạng thái vòng đời kho lưu trữ (VaultManager singleton).
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#pragma once

#include "models/vault_types.h"

#include <QList>
#include <QObject>
#include <QString>

namespace svm::app {

class VaultManager : public QObject {
  Q_OBJECT

public:
  static VaultManager &instance();

  ~VaultManager() override = default;

  // Xóa copy và gán
  VaultManager(const VaultManager &) = delete;
  VaultManager &operator=(const VaultManager &) = delete;

  /**
   * @brief Khởi tạo hệ thống quản lý Vault, đọc trạng thái từ cấu hình.
   */
  void initialize();

  /**
   * @brief Kiểm tra xem hệ thống đã thiết lập Master Password và Vault ban đầu
   * chưa.
   */
  [[nodiscard]] bool isConfigured() const;

  /**
   * @brief Kiểm tra trạng thái hiện tại đang khóa hay mở khóa.
   */
  [[nodiscard]] bool isLocked() const;

  /**
   * @brief Kiểm tra xem kho đã sẵn sàng và đang mở khóa.
   */
  [[nodiscard]] bool isUnlocked() const {
    return m_isConfigured && !m_isLocked;
  }

  /**
   * @brief Lấy định danh Vault đang active. Luôn đại diện cho 1 vault duy nhất.
   */
  [[nodiscard]] QString activeVaultId() const;
  void setActiveVaultId(const QString &vaultId);

  /**
   * @brief Lấy danh sách tất cả các Vault hiện có.
   */
  [[nodiscard]] QList<models::UiVault> vaults() const;

  /**
   * @brief Lấy thông tin chi tiết một Vault theo ID.
   */
  [[nodiscard]] models::UiVault vault(const QString &vaultId) const;

  /**
   * @brief Lấy thông tin Vault đang được chọn (active).
   */
  [[nodiscard]] models::UiVault activeVault() const;

  /**
   * @brief Lấy thông tin Vault mặc định.
   */
  [[nodiscard]] models::UiVault defaultVault() const;

  /**
   * @brief Tạo mới một Vault.
   */
  bool createVault(const models::UiVault &vault);

  /**
   * @brief Cập nhật thông tin một Vault.
   */
  bool updateVault(const models::UiVault &vault);

  /**
   * @brief Xóa một Vault.
   */
  bool deleteVault(const QString &vaultId);

  /**
   * @brief Xóa toàn bộ dữ liệu (Factory Reset) và đưa ứng dụng về màn hình
   * Onboarding Wizard.
   */
  bool resetAllData();

  /**
   * @brief Đặt một Vault làm mặc định.
   */
  bool setDefaultVault(const QString &vaultId);

  /**
   * @brief Thiết lập khởi tạo lần đầu từ Onboarding Wizard.
   */
  bool setupInitialMasterPassword(const QString &password,
                                  const QString &recoveryKey,
                                  const QString &initialVaultName);

  /**
   * @brief Đổi Master Password hiện tại sang mật khẩu mới mà không làm mất dữ
   * liệu kho.
   */
  bool changeMasterPassword(const QString &oldPassword,
                            const QString &newPassword);

  /**
   * @brief Đồng bộ lại danh sách Vault và số lượng bản ghi từ Rust Core.
   */
  void refreshVaultsFromCore();

  /**
   * @brief Mở khóa kho bằng Master Password.
   */
  bool unlock(const QString &password);

  /**
   * @brief Mở khóa khẩn cấp và đặt lại mật khẩu bằng Recovery Key.
   */
  bool unlockWithRecoveryKey(const QString &recoveryKey,
                             const QString &newPassword = QString());

  /**
   * @brief Mở khóa bằng sinh trắc học.
   */
  bool unlockWithBiometrics();

  /**
   * @brief Khóa ngay lập tức và giải phóng bộ nhớ.
   */
  void lock();

  /**
   * @brief Kiểm tra mật khẩu chính có khớp với mật khẩu đã lưu không.
   */
  [[nodiscard]] bool validateMasterPassword(const QString &password) const;

  /**
   * @brief Lấy mã Emergency Recovery Key hiện tại (chỉ khi kho đang mở khóa).
   */
  [[nodiscard]] QString recoveryKey() const;

  /**
   * @brief Lấy thông tin Vault đang hoạt động.
   */
  [[nodiscard]] models::UiVault currentVault() const {
    return vault(m_activeVaultId);
  }

  /**
   * @brief Sinh mã Recovery Key ngẫu nhiên theo chuẩn 24 ký tự
   * XXXX-XXXX-XXXX-XXXX-XXXX-XXXX (6 cụm 4 ký tự).
   */
  static QString generateRecoveryKey();

  /**
   * @brief Chuẩn hóa mã Recovery Key (loại bỏ khoảng trắng, format chuẩn 6
   * cụm).
   */
  static QString normalizeRecoveryKey(const QString &raw);

signals:
  void configuredChanged(bool configured);
  void lockStateChanged(bool isLocked);
  void activeVaultChanged(const QString &vaultId);
  void vaultListChanged();
  void vaultCreated(const models::UiVault &vault);

private:
  VaultManager();

  void loadSettings();
  void saveSettings();
  QString hashPassword(const QString &password, const QString &salt) const;

  bool m_isConfigured{false};
  bool m_isLocked{true};
  QString m_activeVaultId{QStringLiteral("personal")};
  QString m_passwordHash;
  QString m_passwordSalt;
  QString m_recoveryKey;
  bool m_biometricEnabled{false};

  QList<models::UiVault> m_vaults;
};

} // namespace svm::app
