//! # category
//!
//! Định nghĩa 9 danh mục nghiệp vụ chuyên biệt của kho dữ liệu SVM.

/// 9 danh mục nghiệp vụ độc lập được phân loại chuẩn hóa.
///
/// Mỗi danh mục đại diện cho một loại thông tin bí mật với cấu trúc payload đặc thù.
/// `Login` quản lý thông tin đăng nhập và mã TOTP 2FA, trong khi `Passkey` và `SshKey`
/// là các cặp khóa mật mã bất đối xứng độc lập.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub enum Category {
    /// Thông tin đăng nhập tài khoản (Username, Password, URLs, mã TOTP 2FA).
    Login,
    /// Khóa xác thực FIDO2 / WebAuthn Resident Key (Khóa bất đối xứng độc lập).
    Passkey,
    /// Thẻ thanh toán quốc tế / nội địa (Số thẻ, ngày hết hạn, CVV, PIN).
    Card,
    /// Hồ sơ định danh cá nhân (Họ tên, CCCD/Passport, địa chỉ, số điện thoại).
    Identity,
    /// Ghi chú an toàn chứa thông tin mật văn bản tự do.
    SecureNote,
    /// Khóa SSH bảo vệ bằng mật khẩu (Private Key, Public Key, Passphrase).
    SshKey,
    /// Thông tin kết nối cơ sở dữ liệu (Host, Port, Database, User, Password).
    Database,
    /// Máy chủ quản trị hạ tầng (IP, SSH port, Root credentials, API keys).
    Server,
    /// Mạng Wi-Fi cục bộ (SSID, Password, Security standard).
    Wifi,
}

impl Category {
    /// Trả về chuỗi định danh chuẩn (snake_case) của danh mục dùng cho serialize JSON.
    pub const fn as_str(&self) -> &'static str {
        match self {
            Self::Login => "login",
            Self::Passkey => "passkey",
            Self::Card => "card",
            Self::Identity => "identity",
            Self::SecureNote => "secure_note",
            Self::SshKey => "ssh_key",
            Self::Database => "database",
            Self::Server => "server",
            Self::Wifi => "wifi",
        }
    }
}
