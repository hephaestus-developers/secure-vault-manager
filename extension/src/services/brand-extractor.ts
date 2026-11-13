/**
 * @file extension/src/services/brand-extractor.ts
 * @brief Logic trích xuất thương hiệu / tên dịch vụ và định danh thông minh cho tài khoản đăng nhập.
 */

/**
 * Loại bỏ các từ khóa hành động đăng nhập phổ biến và các ký tự phân tách thừa.
 */
export function cleanActionWords(raw: string): string {
  return raw
    .replace(
      /\b(sign[-_ ]?in|log[-_ ]?in|đăng[-_ ]?nhập|welcome|authentication|xác[-_ ]?thực|login)\b(?:\s+(?:to|into|on|vào))?/gi,
      "",
    )
    .replace(/[[\]()|•·–—:_-]+/g, " ")
    .trim();
}

/**
 * Trích xuất Tên Dịch vụ / Thương hiệu từ trang web hiện tại (metadata, heading, document.title).
 */
export function extractBrandOrServiceName(formElement?: Document | Element | null): string {
  if (typeof document === "undefined") {
    return "Website";
  }

  // 1. Thẻ metadata chuẩn
  const ogSite = document
    .querySelector('meta[property="og:site_name"], meta[name="og:site_name"]')
    ?.getAttribute("content")
    ?.trim();
  if (ogSite && ogSite.length >= 2 && ogSite.length <= 40) {
    return ogSite;
  }

  const appName = document
    .querySelector('meta[name="application-name"], meta[name="apple-mobile-web-app-title"]')
    ?.getAttribute("content")
    ?.trim();
  if (appName && appName.length >= 2 && appName.length <= 40) {
    return appName;
  }

  // 2. Thẻ tiêu đề Heading của Form đăng nhập (ví dụ: "Sign in to Jenkins", "Đăng nhập Shopee")
  if (formElement) {
    const headingEl = formElement.querySelector(
      "h1, h2, h3, legend, .brand, .title, [role='heading']",
    );
    const headingText = (headingEl?.textContent || "").trim();
    if (headingText) {
      const match =
        /(?:sign[-_ ]?in|log[-_ ]?in|welcome|đăng[-_ ]?nhập)(?:\s+(?:to|into|on|vào))?\s+([^.!?\n\r]+)/i.exec(
          headingText,
        );
      if (match?.[1]) {
        const candidate = cleanActionWords(match[1]);
        if (candidate.length >= 2 && candidate.length <= 40) {
          return candidate;
        }
      }
    }
  }

  // 3. Toàn trang Heading <h1> / <h2>
  const pageHeading = document.querySelector("h1, h2");
  if (pageHeading) {
    const hText = (pageHeading.textContent || "").trim();
    const match =
      /(?:sign[-_ ]?in|log[-_ ]?in|welcome|đăng[-_ ]?nhập)(?:\s+(?:to|into|on|vào))?\s+([^.!?\n\r]+)/i.exec(
        hText,
      );
    if (match?.[1]) {
      const candidate = cleanActionWords(match[1]);
      if (candidate.length >= 2 && candidate.length <= 40) {
        return candidate;
      }
    }
  }

  // 4. Phân tích document.title
  const docTitle = document.title || "";
  if (docTitle) {
    const parts = docTitle.split(/[-|•·–—[\]()]+/);
    for (const part of parts) {
      const cleaned = cleanActionWords(part);
      if (
        cleaned.length >= 2 &&
        cleaned.length <= 35 &&
        !/^(home|index|dashboard|portal|app|welcome|login|sign in|auth)$/i.test(cleaned)
      ) {
        return cleaned;
      }
    }
  }

  // 5. Dự phòng theo hostname (cắt www.)
  if (typeof window !== "undefined" && window.location) {
    const host = window.location.hostname.replace(/^www\./, "").toLowerCase();
    if (host) return host;
  }
  return "Website";
}

/**
 * Tạo tiêu đề định danh thông minh cho mục đăng nhập: "<Brand>: <username>".
 */
export function generateSmartCredentialTitle(
  username?: string,
  formElement?: Document | Element | null,
): string {
  const brand = extractBrandOrServiceName(formElement);
  const cleanUser = (username || "").trim();
  if (cleanUser) {
    return `${brand}: ${cleanUser}`;
  }
  return brand;
}
