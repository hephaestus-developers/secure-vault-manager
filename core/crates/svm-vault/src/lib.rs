//! # svm-vault
//!
//! Mô-đun nghiệp vụ quản lý vòng đời kho dữ liệu, phiên mở khóa trong RAM,
//! cơ chế tự động khóa (Auto-Lock) và các thao tác thêm, sửa, xóa mục lưu trữ.

/// Dịch vụ quản lý phiên mở khóa và nghiệp vụ kho dữ liệu.
pub struct VaultService {
    unlocked: bool,
}

impl VaultService {
    /// Khởi tạo dịch vụ VaultService mới ở trạng thái bị khóa.
    pub fn new() -> Self {
        Self { unlocked: false }
    }

    /// Kiểm tra xem kho hiện tại có đang trong trạng thái mở khóa hay không.
    pub fn is_unlocked(&self) -> bool {
        self.unlocked
    }

    /// Khóa kho dữ liệu và thu hồi toàn bộ khóa giải mã khỏi bộ nhớ RAM.
    pub fn lock(&mut self) {
        self.unlocked = false;
    }

    /// Đánh dấu mở khóa kho dữ liệu sau khi xác thực Master Password thành công.
    pub fn unlock(&mut self) {
        self.unlocked = true;
    }
}

impl Default for VaultService {
    fn default() -> Self {
        Self::new()
    }
}
