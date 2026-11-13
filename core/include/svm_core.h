/**
 * @file svm_core.h
 * @brief C-ABI Dynamic Interface Header for Secure Vault Manager (SVM).
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 * @version 0.1.0
 *
 * @details
 * Tệp tiêu đề C-ABI cung cấp giao diện nhị phân (FFI) cho thư viện chia sẻ động
 * `libsvm_core`. Được thiết kế để tương thích với C++17 Qt6 Desktop UI và các
 * client native khác. Tuân thủ quy chuẩn quản lý bộ nhớ ranh giới: Mọi tài
 * nguyên do Rust cấp phát bắt buộc phải được giải phóng qua
 * `svm_free_string()`, `svm_free_secret()` hoặc `svm_free_buffer()`.
 */

#ifndef SVM_CORE_H
#define SVM_CORE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================= */
/* Kiểu Dữ Liệu Cơ Bản & Enums                                               */
/* ========================================================================= */

/**
 * @struct SvmContext
 * @brief Con trỏ đục lỗ (opaque pointer) đại diện cho ngữ cảnh runtime hoàn
 * chỉnh của ứng dụng.
 * @details Quản lý trạng thái đa luồng (thread-safe), tokio runtime, các
 * service nền và phiên làm việc.
 */
typedef struct SvmContext SvmContext;

/**
 * @struct SvmBuffer
 * @brief Bộ đệm nhị phân đại diện cho tệp đính kèm, icon hoặc khối dữ liệu lớn.
 */
typedef struct {
  uint8_t *ptr; /**< Con trỏ tới vùng nhớ dữ liệu nhị phân */
  size_t len;   /**< Độ dài vùng nhớ dữ liệu tính bằng bytes */
} SvmBuffer;

/**
 * @enum SvmStatus
 * @brief Bảng mã trạng thái lỗi chuẩn của C-ABI.
 */
typedef enum {
  SVM_OK = 0, /**< Thao tác thành công hoàn toàn. */
  SVM_ERR_INVALID_ARG =
      1, /**< Tham số truyền vào không hợp lệ hoặc con trỏ NULL. */
  SVM_ERR_LOCKED =
      2, /**< Vault đang trong trạng thái khóa, cần mở khóa trước. */
  SVM_ERR_WRONG_PASSWORD =
      3, /**< Mật khẩu Master Password hoặc xác thực không chính xác. */
  SVM_ERR_IO = 4,     /**< Lỗi nhập/xuất hệ thống tệp hoặc socket IPC. */
  SVM_ERR_CRYPTO = 5, /**< Lỗi mật mã học (giải mã thất bại, sai Auth Tag). */
  SVM_ERR_NOT_FOUND =
      6, /**< Mục dữ liệu, thư mục hoặc tài nguyên yêu cầu không tồn tại. */
  SVM_ERR_PARSE = 7,   /**< Lỗi phân tích cú pháp JSON, CBOR hoặc XML. */
  SVM_ERR_NETWORK = 8, /**< Lỗi kết nối mạng (tải Favicon). */
  SVM_ERR_DENIED =
      9, /**< Quyền truy cập bị từ chối hoặc người dùng bấm Deny trên UI. */
  SVM_ERR_TIMEOUT =
      10, /**< Hết thời gian chờ xử lý yêu cầu (Prompt timeout). */
  SVM_ERR_CONFLICT =
      11, /**< Xung đột phiên bản dữ liệu hoặc tên bị trùng lặp. */
  SVM_ERR_UNSUPPORTED =
      12, /**< Tính năng hoặc định dạng không được hỗ trợ trên nền tảng này. */
  SVM_ERR_CANCELLED =
      13, /**< Tác vụ bị hủy bỏ chủ động trước khi hoàn thành. */
  SVM_ERR_PANIC =
      99 /**< Lỗi ngoại lệ nội bộ Rust (được chặn tại ranh giới FFI). */
} SvmStatus;

/**
 * @enum SvmEventKind
 * @brief Định danh các sự kiện phát sinh từ Rust Core gửi lên giao diện người
 * dùng.
 */
typedef enum {
  SVM_EVENT_VAULT_LOCKED = 1,   /**< Vault vừa chuyển sang trạng thái khóa. */
  SVM_EVENT_VAULT_UNLOCKED = 2, /**< Vault vừa được mở khóa thành công. */
  SVM_EVENT_ITEMS_CHANGED = 3, /**< Dữ liệu các mục trong kho có sự thay đổi. */
  SVM_EVENT_PROMPT =
      4, /**< Có yêu cầu phê duyệt mới cần người dùng tương tác. */
  SVM_EVENT_PROMPT_CANCELLED =
      5, /**< Một yêu cầu phê duyệt vừa bị hủy bỏ hoặc timeout. */
  SVM_EVENT_BRIDGE_STATUS =
      7, /**< Trạng thái kết nối Browser Extension thay đổi. */
  SVM_EVENT_SSH_STATUS = 8, /**< Trạng thái OpenSSH Agent daemon thay đổi. */
  SVM_EVENT_BRIDGE_CLIENTS_CHANGED =
      10, /**< Danh sách Extension đã kết nối có sự thay đổi. */
  SVM_EVENT_SETTINGS_CHANGED =
      11 /**< Cấu hình settings.json của ứng dụng có sự thay đổi. */
} SvmEventKind;

/**
 * @enum SvmFormat
 * @brief Định dạng tệp phục vụ tính năng Nhập & Xuất dữ liệu (Import / Export).
 */
typedef enum {
  SVM_FORMAT_SVM = 1, /**< Định dạng native SVM nhị phân mã hóa (.svm). */
  SVM_FORMAT_BITWARDEN_JSON = 2, /**< Tệp xuất JSON của Bitwarden (.json). */
  SVM_FORMAT_1PUX = 3,           /**< Container 1PUX của 1Password (.1pux). */
  SVM_FORMAT_KEEPASS_XML = 4,    /**< Tệp xuất XML của KeePass 2.x (.xml). */
  SVM_FORMAT_CSV =
      5 /**< Tệp CSV chuẩn của trình duyệt Chrome/Firefox (.csv). */
} SvmFormat;

/**
 * @brief Định nghĩa con trỏ hàm callback nhận sự kiện từ Rust Core.
 * @param[in] kind Mã định danh sự kiện thuộc @ref SvmEventKind.
 * @param[in] payload_json Chuỗi JSON chứa thông tin chi tiết sự kiện (Caller
 * KHÔNG giải phóng).
 * @param[in] user_data Con trỏ dữ liệu tùy biến người dùng đăng ký trước đó.
 */
typedef void (*SvmEventCallback)(int32_t kind, const char *payload_json,
                                 void *user_data);

/* ========================================================================= */
/* 3.1. Quản Lý Vòng Đời & Bộ Nhớ (Lifecycle & Memory)                       */
/* ========================================================================= */

/**
 * @brief Lấy mã số phiên bản nhị phân C-ABI.
 * @details Được UI sử dụng để kiểm tra tính tương thích ABI trước khi gọi các
 * hàm khác.
 * @return Số nguyên phiên bản ABI (hiện tại là 1).
 */
uint32_t svm_abi_version(void);

/**
 * @brief Lấy chuỗi phiên bản ngữ nghĩa (SemVer) tĩnh của lõi SVM Core.
 * @note Con trỏ chuỗi này trỏ vào vùng nhớ tĩnh của thư viện, caller TUYỆT ĐỐI
 * KHÔNG giải phóng.
 * @return Con trỏ chuỗi C-string (ví dụ: "0.1.0").
 */
const char *svm_core_version(void);

/**
 * @brief Khởi tạo một ngữ cảnh ứng dụng SvmContext mới.
 * @param[in] data_dir Đường dẫn thư mục lưu trữ dữ liệu. Có thể truyền NULL để
 * dùng đường dẫn mặc định của OS.
 * @return Con trỏ tới @ref SvmContext được cấp phát, hoặc NULL nếu khởi tạo
 * thất bại.
 */
SvmContext *svm_context_new(const char *data_dir);

/**
 * @brief Hủy ngữ cảnh ứng dụng và thu hồi toàn bộ tài nguyên liên quan.
 * @param[in] ctx Con trỏ ngữ cảnh cần hủy. An toàn nếu truyền NULL.
 */
void svm_context_free(SvmContext *ctx);

/**
 * @brief Lấy thông điệp lỗi chi tiết nhất phát sinh trên luồng hiện tại.
 * @note Chuỗi được mượn từ Thread-Local Storage (TLS), caller TUYỆT ĐỐI KHÔNG
 * giải phóng.
 * @return Con trỏ chuỗi C-string mô tả lỗi chi tiết.
 */
const char *svm_last_error(void);

/**
 * @brief Đăng ký hàm callback nhận thông báo sự kiện từ Rust Core lên UI.
 * @param[in] ctx Con trỏ ngữ cảnh ứng dụng.
 * @param[in] cb Con trỏ hàm callback nhận sự kiện.
 * @param[in] user_data Con trỏ dữ liệu tùy biến truyền vào callback khi phát
 * sinh sự kiện.
 */
void svm_set_event_callback(SvmContext *ctx, SvmEventCallback cb,
                            void *user_data);

/**
 * @brief Kích hoạt các biện pháp gia cố an ninh tiến trình của hệ điều hành.
 * @details Vô hiệu hóa core dump (PR_SET_DUMPABLE=0), chặn ptrace và khóa bộ
 * nhớ.
 * @return Mã trạng thái @ref SvmStatus.
 */
SvmStatus svm_harden_process(void);

/**
 * @brief Giải phóng chuỗi C-string thông thường do Rust cấp phát.
 * @param[in] p Con trỏ chuỗi cần giải phóng. An toàn nếu truyền NULL.
 */
void svm_free_string(char *p);

/**
 * @brief Giải phóng chuỗi mật mã: ghi đè bằng số 0 (zeroize) trước khi giải
 * phóng bộ nhớ.
 * @param[in] p Con trỏ chuỗi mật mã cần giải phóng. An toàn nếu truyền NULL.
 */
void svm_free_secret(char *p);

/**
 * @brief Giải phóng bộ đệm nhị phân SvmBuffer (tự động zeroize trước khi thu
 * hồi).
 * @param[in] buf Cấu trúc @ref SvmBuffer cần giải phóng.
 */
void svm_free_buffer(SvmBuffer buf);

/* ========================================================================= */
/* 3.2. Điều Phối Yêu Cầu Tương Tác (Prompt Broker)                          */
/* ========================================================================= */

/**
 * @brief Phản hồi quyết định của người dùng cho một yêu cầu phê duyệt (Prompt).
 * @param[in] ctx Con trỏ ngữ cảnh ứng dụng.
 * @param[in] request_id Chuỗi UUID định danh yêu cầu phê duyệt.
 * @param[in] decision_json Chuỗi JSON định dạng kết quả phê duyệt.
 * @return Mã trạng thái @ref SvmStatus.
 */
SvmStatus svm_prompt_respond(SvmContext *ctx, const char *request_id,
                             const char *decision_json);

/* ========================================================================= */
/* 3.3. Quản Trị Vault (Vault Registry & Lifecycle)                          */
/* ========================================================================= */

/**
 * @brief Lấy danh sách các hồ sơ Vault đã tạo hoặc đã đăng ký trong hệ thống.
 * @param[in] ctx Con trỏ ngữ cảnh ứng dụng.
 * @param[out] out_registry_json Con trỏ nhận chuỗi JSON chứa danh sách hồ sơ
 * Vault. Giải phóng bằng @ref svm_free_string.
 * @return Mã trạng thái @ref SvmStatus.
 */
SvmStatus svm_vault_list(SvmContext *ctx, char **out_registry_json);

/**
 * @brief Tạo mới một Vault hoàn chỉnh với Master Password chỉ định.
 * @param[in] ctx Con trỏ ngữ cảnh ứng dụng.
 * @param[in] name Tên hiển thị của kho dữ liệu.
 * @param[in] password Mật khẩu Master Password dùng để dẫn xuất khóa bảo vệ
 * kho.
 * @param[out] out_vault_id Con trỏ nhận chuỗi UUID của Vault vừa tạo. Giải
 * phóng bằng @ref svm_free_string.
 * @return Mã trạng thái @ref SvmStatus.
 */
SvmStatus svm_vault_create(SvmContext *ctx, const char *name,
                           const char *password, char **out_vault_id);

/**
 * @brief Mở khóa Vault bằng Master Password.
 * @param[in] ctx Con trỏ ngữ cảnh ứng dụng.
 * @param[in] vault_id Định danh UUID của Vault cần mở khóa.
 * @param[in] password Mật khẩu Master Password.
 * @return Mã trạng thái @ref SvmStatus. Trả về @ref SVM_ERR_WRONG_PASSWORD nếu
 * mật khẩu không đúng.
 */
SvmStatus svm_vault_unlock(SvmContext *ctx, const char *vault_id,
                           const char *password);

/**
 * @brief Kiểm tra phần cứng và dịch vụ xác thực sinh trắc học của OS có khả
 * dụng hay không.
 * @return true nếu hỗ trợ sinh trắc học (Windows Hello, Touch ID, Linux PAM),
 * false nếu không.
 */
bool svm_biometric_is_available(void);

/**
 * @brief Kích hoạt mở khóa sinh trắc học cho Vault (bọc khóa DEK vào OS
 * Keystore).
 * @param[in] ctx Con trỏ ngữ cảnh ứng dụng.
 * @param[in] vault_id Định danh UUID của Vault.
 * @param[in] password Mật khẩu Master Password để xác thực quyền kích hoạt.
 * @return Mã trạng thái @ref SvmStatus.
 */
SvmStatus svm_vault_biometric_enable(SvmContext *ctx, const char *vault_id,
                                     const char *password);

/**
 * @brief Hủy kích hoạt sinh trắc học cho Vault và xóa khóa bọc khỏi OS
 * Keystore.
 * @param[in] ctx Con trỏ ngữ cảnh ứng dụng.
 * @param[in] vault_id Định danh UUID của Vault.
 * @return Mã trạng thái @ref SvmStatus.
 */
SvmStatus svm_vault_biometric_disable(SvmContext *ctx, const char *vault_id);

/**
 * @brief Mở khóa Vault bằng cơ chế xác thực sinh trắc học của hệ điều hành.
 * @param[in] ctx Con trỏ ngữ cảnh ứng dụng.
 * @param[in] vault_id Định danh UUID của Vault cần mở khóa.
 * @return Mã trạng thái @ref SvmStatus.
 */
SvmStatus svm_vault_unlock_biometric(SvmContext *ctx, const char *vault_id);

/**
 * @brief Khóa tức thì Vault hiện tại và xóa sạch toàn bộ khóa giải mã trong
 * RAM.
 * @param[in] ctx Con trỏ ngữ cảnh ứng dụng.
 * @return Mã trạng thái @ref SvmStatus.
 */
SvmStatus svm_vault_lock(SvmContext *ctx);

/**
 * @brief Kiểm tra xem Vault hiện tại có đang trong trạng thái mở khóa hay
 * không.
 * @param[in] ctx Con trỏ ngữ cảnh ứng dụng.
 * @return true nếu đang mở khóa, false nếu đang khóa hoặc chưa chọn Vault.
 */
bool svm_vault_is_unlocked(const SvmContext *ctx);

/**
 * @brief Đọc Emergency Recovery Key của Vault (chỉ khả dụng khi kho đang MỞ
 * KHÓA).
 * @param[in] ctx Con trỏ ngữ cảnh ứng dụng.
 * @param[out] out_key Con trỏ nhận chuỗi ký tự chứa Recovery Key. Giải phóng
 * bằng @ref svm_free_secret.
 * @return Mã trạng thái @ref SvmStatus.
 */
SvmStatus svm_vault_get_recovery_key(SvmContext *ctx, char **out_key);

/**
 * @brief Đổi tên hiển thị của một Vault.
 * @param[in] ctx Con trỏ ngữ cảnh ứng dụng.
 * @param[in] vault_id Định danh UUID của Vault.
 * @param[in] name Tên hiển thị mới.
 * @return Mã trạng thái @ref SvmStatus.
 */
SvmStatus svm_vault_rename(SvmContext *ctx, const char *vault_id,
                           const char *name);

/**
 * @brief Xóa vĩnh viễn một Vault và toàn bộ tệp nhị phân liên quan khỏi đĩa.
 * @param[in] ctx Con trỏ ngữ cảnh ứng dụng.
 * @param[in] vault_id Định danh UUID của Vault cần xóa.
 * @param[in] password Mật khẩu Master Password để xác nhận quyền xóa.
 * @return Mã trạng thái @ref SvmStatus.
 */
SvmStatus svm_vault_delete(SvmContext *ctx, const char *vault_id,
                           const char *password);

/**
 * @brief Đặt một Vault làm kho mặc định khi ứng dụng khởi động.
 * @param[in] ctx Con trỏ ngữ cảnh ứng dụng.
 * @param[in] vault_id Định danh UUID của Vault.
 * @return Mã trạng thái @ref SvmStatus.
 */
SvmStatus svm_vault_set_default(SvmContext *ctx, const char *vault_id);

/**
 * @brief Đổi Master Password của Vault hiện tại (tái mã hóa DEK bằng KEK mới).
 * @param[in] ctx Con trỏ ngữ cảnh ứng dụng.
 * @param[in] old_pw Mật khẩu Master Password hiện tại.
 * @param[in] new_pw Mật khẩu Master Password mới.
 * @return Mã trạng thái @ref SvmStatus.
 */
SvmStatus svm_vault_change_password(SvmContext *ctx, const char *old_pw,
                                    const char *new_pw);

/**
 * @brief Tạo lại (xoay) Recovery Key mới cho Vault (yêu cầu Master Password hợp
 * lệ).
 * @param[in] ctx Con trỏ ngữ cảnh ứng dụng.
 * @param[in] master_pw Mật khẩu Master Password hiện tại để xác minh danh tính.
 * @param[out] out_key Con trỏ nhận chuỗi khóa khôi phục mới (phải giải phóng
 * bằng @ref svm_free_secret).
 * @return Mã trạng thái @ref SvmStatus.
 */
SvmStatus svm_vault_rotate_recovery_key(SvmContext *ctx, const char *master_pw,
                                        char **out_key);

/**
 * @brief Đặt lại toàn bộ dữ liệu kho (xóa toàn bộ kho và cài đặt lưu trữ cục
 * bộ).
 * @param[in] ctx Con trỏ ngữ cảnh ứng dụng.
 * @return Mã trạng thái @ref SvmStatus.
 */
SvmStatus svm_vault_reset(SvmContext *ctx);

/**
 * @brief Báo cáo thời gian nhàn rỗi (idle) của hệ thống phục vụ bộ đếm
 * Auto-Lock.
 * @param[in] ctx Con trỏ ngữ cảnh ứng dụng.
 * @param[in] idle_ms Số mili-giây nhàn rỗi người dùng không thao tác
 * phím/chuột.
 */
void svm_vault_report_idle(SvmContext *ctx, uint64_t idle_ms);

/* ========================================================================= */
/* 3.4. Quản Lý Mục Dữ Liệu & Lịch Sử (Items & History)                      */
/* ========================================================================= */

/**
 * @brief Lấy danh sách metadata của các mục trong Vault theo bộ lọc chỉ định.
 * @details Không bao gồm plaintext của các trường nhạy cảm để bảo vệ an toàn bộ
 * nhớ.
 * @param[in] ctx Con trỏ ngữ cảnh ứng dụng.
 * @param[in] filter_json Chuỗi JSON bộ lọc tìm kiếm (hoặc NULL để lấy tất cả).
 * @param[out] out_items_json Con trỏ nhận chuỗi JSON danh sách mục. Giải phóng
 * bằng @ref svm_free_string.
 * @return Mã trạng thái @ref SvmStatus.
 */
SvmStatus svm_items_list(SvmContext *ctx, const char *filter_json,
                         char **out_items_json);

/**
 * @brief Lấy thông tin chi tiết của một mục trong Vault.
 * @details Các trường bí mật (password, pin, private_key) được masked với
 * `{"concealed": true}`.
 * @param[in] ctx Con trỏ ngữ cảnh ứng dụng.
 * @param[in] item_id Định danh UUID của mục cần đọc.
 * @param[out] out_item_json Con trỏ nhận chuỗi JSON chi tiết mục. Giải phóng
 * bằng @ref svm_free_string.
 * @return Mã trạng thái @ref SvmStatus.
 */
SvmStatus svm_item_get(SvmContext *ctx, const char *item_id,
                       char **out_item_json);

/**
 * @brief Thêm mới hoặc cập nhật một mục trong Vault.
 * @param[in] ctx Con trỏ ngữ cảnh ứng dụng.
 * @param[in] item_json Chuỗi JSON thông tin mục cần lưu trữ.
 * @param[out] out_item_id Con trỏ nhận chuỗi UUID của mục vừa lưu. Giải phóng
 * bằng @ref svm_free_string.
 * @return Mã trạng thái @ref SvmStatus.
 */
SvmStatus svm_item_upsert(SvmContext *ctx, const char *item_json,
                          char **out_item_id);

/**
 * @brief Đưa một mục vào Thùng rác (xóa mềm).
 * @param[in] ctx Con trỏ ngữ cảnh ứng dụng.
 * @param[in] item_id Định danh UUID của mục cần chuyển vào thùng rác.
 * @return Mã trạng thái @ref SvmStatus.
 */
SvmStatus svm_item_trash(SvmContext *ctx, const char *item_id);

/**
 * @brief Khôi phục một mục từ Thùng rác về vị trí ban đầu.
 * @param[in] ctx Con trỏ ngữ cảnh ứng dụng.
 * @param[in] item_id Định danh UUID của mục cần phục hồi.
 * @return Mã trạng thái @ref SvmStatus.
 */
SvmStatus svm_item_restore(SvmContext *ctx, const char *item_id);

/**
 * @brief Xóa vĩnh viễn và triệt để một mục khỏi Thùng rác.
 * @param[in] ctx Con trỏ ngữ cảnh ứng dụng.
 * @param[in] item_id Định danh UUID của mục cần xóa triệt để.
 * @return Mã trạng thái @ref SvmStatus.
 */
SvmStatus svm_item_purge(SvmContext *ctx, const char *item_id);

/**
 * @brief Trích xuất giá trị plaintext nhạy cảm của một trường bí mật cụ thể.
 * @warning Vùng nhớ trả về qua out_secret BẮT BUỘC phải được giải phóng bằng
 * @ref svm_free_secret (tự động zeroize).
 * @param[in] ctx Con trỏ ngữ cảnh ứng dụng.
 * @param[in] item_id Định danh UUID của mục.
 * @param[in] field_id Định danh trường nhạy cảm (ví dụ: "payload.password",
 * "custom:<uuid>").
 * @param[out] out_secret Con trỏ nhận chuỗi plaintext bí mật. Giải phóng bằng
 * @ref svm_free_secret.
 * @return Mã trạng thái @ref SvmStatus.
 */
SvmStatus svm_item_reveal_field(SvmContext *ctx, const char *item_id,
                                const char *field_id, char **out_secret);

/**
 * @brief Lấy lịch sử các bản sửa đổi (revisions) của một mục.
 * @param[in] ctx Con trỏ ngữ cảnh ứng dụng.
 * @param[in] item_id Định danh UUID của mục.
 * @param[out] out_json Con trỏ nhận chuỗi JSON danh sách lịch sử sửa đổi. Giải
 * phóng bằng @ref svm_free_string.
 * @return Mã trạng thái @ref SvmStatus.
 */
SvmStatus svm_history_list(SvmContext *ctx, const char *item_id,
                           char **out_json);

/**
 * @brief Phục hồi một mục về nội dung tại một bản sửa đổi trong lịch sử.
 * @param[in] ctx Con trỏ ngữ cảnh ứng dụng.
 * @param[in] item_id Định danh UUID của mục.
 * @param[in] revision Số thứ tự bản sửa đổi cần khôi phục.
 * @return Mã trạng thái @ref SvmStatus.
 */
SvmStatus svm_history_restore(SvmContext *ctx, const char *item_id,
                              uint32_t revision);

/* ========================================================================= */
/* 3.5. Quản Lý Tệp Đính Kèm (Secure Attachments)                            */
/* ========================================================================= */

/**
 * @brief Thêm tệp đính kèm vào mục (mã hóa streaming từng chunk 64KB).
 * @param[in] ctx Con trỏ ngữ cảnh ứng dụng.
 * @param[in] item_id Định danh UUID của mục.
 * @param[in] src_path Đường dẫn tệp nguồn trên đĩa cần đính kèm.
 * @param[out] out_attachment_json Con trỏ nhận JSON metadata tệp đính kèm. Giải
 * phóng bằng @ref svm_free_string.
 * @return Mã trạng thái @ref SvmStatus.
 */
SvmStatus svm_attachment_add(SvmContext *ctx, const char *item_id,
                             const char *src_path, char **out_attachment_json);

/**
 * @brief Giải mã và xuất tệp đính kèm ra một đường dẫn trên hệ thống tệp.
 * @param[in] ctx Con trỏ ngữ cảnh ứng dụng.
 * @param[in] attachment_id Định danh UUID của tệp đính kèm.
 * @param[in] dst_path Đường dẫn đích trên đĩa để ghi dữ liệu giải mã.
 * @return Mã trạng thái @ref SvmStatus.
 */
SvmStatus svm_attachment_export(SvmContext *ctx, const char *attachment_id,
                                const char *dst_path);

/**
 * @brief Xóa một tệp đính kèm khỏi kho dữ liệu.
 * @param[in] ctx Con trỏ ngữ cảnh ứng dụng.
 * @param[in] attachment_id Định danh UUID của tệp đính kèm cần xóa.
 * @return Mã trạng thái @ref SvmStatus.
 */
SvmStatus svm_attachment_delete(SvmContext *ctx, const char *attachment_id);

/**
 * @brief Lấy danh sách metadata tệp đính kèm của một mục dưới dạng JSON.
 */
SvmStatus svm_attachments_list(SvmContext *ctx, const char *item_id,
                               char **out_json);

/* ========================================================================= */
/* 3.6. Tiện Ích Mật Mã & TOTP                                               */
/* ========================================================================= */

/**
 * @brief Tính toán mã số xác thực hai lớp TOTP theo chuẩn RFC 6238.
 * @param[in] ctx Con trỏ ngữ cảnh ứng dụng.
 * @param[in] item_id Định danh UUID của mục chứa secret TOTP.
 * @param[in] unix_ts Dấu thời gian Unix Epoch tính bằng giây (truyền 0 để dùng
 * thời gian hiện tại).
 * @param[out] out_code Con trỏ nhận chuỗi mã số 6-8 chữ số. Giải phóng bằng
 * @ref svm_free_string.
 * @param[out] out_remaining Con trỏ nhận số giây còn lại trước khi mã hết hạn.
 * @return Mã trạng thái @ref SvmStatus.
 */
SvmStatus svm_totp_generate(SvmContext *ctx, const char *item_id,
                            uint64_t unix_ts, char **out_code,
                            uint32_t *out_remaining);

/**
 * @brief Phân tích cú pháp URI otpauth:// chuẩn (từ mã QR).
 * @param[in] uri Chuỗi URI otpauth:// cần phân tích.
 * @param[out] out_json Con trỏ nhận JSON chứa thông số cấu hình OTP. Giải phóng
 * bằng @ref svm_free_string.
 * @return Mã trạng thái @ref SvmStatus.
 */
SvmStatus svm_otpauth_parse(const char *uri, char **out_json);

/**
 * @brief Quét và giải mã mã QR từ bộ đệm điểm ảnh RGBA.
 * @param[in] rgba Con trỏ tới mảng byte điểm ảnh định dạng RGBA.
 * @param[in] w Chiều rộng ảnh tính bằng pixel.
 * @param[in] h Chiều cao ảnh tính bằng pixel.
 * @param[out] out_text Con trỏ nhận chuỗi văn bản giải mã từ mã QR. Giải phóng
 * bằng @ref svm_free_string.
 * @return Mã trạng thái @ref SvmStatus.
 */
SvmStatus svm_qr_decode_rgba(const uint8_t *rgba, uint32_t w, uint32_t h,
                             char **out_text);

/**
 * @brief Sinh mật khẩu ngẫu nhiên hoặc cụm từ mật khẩu (passphrase) Diceware.
 * @param[in] options_json Chuỗi JSON cấu hình sinh mật khẩu (độ dài, ký tự đặc
 * biệt, từ điển).
 * @param[out] out_secret Con trỏ nhận chuỗi mật khẩu được sinh. Giải phóng bằng
 * @ref svm_free_secret.
 * @return Mã trạng thái @ref SvmStatus.
 */
SvmStatus svm_password_generate(const char *options_json, char **out_secret);

/**
 * @brief Đánh giá độ mạnh của mật khẩu sử dụng thuật toán phân tích zxcvbn.
 * @param[in] password Chuỗi mật khẩu cần đánh giá.
 * @param[out] out_json Con trỏ nhận JSON kết quả đánh giá (score, feedback).
 * Giải phóng bằng @ref svm_free_string.
 * @return Mã trạng thái @ref SvmStatus.
 */
SvmStatus svm_password_strength(const char *password, char **out_json);

/* ========================================================================= */
/* 3.7. Nhập & Xuất Dữ Liệu (Import & Export)                                */
/* ========================================================================= */

/**
 * @brief Phân tích tệp nguồn và tạo bản xem trước (preview) quá trình nhập dữ
 * liệu.
 * @param[in] ctx Con trỏ ngữ cảnh ứng dụng.
 * @param[in] format Định dạng tệp nguồn thuộc @ref SvmFormat.
 * @param[in] src_path Đường dẫn tệp nguồn trên đĩa.
 * @param[in] password Mật khẩu tệp nguồn nếu tệp được mã hóa (ví dụ: KeePass,
 * 1PUX).
 * @param[out] out_preview_json Con trỏ nhận JSON bản xem trước. Giải phóng bằng
 * @ref svm_free_string.
 * @return Mã trạng thái @ref SvmStatus.
 */
SvmStatus svm_import_preview(SvmContext *ctx, int32_t format,
                             const char *src_path, const char *password,
                             char **out_preview_json);

/**
 * @brief Thực thi việc nhập các mục từ bản xem trước vào Vault hiện tại.
 * @param[in] ctx Con trỏ ngữ cảnh ứng dụng.
 * @param[in] preview_id Định danh bản xem trước đã tạo trước đó.
 * @param[in] options_json Tùy chọn xử lý xung đột mục trùng lặp.
 * @param[out] out_summary_json Con trỏ nhận JSON báo cáo kết quả nhập. Giải
 * phóng bằng @ref svm_free_string.
 * @return Mã trạng thái @ref SvmStatus.
 */
SvmStatus svm_import_execute(SvmContext *ctx, const char *preview_id,
                             const char *options_json, char **out_summary_json);

/**
 * @brief Xuất toàn bộ hoặc một phần dữ liệu Vault ra tệp định dạng chỉ định.
 * @param[in] ctx Con trỏ ngữ cảnh ứng dụng.
 * @param[in] format Định dạng xuất thuộc @ref SvmFormat.
 * @param[in] dst_path Đường dẫn tệp đích cần ghi dữ liệu.
 * @param[in] password Mật khẩu mã hóa tệp xuất (nếu định dạng hỗ trợ).
 * @return Mã trạng thái @ref SvmStatus.
 */
SvmStatus svm_export(SvmContext *ctx, int32_t format, const char *dst_path,
                     const char *password);

/* ========================================================================= */
/* 3.8. SSH Agent & Passkeys                                                 */
/* ========================================================================= */

/**
 * @brief Khởi chạy OpenSSH Agent daemon lắng nghe trên Unix Socket hoặc Named
 * Pipe.
 * @param[in] ctx Con trỏ ngữ cảnh ứng dụng.
 * @return Mã trạng thái @ref SvmStatus.
 */
SvmStatus svm_ssh_agent_start(SvmContext *ctx);

/**
 * @brief Dừng OpenSSH Agent daemon và đóng các kết nối client hiện hành.
 * @param[in] ctx Con trỏ ngữ cảnh ứng dụng.
 * @return Mã trạng thái @ref SvmStatus.
 */
SvmStatus svm_ssh_agent_stop(SvmContext *ctx);

/**
 * @brief Lấy trạng thái hoạt động của SSH Agent và danh sách khóa đang nạp.
 * @param[in] ctx Con trỏ ngữ cảnh ứng dụng.
 * @param[out] out_json Con trỏ nhận JSON trạng thái SSH Agent. Giải phóng bằng
 * @ref svm_free_string.
 * @return Mã trạng thái @ref SvmStatus.
 */
SvmStatus svm_ssh_agent_status(SvmContext *ctx, char **out_json);

/**
 * @brief Sinh cặp khóa SSH mới (Ed25519 hoặc RSA 4096) và lưu trực tiếp vào
 * Vault.
 * @param[in] ctx Con trỏ ngữ cảnh ứng dụng.
 * @param[in] options_json Chuỗi JSON cấu hình loại khóa và comment.
 * @param[out] out_item_id Con trỏ nhận chuỗi UUID của mục khóa SSH vừa tạo.
 * Giải phóng bằng @ref svm_free_string.
 * @return Mã trạng thái @ref SvmStatus.
 */
SvmStatus svm_ssh_key_generate(SvmContext *ctx, const char *options_json,
                               char **out_item_id);

/* ========================================================================= */
/* 3.9. Cầu Nối Trình Duyệt & Cài Đặt Hệ Thống (Bridge & Settings)           */
/* ========================================================================= */

/**
 * @brief Khởi chạy dịch vụ Bridge lắng nghe IPC từ Native Messaging Host.
 * @param[in] ctx Con trỏ ngữ cảnh ứng dụng.
 * @return Mã trạng thái @ref SvmStatus.
 */
SvmStatus svm_bridge_start(SvmContext *ctx);

/**
 * @brief Dừng dịch vụ Bridge IPC listener.
 * @param[in] ctx Con trỏ ngữ cảnh ứng dụng.
 * @return Mã trạng thái @ref SvmStatus.
 */
SvmStatus svm_bridge_stop(SvmContext *ctx);

/**
 * @brief Lấy danh sách các Extension và client đang kết nối.
 * @param[in] ctx Con trỏ ngữ cảnh ứng dụng.
 * @param[out] out_json Con trỏ nhận JSON danh sách client. Giải phóng bằng @ref
 * svm_free_string.
 * @return Mã trạng thái @ref SvmStatus.
 */
SvmStatus svm_bridge_clients_list(SvmContext *ctx, char **out_json);

/**
 * @brief Đăng ký tệp JSON Native Messaging Host manifest vào cấu hình các trình
 * duyệt trên máy.
 * @param[in] ctx Con trỏ ngữ cảnh ứng dụng.
 * @param[in] browsers_json Chuỗi JSON danh sách trình duyệt cần cài đặt
 * (Chrome, Firefox, Edge, Brave).
 * @return Mã trạng thái @ref SvmStatus.
 */
SvmStatus svm_native_host_install(SvmContext *ctx, const char *browsers_json);

/**
 * @brief Đọc cấu hình người dùng toàn cục AppSettings dưới dạng JSON.
 * @param[in] ctx Con trỏ ngữ cảnh ứng dụng.
 * @param[out] out_json Con trỏ nhận chuỗi JSON cấu hình ứng dụng. Giải phóng
 * bằng @ref svm_free_string.
 * @return Mã trạng thái @ref SvmStatus.
 */
SvmStatus svm_settings_get(SvmContext *ctx, char **out_json);

/**
 * @brief Lưu cấu hình người dùng toàn cục AppSettings.
 * @param[in] ctx Con trỏ ngữ cảnh ứng dụng.
 * @param[in] json Chuỗi JSON cấu hình mới cần lưu.
 * @return Mã trạng thái @ref SvmStatus.
 */
SvmStatus svm_settings_save(SvmContext *ctx, const char *json);

/**
 * @brief Tải biểu tượng website (Favicon) theo tên miền có kiểm soát quyền
 * riêng tư (opt-in).
 * @param[in] ctx Con trỏ ngữ cảnh ứng dụng.
 * @param[in] domain Tên miền website (ví dụ: "github.com").
 * @param[out] out_png Cấu trúc @ref SvmBuffer nhận dữ liệu ảnh PNG biểu tượng.
 * Giải phóng bằng @ref svm_free_buffer.
 * @return Mã trạng thái @ref SvmStatus.
 */
SvmStatus svm_favicon_fetch(SvmContext *ctx, const char *domain,
                            SvmBuffer *out_png);

#ifdef __cplusplus
}
#endif

#endif /* SVM_CORE_H */
