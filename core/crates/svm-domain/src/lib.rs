//! # svm-domain
//!
//! Crate định nghĩa các thực thể miền (Domain Entities), Value Objects,
//! Enums và mã lỗi chuẩn cho toàn bộ hệ sinh thái **Secure Vault Manager (SVM)**.
//!
//! Crate này hoàn toàn độc lập với các tác vụ I/O đĩa hoặc mạng, bảo đảm
//! tính thuần khiết của mô hình miền nghiệp vụ.

pub mod category;
pub mod custom_field;
pub mod error;
pub mod folder;
pub mod item;
pub mod prompt;
pub mod status;
pub mod trash;

pub use category::Category;
pub use custom_field::{CustomField, FieldKind};
pub use error::DomainError;
pub use folder::Folder;
pub use item::VaultItem;
pub use prompt::PromptKind;
pub use status::SvmStatusCode;
pub use trash::TrashItem;
