//! Giải mã mã QR từ bộ đệm hình ảnh RGBA hoặc Luma8 (grayscale) bằng thuật toán `rqrr`.

use svm_domain::DomainError;

fn validate_dims(
    width: u32,
    height: u32,
    bpp: usize,
    len: usize,
) -> Result<(usize, usize), DomainError> {
    if width == 0 || height == 0 || width > 8192 || height > 8192 {
        return Err(DomainError::InvalidArg(
            "Invalid image dimensions for QR decode".to_string(),
        ));
    }
    let expected_len = (width as usize)
        .checked_mul(height as usize)
        .and_then(|px| px.checked_mul(bpp))
        .ok_or_else(|| DomainError::InvalidArg("Image dimension overflow".to_string()))?;
    if len != expected_len {
        return Err(DomainError::InvalidArg(
            "Image buffer length does not match dimensions".to_string(),
        ));
    }
    Ok((width as usize, height as usize))
}

fn decode_prepared_luma<F>(w: usize, h: usize, fill: F) -> Result<String, DomainError>
where
    F: FnMut(usize, usize) -> u8,
{
    let mut prepared = rqrr::PreparedImage::prepare_from_greyscale(w, h, fill);
    for grid in prepared.detect_grids() {
        if let Ok((_, content)) = grid.decode() {
            return Ok(content);
        }
    }
    Err(DomainError::NotFound(
        "No valid QR code detected in image".to_string(),
    ))
}

/// Giải mã nội dung văn bản từ bộ đệm pixel grayscale Luma8 (`width * height` bytes).
pub fn decode_qr_luma(pixels: &[u8], width: u32, height: u32) -> Result<String, DomainError> {
    let (w, h) = validate_dims(width, height, 1, pixels.len())?;
    decode_prepared_luma(w, h, |x, y| pixels[y * w + x])
}

/// Giải mã nội dung văn bản từ bộ đệm pixel RGBA8888 (`width * height * 4` bytes).
pub fn decode_qr_rgba(pixels: &[u8], width: u32, height: u32) -> Result<String, DomainError> {
    let (w, h) = validate_dims(width, height, 4, pixels.len())?;
    decode_prepared_luma(w, h, |x, y| {
        let idx = (y * w + x) * 4;
        let r = pixels[idx] as u32;
        let g = pixels[idx + 1] as u32;
        let b = pixels[idx + 2] as u32;
        let a = pixels[idx + 3] as u32;
        let luma = (299 * r + 587 * g + 114 * b) / 1000;
        // Trộn alpha trên nền trắng để mã QR có nền trong suốt vẫn giữ tương phản chuẩn
        ((luma * a + 255 * (255 - a)) / 255) as u8
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_decode_qr_rgba_validation() {
        assert!(matches!(
            decode_qr_rgba(&[0u8; 4], 0, 1),
            Err(DomainError::InvalidArg(_))
        ));
        assert!(matches!(
            decode_qr_rgba(&[0u8; 4], 9000, 1),
            Err(DomainError::InvalidArg(_))
        ));
        assert!(matches!(
            decode_qr_rgba(&[0u8; 3], 1, 1),
            Err(DomainError::InvalidArg(_))
        ));
        let blank = vec![255u8; 32 * 32 * 4];
        assert!(matches!(
            decode_qr_rgba(&blank, 32, 32),
            Err(DomainError::NotFound(_))
        ));

        let blank_luma = vec![255u8; 32 * 32];
        assert!(matches!(
            decode_qr_luma(&blank_luma, 32, 32),
            Err(DomainError::NotFound(_))
        ));
        assert!(matches!(
            decode_qr_luma(&[0u8; 3], 2, 2),
            Err(DomainError::InvalidArg(_))
        ));
    }
}
