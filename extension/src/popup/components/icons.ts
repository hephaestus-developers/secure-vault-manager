/**
 * @file extension/src/popup/components/icons.ts
 * @brief Tiện ích tạo biểu tượng SVG dùng chung cho các thành phần Popup.
 */

export function createSvgPathIcon(pathData: string, size = 14): SVGSVGElement {
  const svg = document.createElementNS("http://www.w3.org/2000/svg", "svg");
  svg.setAttribute("width", String(size));
  svg.setAttribute("height", String(size));
  svg.setAttribute("viewBox", "0 0 24 24");
  svg.setAttribute("fill", "none");
  svg.setAttribute("stroke", "currentColor");
  svg.setAttribute("stroke-width", "2");
  const path = document.createElementNS("http://www.w3.org/2000/svg", "path");
  path.setAttribute("d", pathData);
  svg.appendChild(path);
  return svg;
}

export function createQrCodeIconSvg(size = 14): SVGSVGElement {
  const svg = document.createElementNS("http://www.w3.org/2000/svg", "svg");
  svg.setAttribute("width", String(size));
  svg.setAttribute("height", String(size));
  svg.setAttribute("viewBox", "0 0 24 24");
  svg.setAttribute("fill", "none");
  svg.setAttribute("stroke", "currentColor");
  svg.setAttribute("stroke-width", "2");
  for (const [x, y, w, h] of [
    [3, 3, 7, 7],
    [14, 3, 7, 7],
    [3, 14, 7, 7],
  ]) {
    const r = document.createElementNS("http://www.w3.org/2000/svg", "rect");
    r.setAttribute("x", String(x));
    r.setAttribute("y", String(y));
    r.setAttribute("width", String(w));
    r.setAttribute("height", String(h));
    r.setAttribute("rx", "1");
    svg.appendChild(r);
  }
  const path = document.createElementNS("http://www.w3.org/2000/svg", "path");
  path.setAttribute("d", "M14 14h3v3h-3zM19 14v3M14 19h7M19 19v2");
  svg.appendChild(path);
  return svg;
}
