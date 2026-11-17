//! Bộ xử lý dùng chung cho các yêu cầu giải mã QR và cập nhật TOTP qua Bridge / Native Host.

use svm_crypto::base64_decode;
use svm_domain::{Category, DomainError, JsonValue, VaultItem};
use svm_totp::{decode_qr_luma, decode_qr_rgba, generate_totp, parse_otpauth_uri};
use svm_vault::VaultService;

/// Kết quả lỗi chuẩn hóa cho Bridge IPC (`(error_code, message)`).
pub type BridgeActionErr = (&'static str, String);

/// Giải mã QR từ ảnh Luma8 hoặc RGBA Base64 do Extension gửi xuống và phân tích trước metadata `otpauth://`.
pub fn handle_decode_qr_action(
    unlocked: bool,
    payload: &JsonValue,
) -> Result<JsonValue, BridgeActionErr> {
    if !unlocked {
        return Err((
            "VAULT_LOCKED",
            "Kho dữ liệu đang bị khóa. Vui lòng mở khóa trên Desktop.".to_string(),
        ));
    }

    let luma_b64 = payload.get_str_or("lumaBase64", "");
    let rgba_b64 = payload.get_str_or("rgbaBase64", "");
    let width = payload
        .get("width")
        .and_then(|v| v.as_i64())
        .unwrap_or(0)
        .max(0) as u32;
    let height = payload
        .get("height")
        .and_then(|v| v.as_i64())
        .unwrap_or(0)
        .max(0) as u32;
    if (luma_b64.is_empty() && rgba_b64.is_empty())
        || width == 0
        || height == 0
        || width > 4096
        || height > 4096
    {
        return Err((
            "INVALID_ARG",
            "Dữ liệu ảnh QR hoặc kích thước không hợp lệ".to_string(),
        ));
    }

    let decoded_res = if !luma_b64.is_empty() {
        let luma = base64_decode(luma_b64).map_err(|e| {
            (
                "INVALID_ARG",
                format!("Invalid base64 Luma: {}", e.message()),
            )
        })?;
        decode_qr_luma(&luma, width, height)
    } else {
        let rgba = base64_decode(rgba_b64).map_err(|e| {
            (
                "INVALID_ARG",
                format!("Invalid base64 RGBA: {}", e.message()),
            )
        })?;
        decode_qr_rgba(&rgba, width, height)
    };

    let text = decoded_res.map_err(|e| match e {
        DomainError::NotFound(_) => (
            "QR_NOT_FOUND",
            "Không tìm thấy mã QR hợp lệ trong vùng ảnh đã chọn".to_string(),
        ),
        other => ("INVALID_ARG", other.message().to_string()),
    })?;

    let trimmed = text.trim();
    let lower = trimmed.to_ascii_lowercase();
    let is_otp_auth = lower.starts_with("otpauth://");
    let is_passkey_qr = lower.starts_with("fido:/");

    let mut data = JsonValue::new_object();
    data.insert("text", JsonValue::String(trimmed.to_string()));
    data.insert("isOtpAuth", JsonValue::Bool(is_otp_auth));
    data.insert("isPasskeyQr", JsonValue::Bool(is_passkey_qr));

    if is_otp_auth {
        let cfg =
            parse_otpauth_uri(trimmed).map_err(|e| ("INVALID_OTPAUTH", e.message().to_string()))?;
        let mut meta = JsonValue::new_object();
        meta.insert("kind", JsonValue::String(cfg.kind));
        meta.insert("issuer", JsonValue::String(cfg.issuer));
        meta.insert("account", JsonValue::String(cfg.account));
        meta.insert("digits", JsonValue::Number(cfg.digits as f64));
        meta.insert("period", JsonValue::Number(cfg.period as f64));
        data.insert("otpauth", meta);
    } else {
        data.insert("otpauth", JsonValue::Null);
    }

    Ok(data)
}

/// Gắn hoặc cập nhật mã TOTP (`otpauth://...`) vào một bản ghi Vault đã có hoặc tạo mới.
pub fn handle_update_totp_action(
    vault: &mut VaultService,
    payload: &JsonValue,
) -> Result<JsonValue, BridgeActionErr> {
    if !vault.is_unlocked() {
        return Err((
            "VAULT_LOCKED",
            "Kho dữ liệu đang bị khóa. Vui lòng mở khóa trên Desktop.".to_string(),
        ));
    }

    let uri = payload
        .get_str_or("uri", payload.get_str_or("secret", ""))
        .trim();
    if uri.is_empty()
        || (uri.contains("://") && !uri.to_ascii_lowercase().starts_with("otpauth://"))
    {
        return Err((
            "INVALID_OTPAUTH",
            "Mã QR không phải định dạng TOTP (otpauth://)".to_string(),
        ));
    }

    let parsed_cfg =
        parse_otpauth_uri(uri).map_err(|e| ("INVALID_OTPAUTH", e.message().to_string()))?;

    let item_id = payload
        .get_str_or("itemId", payload.get_str_or("item_id", ""))
        .trim();
    let create_new = payload.get_bool_or("createNew", false);

    let saved_id = if !item_id.is_empty() {
        let Some(mut existing) = vault
            .items()
            .iter()
            .find(|it| !it.trashed && it.id == item_id)
            .cloned()
        else {
            return Err(("NOT_FOUND", format!("Không tìm thấy bản ghi {item_id}")));
        };
        existing.totp_secret = parsed_cfg.secret.clone();
        existing.has_totp = true;
        vault
            .upsert_item(existing)
            .map_err(|e| ("SAVE_FAILED", e.message().to_string()))?
    } else if create_new {
        let raw_title = payload.get_str_or("title", "").trim();
        let website_url = payload.get_str_or("websiteUrl", "").trim();
        let username = payload.get_str_or("username", "").trim();
        let fallback_title = if !raw_title.is_empty() {
            raw_title.to_string()
        } else if !parsed_cfg.issuer.trim().is_empty() {
            parsed_cfg.issuer.trim().to_string()
        } else if !website_url.is_empty() {
            website_url.to_string()
        } else {
            "2FA Account".to_string()
        };
        let resolved_user = if !username.is_empty() {
            username.to_string()
        } else {
            parsed_cfg.account.clone()
        };
        let new_item = VaultItem {
            vault_id: vault.default_vault_id(),
            title: fallback_title,
            category: Category::Login,
            username: resolved_user,
            website_url: website_url.to_string(),
            has_totp: true,
            totp_secret: parsed_cfg.secret.clone(),
            ..Default::default()
        };
        vault
            .upsert_item(new_item)
            .map_err(|e| ("SAVE_FAILED", e.message().to_string()))?
    } else {
        return Err((
            "INVALID_ARG",
            "Thiếu itemId hoặc tùy chọn createNew để lưu TOTP".to_string(),
        ));
    };

    let (code, remaining) = generate_totp(&parsed_cfg.secret, 0);
    let mut data = JsonValue::new_object();
    data.insert("itemId", JsonValue::String(saved_id));
    data.insert("code", JsonValue::String(code));
    data.insert("remaining", JsonValue::Number(remaining as f64));
    data.insert("issuer", JsonValue::String(parsed_cfg.issuer));
    data.insert("account", JsonValue::String(parsed_cfg.account));
    Ok(data)
}

#[cfg(test)]
mod tests {
    use super::*;
    use svm_storage::VaultStorage;

    #[test]
    fn test_update_totp_action_requires_unlock_and_valid_item() {
        let dir = std::env::temp_dir().join(format!(
            "svm-bridge-totp-test-{}",
            svm_crypto::generate_uuid_v4()
        ));
        let mut svc = VaultService::with_storage(VaultStorage::new(dir.to_string_lossy()));
        let mut payload = JsonValue::new_object();
        payload.insert(
            "uri",
            JsonValue::String("otpauth://totp/GitLab:alice?secret=JBSWY3DPEHPK3PXP".to_string()),
        );
        payload.insert("createNew", JsonValue::Bool(true));

        let locked_res = handle_update_totp_action(&mut svc, &payload);
        assert_eq!(locked_res.unwrap_err().0, "VAULT_LOCKED");

        svc.create_vault("Personal Vault", "MasterPass!123")
            .unwrap();

        let missing_res = handle_update_totp_action(
            &mut svc,
            &JsonValue::parse(
                r#"{"itemId":"missing-id","uri":"otpauth://totp/GitLab:alice?secret=JBSWY3DPEHPK3PXP"}"#,
            )
            .unwrap(),
        );
        assert_eq!(missing_res.unwrap_err().0, "NOT_FOUND");

        let fido_res = handle_update_totp_action(
            &mut svc,
            &JsonValue::parse(r#"{"createNew":true,"uri":"FIDO:/123456"}"#).unwrap(),
        );
        assert_eq!(fido_res.unwrap_err().0, "INVALID_OTPAUTH");

        let created = handle_update_totp_action(&mut svc, &payload).unwrap();
        let saved_id = created.get_string("itemId").unwrap();
        assert_eq!(created.get_string("issuer").unwrap(), "GitLab");
        assert_eq!(created.get_string("code").unwrap().len(), 6);
        assert!(svc
            .items()
            .iter()
            .any(|it| it.id == saved_id && it.has_totp));

        // Cập nhật lại item vừa tạo với secret mới
        let update_payload = JsonValue::parse(&format!(
            r#"{{"itemId":"{saved_id}","uri":"otpauth://totp/GitLab:alice?secret=GEZDGNBVGY3TQOJQGEZDGNBVGY3TQOJQ"}}"#
        ))
        .unwrap();
        let updated = handle_update_totp_action(&mut svc, &update_payload).unwrap();
        assert_eq!(updated.get_string("itemId").unwrap(), saved_id);

        let _ = std::fs::remove_dir_all(&dir);
    }
}
