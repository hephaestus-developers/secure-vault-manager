//! # prompt
//!
//! Định nghĩa các loại yêu cầu tương tác phê duyệt của người dùng (Prompt Kinds).

/// Phân loại các yêu cầu phê duyệt gửi từ tầng dịch vụ bất đồng bộ lên UI Desktop.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum PromptKind {
    /// Đăng ký tạo Passkey mới (WebAuthn MakeCredential).
    PasskeyCreate,
    /// Xác thực đăng nhập bằng Passkey (WebAuthn GetAssertion).
    PasskeyGet,
    /// Phê duyệt ký số bằng khóa OpenSSH Agent.
    SshApproval,
    /// Lưu mật khẩu mới được phát hiện từ trình duyệt (Browser Autofill Save).
    ExtensionSave,
    /// Yêu cầu mở khóa kho từ Browser Extension.
    ExtensionUnlock,
}
