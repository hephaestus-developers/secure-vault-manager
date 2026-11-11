/**
 * @file ui/core/core_bridge.h
 * @brief Cầu nối C++ bọc thư viện lõi Rust C-ABI (svm_core).
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#pragma once

#include "models/vault_item.h"
#include "models/vault_types.h"

#include <QHash>
#include <QImage>
#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QPair>
#include <QSet>
#include <QString>
#include <atomic>
#include <memory>
#include <optional>
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
   * @brief Dừng các dịch vụ nền, luồng Qt và giải phóng Rust Core trước khi
   * thoát.
   */
  void shutdown();

  /**
   * @brief Đặt lại toàn bộ dữ liệu lưu trữ vật lý của kho và khởi tạo lại ngữ
   * cảnh.
   */
  bool resetStorage();

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

  /* ======================================================================= */
  /* Quản lý Kho (Multi-Vault & Vòng đời Khóa/Mở khóa)                        */
  /* ======================================================================= */

  /**
   * @brief Lấy danh sách Vault (dưới dạng chuỗi JSON thô).
   */
  [[nodiscard]] QString listVaults();

  /**
   * @brief Lấy danh sách cấu trúc UiVault đã phân tích từ Rust Core.
   */
  [[nodiscard]] QList<models::UiVault>
  fetchVaults(bool *outConfigured = nullptr);

  /**
   * @brief Tạo mới một Vault hoặc khởi tạo kho lần đầu từ Onboarding Wizard.
   */
  bool createVault(const models::UiVault &vault,
                   const QString &password = QString(),
                   const QString &recoveryKey = QString(),
                   bool initialSetup = false, QString *outVaultId = nullptr);

  /**
   * @brief Cập nhật thông tin cấu hình của một Vault.
   */
  bool updateVault(const models::UiVault &vault);

  /**
   * @brief Xóa một Vault theo định danh.
   */
  bool deleteVault(const QString &vaultId, const QString &password = QString());

  /**
   * @brief Đặt một Vault làm mặc định trong Rust Core.
   */
  bool setDefaultVault(const QString &vaultId);

  /**
   * @brief Mở khóa kho bằng Master Password.
   */
  bool unlockVault(const QString &vaultId, const QString &password);

  /**
   * @brief Mở khóa khẩn cấp bằng Recovery Key (kèm tùy chọn đặt lại Master
   * Password).
   */
  bool unlockWithRecoveryKey(const QString &recoveryKey,
                             const QString &newPassword = QString());

  /**
   * @brief Mở khóa kho bằng khóa sinh trắc học đã lưu.
   */
  bool
  unlockWithBiometrics(const QString &vaultId = QStringLiteral("personal"));

  /**
   * @brief Khóa kho ngay lập tức và xóa DEK trên RAM của Rust Core.
   */
  bool lockVault();

  /**
   * @brief Kiểm tra xem kho trên Rust Core hiện đang mở khóa hay không.
   */
  [[nodiscard]] bool isVaultUnlocked() const;

  /**
   * @brief Lấy Recovery Key hiện tại từ Rust Core (chỉ khi kho đang mở khóa).
   */
  [[nodiscard]] QString recoveryKey() const;

  /**
   * @brief Tạo lại (xoay) Recovery Key mới cho Vault (yêu cầu Master Password
   * hợp lệ).
   */
  QString rotateRecoveryKey(const QString &masterPassword);

  /**
   * @brief Thay đổi Master Password và bọc lại DEK mà không mất dữ liệu.
   */
  bool
  changeMasterPassword(const QString &oldPassword, const QString &newPassword,
                       const QString &vaultId = QStringLiteral("personal"));

  /**
   * @brief Kiểm tra phần cứng/OS có hỗ trợ xác thực sinh trắc học hoặc OS user
   * hay không.
   */
  [[nodiscard]] bool isBiometricAvailable() const;

  /**
   * @brief Bật mở khóa sinh trắc học cho kho.
   */
  bool enableBiometric(const QString &vaultId, const QString &password);

  /**
   * @brief Tắt mở khóa sinh trắc học.
   */
  bool disableBiometric(const QString &vaultId = QStringLiteral("personal"));

  /**
   * @brief Báo cáo thời gian nhàn rỗi của người dùng sang Rust Core.
   */
  void reportIdle(quint64 idleMs);

  /* ======================================================================= */
  /* Thao tác Bản ghi (Vault Items & Trash)                                   */
  /* ======================================================================= */

  /**
   * @brief Lấy danh sách bản ghi từ kho đang mở.
   */
  [[nodiscard]] QList<models::UiVaultItem>
  listItems(const QString &filterJson = QString());

  /**
   * @brief Lấy chi tiết một bản ghi theo ID.
   */
  [[nodiscard]] std::optional<models::UiVaultItem>
  getItem(const QString &itemId);

  /**
   * @brief Thêm mới hoặc cập nhật một bản ghi vào kho mã hóa Rust Core.
   * @return ID của bản ghi sau khi lưu, hoặc chuỗi rỗng nếu thất bại.
   */
  QString upsertItem(const models::UiVaultItem &item);

  /**
   * @brief Chuyển bản ghi vào thùng rác.
   */
  bool trashItem(const QString &itemId);

  /**
   * @brief Khôi phục bản ghi từ thùng rác.
   */
  bool restoreItem(const QString &itemId);

  /**
   * @brief Xóa vĩnh viễn một bản ghi khỏi kho.
   */
  bool purgeItem(const QString &itemId);

  /**
   * @brief Dọn sạch toàn bộ thùng rác.
   */
  bool emptyTrash();

  /**
   * @brief Giải mã có chọn lọc một trường nhạy cảm (password, totp, cvv...).
   */
  [[nodiscard]] QString revealField(const QString &itemId,
                                    const QString &fieldId);

  /* ======================================================================= */
  /* Thư mục, Lịch sử Sửa đổi & Tệp Đính kèm                                  */
  /* ======================================================================= */

  [[nodiscard]] QList<models::UiFolder> listFolders();
  QString upsertFolder(const models::UiFolder &folder);
  bool deleteFolder(const QString &folderId);

  [[nodiscard]] QList<models::UiItemRevision>
  listHistory(const QString &itemId);
  bool restoreHistory(const QString &itemId, int revision);

  [[nodiscard]] QList<models::UiAttachmentMeta>
  listAttachments(const QString &itemId);
  std::optional<models::UiAttachmentMeta> addAttachment(const QString &itemId,
                                                        const QString &srcPath);
  bool exportAttachment(const QString &attachmentId, const QString &dstPath);
  bool deleteAttachment(const QString &attachmentId);

  /* ======================================================================= */
  /* Tiện ích Mật mã & TOTP (RFC 6238)                                        */
  /* ======================================================================= */

  /**
   * @brief Sinh mã TOTP 6 chữ số và số giây còn lại từ item ID hoặc Base32
   * secret.
   */
  [[nodiscard]] QPair<QString, int> generateTotp(const QString &itemIdOrSecret,
                                                 quint64 unixTs = 0);

  /**
   * @brief Phân tích chuỗi URI otpauth://totp/...
   */
  [[nodiscard]] QJsonObject parseOtpAuth(const QString &uri);

  /**
   * @brief Giải mã mã QR từ QImage (chuyển đổi sang RGBA8888 và gọi Rust rqrr).
   */
  [[nodiscard]] QString decodeQrImage(const QImage &image);

  /**
   * @brief Tải biểu tượng website (Favicon) có kiểm soát opt-in qua Rust Core.
   */
  [[nodiscard]] QImage fetchFavicon(const QString &domain);

  /**
   * @brief Chuẩn hóa URL/domain thành khóa cache favicon (ví dụ "gitlab.com").
   */
  [[nodiscard]] static QString normalizeFaviconKey(const QString &urlOrDomain);

  /**
   * @brief Trả về favicon đã cache trong RAM (hoặc null QImage nếu chưa tải).
   */
  [[nodiscard]] QImage cachedFavicon(const QString &urlOrDomain) const;

  /**
   * @brief Yêu cầu tải bất đồng bộ favicon cho domain nếu bật opt-in và chưa có
   * trong cache.
   */
  void requestFaviconAsync(const QString &urlOrDomain);

  /**
   * @brief Sinh mật khẩu ngẫu nhiên an toàn mật mã từ CSPRNG của Rust Core.
   */
  [[nodiscard]] QString generatePassword(int length = 20, bool upper = true,
                                         bool lower = true, bool digits = true,
                                         bool symbols = true);

  /**
   * @brief Đánh giá độ mạnh mật khẩu qua Rust Core.
   */
  [[nodiscard]] QJsonObject evaluatePasswordStrength(const QString &password);

  /* ======================================================================= */
  /* Nhập / Xuất Dữ liệu (Import / Export)                                    */
  /* ======================================================================= */

  /**
   * @brief Nhập dữ liệu từ tệp bên ngoài vào kho hiện tại.
   * @return Số lượng bản ghi đã nhập thành công, hoặc -1 nếu lỗi.
   */
  int importItems(int format, const QString &filePath,
                  const QString &password = QString());

  /**
   * @brief Xuất dữ liệu kho ra tệp (Encrypted .svm, CSV, JSON...).
   */
  bool exportItems(int format, const QString &filePath,
                   const QString &password = QString());

  /* ======================================================================= */
  /* SSH Agent, Browser Bridge & Cấu hình Hệ thống                            */
  /* ======================================================================= */

  bool startSshAgent();
  bool stopSshAgent();
  [[nodiscard]] QJsonObject sshAgentStatus();
  [[nodiscard]] QJsonObject
  generateSshKey(const QString &title, const QString &comment,
                 const QString &vaultId = QStringLiteral("personal"));

  bool startBridge();
  bool stopBridge();
  [[nodiscard]] QJsonArray listBridgeClients();
  bool revokeBridgeClient(const QString &clientId);
  bool installNativeHostManifests();

  [[nodiscard]] QJsonObject getCoreSettings();
  bool saveCoreSettings(const QJsonObject &settings);

  [[nodiscard]] QJsonArray listPasskeys(const QString &rpId = QString());
  bool deletePasskey(const QString &itemId);
  bool respondPrompt(const QString &requestId, bool approved);

  /**
   * @brief Chuyển đổi qua lại giữa UiVaultItem và QJsonObject đồng bộ với
   * svm-domain.
   */
  static models::UiVaultItem itemFromJson(const QJsonObject &obj);
  static QJsonObject itemToJson(const models::UiVaultItem &item);

signals:
  void initializedChanged(bool initialized);
  void errorOccurred(const QString &errorMessage);
  void coreEventReceived(int eventKind, const QString &payloadJson);
  void vaultLockedEvent();
  void vaultUnlockedEvent();
  void itemsChangedEvent();
  void bridgeClientsChangedEvent();
  void promptRequestedEvent(const QJsonObject &prompt);
  void faviconUpdated(const QString &domainKey);
  void coreSettingsChanged(const QJsonObject &settings);

private:
  CoreBridge();

  static void onRustEventCallback(int32_t kind, const char *payloadJson,
                                  void *userData);
  void handleRustEvent(int kind, const QString &payloadJson);

  SvmContext *m_context{nullptr};
  bool m_hardened{false};
  bool m_bridgeStarted{false};
  std::atomic<bool> m_shuttingDown{false};
  QHash<QString, QImage> m_faviconCache;
  QSet<QString> m_faviconInFlight;
  QSet<QString> m_faviconFailed;
};

} // namespace svm::core
