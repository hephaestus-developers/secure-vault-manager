/**
 * @file extension/src/popup/components/icons.ts
 * @brief Unified SVG icon catalog synchronized with Desktop App (ui/resources/icons).
 */

export function createSvgElement<T extends SVGElement>(
  tag: string,
  attrs: Record<string, string>,
): T {
  const el = document.createElementNS("http://www.w3.org/2000/svg", tag) as T;
  for (const [key, val] of Object.entries(attrs)) {
    el.setAttribute(key, val);
  }
  return el;
}

export function createBaseSvg(
  viewBox = "0 0 24 24",
  width = 16,
  height = 16,
  strokeWidth = 2,
): SVGSVGElement {
  return createSvgElement<SVGSVGElement>("svg", {
    viewBox,
    width: String(width),
    height: String(height),
    fill: "none",
    stroke: "currentColor",
    "stroke-width": String(strokeWidth),
    "stroke-linecap": "round",
    "stroke-linejoin": "round",
  });
}

/**
 * Backward compatibility: Single path SVG icon.
 */
export function createSvgPathIcon(pathData: string, size = 14): SVGSVGElement {
  const svg = createBaseSvg("0 0 24 24", size, size, 2);
  const path = createSvgElement<SVGPathElement>("path", { d: pathData });
  svg.appendChild(path);
  return svg;
}

/**
 * Search icon matching ui/resources/icons/actions/search.svg.
 */
export function createSearchIcon(size = 14): SVGSVGElement {
  const svg = createBaseSvg("0 0 24 24", size, size, 2);
  const circle = createSvgElement<SVGCircleElement>("circle", {
    cx: "11",
    cy: "11",
    r: "8",
  });
  const line = createSvgElement<SVGLineElement>("line", {
    x1: "21",
    y1: "21",
    x2: "16.65",
    y2: "16.65",
  });
  svg.appendChild(circle);
  svg.appendChild(line);
  return svg;
}

/**
 * Copy icon matching ui/resources/icons/actions/copy.svg.
 */
export function createCopyIcon(size = 14): SVGSVGElement {
  const svg = createBaseSvg("0 0 24 24", size, size, 2);
  const rect = createSvgElement<SVGRectElement>("rect", {
    width: "14",
    height: "14",
    x: "8",
    y: "8",
    rx: "2",
    ry: "2",
  });
  const path = createSvgElement<SVGPathElement>("path", {
    d: "M4 16c-1.1 0-2-.9-2-2V4c0-1.1.9-2 2-2h10c1.1 0 2 .9 2 2",
  });
  svg.appendChild(rect);
  svg.appendChild(path);
  return svg;
}

/**
 * Lock icon matching ui/resources/icons/security/lock.svg.
 */
export function createLockIcon(size = 16): SVGSVGElement {
  const svg = createBaseSvg("0 0 24 24", size, size, 2);
  const rect = createSvgElement<SVGRectElement>("rect", {
    x: "3",
    y: "11",
    width: "18",
    height: "11",
    rx: "2",
    ry: "2",
  });
  const path = createSvgElement<SVGPathElement>("path", {
    d: "M7 11V7a5 5 0 0 1 10 0v4",
  });
  svg.appendChild(rect);
  svg.appendChild(path);
  return svg;
}

/**
 * Unlock icon matching ui/resources/icons/security/unlock.svg.
 */
export function createUnlockIcon(size = 16): SVGSVGElement {
  const svg = createBaseSvg("0 0 24 24", size, size, 2);
  const rect = createSvgElement<SVGRectElement>("rect", {
    x: "3",
    y: "11",
    width: "18",
    height: "11",
    rx: "2",
    ry: "2",
  });
  const path = createSvgElement<SVGPathElement>("path", {
    d: "M7 11V7a5 5 0 0 1 9.9-1",
  });
  svg.appendChild(rect);
  svg.appendChild(path);
  return svg;
}

/**
 * Eye icon matching ui/resources/icons/actions/eye.svg.
 */
export function createEyeIcon(size = 16): SVGSVGElement {
  const svg = createBaseSvg("0 0 24 24", size, size, 2);
  const path = createSvgElement<SVGPathElement>("path", {
    d: "M2.062 12.348a1 1 0 0 1 0-.696 10.75 10.75 0 0 1 19.876 0 1 1 0 0 1 0 .696 10.75 10.75 0 0 1-19.876 0",
  });
  const circle = createSvgElement<SVGCircleElement>("circle", {
    cx: "12",
    cy: "12",
    r: "3",
  });
  svg.appendChild(path);
  svg.appendChild(circle);
  return svg;
}

/**
 * Eye-off icon matching ui/resources/icons/actions/eye_off.svg.
 */
export function createEyeOffIcon(size = 16): SVGSVGElement {
  const svg = createBaseSvg("0 0 24 24", size, size, 2);
  const p1 = createSvgElement<SVGPathElement>("path", {
    d: "M10.733 5.076a10.744 10.744 0 0 1 11.205 6.575 1 1 0 0 1 0 .696 10.747 10.747 0 0 1-1.444 2.49",
  });
  const p2 = createSvgElement<SVGPathElement>("path", {
    d: "M14.084 14.158a3 3 0 0 1-4.242-4.242",
  });
  const p3 = createSvgElement<SVGPathElement>("path", {
    d: "M17.479 17.499a10.75 10.75 0 0 1-15.417-5.151 1 1 0 0 1 0-.696 10.75 10.75 0 0 1 4.446-5.143",
  });
  const line = createSvgElement<SVGLineElement>("line", {
    x1: "2",
    y1: "2",
    x2: "22",
    y2: "22",
  });
  svg.appendChild(p1);
  svg.appendChild(p2);
  svg.appendChild(p3);
  svg.appendChild(line);
  return svg;
}

/**
 * Shield check icon matching ui/resources/icons/security/shield_check.svg.
 */
export function createShieldCheckIcon(size = 18): SVGSVGElement {
  const svg = createBaseSvg("0 0 24 24", size, size, 2);
  const p1 = createSvgElement<SVGPathElement>("path", {
    d: "M12 22s8-4 8-10V5l-8-3-8 3v7c0 6 8 10 8 10z",
  });
  const p2 = createSvgElement<SVGPathElement>("path", {
    d: "M9 12l2 2 4-4",
  });
  svg.appendChild(p1);
  svg.appendChild(p2);
  return svg;
}

/**
 * Shield alert icon (for offline / connection errors).
 */
export function createShieldAlertIcon(size = 28): SVGSVGElement {
  const svg = createBaseSvg("0 0 24 24", size, size, 1.8);
  const path = createSvgElement<SVGPathElement>("path", {
    d: "M12 22s8-4 8-10V5l-8-3-8 3v7c0 6 8 10 8 10z",
  });
  const l1 = createSvgElement<SVGLineElement>("line", {
    x1: "12",
    y1: "8",
    x2: "12",
    y2: "12",
  });
  const l2 = createSvgElement<SVGLineElement>("line", {
    x1: "12",
    y1: "16",
    x2: "12.01",
    y2: "16",
  });
  svg.appendChild(path);
  svg.appendChild(l1);
  svg.appendChild(l2);
  return svg;
}

/**
 * Key icon matching ui/resources/icons/categories/category_login.svg.
 */
export function createKeyIcon(size = 16): SVGSVGElement {
  const svg = createBaseSvg("0 0 24 24", size, size, 2);
  const path = createSvgElement<SVGPathElement>("path", {
    d: "M2 18v3c0 .6.4 1 1 1h4v-3h3v-3h2l1.4-1.4a6.5 6.5 0 1 0-4-4Z",
  });
  const circle = createSvgElement<SVGCircleElement>("circle", {
    cx: "16.5",
    cy: "7.5",
    r: "1.8",
  });
  svg.appendChild(path);
  svg.appendChild(circle);
  return svg;
}

/**
 * Passkey icon matching ui/resources/icons/categories/category_passkey.svg.
 */
export function createPasskeyIcon(size = 16): SVGSVGElement {
  const svg = createBaseSvg("0 0 24 24", size, size, 2);
  const p1 = createSvgElement<SVGPathElement>("path", {
    d: "M12 2L4 5V11C4 16.5 7.4 20.8 12 22C16.6 20.8 20 16.5 20 11V5L12 2Z",
  });
  const p2 = createSvgElement<SVGPathElement>("path", {
    d: "M9 12L11 14L15 10",
  });
  svg.appendChild(p1);
  svg.appendChild(p2);
  return svg;
}

/**
 * Desktop monitor icon.
 */
export function createDesktopIcon(size = 24): SVGSVGElement {
  const svg = createBaseSvg("0 0 24 24", size, size, 1.8);
  const rect = createSvgElement<SVGRectElement>("rect", {
    x: "2",
    y: "3",
    width: "20",
    height: "14",
    rx: "2",
    ry: "2",
  });
  const l1 = createSvgElement<SVGLineElement>("line", {
    x1: "8",
    y1: "21",
    x2: "16",
    y2: "21",
  });
  const l2 = createSvgElement<SVGLineElement>("line", {
    x1: "12",
    y1: "17",
    x2: "12",
    y2: "21",
  });
  svg.appendChild(rect);
  svg.appendChild(l1);
  svg.appendChild(l2);
  return svg;
}

/**
 * Refresh circular icon.
 */
export function createRefreshIcon(size = 13): SVGSVGElement {
  const svg = createBaseSvg("0 0 24 24", size, size, 2);
  const p1 = createSvgElement<SVGPathElement>("path", { d: "M23 4v6h-6" });
  const p2 = createSvgElement<SVGPathElement>("path", { d: "M1 20v-6h6" });
  const p3 = createSvgElement<SVGPathElement>("path", {
    d: "M3.51 9a9 9 0 0 1 14.85-3.36L23 10M1 14l4.64 4.36A9 9 0 0 0 20.49 15",
  });
  svg.appendChild(p1);
  svg.appendChild(p2);
  svg.appendChild(p3);
  return svg;
}

/**
 * Biometric / Fingerprint icon.
 */
export function createFingerprintIcon(size = 18): SVGSVGElement {
  const svg = createBaseSvg("0 0 24 24", size, size, 2);
  const p1 = createSvgElement<SVGPathElement>("path", {
    d: "M12 11c0 2-1 3-2 4s-1 2-1 4",
  });
  const p2 = createSvgElement<SVGPathElement>("path", {
    d: "M14 18a8.3 8.3 0 0 0 1-4c0-2-1-3-2-4",
  });
  const p3 = createSvgElement<SVGPathElement>("path", {
    d: "M16 11c0-3.3-2.7-6-6-6s-6 2.7-6 6a10 10 0 0 0 1 4.5",
  });
  const p4 = createSvgElement<SVGPathElement>("path", {
    d: "M19 11a9 9 0 0 0-1-4",
  });
  const p5 = createSvgElement<SVGPathElement>("path", {
    d: "M8 11a4 4 0 0 1 4-4 4 4 0 0 1 4 4",
  });
  svg.appendChild(p1);
  svg.appendChild(p2);
  svg.appendChild(p3);
  svg.appendChild(p4);
  svg.appendChild(p5);
  return svg;
}

/**
 * Animated spinner for async actions.
 */
export function createSpinnerIcon(size = 14): SVGSVGElement {
  const svg = createBaseSvg("0 0 24 24", size, size, 2);
  svg.style.animation = "spin 0.8s linear infinite";

  const circle = createSvgElement<SVGCircleElement>("circle", {
    cx: "12",
    cy: "12",
    r: "10",
    "stroke-opacity": "0.25",
  });
  const path = createSvgElement<SVGPathElement>("path", {
    d: "M12 2a10 10 0 0 1 10 10",
  });
  svg.appendChild(circle);
  svg.appendChild(path);
  return svg;
}

/**
 * Checkmark icon matching ui/resources/icons/controls/checkbox_check.svg.
 */
export function createCheckIcon(size = 16, strokeWidth = 2.5): SVGSVGElement {
  const svg = createBaseSvg("0 0 24 24", size, size, strokeWidth);
  const polyline = createSvgElement<SVGPolylineElement>("polyline", {
    points: "20 6 9 17 4 12",
  });
  svg.appendChild(polyline);
  return svg;
}

/**
 * Close cross icon matching ui/resources/icons/controls/win_close.svg.
 */
export function createCloseIcon(size = 14): SVGSVGElement {
  const svg = createBaseSvg("0 0 24 24", size, size, 2.2);
  const l1 = createSvgElement<SVGLineElement>("line", {
    x1: "18",
    y1: "6",
    x2: "6",
    y2: "18",
  });
  const l2 = createSvgElement<SVGLineElement>("line", {
    x1: "6",
    y1: "6",
    x2: "18",
    y2: "18",
  });
  svg.appendChild(l1);
  svg.appendChild(l2);
  return svg;
}

/**
 * Chevron down icon matching ui/resources/icons/controls/chevron_down.svg.
 */
export function createChevronDownIcon(size = 14): SVGSVGElement {
  const svg = createBaseSvg("0 0 24 24", size, size, 2.2);
  const polyline = createSvgElement<SVGPolylineElement>("polyline", {
    points: "6 9 12 15 18 9",
  });
  svg.appendChild(polyline);
  return svg;
}

/**
 * Autofill arrow icon.
 */
export function createAutofillArrowIcon(size = 13): SVGSVGElement {
  const svg = createBaseSvg("0 0 24 24", size, size, 2);
  const path = createSvgElement<SVGPathElement>("path", {
    d: "M14 5l7 7m0 0l-7 7m7-7H3",
  });
  svg.appendChild(path);
  return svg;
}

/**
 * Clock icon for TOTP codes.
 */
export function createTotpClockIcon(size = 13): SVGSVGElement {
  const svg = createBaseSvg("0 0 24 24", size, size, 2);
  const circle = createSvgElement<SVGCircleElement>("circle", {
    cx: "12",
    cy: "12",
    r: "9",
  });
  const polyline = createSvgElement<SVGPolylineElement>("polyline", {
    points: "12 7 12 12 15 15",
  });
  svg.appendChild(circle);
  svg.appendChild(polyline);
  return svg;
}

/**
 * User silhouette icon.
 */
export function createUserIcon(size = 13): SVGSVGElement {
  const svg = createBaseSvg("0 0 24 24", size, size, 2);
  const p1 = createSvgElement<SVGPathElement>("path", {
    d: "M16 21v-2a4 4 0 0 0-4-4H8a4 4 0 0 0-4 4v2",
  });
  const c = createSvgElement<SVGCircleElement>("circle", {
    cx: "12",
    cy: "7",
    r: "4",
  });
  svg.appendChild(p1);
  svg.appendChild(c);
  return svg;
}

/**
 * Password Generator keypad icon.
 */
export function createGeneratorIcon(size = 15): SVGSVGElement {
  const svg = createBaseSvg("0 0 24 24", size, size, 2);
  const rect = createSvgElement<SVGRectElement>("rect", {
    x: "3",
    y: "5",
    width: "18",
    height: "14",
    rx: "2",
  });
  const p = createSvgElement<SVGPathElement>("path", {
    d: "M7 10h.01M12 10h.01M17 10h.01M7 14h.01M12 14h.01M17 14h.01",
  });
  svg.appendChild(rect);
  svg.appendChild(p);
  return svg;
}

/**
 * 2FA QR Code Scanner icon.
 */
export function createQrCodeIconSvg(size = 14): SVGSVGElement {
  const svg = createBaseSvg("0 0 24 24", size, size, 2);
  for (const [x, y, w, h] of [
    [3, 3, 7, 7],
    [14, 3, 7, 7],
    [3, 14, 7, 7],
  ]) {
    const r = createSvgElement<SVGRectElement>("rect", {
      x: String(x),
      y: String(y),
      width: String(w),
      height: String(h),
      rx: "1",
    });
    svg.appendChild(r);
  }
  const path = createSvgElement<SVGPathElement>("path", {
    d: "M14 14h3v3h-3zM19 14v3M14 19h7M19 19v2",
  });
  svg.appendChild(path);
  return svg;
}
