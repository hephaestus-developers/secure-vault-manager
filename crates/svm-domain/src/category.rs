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

    /// Chuyển đổi chuỗi hoặc số nguyên thành `Category`.
    pub fn parse_str(s: &str) -> Self {
        match s.trim().to_ascii_lowercase().as_str() {
            "login" | "logins" | "0" => Self::Login,
            "passkey" | "passkeys" | "1" => Self::Passkey,
            "card" | "credit_card" | "cards" | "2" => Self::Card,
            "identity" | "identities" | "3" => Self::Identity,
            "secure_note" | "note" | "notes" | "4" => Self::SecureNote,
            "ssh_key" | "ssh" | "5" => Self::SshKey,
            "database" | "db" | "6" => Self::Database,
            "server" | "servers" | "7" => Self::Server,
            "wifi" | "wi-fi" | "8" => Self::Wifi,
            _ => Self::Login,
        }
    }

    /// Trả về chỉ mục số nguyên (0..8) tương thích với `ItemCategory` của Qt UI.
    pub const fn to_index(&self) -> i32 {
        match self {
            Self::Login => 0,
            Self::Passkey => 1,
            Self::Card => 2,
            Self::Identity => 3,
            Self::SecureNote => 4,
            Self::SshKey => 5,
            Self::Database => 6,
            Self::Server => 7,
            Self::Wifi => 8,
        }
    }
}
