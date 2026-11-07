//! Điểm nhập C-ABI Linear Memory Exports cho trình nạp WebAssembly thuần (`svm_wasm_alloc`, `svm_wasm_dealloc`, `svm_wasm_invoke`).

use svm_crypto::seed_entropy;

use crate::dispatcher::wasm_dispatch_json;

/// Điểm nhập C-ABI cho môi trường WebAssembly để nạp hạt giống ngẫu nhiên.
#[unsafe(no_mangle)]
pub extern "C" fn svm_wasm_seed(seed: u64) {
    seed_entropy(seed);
}

/// Cấp phát một vùng nhớ có kích thước `size` byte trên bộ nhớ tuyến tính WASM.
#[unsafe(no_mangle)]
pub extern "C" fn svm_wasm_alloc(size: usize) -> *mut u8 {
    let mut buf = Vec::with_capacity(size);
    let ptr = buf.as_mut_ptr();
    std::mem::forget(buf);
    ptr
}

/// Giải phóng vùng nhớ đã cấp phát bởi `svm_wasm_alloc`.
///
/// # Safety
/// `ptr` và `cap` phải khớp với vùng nhớ đã được cấp phát trước đó.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn svm_wasm_dealloc(ptr: *mut u8, cap: usize) {
    if !ptr.is_null() && cap > 0 {
        unsafe {
            let _ = Vec::from_raw_parts(ptr, 0, cap);
        }
    }
}

/// Thực thi lệnh JSON từ bộ nhớ tuyến tính WASM và ghi độ dài kết quả vào `out_len_ptr`.
///
/// # Safety
/// `req_ptr` phải trỏ tới vùng nhớ UTF-8 hợp lệ dài `req_len` byte, và `out_len_ptr` phải hợp lệ.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn svm_wasm_invoke(
    req_ptr: *const u8,
    req_len: usize,
    out_len_ptr: *mut usize,
) -> *mut u8 {
    let req_slice = unsafe { std::slice::from_raw_parts(req_ptr, req_len) };
    let req_str = std::str::from_utf8(req_slice).unwrap_or("{}");
    let resp_str = wasm_dispatch_json(req_str);
    let mut resp_bytes = resp_str.into_bytes();
    resp_bytes.shrink_to_fit();
    let len = resp_bytes.len();
    let ptr = resp_bytes.as_mut_ptr();
    std::mem::forget(resp_bytes);
    if !out_len_ptr.is_null() {
        unsafe {
            *out_len_ptr = len;
        }
    }
    ptr
}
