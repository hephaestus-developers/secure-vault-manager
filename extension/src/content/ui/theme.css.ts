/**
 * @file extension/src/content/ui/theme.css.ts
 * @brief Hệ thống CSS Theme Catppuccin và Reset Styles cô lập trong Shadow DOM.
 */

export const COMMON_SHADOW_RESET_CSS = `
  :host {
    all: initial;
    box-sizing: border-box;
    font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif;
    font-size: 13px;
    line-height: 1.5;
  }
  *, *::before, *::after {
    box-sizing: border-box;
  }
`;

export const COMMON_THEME_CSS = `
  @keyframes svmSlideIn {
    from {
      opacity: 0;
      transform: translateY(-16px) scale(0.96);
    }
    to {
      opacity: 1;
      transform: translateY(0) scale(1);
    }
  }

  @keyframes svmFadeIn {
    from {
      opacity: 0;
      transform: scale(0.96);
    }
    to {
      opacity: 1;
      transform: scale(1);
    }
  }

  .svm-close-btn {
    background: transparent;
    border: none;
    color: #6c7086;
    cursor: pointer;
    font-size: 16px;
    line-height: 1;
    padding: 4px;
    border-radius: 4px;
    display: inline-flex;
    align-items: center;
    justify-content: center;
    transition: color 0.15s ease, background 0.15s ease;
  }
  .svm-close-btn:hover {
    color: #cdd6f4;
    background: #313244;
  }

  .svm-status-badge {
    font-size: 10.5px;
    font-weight: 600;
    padding: 2px 7px;
    border-radius: 999px;
    display: inline-flex;
    align-items: center;
    gap: 3px;
    letter-spacing: 0.2px;
  }
  .svm-badge-locked {
    background: rgba(243, 139, 168, 0.15);
    color: #f38ba8;
    border: 1px solid rgba(243, 139, 168, 0.3);
  }
  .svm-badge-unlocked {
    background: rgba(166, 227, 161, 0.15);
    color: #a6e3a1;
    border: 1px solid rgba(166, 227, 161, 0.3);
  }

  .svm-input {
    width: 100%;
    box-sizing: border-box;
    background: #11111b;
    border: 1px solid #313244;
    border-radius: 8px;
    padding: 8px 10px;
    color: #cdd6f4;
    font-size: 13px;
    outline: none;
    font-family: inherit;
    transition: border-color 0.15s ease, box-shadow 0.15s ease;
  }
  .svm-input:focus {
    border-color: #89b4fa;
    box-shadow: 0 0 0 2px rgba(137, 180, 250, 0.2);
  }

  .svm-btn {
    padding: 7px 14px;
    border-radius: 6px;
    cursor: pointer;
    font-size: 12.5px;
    font-weight: 600;
    border: none;
    transition: background 0.15s ease, opacity 0.15s ease;
    display: inline-flex;
    align-items: center;
    justify-content: center;
    gap: 6px;
    white-space: nowrap;
    text-decoration: none;
  }
  .svm-btn:disabled {
    opacity: 0.5;
    cursor: not-allowed;
  }
  .svm-btn-primary {
    background: #89b4fa;
    color: #11111b;
  }
  .svm-btn-primary:hover:not(:disabled) {
    background: #b4befe;
  }
  .svm-btn-secondary {
    background: #313244;
    color: #cdd6f4;
    border: 1px solid #45475a;
  }
  .svm-btn-secondary:hover:not(:disabled) {
    background: #45475a;
  }
  .svm-btn-bio {
    background: #a6e3a1;
    color: #11111b;
  }
  .svm-btn-bio:hover:not(:disabled) {
    background: #94e2d5;
  }

  @media (prefers-color-scheme: light) {
    .svm-close-btn {
      color: #8c8fa1;
    }
    .svm-close-btn:hover {
      color: #4c4f69;
      background: #e6e9ef;
    }
    .svm-input {
      background: #f4f5f8;
      border-color: #ccd0da;
      color: #1e1e2e;
    }
    .svm-btn-primary {
      background: #1e66f5;
      color: #ffffff;
    }
    .svm-btn-primary:hover:not(:disabled) {
      background: #04a5e5;
    }
    .svm-btn-secondary {
      background: #e6e9ef;
      color: #1e1e2e;
      border-color: #ccd0da;
    }
    .svm-btn-secondary:hover:not(:disabled) {
      background: #dce0e8;
    }
  }
`;
