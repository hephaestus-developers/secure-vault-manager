//! Định nghĩa các định dạng tệp nhập/xuất (`TransferFormat`) và hằng số container sao lưu.

/// Magic bytes nhận diện tệp sao lưu mã hóa `.svm` / `.svmb`.
pub const BACKUP_MAGIC: &[u8; 4] = b"SVMB";
/// Dữ liệu xác thực bổ sung (AAD) cho tệp sao lưu mã hóa.
pub const AAD_BACKUP: &[u8] = b"svm-encrypted-backup-v1";

/// Các định dạng tệp được hỗ trợ trong tính năng Nhập và Xuất dữ liệu.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum TransferFormat {
    /// Định dạng native nhị phân mã hóa của SVM (.svm).
    Svm = 1,
    /// Tệp xuất định dạng JSON của Bitwarden.
    BitwardenJson = 2,
    /// Container sao lưu 1PUX của 1Password.
    OnePux = 3,
    /// Tệp xuất XML của KeePass 2.x.
    KeePassXml = 4,
    /// Tệp bảng tính CSV chuẩn của các trình duyệt web.
    Csv = 5,
}

impl TransferFormat {
    /// Ánh xạ mã số nguyên C-ABI sang `TransferFormat`.
    pub fn from_code(code: i32) -> Self {
        match code {
            1 => Self::Svm,
            2 => Self::BitwardenJson,
            3 => Self::OnePux,
            4 => Self::KeePassXml,
            _ => Self::Csv,
        }
    }
}
