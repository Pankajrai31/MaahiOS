<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Maahi OS - Design System v2</title>
    <style>
        /* ============================================================
           PAGE STYLES — Clean documentation wrapper (NOT the OS theme)
           ============================================================ */
        * { margin: 0; padding: 0; box-sizing: border-box; }

        body {
            font-family: 'Segoe UI', Tahoma, sans-serif;
            background: #f5f7fa;
            color: #1a1a2e;
            line-height: 1.6;
            padding: 40px;
        }

        .design-system {
            max-width: 1200px;
            margin: 0 auto;
            background: #fff;
            border-radius: 12px;
            padding: 48px;
            box-shadow: 0 4px 20px rgba(0,0,0,0.08);
        }

        h1.main-title {
            font-size: 42px;
            font-weight: 700;
            color: #131A22;
            margin-bottom: 8px;
        }

        .main-subtitle {
            font-size: 16px;
            color: #888;
            margin-bottom: 48px;
        }

        .section {
            margin-bottom: 48px;
            padding-bottom: 32px;
            border-bottom: 1px solid #eee;
        }

        .section:last-child { border-bottom: none; }

        .section-title {
            font-size: 22px;
            font-weight: 600;
            color: #131A22;
            margin-bottom: 8px;
        }

        .section-desc {
            font-size: 14px;
            color: #888;
            margin-bottom: 24px;
        }

        .subsection-title {
            font-size: 13px;
            font-weight: 600;
            color: #666;
            margin-bottom: 12px;
            text-transform: uppercase;
            letter-spacing: 0.5px;
        }

        .preview-box {
            background: #f8f9fa;
            border: 1px solid #e0e0e0;
            border-radius: 8px;
            padding: 24px;
        }

        .preview-row {
            display: flex;
            gap: 16px;
            flex-wrap: wrap;
            align-items: flex-start;
        }

        .preview-col {
            display: flex;
            flex-direction: column;
            gap: 12px;
        }

        .component-label {
            font-size: 11px;
            color: #999;
            text-transform: uppercase;
            letter-spacing: 0.5px;
            margin-bottom: 4px;
        }

        /* ============================================================
           MAAHI OS THEME — The actual OS component styles
           Blend: MaahiOS light blue-gray palette + embossed 3D depth
           NOT a Windows clone — MaahiOS has its own identity
           ============================================================ */

        /* --- CSS Variables for the OS Theme --- */
        :root {
            /* MaahiOS Chrome - Light blue-gray base */
            --os-chrome: #D8DBE8;
            --os-chrome-light: #E8EAF2;
            --os-chrome-lighter: #F0F1F6;
            --os-chrome-dark: #C0C4D4;
            --os-chrome-darker: #B0B4C6;

            /* 3D Bevel Colors (for embossed effect on light chrome) */
            --bevel-light: #F4F5FA;
            --bevel-dark: #9498AC;

            /* MaahiOS Accent Colors */
            --os-accent: #2B5BB5;
            --os-accent-light: #4A7BD5;
            --os-accent-dark: #1B4A9A;
            --os-teal: #1E8A65;
            --os-teal-dark: #156B4E;
            --os-cyan: #18A080;

            /* Title Bar Gradient — MaahiOS signature blue */
            --titlebar-start: #1B3F8B;
            --titlebar-end: #2B5BB5;
            --titlebar-inactive-start: #9498AC;
            --titlebar-inactive-end: #B0B4C6;

            /* Surfaces */
            --os-surface: #FFFFFF;
            --os-surface-raised: #D8DBE8;
            --os-surface-sunken: #C8CBD8;
            --os-input-bg: #FFFFFF;
            --os-white: #FFFFFF;

            /* Status */
            --os-success: #28A745;
            --os-warning: #E8A317;
            --os-danger: #DC3545;
            --os-info: #17A2B8;

            /* Text */
            --os-text: #1A1A2E;
            --os-text-secondary: #5A5D76;
            --os-text-disabled: #9CA0B4;
            --os-text-inverse: #FFFFFF;

            /* Font */
            --os-font: 'Segoe UI', Tahoma, sans-serif;
            --os-font-mono: 'Consolas', 'Courier New', monospace;
        }

        /* --- Embossed border mixins (via classes) --- */
        .os-raised {
            border: 2px solid;
            border-color: var(--bevel-light) var(--bevel-dark) var(--bevel-dark) var(--bevel-light);
        }

        .os-sunken {
            border: 2px solid;
            border-color: var(--bevel-dark) var(--bevel-light) var(--bevel-light) var(--bevel-dark);
        }

        .os-groove {
            border: 2px groove var(--os-chrome-light);
        }

        .os-ridge {
            border: 2px ridge var(--os-chrome-light);
        }

        /* Desktop background */
        .os-desktop-bg {
            background: linear-gradient(135deg, #3A7BB8 0%, #5A9BD8 50%, #3A8BC8 100%);
        }
    </style>
</head>
<body>
    <div class="design-system">
        <h1 class="main-title">Maahi OS Design System</h1>
        <div class="main-subtitle">Version 2 · Embossed Light Theme · Component Reference · March 2026</div>

        <!-- ============================================================ -->
        <!-- COLOR PALETTE -->
        <!-- ============================================================ -->
        <div class="section">
            <h2 class="section-title">Color Palette</h2>
            <div class="section-desc">MaahiOS uses a light blue-gray chrome with teal/blue accents. Depth comes from embossed 3D borders, not drop shadows.</div>

            <div class="subsection-title">Chrome (System UI Base)</div>
            <div class="preview-box" style="margin-bottom: 20px;">
                <div class="preview-row">
                    <div style="text-align:center"><div style="width:64px;height:64px;background:#B0B4C6;border:1px solid #999;"></div><div style="font-size:10px;margin-top:4px;color:#666">#B0B4C6<br>Darker</div></div>
                    <div style="text-align:center"><div style="width:64px;height:64px;background:#C0C4D4;border:1px solid #999;"></div><div style="font-size:10px;margin-top:4px;color:#666">#C0C4D4<br>Dark</div></div>
                    <div style="text-align:center"><div style="width:64px;height:64px;background:#D8DBE8;border:1px solid #999;"></div><div style="font-size:10px;margin-top:4px;color:#666">#D8DBE8<br>Chrome</div></div>
                    <div style="text-align:center"><div style="width:64px;height:64px;background:#E8EAF2;border:1px solid #999;"></div><div style="font-size:10px;margin-top:4px;color:#666">#E8EAF2<br>Light</div></div>
                    <div style="text-align:center"><div style="width:64px;height:64px;background:#F0F1F6;border:1px solid #999;"></div><div style="font-size:10px;margin-top:4px;color:#666">#F0F1F6<br>Lighter</div></div>
                    <div style="text-align:center"><div style="width:64px;height:64px;background:#F4F5FA;border:1px solid #999;"></div><div style="font-size:10px;margin-top:4px;color:#666">#F4F5FA<br>Bevel Light</div></div>
                </div>
            </div>

            <div class="subsection-title">Accent Colors</div>
            <div class="preview-box" style="margin-bottom: 20px;">
                <div class="preview-row">
                    <div style="text-align:center"><div style="width:64px;height:64px;background:#1B3F8B;border:1px solid #333;"></div><div style="font-size:10px;margin-top:4px;color:#666">#1B3F8B<br>Deep Blue</div></div>
                    <div style="text-align:center"><div style="width:64px;height:64px;background:#2B5BB5;border:1px solid #333;"></div><div style="font-size:10px;margin-top:4px;color:#666">#2B5BB5<br>Accent Blue</div></div>
                    <div style="text-align:center"><div style="width:64px;height:64px;background:#156B4E;border:1px solid #333;"></div><div style="font-size:10px;margin-top:4px;color:#666">#156B4E<br>Teal Dark</div></div>
                    <div style="text-align:center"><div style="width:64px;height:64px;background:#1E8A65;border:1px solid #333;"></div><div style="font-size:10px;margin-top:4px;color:#666">#1E8A65<br>Teal</div></div>
                    <div style="text-align:center"><div style="width:64px;height:64px;background:#18A080;border:1px solid #333;"></div><div style="font-size:10px;margin-top:4px;color:#666">#18A080<br>Cyan</div></div>
                </div>
            </div>

            <div class="subsection-title">Status Colors</div>
            <div class="preview-box" style="margin-bottom: 20px;">
                <div class="preview-row">
                    <div style="text-align:center"><div style="width:64px;height:64px;background:#28A745;border:1px solid #333;"></div><div style="font-size:10px;margin-top:4px;color:#666">#28A745<br>Success</div></div>
                    <div style="text-align:center"><div style="width:64px;height:64px;background:#E8A317;border:1px solid #333;"></div><div style="font-size:10px;margin-top:4px;color:#666">#E8A317<br>Warning</div></div>
                    <div style="text-align:center"><div style="width:64px;height:64px;background:#DC3545;border:1px solid #333;"></div><div style="font-size:10px;margin-top:4px;color:#666">#DC3545<br>Danger</div></div>
                    <div style="text-align:center"><div style="width:64px;height:64px;background:#17A2B8;border:1px solid #333;"></div><div style="font-size:10px;margin-top:4px;color:#666">#17A2B8<br>Info</div></div>
                </div>
            </div>

            <div class="subsection-title">Text & Surfaces</div>
            <div class="preview-box">
                <div class="preview-row">
                    <div style="text-align:center"><div style="width:64px;height:64px;background:#1A1A2E;border:1px solid #333;"></div><div style="font-size:10px;margin-top:4px;color:#666">#1A1A2E<br>Text</div></div>
                    <div style="text-align:center"><div style="width:64px;height:64px;background:#5A5D76;border:1px solid #333;"></div><div style="font-size:10px;margin-top:4px;color:#666">#5A5D76<br>Secondary</div></div>
                    <div style="text-align:center"><div style="width:64px;height:64px;background:#9CA0B4;border:1px solid #333;"></div><div style="font-size:10px;margin-top:4px;color:#666">#9CA0B4<br>Disabled</div></div>
                    <div style="text-align:center"><div style="width:64px;height:64px;background:#FFFFFF;border:1px solid #999;"></div><div style="font-size:10px;margin-top:4px;color:#666">#FFFFFF<br>Input BG</div></div>
                    <div style="text-align:center"><div style="width:64px;height:64px;background:linear-gradient(135deg,#3A7BB8,#5A9BD8,#3A8BC8);border:1px solid #333;"></div><div style="font-size:10px;margin-top:4px;color:#666">Gradient<br>Desktop BG</div></div>
                </div>
            </div>
        </div>

        <!-- ============================================================ -->
        <!-- BORDER STYLES -->
        <!-- ============================================================ -->
        <div class="section">
            <h2 class="section-title">Border Styles (Embossed Vocabulary)</h2>
        <div class="section-desc">The 3D beveled border is the core visual language. Raised = interactive/clickable. Sunken = content well/input. Groove/Ridge = grouping.</div>
            <div class="preview-box">
                <div class="preview-row" style="gap: 24px;">
                    <div style="text-align:center;">
                        <div class="os-raised" style="width:120px;height:60px;background:var(--os-chrome);display:flex;align-items:center;justify-content:center;color:var(--os-text);font-size:12px;">Raised (Outset)</div>
                        <div style="font-size:10px;color:#888;margin-top:4px;">Buttons, toolbars</div>
                    </div>
                    <div style="text-align:center;">
                        <div class="os-sunken" style="width:120px;height:60px;background:var(--os-surface-sunken);display:flex;align-items:center;justify-content:center;color:var(--os-text);font-size:12px;">Sunken (Inset)</div>
                        <div style="font-size:10px;color:#888;margin-top:4px;">Inputs, content areas</div>
                    </div>
                    <div style="text-align:center;">
                        <div class="os-groove" style="width:120px;height:60px;background:var(--os-chrome);display:flex;align-items:center;justify-content:center;color:var(--os-text);font-size:12px;">Groove</div>
                        <div style="font-size:10px;color:#888;margin-top:4px;">Groupboxes, sections</div>
                    </div>
                    <div style="text-align:center;">
                        <div class="os-ridge" style="width:120px;height:60px;background:var(--os-chrome);display:flex;align-items:center;justify-content:center;color:var(--os-text);font-size:12px;">Ridge</div>
                        <div style="font-size:10px;color:#888;margin-top:4px;">Highlighted dividers</div>
                    </div>
                </div>
                <div style="margin-top:16px;">
                    <div style="border-top:1px solid var(--bevel-dark);border-bottom:1px solid var(--bevel-light);margin:8px 0;"></div>
                    <div style="font-size:11px;color:#888;">↑ Etched separator line (1px dark + 1px light)</div>
                </div>
            </div>
        </div>

        <!-- ============================================================ -->
        <!-- TYPOGRAPHY -->
        <!-- ============================================================ -->
        <div class="section">
            <h2 class="section-title">Typography</h2>
            <div class="section-desc">System font stack with sizes suited for OS UI. Monospace for terminal/code content.</div>
            <div class="preview-box" style="background: var(--os-chrome); padding: 20px;">
                <div style="display:flex;flex-direction:column;gap:8px;">
                    <div style="display:flex;align-items:baseline;gap:16px;"><span style="min-width:100px;font-size:11px;color:var(--os-text-secondary);">Window Title</span><span style="min-width:50px;font-size:11px;color:var(--os-text-disabled);font-family:monospace;">12px b</span><span style="font-size:12px;font-weight:700;color:var(--os-text-inverse);">Maahi OS — File Explorer</span></div>
                    <div style="display:flex;align-items:baseline;gap:16px;"><span style="min-width:100px;font-size:11px;color:var(--os-text-secondary);">Heading 1</span><span style="min-width:50px;font-size:11px;color:var(--os-text-disabled);font-family:monospace;">24px</span><span style="font-size:24px;font-weight:700;color:var(--os-text);">The quick brown fox</span></div>
                    <div style="display:flex;align-items:baseline;gap:16px;"><span style="min-width:100px;font-size:11px;color:var(--os-text-secondary);">Heading 2</span><span style="min-width:50px;font-size:11px;color:var(--os-text-disabled);font-family:monospace;">18px</span><span style="font-size:18px;font-weight:700;color:var(--os-text);">The quick brown fox</span></div>
                    <div style="display:flex;align-items:baseline;gap:16px;"><span style="min-width:100px;font-size:11px;color:var(--os-text-secondary);">Heading 3</span><span style="min-width:50px;font-size:11px;color:var(--os-text-disabled);font-family:monospace;">15px</span><span style="font-size:15px;font-weight:600;color:var(--os-text);">The quick brown fox</span></div>
                    <div style="display:flex;align-items:baseline;gap:16px;"><span style="min-width:100px;font-size:11px;color:var(--os-text-secondary);">Body</span><span style="min-width:50px;font-size:11px;color:var(--os-text-disabled);font-family:monospace;">13px</span><span style="font-size:13px;color:var(--os-text);">The quick brown fox jumps over the lazy dog</span></div>
                    <div style="display:flex;align-items:baseline;gap:16px;"><span style="min-width:100px;font-size:11px;color:var(--os-text-secondary);">Small</span><span style="min-width:50px;font-size:11px;color:var(--os-text-disabled);font-family:monospace;">11px</span><span style="font-size:11px;color:var(--os-text-secondary);">The quick brown fox jumps over the lazy dog</span></div>
                    <div style="display:flex;align-items:baseline;gap:16px;"><span style="min-width:100px;font-size:11px;color:var(--os-text-secondary);">Caption</span><span style="min-width:50px;font-size:11px;color:var(--os-text-disabled);font-family:monospace;">10px</span><span style="font-size:10px;color:var(--os-text-disabled);">The quick brown fox jumps over the lazy dog</span></div>
                    <div style="display:flex;align-items:baseline;gap:16px;"><span style="min-width:100px;font-size:11px;color:var(--os-text-secondary);">Monospace</span><span style="min-width:50px;font-size:11px;color:var(--os-text-disabled);font-family:monospace;">13px</span><span style="font-size:13px;font-family:var(--os-font-mono);color:var(--os-teal);">0x0000FFFF kernel_main()</span></div>
                </div>
            </div>
        </div>

        <!-- ============================================================ -->
        <!-- BUTTONS -->
        <!-- ============================================================ -->
        <div class="section">
            <h2 class="section-title">Buttons</h2>
            <div class="section-desc">Embossed raised buttons. Press effect inverts the bevel to inset. Default button has extra outer glow.</div>
            <style>
                .os-btn {
                    padding: 5px 20px;
                    min-width: 75px;
                    min-height: 25px;
                    background: var(--os-chrome);
                    border: 2px solid;
                    border-color: var(--bevel-light) var(--bevel-dark) var(--bevel-dark) var(--bevel-light);
                    color: var(--os-text);
                    font-size: 13px;
                    font-family: var(--os-font);
                    cursor: pointer;
                    text-align: center;
                }
                .os-btn:active {
                    border-color: var(--bevel-dark) var(--bevel-light) var(--bevel-light) var(--bevel-dark);
                    padding: 6px 19px 4px 21px;
                }
                .os-btn:focus { outline: 1px dotted var(--os-text); outline-offset: -4px; }
                .os-btn-default {
                    box-shadow: 0 0 0 1px var(--os-teal);
                    font-weight: 700;
                }
                .os-btn-accent {
                    background: linear-gradient(180deg, var(--os-accent-light) 0%, var(--os-accent-dark) 100%);
                    border-color: var(--os-accent-light) var(--os-accent-dark) var(--os-accent-dark) var(--os-accent-light);
                    color: var(--os-text-inverse);
                    font-weight: 600;
                }
                .os-btn-accent:active {
                    border-color: var(--os-accent-dark) var(--os-accent-light) var(--os-accent-light) var(--os-accent-dark);
                }
                .os-btn-flat {
                    border: 1px solid transparent;
                    background: transparent;
                    color: var(--os-text);
                    padding: 3px 8px;
                    min-width: auto;
                }
                .os-btn-flat:hover {
                    border-color: var(--bevel-light) var(--bevel-dark) var(--bevel-dark) var(--bevel-light);
                    background: var(--os-chrome-light);
                }
                .os-btn-disabled {
                    color: var(--os-text-disabled);
                    cursor: not-allowed;
                }
                .os-btn-sm { padding: 2px 10px; font-size: 11px; min-width: 50px; min-height: 20px; }
                .os-btn-lg { padding: 7px 28px; font-size: 14px; min-width: 100px; min-height: 30px; }
            </style>
            <div class="preview-box" style="background: var(--os-chrome);">
                <div class="preview-row" style="gap: 32px;">
                    <div class="preview-col">
                        <div class="component-label" style="color:var(--os-text-secondary);">Standard</div>
                        <button class="os-btn os-btn-sm">Small</button>
                        <button class="os-btn">Button</button>
                        <button class="os-btn os-btn-lg">Large</button>
                    </div>
                    <div class="preview-col">
                        <div class="component-label" style="color:var(--os-text-secondary);">Default (OK)</div>
                        <button class="os-btn os-btn-default">OK</button>
                        <button class="os-btn os-btn-default os-btn-lg">Accept</button>
                    </div>
                    <div class="preview-col">
                        <div class="component-label" style="color:var(--os-text-secondary);">Accent</div>
                        <button class="os-btn os-btn-accent">Apply</button>
                        <button class="os-btn os-btn-accent os-btn-lg">Save</button>
                    </div>
                    <div class="preview-col">
                        <div class="component-label" style="color:var(--os-text-secondary);">Flat (Toolbar)</div>
                        <div style="display:flex;gap:4px;">
                            <button class="os-btn os-btn-flat">📁</button>
                            <button class="os-btn os-btn-flat">💾</button>
                            <button class="os-btn os-btn-flat">📋</button>
                            <button class="os-btn os-btn-flat">✂️</button>
                        </div>
                    </div>
                    <div class="preview-col">
                        <div class="component-label" style="color:var(--os-text-secondary);">Disabled</div>
                        <button class="os-btn os-btn-disabled" disabled>Disabled</button>
                    </div>
                </div>
            </div>
        </div>

        <!-- ============================================================ -->
        <!-- FORM CONTROLS -->
        <!-- ============================================================ -->
        <div class="section">
            <h2 class="section-title">Form Controls</h2>
            <div class="section-desc">Sunken inset inputs on light chrome. Combo box and spin controls for OS-native feel.</div>
            <style>
                .os-input {
                    padding: 3px 6px;
                    border: 2px solid;
                    border-color: var(--bevel-dark) var(--bevel-light) var(--bevel-light) var(--bevel-dark);
                    background: var(--os-input-bg);
                    color: var(--os-text);
                    font-size: 13px;
                    font-family: var(--os-font);
                    width: 100%;
                }
                .os-input:focus { outline: none; border-color: var(--os-accent) var(--os-accent-light) var(--os-accent-light) var(--os-accent); }
                .os-input:disabled { background: var(--os-chrome); color: var(--os-text-disabled); cursor: not-allowed; }
                .os-input-error { border-color: var(--os-danger) !important; }
                .os-textarea { min-height: 70px; resize: vertical; font-family: var(--os-font); }
                .os-label { display: block; font-size: 13px; color: var(--os-text); margin-bottom: 3px; }
                .os-helper { font-size: 11px; color: var(--os-text-secondary); margin-top: 2px; }
                .os-helper-error { color: var(--os-danger); }
                .os-form-group { margin-bottom: 12px; }
                .os-combobox { display: flex; align-items: stretch; }
                .os-combobox-input { flex: 1; border-right: none; }
                .os-combobox-btn {
                    width: 18px;
                    border: 2px solid;
                    border-color: var(--bevel-light) var(--bevel-dark) var(--bevel-dark) var(--bevel-light);
                    background: var(--os-chrome);
                    color: var(--os-text);
                    display: flex; align-items: center; justify-content: center;
                    cursor: pointer; font-size: 8px;
                }
                .os-combobox-btn:active { border-color: var(--bevel-dark) var(--bevel-light) var(--bevel-light) var(--bevel-dark); }
                .os-updown { display: flex; align-items: stretch; }
                .os-updown-input { width: 60px; text-align: right; border-right: none; }
                .os-updown-btns { display: flex; flex-direction: column; }
                .os-updown-btn {
                    width: 16px; height: 12px;
                    border: 2px solid;
                    border-color: var(--bevel-light) var(--bevel-dark) var(--bevel-dark) var(--bevel-light);
                    background: var(--os-chrome);
                    color: var(--os-text);
                    display: flex; align-items: center; justify-content: center;
                    cursor: pointer; font-size: 6px;
                }
                .os-updown-btn:active { border-color: var(--bevel-dark) var(--bevel-light) var(--bevel-light) var(--bevel-dark); }
            </style>
            <div class="preview-box" style="background: var(--os-chrome);">
                <div style="display:grid;grid-template-columns:1fr 1fr;gap:24px;">
                    <div>
                        <div class="os-form-group">
                            <label class="os-label">Text Input</label>
                            <input type="text" class="os-input" placeholder="Enter text...">
                        </div>
                        <div class="os-form-group">
                            <label class="os-label">With Helper</label>
                            <input type="text" class="os-input" placeholder="Value...">
                            <div class="os-helper">Press F1 for help.</div>
                        </div>
                        <div class="os-form-group">
                            <label class="os-label">Error State</label>
                            <input type="text" class="os-input os-input-error" value="Invalid">
                            <div class="os-helper os-helper-error">This field is required.</div>
                        </div>
                        <div class="os-form-group">
                            <label class="os-label">Disabled</label>
                            <input type="text" class="os-input" value="Read only" disabled>
                        </div>
                    </div>
                    <div>
                        <div class="os-form-group">
                            <label class="os-label">Select</label>
                            <select class="os-input">
                                <option>Choose an option...</option>
                                <option>Option 1</option>
                                <option>Option 2</option>
                            </select>
                        </div>
                        <div class="os-form-group">
                            <label class="os-label">Textarea</label>
                            <textarea class="os-input os-textarea" placeholder="Enter message..."></textarea>
                        </div>
                        <div class="os-form-group">
                            <label class="os-label">Combo Box</label>
                            <div class="os-combobox" style="max-width:200px;">
                                <input type="text" class="os-input os-combobox-input" value="Times New Roman">
                                <div class="os-combobox-btn">▼</div>
                            </div>
                        </div>
                        <div class="os-form-group">
                            <label class="os-label">Spin Control</label>
                            <div class="os-updown">
                                <input type="text" class="os-input os-updown-input" value="42">
                                <div class="os-updown-btns">
                                    <div class="os-updown-btn">▲</div>
                                    <div class="os-updown-btn">▼</div>
                                </div>
                            </div>
                        </div>
                    </div>
                </div>
            </div>
        </div>

        <!-- ============================================================ -->
        <!-- CHECKBOXES, RADIOS, GROUPBOX -->
        <!-- ============================================================ -->
        <div class="section">
            <h2 class="section-title">Checkboxes, Radios &amp; Group Boxes</h2>
            <div class="section-desc">Sunken checkbox/radio wells with light background. Groove-bordered group boxes for organizing form sections.</div>
            <style>
                .os-checkbox-item, .os-radio-item {
                    display: flex; align-items: center; gap: 6px; cursor: pointer; font-size: 13px; color: var(--os-text);
                }
                .os-checkbox-item input, .os-radio-item input { display: none; }
                .os-custom-checkbox {
                    width: 13px; height: 13px;
                    border: 2px solid;
                    border-color: var(--bevel-dark) var(--bevel-light) var(--bevel-light) var(--bevel-dark);
                    background: var(--os-input-bg);
                    display: flex; align-items: center; justify-content: center; flex-shrink: 0;
                }
                .os-checkbox-item input:checked + .os-custom-checkbox::after { content:"✓"; color:var(--os-teal); font-size:10px; font-weight:bold; }
                .os-custom-radio {
                    width: 13px; height: 13px;
                    border: 2px solid;
                    border-color: var(--bevel-dark) var(--bevel-light) var(--bevel-light) var(--bevel-dark);
                    border-radius: 50%;
                    background: var(--os-input-bg);
                    display: flex; align-items: center; justify-content: center; flex-shrink: 0;
                }
                .os-radio-item input:checked + .os-custom-radio::after { content:""; width:5px; height:5px; background:var(--os-teal); border-radius:50%; }
                .os-groupbox {
                    border: 2px groove var(--os-chrome-light);
                    padding: 12px; margin-bottom: 12px; background: var(--os-chrome);
                }
                .os-groupbox-title {
                    font-size: 13px; color: var(--os-text); background: var(--os-chrome); padding: 0 6px;
                    margin-left: 8px; margin-top: -22px; display: inline-block; position: relative;
                }
            </style>
            <div class="preview-box" style="background: var(--os-chrome);">
                <div style="display:flex;gap:40px;flex-wrap:wrap;">
                    <div>
                        <div class="component-label" style="color:var(--os-text-secondary);">Checkboxes</div>
                        <div style="display:flex;flex-direction:column;gap:6px;">
                            <label class="os-checkbox-item"><input type="checkbox" checked><span class="os-custom-checkbox"></span>Show hidden files</label>
                            <label class="os-checkbox-item"><input type="checkbox" checked><span class="os-custom-checkbox"></span>Show file extensions</label>
                            <label class="os-checkbox-item"><input type="checkbox"><span class="os-custom-checkbox"></span>Show system files</label>
                        </div>
                    </div>
                    <div>
                        <div class="component-label" style="color:var(--os-text-secondary);">Radio Buttons</div>
                        <div style="display:flex;flex-direction:column;gap:6px;">
                            <label class="os-radio-item"><input type="radio" name="os-view" checked><span class="os-custom-radio"></span>Large Icons</label>
                            <label class="os-radio-item"><input type="radio" name="os-view"><span class="os-custom-radio"></span>Details</label>
                            <label class="os-radio-item"><input type="radio" name="os-view"><span class="os-custom-radio"></span>List</label>
                        </div>
                    </div>
                    <div>
                        <div class="component-label" style="color:var(--os-text-secondary);">Group Box</div>
                        <fieldset class="os-groupbox" style="min-width:200px;">
                            <legend class="os-groupbox-title">Display Settings</legend>
                            <div class="os-form-group" style="margin-bottom:8px;">
                                <label class="os-label">Resolution</label>
                                <select class="os-input"><option>1024 × 768</option><option>800 × 600</option></select>
                            </div>
                            <div class="os-form-group" style="margin-bottom:0;">
                                <label class="os-label">Color Depth</label>
                                <select class="os-input"><option>32 bit</option><option>16 bit</option></select>
                            </div>
                        </fieldset>
                    </div>
                </div>
            </div>
        </div>

        <!-- ============================================================ -->
        <!-- SLIDERS / TRACKBARS -->
        <!-- ============================================================ -->
        <div class="section">
            <h2 class="section-title">Sliders &amp; Trackbars</h2>
            <div class="section-desc">Volume and parameter sliders with embossed thumb controls and tick marks.</div>
            <style>
                .os-slider-track {
                    height: 4px; background: var(--os-surface-sunken);
                    border: 2px solid;
                    border-color: var(--bevel-dark) var(--bevel-light) var(--bevel-light) var(--bevel-dark);
                    flex: 1; position: relative;
                }
                .os-slider-fill { position:absolute;top:0;left:0;bottom:0;width:40%;background:var(--os-accent); }
                .os-slider-thumb {
                    width: 12px; height: 20px;
                    border: 2px solid;
                    border-color: var(--bevel-light) var(--bevel-dark) var(--bevel-dark) var(--bevel-light);
                    background: var(--os-chrome-light);
                    position: absolute; top: -10px; left: 40%; transform: translateX(-50%);
                    cursor: ew-resize;
                }
                .os-slider-label { font-size: 12px; color: var(--os-text-secondary); min-width: 24px; text-align: center; }
                .os-slider-row { display: flex; align-items: center; gap: 10px; margin-bottom: 16px; }
                .os-ticks { display: flex; justify-content: space-between; margin-top: 6px; padding: 0 2px; }
                .os-tick { width: 1px; height: 6px; background: var(--os-text-secondary); }
                .os-slider-wrap { flex: 1; }
            </style>
            <div class="preview-box" style="background: var(--os-chrome);">
                <div style="max-width:400px;">
                    <div class="component-label" style="color:var(--os-text-secondary);margin-bottom:2px;">Volume</div>
                    <div class="os-slider-row">
                        <span class="os-slider-label" style="font-size:16px;">🔈</span>
                        <div class="os-slider-wrap">
                            <div class="os-slider-track">
                                <div class="os-slider-fill" style="width:60%;"></div>
                                <div class="os-slider-thumb" style="left:60%;"></div>
                            </div>
                            <div class="os-ticks">
                                <span class="os-tick"></span><span class="os-tick"></span><span class="os-tick"></span>
                                <span class="os-tick"></span><span class="os-tick"></span><span class="os-tick"></span>
                                <span class="os-tick"></span><span class="os-tick"></span><span class="os-tick"></span>
                            </div>
                        </div>
                        <span class="os-slider-label" style="font-size:16px;">🔊</span>
                    </div>
                    <div class="component-label" style="color:var(--os-text-secondary);margin-bottom:2px;">Brightness</div>
                    <div class="os-slider-row">
                        <span class="os-slider-label">0</span>
                        <div class="os-slider-wrap">
                            <div class="os-slider-track">
                                <div class="os-slider-fill" style="width:75%;background:var(--os-teal);"></div>
                                <div class="os-slider-thumb" style="left:75%;"></div>
                            </div>
                        </div>
                        <span class="os-slider-label">100</span>
                    </div>
                </div>
            </div>
        </div>

        <!-- ============================================================ -->
        <!-- WINDOWS -->
        <!-- ============================================================ -->
        <div class="section">
            <h2 class="section-title">Windows</h2>
            <div class="section-desc">Full window chrome: gradient titlebar, menu bar, toolbar, scrollable content area, and status bar. Active and inactive states.</div>
            <style>
                .os-window {
                    border: 2px solid;
                    border-color: var(--bevel-light) var(--bevel-dark) var(--bevel-dark) var(--bevel-light);
                    background: var(--os-chrome);
                    box-shadow: 2px 2px 8px rgba(0,0,0,0.5);
                    max-width: 520px;
                }
                .os-titlebar {
                    background: linear-gradient(90deg, var(--titlebar-start), var(--titlebar-end));
                    padding: 3px 4px;
                    display: flex; align-items: center; gap: 6px;
                }
                .os-titlebar.inactive { background: linear-gradient(90deg, #9498AC, #B0B4C6); }
                .os-titlebar-icon { width: 16px; height: 16px; font-size: 12px; display: flex; align-items: center; justify-content: center; }
                .os-titlebar-text { flex: 1; font-size: 12px; font-weight: bold; color: #fff; white-space: nowrap; overflow: hidden; text-overflow: ellipsis; }
                .os-titlebar.inactive .os-titlebar-text { color: var(--os-text-disabled); }
                .os-titlebar-btns { display: flex; gap: 2px; }
                .os-titlebar-btn {
                    width: 20px; height: 18px;
                    border: 2px solid;
                    border-color: var(--bevel-light) var(--bevel-dark) var(--bevel-dark) var(--bevel-light);
                    background: var(--os-chrome);
                    font-size: 9px; display: flex; align-items: center; justify-content: center;
                    color: var(--os-text); cursor: pointer;
                }
                .os-titlebar-btn:active { border-color: var(--bevel-dark) var(--bevel-light) var(--bevel-light) var(--bevel-dark); }
                .os-menubar {
                    display: flex; gap: 0; padding: 1px 2px;
                    border-bottom: 1px solid var(--bevel-dark);
                    background: var(--os-chrome);
                }
                .os-menu-item {
                    padding: 2px 8px; font-size: 12px; color: var(--os-text); cursor: pointer;
                }
                .os-menu-item:hover { background: var(--os-accent); color: #fff; }
                .os-toolbar {
                    display: flex; gap: 2px; padding: 3px 4px;
                    border-bottom: 1px solid var(--bevel-dark);
                    background: var(--os-chrome);
                }
                .os-toolbar-btn {
                    width: 24px; height: 22px;
                    border: 1px solid transparent;
                    background: transparent;
                    display: flex; align-items: center; justify-content: center;
                    color: var(--os-text); cursor: pointer; font-size: 12px;
                }
                .os-toolbar-btn:hover { border-color: var(--bevel-light) var(--bevel-dark) var(--bevel-dark) var(--bevel-light); }
                .os-toolbar-sep { width: 1px; background: var(--bevel-dark); margin: 2px 3px; }
                .os-addressbar {
                    display: flex; align-items: center; gap: 4px; padding: 3px 6px;
                    border-bottom: 1px solid var(--bevel-dark);
                    background: var(--os-chrome);
                }
                .os-addressbar-label { font-size: 12px; color: var(--os-text-secondary); white-space: nowrap; }
                .os-addressbar-input {
                    flex: 1; padding: 2px 4px;
                    border: 2px solid;
                    border-color: var(--bevel-dark) var(--bevel-light) var(--bevel-light) var(--bevel-dark);
                    background: var(--os-input-bg);
                    color: var(--os-text); font-size: 12px;
                }
                .os-window-body {
                    background: var(--os-surface);
                    border: 2px solid;
                    border-color: var(--bevel-dark) var(--bevel-light) var(--bevel-light) var(--bevel-dark);
                    margin: 2px;
                    padding: 8px;
                    min-height: 80px;
                    max-height: 140px;
                    overflow-y: auto;
                    color: var(--os-text); font-size: 13px;
                }
                .os-statusbar {
                    display: flex; gap: 4px; padding: 2px 4px;
                    border-top: 2px solid;
                    border-color: var(--bevel-dark) transparent transparent var(--bevel-dark);
                }
                .os-statusbar-section {
                    border: 1px solid;
                    border-color: var(--bevel-dark) var(--bevel-light) var(--bevel-light) var(--bevel-dark);
                    padding: 1px 8px;
                    font-size: 11px; color: var(--os-text-secondary);
                }
                .os-statusbar-section:first-child { flex: 1; }
            </style>
            <div class="preview-box" style="background: var(--os-surface-sunken);">
                <div style="display:flex;flex-direction:column;gap:24px;">
                    <!-- Active Window -->
                    <div>
                        <div class="component-label" style="color:var(--os-text-secondary);">Active Window</div>
                        <div class="os-window">
                            <div class="os-titlebar">
                                <span class="os-titlebar-icon">📁</span>
                                <span class="os-titlebar-text">MaahiOS File Explorer — C:\Users\Documents</span>
                                <div class="os-titlebar-btns">
                                    <div class="os-titlebar-btn">─</div>
                                    <div class="os-titlebar-btn">□</div>
                                    <div class="os-titlebar-btn">✕</div>
                                </div>
                            </div>
                            <div class="os-menubar">
                                <span class="os-menu-item"><u>F</u>ile</span>
                                <span class="os-menu-item"><u>E</u>dit</span>
                                <span class="os-menu-item"><u>V</u>iew</span>
                                <span class="os-menu-item"><u>H</u>elp</span>
                            </div>
                            <div class="os-toolbar">
                                <div class="os-toolbar-btn">⬅</div>
                                <div class="os-toolbar-btn">➡</div>
                                <div class="os-toolbar-btn">⬆</div>
                                <div class="os-toolbar-sep"></div>
                                <div class="os-toolbar-btn">✂</div>
                                <div class="os-toolbar-btn">📋</div>
                                <div class="os-toolbar-sep"></div>
                                <div class="os-toolbar-btn">🗑</div>
                            </div>
                            <div class="os-addressbar">
                                <span class="os-addressbar-label">Address</span>
                                <input type="text" class="os-addressbar-input" value="C:\Users\Documents">
                            </div>
                            <div class="os-window-body">
                                <div>📄 readme.txt</div>
                                <div>📁 Projects</div>
                                <div>📁 Downloads</div>
                                <div>📄 notes.md</div>
                                <div>📄 config.sys</div>
                                <div>📁 Sources</div>
                            </div>
                            <div class="os-statusbar">
                                <span class="os-statusbar-section">6 object(s)</span>
                                <span class="os-statusbar-section">1.2 KB</span>
                                <span class="os-statusbar-section">My Computer</span>
                            </div>
                        </div>
                    </div>
                    <!-- Inactive Window -->
                    <div>
                        <div class="component-label" style="color:var(--os-text-secondary);">Inactive Window</div>
                        <div class="os-window" style="max-width:360px;">
                            <div class="os-titlebar inactive">
                                <span class="os-titlebar-icon">📝</span>
                                <span class="os-titlebar-text">Untitled — Notepad</span>
                                <div class="os-titlebar-btns">
                                    <div class="os-titlebar-btn">─</div>
                                    <div class="os-titlebar-btn">□</div>
                                    <div class="os-titlebar-btn">✕</div>
                                </div>
                            </div>
                            <div class="os-menubar">
                                <span class="os-menu-item">File</span>
                                <span class="os-menu-item">Edit</span>
                                <span class="os-menu-item">Format</span>
                                <span class="os-menu-item">Help</span>
                            </div>
                            <div class="os-window-body" style="min-height:50px;max-height:60px;">
                                <span style="color:var(--os-text-disabled);">Start typing...</span>
                            </div>
                            <div class="os-statusbar">
                                <span class="os-statusbar-section">Ln 1, Col 1</span>
                                <span class="os-statusbar-section">UTF-8</span>
                            </div>
                        </div>
                    </div>
                </div>
            </div>
        </div>

        <!-- ============================================================ -->
        <!-- DIALOG BOXES -->
        <!-- ============================================================ -->
        <div class="section">
            <h2 class="section-title">Dialog Boxes</h2>
            <div class="section-desc">Confirmation, Error, and About dialogs with proper icon placement and button layout.</div>
            <style>
                .os-dialog {
                    border: 2px solid;
                    border-color: var(--bevel-light) var(--bevel-dark) var(--bevel-dark) var(--bevel-light);
                    background: var(--os-chrome);
                    box-shadow: 2px 2px 8px rgba(0,0,0,0.5);
                    min-width: 280px; max-width: 380px;
                }
                .os-dialog-titlebar {
                    background: linear-gradient(90deg, var(--titlebar-start), var(--titlebar-end));
                    padding: 3px 6px;
                    display: flex; align-items: center; gap: 6px;
                }
                .os-dialog-title { flex:1; font-size:12px; font-weight:bold; color:#fff; }
                .os-dialog-close {
                    width: 18px; height: 16px;
                    border: 2px solid;
                    border-color: var(--bevel-light) var(--bevel-dark) var(--bevel-dark) var(--bevel-light);
                    background: var(--os-chrome);
                    font-size: 8px; display: flex; align-items: center; justify-content: center;
                    color: var(--os-text); cursor: pointer;
                }
                .os-dialog-body { padding: 16px; display: flex; gap: 12px; align-items: flex-start; }
                .os-dialog-icon { font-size: 28px; flex-shrink: 0; margin-top: 2px; }
                .os-dialog-msg { font-size: 13px; color: var(--os-text); line-height: 1.4; }
                .os-dialog-buttons {
                    display: flex; justify-content: center; gap: 6px;
                    padding: 0 16px 14px;
                }
            </style>
            <div class="preview-box" style="background: var(--os-surface-sunken);">
                <div style="display:flex;flex-wrap:wrap;gap:24px;">
                    <!-- Confirm Dialog -->
                    <div>
                        <div class="component-label" style="color:var(--os-text-secondary);">Confirm</div>
                        <div class="os-dialog">
                            <div class="os-dialog-titlebar">
                                <span class="os-dialog-title">Confirm Delete</span>
                                <div class="os-dialog-close">✕</div>
                            </div>
                            <div class="os-dialog-body">
                                <div class="os-dialog-icon">❓</div>
                                <div class="os-dialog-msg">Are you sure you want to delete "readme.txt"? This action cannot be undone.</div>
                            </div>
                            <div class="os-dialog-buttons">
                                <button class="os-btn os-btn-default" style="min-width:72px;">Yes</button>
                                <button class="os-btn" style="min-width:72px;">No</button>
                            </div>
                        </div>
                    </div>
                    <!-- Error Dialog -->
                    <div>
                        <div class="component-label" style="color:var(--os-text-secondary);">Error</div>
                        <div class="os-dialog">
                            <div class="os-dialog-titlebar">
                                <span class="os-dialog-title">Error</span>
                                <div class="os-dialog-close">✕</div>
                            </div>
                            <div class="os-dialog-body">
                                <div class="os-dialog-icon">⛔</div>
                                <div class="os-dialog-msg">Access denied. The file "config.sys" is protected by the system.</div>
                            </div>
                            <div class="os-dialog-buttons">
                                <button class="os-btn os-btn-default" style="min-width:72px;">OK</button>
                            </div>
                        </div>
                    </div>
                    <!-- About Dialog -->
                    <div>
                        <div class="component-label" style="color:var(--os-text-secondary);">About</div>
                        <div class="os-dialog">
                            <div class="os-dialog-titlebar">
                                <span class="os-dialog-title">About MaahiOS</span>
                                <div class="os-dialog-close">✕</div>
                            </div>
                            <div class="os-dialog-body" style="flex-direction:column;align-items:center;text-align:center;">
                                <div style="font-size:36px;">🖥️</div>
                                <div style="font-size:16px;font-weight:bold;color:var(--os-teal);">MaahiOS</div>
                                <div class="os-dialog-msg">Version 1.0.0 build 2025<br>Custom x86 Operating System<br><br>© 2025 Maahi. All rights reserved.</div>
                            </div>
                            <div class="os-dialog-buttons">
                                <button class="os-btn os-btn-default" style="min-width:72px;">OK</button>
                            </div>
                        </div>
                    </div>
                </div>
            </div>
        </div>

        <!-- ============================================================ -->
        <!-- TABS (PROPERTY SHEETS) -->
        <!-- ============================================================ -->
        <div class="section">
            <h2 class="section-title">Tabs (Property Sheets)</h2>
            <div class="section-desc">Tabbed panels for settings and properties. Active tab raises above the content panel.</div>
            <style>
                .os-tabs-container { background: var(--os-chrome); }
                .os-tab-bar { display: flex; gap: 0; padding: 0 6px; }
                .os-tab {
                    padding: 4px 14px; font-size: 12px; color: var(--os-text-secondary);
                    border: 2px solid;
                    border-color: var(--bevel-light) var(--bevel-dark) transparent var(--bevel-light);
                    border-bottom: none;
                    background: var(--os-chrome-light);
                    cursor: pointer;
                    margin-top: 2px;
                    position: relative;
                }
                .os-tab.active {
                    color: var(--os-text);
                    background: var(--os-chrome);
                    margin-top: 0;
                    padding-bottom: 6px;
                    border-bottom: 2px solid var(--os-chrome);
                    z-index: 1;
                }
                .os-tab-content {
                    border: 2px solid;
                    border-color: var(--bevel-light) var(--bevel-dark) var(--bevel-dark) var(--bevel-light);
                    background: var(--os-chrome);
                    padding: 16px;
                    margin-top: -2px;
                    font-size: 13px; color: var(--os-text);
                }
            </style>
            <div class="preview-box" style="background: var(--os-chrome);">
                <div style="max-width:420px;">
                    <div class="os-tabs-container">
                        <div class="os-tab-bar">
                            <div class="os-tab active">General</div>
                            <div class="os-tab">Display</div>
                            <div class="os-tab">Sound</div>
                            <div class="os-tab">Network</div>
                        </div>
                        <div class="os-tab-content">
                            <fieldset class="os-groupbox" style="margin-bottom:12px;">
                                <legend class="os-groupbox-title">Computer Name</legend>
                                <div class="os-form-group" style="margin-bottom:4px;">
                                    <input type="text" class="os-input" value="MAAHI-PC">
                                </div>
                            </fieldset>
                            <fieldset class="os-groupbox" style="margin-bottom:0;">
                                <legend class="os-groupbox-title">Startup</legend>
                                <label class="os-checkbox-item"><input type="checkbox" checked><span class="os-custom-checkbox"></span>Load shell on boot</label>
                                <label class="os-checkbox-item" style="margin-top:4px;"><input type="checkbox"><span class="os-custom-checkbox"></span>Enable verbose logging</label>
                            </fieldset>
                            <div style="margin-top:14px;display:flex;justify-content:flex-end;gap:6px;">
                                <button class="os-btn os-btn-default">OK</button>
                                <button class="os-btn">Cancel</button>
                                <button class="os-btn">Apply</button>
                            </div>
                        </div>
                    </div>
                </div>
            </div>
        </div>

        <!-- ============================================================ -->
        <!-- MENUS -->
        <!-- ============================================================ -->
        <div class="section">
            <h2 class="section-title">Menus</h2>
            <div class="section-desc">Drop-down file menus, context menus, and view menus with checkmarks and keyboard shortcuts.</div>
            <style>
                .os-menu {
                    border: 2px solid;
                    border-color: var(--bevel-light) var(--bevel-dark) var(--bevel-dark) var(--bevel-light);
                    background: var(--os-chrome);
                    padding: 2px 0;
                    min-width: 180px;
                    box-shadow: 2px 2px 6px rgba(0,0,0,0.4);
                }
                .os-menu-entry {
                    padding: 3px 24px 3px 28px;
                    font-size: 12px; color: var(--os-text);
                    display: flex; align-items: center; gap: 6px;
                    cursor: pointer; position: relative;
                }
                .os-menu-entry:hover { background: var(--os-accent); color: #fff; }
                .os-menu-entry.disabled { color: var(--os-text-disabled); cursor: default; }
                .os-menu-entry.disabled:hover { background: transparent; color: var(--os-text-disabled); }
                .os-menu-shortcut { margin-left: auto; font-size: 11px; color: var(--os-text-secondary); }
                .os-menu-entry:hover .os-menu-shortcut { color: rgba(255,255,255,0.7); }
                .os-menu-check { position: absolute; left: 8px; font-size: 10px; }
                .os-menu-sep { height: 1px; background: var(--bevel-dark); margin: 3px 2px; border-bottom: 1px solid var(--bevel-light); }
                .os-menu-arrow { margin-left: auto; font-size: 8px; }
            </style>
            <div class="preview-box" style="background: var(--os-surface-sunken);">
                <div style="display:flex;flex-wrap:wrap;gap:28px;">
                    <!-- File Menu -->
                    <div>
                        <div class="component-label" style="color:var(--os-text-secondary);">File Menu</div>
                        <div class="os-menu">
                            <div class="os-menu-entry">New<span class="os-menu-shortcut">Ctrl+N</span></div>
                            <div class="os-menu-entry">Open...<span class="os-menu-shortcut">Ctrl+O</span></div>
                            <div class="os-menu-entry">Save<span class="os-menu-shortcut">Ctrl+S</span></div>
                            <div class="os-menu-entry">Save As...</div>
                            <div class="os-menu-sep"></div>
                            <div class="os-menu-entry">Print...<span class="os-menu-shortcut">Ctrl+P</span></div>
                            <div class="os-menu-sep"></div>
                            <div class="os-menu-entry">Exit<span class="os-menu-shortcut">Alt+F4</span></div>
                        </div>
                    </div>
                    <!-- Context Menu -->
                    <div>
                        <div class="component-label" style="color:var(--os-text-secondary);">Context Menu</div>
                        <div class="os-menu">
                            <div class="os-menu-entry">Open</div>
                            <div class="os-menu-entry">Open with... <span class="os-menu-arrow">▶</span></div>
                            <div class="os-menu-sep"></div>
                            <div class="os-menu-entry">Cut<span class="os-menu-shortcut">Ctrl+X</span></div>
                            <div class="os-menu-entry">Copy<span class="os-menu-shortcut">Ctrl+C</span></div>
                            <div class="os-menu-entry disabled">Paste<span class="os-menu-shortcut">Ctrl+V</span></div>
                            <div class="os-menu-entry">Delete<span class="os-menu-shortcut">Del</span></div>
                            <div class="os-menu-sep"></div>
                            <div class="os-menu-entry">Rename</div>
                            <div class="os-menu-entry">Properties</div>
                        </div>
                    </div>
                    <!-- View Menu -->
                    <div>
                        <div class="component-label" style="color:var(--os-text-secondary);">View Menu (checks)</div>
                        <div class="os-menu">
                            <div class="os-menu-entry"><span class="os-menu-check">✓</span>Toolbar</div>
                            <div class="os-menu-entry"><span class="os-menu-check">✓</span>Status Bar</div>
                            <div class="os-menu-entry">Address Bar</div>
                            <div class="os-menu-sep"></div>
                            <div class="os-menu-entry"><span class="os-menu-check">●</span>Large Icons</div>
                            <div class="os-menu-entry">Small Icons</div>
                            <div class="os-menu-entry">Details</div>
                            <div class="os-menu-sep"></div>
                            <div class="os-menu-entry">Refresh<span class="os-menu-shortcut">F5</span></div>
                        </div>
                    </div>
                </div>
            </div>
        </div>

        <!-- ============================================================ -->
        <!-- LIST BOX & TREE VIEW -->
        <!-- ============================================================ -->
        <div class="section">
            <h2 class="section-title">List Box &amp; Tree View</h2>
            <div class="section-desc">Sunken list boxes with selection highlighting, and a hierarchical tree view with expand/collapse nodes.</div>
            <style>
                .os-listbox {
                    border: 2px solid;
                    border-color: var(--bevel-dark) var(--bevel-light) var(--bevel-light) var(--bevel-dark);
                    background: var(--os-input-bg);
                    min-height: 140px; max-height: 180px; overflow-y: auto;
                    font-size: 12px;
                }
                .os-listbox-item { padding: 2px 6px; color: var(--os-text); cursor: pointer; }
                .os-listbox-item:hover { background: rgba(43,91,181,0.3); }
                .os-listbox-item.selected { background: var(--os-accent); color: #fff; }
                .os-treeview {
                    border: 2px solid;
                    border-color: var(--bevel-dark) var(--bevel-light) var(--bevel-light) var(--bevel-dark);
                    background: var(--os-input-bg);
                    padding: 4px; font-size: 12px; color: var(--os-text);
                    min-height: 140px; max-height: 200px; overflow-y: auto;
                }
                .os-tree-node { padding-left: 16px; }
                .os-tree-label { display: flex; align-items: center; gap: 4px; padding: 1px 4px; cursor: pointer; }
                .os-tree-label:hover { background: rgba(43,91,181,0.2); }
                .os-tree-label.selected { background: var(--os-accent); color: #fff; }
                .os-tree-toggle { font-size: 8px; width: 12px; cursor: pointer; color: var(--os-text-secondary); }
                .os-tree-icon { font-size: 12px; }
            </style>
            <div class="preview-box" style="background: var(--os-chrome);">
                <div style="display:flex;gap:32px;flex-wrap:wrap;">
                    <!-- List Box -->
                    <div>
                        <div class="component-label" style="color:var(--os-text-secondary);">List Box</div>
                        <div class="os-listbox" style="width:200px;">
                            <div class="os-listbox-item">Arial</div>
                            <div class="os-listbox-item">Courier New</div>
                            <div class="os-listbox-item selected">Segoe UI</div>
                            <div class="os-listbox-item">Tahoma</div>
                            <div class="os-listbox-item">Times New Roman</div>
                            <div class="os-listbox-item">Trebuchet MS</div>
                            <div class="os-listbox-item">Verdana</div>
                            <div class="os-listbox-item">MS Sans Serif</div>
                            <div class="os-listbox-item">Consolas</div>
                        </div>
                    </div>
                    <!-- Tree View -->
                    <div>
                        <div class="component-label" style="color:var(--os-text-secondary);">Tree View</div>
                        <div class="os-treeview" style="width:240px;">
                            <div class="os-tree-label"><span class="os-tree-toggle">▼</span><span class="os-tree-icon">💻</span> My Computer</div>
                            <div class="os-tree-node">
                                <div class="os-tree-label"><span class="os-tree-toggle">▼</span><span class="os-tree-icon">💿</span> C:</div>
                                <div class="os-tree-node">
                                    <div class="os-tree-label"><span class="os-tree-toggle">▶</span><span class="os-tree-icon">📁</span> MaahiOS</div>
                                    <div class="os-tree-label selected"><span class="os-tree-toggle">▼</span><span class="os-tree-icon">📁</span> Users</div>
                                    <div class="os-tree-node">
                                        <div class="os-tree-label"><span class="os-tree-toggle">▶</span><span class="os-tree-icon">📁</span> Documents</div>
                                        <div class="os-tree-label"><span class="os-tree-toggle">▶</span><span class="os-tree-icon">📁</span> Desktop</div>
                                    </div>
                                    <div class="os-tree-label"><span class="os-tree-toggle">▶</span><span class="os-tree-icon">📁</span> System</div>
                                </div>
                                <div class="os-tree-label"><span class="os-tree-toggle">▶</span><span class="os-tree-icon">💿</span> D:</div>
                            </div>
                        </div>
                    </div>
                </div>
            </div>
        </div>

        <!-- ============================================================ -->
        <!-- PROGRESS BARS -->
        <!-- ============================================================ -->
        <div class="section">
            <h2 class="section-title">Progress Bars</h2>
            <div class="section-desc">Solid and segmented progress bars for file operations, installation, and loading states.</div>
            <style>
                .os-progress {
                    height: 18px;
                    border: 2px solid;
                    border-color: var(--bevel-dark) var(--bevel-light) var(--bevel-light) var(--bevel-dark);
                    background: var(--os-surface-sunken);
                    position: relative;
                }
                .os-progress-fill { height: 100%; transition: width 0.3s; }
                .os-progress-fill.solid { background: var(--os-accent); }
                .os-progress-fill.teal { background: var(--os-teal); }
                .os-progress-fill.danger { background: var(--os-danger); }
                .os-progress-segmented .os-progress-fill {
                    background: repeating-linear-gradient(90deg,
                        var(--os-accent) 0px, var(--os-accent) 10px,
                        var(--os-surface-sunken) 10px, var(--os-surface-sunken) 12px);
                }
                .os-progress-label {
                    position: absolute; right: 6px; top: 50%; transform: translateY(-50%);
                    font-size: 11px; color: var(--os-text); z-index: 1;
                }
                .os-progress-marquee .os-progress-fill {
                    width: 30% !important;
                    animation: marquee 2s linear infinite;
                    background: var(--os-teal);
                }
                @keyframes marquee { 0%{margin-left:0%} 100%{margin-left:70%} }
            </style>
            <div class="preview-box" style="background: var(--os-chrome);">
                <div style="max-width:400px;display:flex;flex-direction:column;gap:16px;">
                    <div>
                        <div class="component-label" style="color:var(--os-text-secondary);">Solid — 65%</div>
                        <div class="os-progress">
                            <div class="os-progress-fill solid" style="width:65%;"></div>
                            <span class="os-progress-label">65%</span>
                        </div>
                    </div>
                    <div>
                        <div class="component-label" style="color:var(--os-text-secondary);">Segmented — 40%</div>
                        <div class="os-progress os-progress-segmented">
                            <div class="os-progress-fill" style="width:40%;"></div>
                        </div>
                    </div>
                    <div>
                        <div class="component-label" style="color:var(--os-text-secondary);">Teal — 90%</div>
                        <div class="os-progress">
                            <div class="os-progress-fill teal" style="width:90%;"></div>
                            <span class="os-progress-label">90%</span>
                        </div>
                    </div>
                    <div>
                        <div class="component-label" style="color:var(--os-text-secondary);">Danger — 80%</div>
                        <div class="os-progress">
                            <div class="os-progress-fill danger" style="width:80%;"></div>
                            <span class="os-progress-label">80%</span>
                        </div>
                    </div>
                    <div>
                        <div class="component-label" style="color:var(--os-text-secondary);">Indeterminate (Marquee)</div>
                        <div class="os-progress os-progress-marquee">
                            <div class="os-progress-fill"></div>
                        </div>
                    </div>
                </div>
            </div>
        </div>

        <!-- ============================================================ -->
        <!-- TABLES -->
        <!-- ============================================================ -->
        <div class="section">
            <h2 class="section-title">Tables</h2>
            <div class="section-desc">Data grids with sunken borders, sortable column headers, and row hover/selection highlighting.</div>
            <style>
                .os-table-wrap {
                    border: 2px solid;
                    border-color: var(--bevel-dark) var(--bevel-light) var(--bevel-light) var(--bevel-dark);
                    background: var(--os-input-bg);
                    overflow: auto;
                }
                .os-table { width: 100%; border-collapse: collapse; font-size: 12px; }
                .os-table th {
                    background: var(--os-chrome);
                    border: 1px solid;
                    border-color: var(--bevel-light) var(--bevel-dark) var(--bevel-dark) var(--bevel-light);
                    padding: 3px 8px;
                    text-align: left;
                    color: var(--os-text);
                    font-weight: normal; cursor: pointer; white-space: nowrap;
                }
                .os-table th:hover { background: var(--os-chrome-light); }
                .os-table td {
                    padding: 2px 8px;
                    border-bottom: 1px solid rgba(90,93,118,0.2);
                    color: var(--os-text);
                }
                .os-table tr:hover td { background: rgba(43,91,181,0.15); }
                .os-table tr.selected td { background: var(--os-accent); color: #fff; }
                .os-table .sort-arrow { font-size: 8px; margin-left: 4px; color: var(--os-text-secondary); }
            </style>
            <div class="preview-box" style="background: var(--os-chrome);">
                <div class="os-table-wrap" style="max-width:500px;">
                    <table class="os-table">
                        <thead>
                            <tr>
                                <th>Name <span class="sort-arrow">▲</span></th>
                                <th>Size</th>
                                <th>Type</th>
                                <th>Date Modified</th>
                            </tr>
                        </thead>
                        <tbody>
                            <tr>
                                <td>📁 Documents</td>
                                <td></td>
                                <td>Folder</td>
                                <td>2025-01-15</td>
                            </tr>
                            <tr class="selected">
                                <td>📄 readme.txt</td>
                                <td>2 KB</td>
                                <td>Text File</td>
                                <td>2025-01-14</td>
                            </tr>
                            <tr>
                                <td>📄 kernel.bin</td>
                                <td>64 KB</td>
                                <td>Binary</td>
                                <td>2025-01-10</td>
                            </tr>
                            <tr>
                                <td>📄 config.sys</td>
                                <td>1 KB</td>
                                <td>System</td>
                                <td>2025-01-08</td>
                            </tr>
                            <tr>
                                <td>📁 Sources</td>
                                <td></td>
                                <td>Folder</td>
                                <td>2025-01-05</td>
                            </tr>
                        </tbody>
                    </table>
                </div>
            </div>
        </div>

        <!-- ============================================================ -->
        <!-- BADGES & STATUS INDICATORS -->
        <!-- ============================================================ -->
        <div class="section">
            <h2 class="section-title">Badges &amp; Status Indicators</h2>
            <div class="section-desc">Small labels for counts and status, plus LED-style indicators for system health.</div>
            <style>
                .os-badge {
                    display: inline-block; padding: 1px 8px;
                    font-size: 11px; font-weight: bold;
                    border: 1px solid;
                }
                .os-badge-default { background: var(--os-chrome-dark); color: var(--os-text); border-color: var(--bevel-dark); }
                .os-badge-accent { background: var(--os-accent); color: #fff; border-color: #1a3a7a; }
                .os-badge-teal { background: var(--os-teal); color: #111; border-color: #2a8a6a; }
                .os-badge-warning { background: var(--os-warning); color: #111; border-color: #b8860b; }
                .os-badge-danger { background: var(--os-danger); color: #fff; border-color: #8b0000; }
                .os-led {
                    width: 10px; height: 10px; border-radius: 50%;
                    border: 1px solid rgba(0,0,0,0.3);
                    display: inline-block;
                }
                .os-led-green { background: #22c55e; box-shadow: 0 0 4px #22c55e; }
                .os-led-yellow { background: #eab308; box-shadow: 0 0 4px #eab308; }
                .os-led-red { background: #ef4444; box-shadow: 0 0 4px #ef4444; }
                .os-led-off { background: #ccc; box-shadow: none; }
                .os-led-row { display: flex; align-items: center; gap: 6px; font-size: 12px; color: var(--os-text); }
            </style>
            <div class="preview-box" style="background: var(--os-chrome);">
                <div style="display:flex;flex-direction:column;gap:20px;">
                    <div>
                        <div class="component-label" style="color:var(--os-text-secondary);">Badges</div>
                        <div style="display:flex;gap:8px;flex-wrap:wrap;margin-top:4px;">
                            <span class="os-badge os-badge-default">Default</span>
                            <span class="os-badge os-badge-accent">3 New</span>
                            <span class="os-badge os-badge-teal">Active</span>
                            <span class="os-badge os-badge-warning">Warning</span>
                            <span class="os-badge os-badge-danger">Critical</span>
                        </div>
                    </div>
                    <div>
                        <div class="component-label" style="color:var(--os-text-secondary);">Status LEDs</div>
                        <div style="display:flex;flex-direction:column;gap:6px;margin-top:4px;">
                            <div class="os-led-row"><span class="os-led os-led-green"></span> CPU — Normal</div>
                            <div class="os-led-row"><span class="os-led os-led-yellow"></span> Memory — 78% Used</div>
                            <div class="os-led-row"><span class="os-led os-led-red"></span> Disk — Critical</div>
                            <div class="os-led-row"><span class="os-led os-led-off"></span> Network — Disconnected</div>
                        </div>
                    </div>
                </div>
            </div>
        </div>

        <!-- ============================================================ -->
        <!-- ALERTS / MESSAGE BARS -->
        <!-- ============================================================ -->
        <div class="section">
            <h2 class="section-title">Alerts &amp; Message Bars</h2>
            <div class="section-desc">In-page message bars for informational, success, warning, and error notifications.</div>
            <style>
                .os-alert {
                    border: 2px solid;
                    border-color: var(--bevel-light) var(--bevel-dark) var(--bevel-dark) var(--bevel-light);
                    padding: 8px 12px;
                    font-size: 12px;
                    display: flex; align-items: flex-start; gap: 8px;
                }
                .os-alert-icon { font-size: 16px; flex-shrink: 0; }
                .os-alert-text { flex: 1; }
                .os-alert-title { font-weight: bold; margin-bottom: 2px; }
                .os-alert-close { cursor: pointer; font-size: 14px; margin-left: auto; opacity: 0.7; }
                .os-alert-close:hover { opacity: 1; }
                .os-alert-info { background: #dbe8ff; color: #1a3a6a; border-color: #2B5BB5 #a0b8e0 #a0b8e0 #2B5BB5; }
                .os-alert-success { background: #d4f0e4; color: #1a4a2a; border-color: #28A745 #a0d8c0 #a0d8c0 #28A745; }
                .os-alert-warning { background: #fff3cd; color: #5a4010; border-color: #E8A317 #e0c880 #e0c880 #E8A317; }
                .os-alert-error { background: #fde4e4; color: #6a1a1a; border-color: #DC3545 #e0a0a0 #e0a0a0 #DC3545; }
            </style>
            <div class="preview-box" style="background: var(--os-chrome);">
                <div style="display:flex;flex-direction:column;gap:10px;max-width:480px;">
                    <div class="os-alert os-alert-info">
                        <span class="os-alert-icon">ℹ️</span>
                        <div class="os-alert-text"><div class="os-alert-title">Information</div>System update available. Restart to apply changes.</div>
                        <span class="os-alert-close">✕</span>
                    </div>
                    <div class="os-alert os-alert-success">
                        <span class="os-alert-icon">✅</span>
                        <div class="os-alert-text"><div class="os-alert-title">Success</div>File saved successfully to C:\Users\Documents.</div>
                        <span class="os-alert-close">✕</span>
                    </div>
                    <div class="os-alert os-alert-warning">
                        <span class="os-alert-icon">⚠️</span>
                        <div class="os-alert-text"><div class="os-alert-title">Warning</div>Disk space is running low. Free up some space.</div>
                        <span class="os-alert-close">✕</span>
                    </div>
                    <div class="os-alert os-alert-error">
                        <span class="os-alert-icon">❌</span>
                        <div class="os-alert-text"><div class="os-alert-title">Error</div>Unable to connect to network drive Z:\.</div>
                        <span class="os-alert-close">✕</span>
                    </div>
                </div>
            </div>
        </div>

        <!-- ============================================================ -->
        <!-- TOOLTIPS -->
        <!-- ============================================================ -->
        <div class="section">
            <h2 class="section-title">Tooltips</h2>
            <div class="section-desc">Classic OS-style yellow tooltip boxes that appear on hover.</div>
            <style>
                .os-tooltip-demo { position: relative; display: inline-block; cursor: help; }
                .os-tooltip-box {
                    background: #ffffe1;
                    color: #000;
                    border: 1px solid #000;
                    padding: 3px 6px;
                    font-size: 11px;
                    white-space: nowrap;
                    box-shadow: 1px 1px 2px rgba(0,0,0,0.3);
                }
                .os-tooltip-dark {
                    background: var(--os-chrome);
                    color: var(--os-text);
                    border: 1px solid var(--bevel-dark);
                }
            </style>
            <div class="preview-box" style="background: var(--os-chrome);">
                <div style="display:flex;gap:40px;flex-wrap:wrap;align-items:flex-end;">
                    <div>
                        <div class="component-label" style="color:var(--os-text-secondary);">Classic Yellow</div>
                        <div style="margin-top:4px;">
                            <button class="os-btn">Hover Me</button>
                            <div class="os-tooltip-box" style="margin-top:4px;">This action saves your file</div>
                        </div>
                    </div>
                    <div>
                        <div class="component-label" style="color:var(--os-text-secondary);">Themed Variant</div>
                        <div style="margin-top:4px;">
                            <button class="os-btn">Hover Me</button>
                            <div class="os-tooltip-box os-tooltip-dark" style="margin-top:4px;">Open file browser (Ctrl+O)</div>
                        </div>
                    </div>
                </div>
            </div>
        </div>

        <!-- ============================================================ -->
        <!-- TASKBAR -->
        <!-- ============================================================ -->
        <div class="section">
            <h2 class="section-title">Taskbar</h2>
            <div class="section-desc">Bottom taskbar with Start button, quick launch area, running application buttons, and system tray with clock.</div>
            <style>
                .os-taskbar {
                    border: 2px solid;
                    border-color: var(--bevel-light) var(--bevel-dark) var(--bevel-dark) var(--bevel-light);
                    background: var(--os-chrome);
                    display: flex; align-items: center; gap: 4px;
                    padding: 2px 4px; height: 32px;
                }
                .os-start-btn {
                    border: 2px solid;
                    border-color: var(--bevel-light) var(--bevel-dark) var(--bevel-dark) var(--bevel-light);
                    background: var(--os-chrome);
                    padding: 2px 8px;
                    display: flex; align-items: center; gap: 4px;
                    cursor: pointer; font-size: 12px; font-weight: bold; color: var(--os-text);
                    height: 24px;
                }
                .os-start-btn:active { border-color: var(--bevel-dark) var(--bevel-light) var(--bevel-light) var(--bevel-dark); }
                .os-start-logo { font-size: 14px; }
                .os-quicklaunch {
                    display: flex; gap: 2px; padding: 0 4px;
                    border-right: 1px solid var(--bevel-dark);
                    margin-right: 2px;
                    height: 22px; align-items: center;
                }
                .os-quicklaunch-btn {
                    width: 20px; height: 20px;
                    display: flex; align-items: center; justify-content: center;
                    cursor: pointer; font-size: 12px;
                    border: 1px solid transparent;
                }
                .os-quicklaunch-btn:hover { border-color: var(--bevel-light) var(--bevel-dark) var(--bevel-dark) var(--bevel-light); }
                .os-task-btn {
                    border: 2px solid;
                    border-color: var(--bevel-light) var(--bevel-dark) var(--bevel-dark) var(--bevel-light);
                    background: var(--os-chrome);
                    padding: 1px 10px;
                    font-size: 11px; color: var(--os-text);
                    cursor: pointer;
                    max-width: 160px; overflow: hidden; text-overflow: ellipsis;
                    white-space: nowrap;
                    height: 22px; display: flex; align-items: center; gap: 4px;
                }
                .os-task-btn.active {
                    border-color: var(--bevel-dark) var(--bevel-light) var(--bevel-light) var(--bevel-dark);
                    background: var(--os-chrome-light);
                    font-weight: bold;
                }
                .os-systray {
                    margin-left: auto;
                    display: flex; align-items: center; gap: 6px;
                    border: 2px solid;
                    border-color: var(--bevel-dark) var(--bevel-light) var(--bevel-light) var(--bevel-dark);
                    padding: 1px 8px; height: 22px;
                    font-size: 11px; color: var(--os-text-secondary);
                }
                .os-systray-icon { font-size: 12px; cursor: pointer; }
            </style>
            <div class="preview-box" style="background: var(--os-surface-sunken);padding:24px 12px;">
                <div class="os-taskbar">
                    <div class="os-start-btn"><span class="os-start-logo">🖥️</span> Start</div>
                    <div class="os-quicklaunch">
                        <span class="os-quicklaunch-btn">📁</span>
                        <span class="os-quicklaunch-btn">🌐</span>
                        <span class="os-quicklaunch-btn">📝</span>
                    </div>
                    <div class="os-task-btn active"><span>📁</span> File Explorer</div>
                    <div class="os-task-btn"><span>📝</span> Notepad</div>
                    <div class="os-systray">
                        <span class="os-systray-icon">🔊</span>
                        <span class="os-systray-icon">🔌</span>
                        <span>12:45 PM</span>
                    </div>
                </div>
            </div>
        </div>

        <!-- ============================================================ -->
        <!-- START MENU -->
        <!-- ============================================================ -->
        <div class="section">
            <h2 class="section-title">Start Menu</h2>
            <div class="section-desc">Classic start menu with MaahiOS branded sidebar banner, program list, and shutdown options.</div>
            <style>
                .os-startmenu {
                    border: 2px solid;
                    border-color: var(--bevel-light) var(--bevel-dark) var(--bevel-dark) var(--bevel-light);
                    background: var(--os-chrome);
                    display: flex; width: 320px;
                    box-shadow: 2px 2px 8px rgba(0,0,0,0.5);
                }
                .os-startmenu-sidebar {
                    width: 28px;
                    background: linear-gradient(to top, var(--titlebar-start), var(--titlebar-end));
                    display: flex; align-items: flex-end; justify-content: center;
                    padding-bottom: 8px;
                }
                .os-startmenu-sidebar-text {
                    writing-mode: vertical-rl; transform: rotate(180deg);
                    font-size: 14px; font-weight: bold; color: #fff; letter-spacing: 2px;
                }
                .os-startmenu-list { flex: 1; padding: 4px 0; }
                .os-startmenu-item {
                    padding: 6px 12px;
                    display: flex; align-items: center; gap: 8px;
                    font-size: 12px; color: var(--os-text); cursor: pointer;
                }
                .os-startmenu-item:hover { background: var(--os-accent); color: #fff; }
                .os-startmenu-item-icon { font-size: 18px; width: 24px; text-align: center; }
                .os-startmenu-item-arrow { margin-left: auto; font-size: 8px; color: var(--os-text-secondary); }
                .os-startmenu-item:hover .os-startmenu-item-arrow { color: rgba(255,255,255,0.7); }
                .os-startmenu-sep { height: 1px; background: var(--bevel-dark); margin: 3px 4px; border-bottom: 1px solid var(--bevel-light); }
            </style>
            <div class="preview-box" style="background: var(--os-surface-sunken);">
                <div class="os-startmenu">
                    <div class="os-startmenu-sidebar">
                        <span class="os-startmenu-sidebar-text">MaahiOS</span>
                    </div>
                    <div class="os-startmenu-list">
                        <div class="os-startmenu-item"><span class="os-startmenu-item-icon">📂</span> Programs <span class="os-startmenu-item-arrow">▶</span></div>
                        <div class="os-startmenu-item"><span class="os-startmenu-item-icon">📄</span> Documents <span class="os-startmenu-item-arrow">▶</span></div>
                        <div class="os-startmenu-item"><span class="os-startmenu-item-icon">⚙️</span> Settings <span class="os-startmenu-item-arrow">▶</span></div>
                        <div class="os-startmenu-item"><span class="os-startmenu-item-icon">🔍</span> Find <span class="os-startmenu-item-arrow">▶</span></div>
                        <div class="os-startmenu-item"><span class="os-startmenu-item-icon">❓</span> Help</div>
                        <div class="os-startmenu-item"><span class="os-startmenu-item-icon">▶️</span> Run...</div>
                        <div class="os-startmenu-sep"></div>
                        <div class="os-startmenu-item"><span class="os-startmenu-item-icon">🔒</span> Log Off...</div>
                        <div class="os-startmenu-item"><span class="os-startmenu-item-icon">⏻</span> Shut Down...</div>
                    </div>
                </div>
            </div>
        </div>

        <!-- ============================================================ -->
        <!-- DESKTOP ICONS -->
        <!-- ============================================================ -->
        <div class="section">
            <h2 class="section-title">Desktop Icons</h2>
            <div class="section-desc">Desktop shortcut icons with text labels. Single-click highlight, double-click to open.</div>
            <style>
                .os-desktop-icon {
                    width: 70px; text-align: center; cursor: pointer; padding: 4px;
                    display: flex; flex-direction: column; align-items: center; gap: 2px;
                }
                .os-desktop-icon:hover .os-desktop-icon-img { opacity: 0.8; }
                .os-desktop-icon-img { font-size: 32px; }
                .os-desktop-icon-label {
                    font-size: 11px; color: #fff;
                    text-shadow: 1px 1px 3px rgba(0,0,0,0.9);
                    word-wrap: break-word; line-height: 1.2;
                }
                .os-desktop-icon.selected .os-desktop-icon-label {
                    background: var(--os-accent); color: #fff; padding: 0 2px;
                }
                .os-desktop-icon.selected .os-desktop-icon-img {
                    filter: brightness(0.7) sepia(1) hue-rotate(200deg) saturate(2);
                }
            </style>
            <div class="preview-box" style="background: linear-gradient(135deg, #3A7BB8 0%, #5A9BD8 50%, #3A8BC8 100%); min-height: 160px;">
                <div style="display:flex;flex-wrap:wrap;gap:12px;">
                    <div class="os-desktop-icon selected">
                        <span class="os-desktop-icon-img">💻</span>
                        <span class="os-desktop-icon-label">My Computer</span>
                    </div>
                    <div class="os-desktop-icon">
                        <span class="os-desktop-icon-img">📁</span>
                        <span class="os-desktop-icon-label">Documents</span>
                    </div>
                    <div class="os-desktop-icon">
                        <span class="os-desktop-icon-img">🗑️</span>
                        <span class="os-desktop-icon-label">Recycle Bin</span>
                    </div>
                    <div class="os-desktop-icon">
                        <span class="os-desktop-icon-img">🌐</span>
                        <span class="os-desktop-icon-label">Network</span>
                    </div>
                    <div class="os-desktop-icon">
                        <span class="os-desktop-icon-img">⚙️</span>
                        <span class="os-desktop-icon-label">Control Panel</span>
                    </div>
                    <div class="os-desktop-icon">
                        <span class="os-desktop-icon-img">📝</span>
                        <span class="os-desktop-icon-label">Notepad</span>
                    </div>
                    <div class="os-desktop-icon">
                        <span class="os-desktop-icon-img">🖥️</span>
                        <span class="os-desktop-icon-label">Terminal</span>
                    </div>
                </div>
            </div>
        </div>

        <!-- ============================================================ -->
        <!-- FILE BROWSER (ICON VIEW) -->
        <!-- ============================================================ -->
        <div class="section">
            <h2 class="section-title">File Browser — Icon View</h2>
            <div class="section-desc">File explorer icon grid for browsing files and folders inside a window body.</div>
            <style>
                .os-file-grid {
                    display: flex; flex-wrap: wrap; gap: 8px; padding: 8px;
                }
                .os-file-item {
                    width: 64px; text-align: center; cursor: pointer; padding: 4px;
                    display: flex; flex-direction: column; align-items: center; gap: 2px;
                }
                .os-file-item:hover { background: rgba(43,91,181,0.2); }
                .os-file-item.selected { background: var(--os-accent); }
                .os-file-icon { font-size: 28px; }
                .os-file-name { font-size: 11px; color: var(--os-text); line-height: 1.2; word-break: break-all; }
                .os-file-item.selected .os-file-name { color: #fff; }
            </style>
            <div class="preview-box" style="background: var(--os-chrome);">
                <div style="max-width:420px;">
                    <div style="border:2px solid;border-color:var(--bevel-dark) var(--bevel-light) var(--bevel-light) var(--bevel-dark);background:var(--os-input-bg);">
                        <div class="os-file-grid">
                            <div class="os-file-item"><span class="os-file-icon">📁</span><span class="os-file-name">Projects</span></div>
                            <div class="os-file-item"><span class="os-file-icon">📁</span><span class="os-file-name">Downloads</span></div>
                            <div class="os-file-item selected"><span class="os-file-icon">📄</span><span class="os-file-name">readme.txt</span></div>
                            <div class="os-file-item"><span class="os-file-icon">🖼️</span><span class="os-file-name">wallpaper.bmp</span></div>
                            <div class="os-file-item"><span class="os-file-icon">⚙️</span><span class="os-file-name">config.sys</span></div>
                            <div class="os-file-item"><span class="os-file-icon">📦</span><span class="os-file-name">kernel.bin</span></div>
                            <div class="os-file-item"><span class="os-file-icon">📄</span><span class="os-file-name">notes.md</span></div>
                            <div class="os-file-item"><span class="os-file-icon">🎵</span><span class="os-file-name">startup.wav</span></div>
                        </div>
                    </div>
                </div>
            </div>
        </div>

        <!-- ============================================================ -->
        <!-- SCROLLBARS -->
        <!-- ============================================================ -->
        <div class="section">
            <h2 class="section-title">Scrollbars</h2>
            <div class="section-desc">Custom embossed scrollbar styling. Applied via CSS custom properties on scrollable containers.</div>
            <style>
                .os-scrollbar-demo {
                    width: 260px; height: 120px; overflow-y: scroll;
                    border: 2px solid;
                    border-color: var(--bevel-dark) var(--bevel-light) var(--bevel-light) var(--bevel-dark);
                    background: var(--os-input-bg);
                    padding: 6px; font-size: 12px; color: var(--os-text);
                }
                .os-scrollbar-demo::-webkit-scrollbar { width: 16px; }
                .os-scrollbar-demo::-webkit-scrollbar-track { background: var(--os-chrome-dark); }
                .os-scrollbar-demo::-webkit-scrollbar-thumb {
                    background: var(--os-chrome);
                    border: 2px solid;
                    border-color: var(--bevel-light) var(--bevel-dark) var(--bevel-dark) var(--bevel-light);
                }
                .os-scrollbar-demo::-webkit-scrollbar-thumb:hover { background: var(--os-chrome-light); }
                .os-scrollbar-demo::-webkit-scrollbar-button {
                    background: var(--os-chrome);
                    border: 2px solid;
                    border-color: var(--bevel-light) var(--bevel-dark) var(--bevel-dark) var(--bevel-light);
                    height: 16px;
                }
            </style>
            <div class="preview-box" style="background: var(--os-chrome);">
                <div style="display:flex;gap:20px;flex-wrap:wrap;align-items:flex-start;">
                    <div>
                        <div class="component-label" style="color:var(--os-text-secondary);">Scrollable List</div>
                        <div class="os-scrollbar-demo">
                            <div>Line 1 — MaahiOS kernel loaded</div>
                            <div>Line 2 — Initializing memory manager</div>
                            <div>Line 3 — VGA driver started</div>
                            <div>Line 4 — Keyboard driver loaded</div>
                            <div>Line 5 — Mouse driver loaded</div>
                            <div>Line 6 — File system mounted</div>
                            <div>Line 7 — Shell initialized</div>
                            <div>Line 8 — Network stack ready</div>
                            <div>Line 9 — Sound driver loaded</div>
                            <div>Line 10 — Desktop environment starting</div>
                            <div>Line 11 — Loading user preferences</div>
                            <div>Line 12 — System ready</div>
                        </div>
                    </div>
                    <div style="font-size:12px;color:var(--os-text);max-width:260px;">
                        <div class="component-label" style="color:var(--os-text-secondary);">CSS Applied</div>
                        <code style="font-size:11px;color:var(--os-teal);display:block;background:var(--os-surface-sunken);padding:8px;border:1px solid var(--bevel-dark);white-space:pre;line-height:1.4;">::-webkit-scrollbar { width: 16px; }
::-webkit-scrollbar-track { background: var(--os-chrome-dark); }
::-webkit-scrollbar-thumb { background: var(--os-chrome); border: 2px solid ... }</code>
                    </div>
                </div>
            </div>
        </div>

        <!-- ============================================================ -->
        <!-- NOTIFICATIONS -->
        <!-- ============================================================ -->
        <div class="section">
            <h2 class="section-title">Notifications</h2>
            <div class="section-desc">System tray popup notifications and toast-style alerts that appear from the taskbar area.</div>
            <style>
                .os-notification {
                    border: 2px solid;
                    border-color: var(--bevel-light) var(--bevel-dark) var(--bevel-dark) var(--bevel-light);
                    background: var(--os-chrome);
                    box-shadow: 2px 2px 8px rgba(0,0,0,0.5);
                    padding: 12px; display: flex; gap: 10px;
                    max-width: 300px;
                }
                .os-notification-icon { font-size: 24px; flex-shrink: 0; }
                .os-notification-body { flex: 1; }
                .os-notification-title { font-size: 12px; font-weight: bold; color: var(--os-text); margin-bottom: 2px; }
                .os-notification-msg { font-size: 11px; color: var(--os-text-secondary); line-height: 1.3; }
                .os-notification-close { font-size: 10px; color: var(--os-text-secondary); cursor: pointer; }
                .os-notification-close:hover { color: var(--os-text); }
                .os-notification-actions { margin-top: 6px; display: flex; gap: 6px; }
            </style>
            <div class="preview-box" style="background: var(--os-surface-sunken);">
                <div style="display:flex;flex-direction:column;gap:12px;align-items:flex-end;">
                    <div class="os-notification">
                        <span class="os-notification-icon">🔔</span>
                        <div class="os-notification-body">
                            <div class="os-notification-title">System Update</div>
                            <div class="os-notification-msg">MaahiOS v1.0.1 is available. Restart to install the update.</div>
                            <div class="os-notification-actions">
                                <button class="os-btn os-btn-sm">Restart Now</button>
                                <button class="os-btn os-btn-sm os-btn-flat">Later</button>
                            </div>
                        </div>
                        <span class="os-notification-close">✕</span>
                    </div>
                    <div class="os-notification">
                        <span class="os-notification-icon">💿</span>
                        <div class="os-notification-body">
                            <div class="os-notification-title">USB Drive Detected</div>
                            <div class="os-notification-msg">New removable drive E:\ is ready to use.</div>
                        </div>
                        <span class="os-notification-close">✕</span>
                    </div>
                </div>
            </div>
        </div>

        <!-- ============================================================ -->
        <!-- CONTROL PANEL GRID -->
        <!-- ============================================================ -->
        <div class="section">
            <h2 class="section-title">Control Panel</h2>
            <div class="section-desc">Grid of system applets using the icon view pattern for a Control Panel interface.</div>
            <style>
                .os-cpanel-grid {
                    display: grid;
                    grid-template-columns: repeat(auto-fill, minmax(80px, 1fr));
                    gap: 8px; padding: 12px;
                }
                .os-cpanel-item {
                    display: flex; flex-direction: column; align-items: center;
                    text-align: center; padding: 8px 4px;
                    cursor: pointer; gap: 4px;
                }
                .os-cpanel-item:hover { background: rgba(43,91,181,0.2); }
                .os-cpanel-icon { font-size: 28px; }
                .os-cpanel-label { font-size: 11px; color: var(--os-text); line-height: 1.2; }
            </style>
            <div class="preview-box" style="background: var(--os-chrome);">
                <div class="os-window" style="max-width:500px;">
                    <div class="os-titlebar">
                        <span class="os-titlebar-icon">⚙️</span>
                        <span class="os-titlebar-text">Control Panel</span>
                        <div class="os-titlebar-btns">
                            <div class="os-titlebar-btn">─</div>
                            <div class="os-titlebar-btn">□</div>
                            <div class="os-titlebar-btn">✕</div>
                        </div>
                    </div>
                    <div style="border:2px solid;border-color:var(--bevel-dark) var(--bevel-light) var(--bevel-light) var(--bevel-dark);background:var(--os-input-bg);margin:2px;">
                        <div class="os-cpanel-grid">
                            <div class="os-cpanel-item"><span class="os-cpanel-icon">🖥️</span><span class="os-cpanel-label">Display</span></div>
                            <div class="os-cpanel-item"><span class="os-cpanel-icon">🔊</span><span class="os-cpanel-label">Sound</span></div>
                            <div class="os-cpanel-item"><span class="os-cpanel-icon">⌨️</span><span class="os-cpanel-label">Keyboard</span></div>
                            <div class="os-cpanel-item"><span class="os-cpanel-icon">🖱️</span><span class="os-cpanel-label">Mouse</span></div>
                            <div class="os-cpanel-item"><span class="os-cpanel-icon">🌐</span><span class="os-cpanel-label">Network</span></div>
                            <div class="os-cpanel-item"><span class="os-cpanel-icon">🕐</span><span class="os-cpanel-label">Date/Time</span></div>
                            <div class="os-cpanel-item"><span class="os-cpanel-icon">👤</span><span class="os-cpanel-label">Users</span></div>
                            <div class="os-cpanel-item"><span class="os-cpanel-icon">🔐</span><span class="os-cpanel-label">Security</span></div>
                            <div class="os-cpanel-item"><span class="os-cpanel-icon">💿</span><span class="os-cpanel-label">Devices</span></div>
                            <div class="os-cpanel-item"><span class="os-cpanel-icon">🔋</span><span class="os-cpanel-label">Power</span></div>
                        </div>
                    </div>
                    <div class="os-statusbar">
                        <span class="os-statusbar-section">10 object(s)</span>
                    </div>
                </div>
            </div>
        </div>

        <!-- ============================================================ -->
        <!-- FULL DESKTOP PREVIEW -->
        <!-- ============================================================ -->
        <div class="section">
            <h2 class="section-title">Full Desktop Preview</h2>
            <div class="section-desc">Complete desktop environment showing wallpaper, icons, open windows, and taskbar all together.</div>
            <style>
                .os-desktop-preview {
                    width: 100%; min-height: 480px;
                    background: linear-gradient(135deg, #3A7BB8 0%, #5A9BD8 50%, #3A8BC8 100%);
                    position: relative; overflow: hidden;
                    border: 2px solid var(--bevel-dark);
                }
                .os-desktop-icons-area {
                    position: absolute; top: 10px; left: 10px;
                    display: flex; flex-direction: column; gap: 8px;
                }
                .os-desktop-float-win {
                    position: absolute;
                }
            </style>
            <div class="preview-box" style="padding:0; background: transparent;">
                <div class="os-desktop-preview">
                    <!-- Desktop Icons -->
                    <div class="os-desktop-icons-area">
                        <div class="os-desktop-icon">
                            <span class="os-desktop-icon-img">💻</span>
                            <span class="os-desktop-icon-label">My Computer</span>
                        </div>
                        <div class="os-desktop-icon">
                            <span class="os-desktop-icon-img">📁</span>
                            <span class="os-desktop-icon-label">Documents</span>
                        </div>
                        <div class="os-desktop-icon">
                            <span class="os-desktop-icon-img">🗑️</span>
                            <span class="os-desktop-icon-label">Recycle Bin</span>
                        </div>
                        <div class="os-desktop-icon">
                            <span class="os-desktop-icon-img">🖥️</span>
                            <span class="os-desktop-icon-label">Terminal</span>
                        </div>
                    </div>

                    <!-- Floating Window 1 -->
                    <div class="os-desktop-float-win" style="top:20px;left:120px;width:400px;">
                        <div class="os-window" style="max-width:none;">
                            <div class="os-titlebar">
                                <span class="os-titlebar-icon">📁</span>
                                <span class="os-titlebar-text">File Explorer — C:\</span>
                                <div class="os-titlebar-btns">
                                    <div class="os-titlebar-btn">─</div>
                                    <div class="os-titlebar-btn">□</div>
                                    <div class="os-titlebar-btn">✕</div>
                                </div>
                            </div>
                            <div class="os-menubar">
                                <span class="os-menu-item">File</span>
                                <span class="os-menu-item">Edit</span>
                                <span class="os-menu-item">View</span>
                            </div>
                            <div style="border:2px solid;border-color:var(--bevel-dark) var(--bevel-light) var(--bevel-light) var(--bevel-dark);background:var(--os-input-bg);margin:2px;">
                                <div class="os-file-grid">
                                    <div class="os-file-item"><span class="os-file-icon">📁</span><span class="os-file-name">MaahiOS</span></div>
                                    <div class="os-file-item"><span class="os-file-icon">📁</span><span class="os-file-name">Users</span></div>
                                    <div class="os-file-item selected"><span class="os-file-icon">📁</span><span class="os-file-name">System</span></div>
                                    <div class="os-file-item"><span class="os-file-icon">📄</span><span class="os-file-name">boot.cfg</span></div>
                                </div>
                            </div>
                            <div class="os-statusbar">
                                <span class="os-statusbar-section">4 object(s)</span>
                                <span class="os-statusbar-section">My Computer</span>
                            </div>
                        </div>
                    </div>

                    <!-- Floating Window 2 (behind, inactive) -->
                    <div class="os-desktop-float-win" style="top:60px;left:280px;width:320px;">
                        <div class="os-window" style="max-width:none;">
                            <div class="os-titlebar inactive">
                                <span class="os-titlebar-icon">📝</span>
                                <span class="os-titlebar-text">Notepad — readme.txt</span>
                                <div class="os-titlebar-btns">
                                    <div class="os-titlebar-btn">─</div>
                                    <div class="os-titlebar-btn">□</div>
                                    <div class="os-titlebar-btn">✕</div>
                                </div>
                            </div>
                            <div class="os-window-body" style="min-height:60px;max-height:80px;font-family:monospace;font-size:12px;">
                                Welcome to MaahiOS!<br>
                                A custom x86 operating system<br>
                                built from scratch.
                            </div>
                        </div>
                    </div>

                    <!-- Dialog overlay -->
                    <div class="os-desktop-float-win" style="top:150px;left:200px;">
                        <div class="os-dialog" style="min-width:260px;">
                            <div class="os-dialog-titlebar">
                                <span class="os-dialog-title">MaahiOS</span>
                                <div class="os-dialog-close">✕</div>
                            </div>
                            <div class="os-dialog-body">
                                <div class="os-dialog-icon">ℹ️</div>
                                <div class="os-dialog-msg">Welcome to MaahiOS!<br>System ready.</div>
                            </div>
                            <div class="os-dialog-buttons">
                                <button class="os-btn os-btn-default" style="min-width:72px;">OK</button>
                            </div>
                        </div>
                    </div>

                    <!-- Taskbar at bottom -->
                    <div style="position:absolute;bottom:0;left:0;right:0;">
                        <div class="os-taskbar">
                            <div class="os-start-btn"><span class="os-start-logo">🖥️</span> Start</div>
                            <div class="os-quicklaunch">
                                <span class="os-quicklaunch-btn">📁</span>
                                <span class="os-quicklaunch-btn">🌐</span>
                                <span class="os-quicklaunch-btn">📝</span>
                            </div>
                            <div class="os-task-btn active"><span>📁</span> File Explorer</div>
                            <div class="os-task-btn"><span>📝</span> Notepad</div>
                            <div class="os-systray">
                                <span class="os-systray-icon">🔊</span>
                                <span class="os-systray-icon">🔌</span>
                                <span>12:45 PM</span>
                            </div>
                        </div>
                    </div>
                </div>
            </div>
        </div>

        <!-- ============================================================ -->
        <!-- SYSTEM APPS -->
        <!-- ============================================================ -->
        <div class="section">
            <h2 class="section-title">System Apps</h2>
            <div class="section-desc">Full application mockups for core MaahiOS system utilities: Process Explorer, File Manager, and Disk Manager.</div>

            <style>
                /* --- Process Explorer --- */
                .pe-header-bar {
                    display: flex; align-items: center; gap: 8px;
                    padding: 4px 8px;
                    background: var(--os-chrome);
                    border-bottom: 1px solid var(--bevel-dark);
                    font-size: 11px; color: var(--os-text-secondary);
                }
                .pe-header-bar .pe-stat {
                    display: flex; align-items: center; gap: 4px;
                }
                .pe-header-bar .pe-stat-value {
                    font-weight: 600; color: var(--os-text);
                }
                .pe-table-wrap {
                    border: 2px solid;
                    border-color: var(--bevel-dark) var(--bevel-light) var(--bevel-light) var(--bevel-dark);
                    background: var(--os-input-bg);
                    margin: 2px;
                    overflow: visible; position: relative;
                    max-height: none;
                }
                .pe-table { width: 100%; border-collapse: collapse; font-size: 11px; }
                .pe-table th {
                    background: var(--os-chrome);
                    border: 1px solid;
                    border-color: var(--bevel-light) var(--bevel-dark) var(--bevel-dark) var(--bevel-light);
                    padding: 2px 8px; text-align: left;
                    color: var(--os-text); font-weight: normal;
                    white-space: nowrap; cursor: pointer;
                    position: sticky; top: 0; z-index: 1;
                }
                .pe-table th:hover { background: var(--os-chrome-light); }
                .pe-table td {
                    padding: 2px 8px; color: var(--os-text);
                    border-bottom: 1px solid rgba(148,152,172,0.2);
                    white-space: nowrap;
                }
                .pe-table tr:hover td { background: rgba(43,91,181,0.12); }
                .pe-table tr.pe-selected td { background: var(--os-accent); color: #fff; }
                .pe-table .pe-cpu-bar {
                    display: inline-block; height: 10px;
                    background: var(--os-teal); vertical-align: middle;
                    min-width: 2px;
                }
                .pe-table .pe-cpu-bar.high { background: var(--os-danger); }
                .pe-table .pe-cpu-bar.med { background: var(--os-warning); }
                .pe-table .pe-pid { color: var(--os-text-secondary); font-family: var(--os-font-mono); font-size: 10px; }
                .pe-graph-area {
                    display: flex; gap: 8px; padding: 6px;
                    background: var(--os-chrome);
                    border-top: 1px solid var(--bevel-dark);
                }
                .pe-graph-box {
                    flex: 1;
                    border: 2px solid;
                    border-color: var(--bevel-dark) var(--bevel-light) var(--bevel-light) var(--bevel-dark);
                    background: #0a1628;
                    height: 60px; position: relative; overflow: hidden;
                }
                .pe-graph-label {
                    position: absolute; top: 2px; left: 4px;
                    font-size: 9px; color: rgba(255,255,255,0.6);
                }
                .pe-graph-value {
                    position: absolute; bottom: 2px; right: 4px;
                    font-size: 10px; font-weight: bold;
                }
                .pe-graph-line {
                    position: absolute; bottom: 0; left: 0; right: 0;
                }
                .pe-graph-line svg { width: 100%; height: 60px; }

                /* Process Explorer – Toolbar */
                .pe-toolbar {
                    display: flex; align-items: center; gap: 2px;
                    padding: 3px 6px;
                    background: var(--os-chrome);
                    border-bottom: 1px solid var(--bevel-dark);
                }
                .pe-toolbar-btn {
                    display: inline-flex; align-items: center; gap: 4px;
                    padding: 2px 8px;
                    font-size: 11px; color: var(--os-text);
                    background: var(--os-chrome);
                    border: 1px solid transparent;
                    cursor: pointer;
                }
                .pe-toolbar-btn:hover {
                    border-color: var(--bevel-light) var(--bevel-dark) var(--bevel-dark) var(--bevel-light);
                    background: var(--os-chrome-light);
                }
                .pe-toolbar-btn:active {
                    border-color: var(--bevel-dark) var(--bevel-light) var(--bevel-light) var(--bevel-dark);
                }
                .pe-toolbar-btn.disabled {
                    opacity: 0.45; pointer-events: none;
                }
                .pe-toolbar-btn .pe-tb-icon { font-size: 13px; }
                .pe-toolbar-sep {
                    width: 1px; height: 18px;
                    background: var(--bevel-dark);
                    margin: 0 4px;
                    box-shadow: 1px 0 0 var(--bevel-light);
                }

                /* Process Explorer – Context menu (right-click) */
                .pe-context-menu {
                    position: absolute;
                    background: var(--os-chrome);
                    border: 2px solid;
                    border-color: var(--bevel-light) var(--bevel-dark) var(--bevel-dark) var(--bevel-light);
                    box-shadow: 2px 3px 6px rgba(0,0,0,0.25);
                    padding: 3px 0;
                    min-width: 180px;
                    z-index: 10;
                    font-size: 11px;
                }
                .pe-ctx-item {
                    display: flex; align-items: center; justify-content: space-between;
                    padding: 3px 20px 3px 10px;
                    color: var(--os-text);
                    cursor: pointer;
                    white-space: nowrap;
                }
                .pe-ctx-item:hover {
                    background: var(--os-accent); color: #fff;
                }
                .pe-ctx-item.disabled {
                    color: var(--os-text-secondary); opacity: 0.5; pointer-events: none;
                }
                .pe-ctx-icon { width: 18px; text-align: center; margin-right: 6px; font-size: 12px; }
                .pe-ctx-shortcut { font-size: 10px; color: var(--os-text-secondary); margin-left: 16px; }
                .pe-ctx-item:hover .pe-ctx-shortcut { color: rgba(255,255,255,0.7); }
                .pe-ctx-sep {
                    height: 1px; margin: 3px 4px;
                    background: var(--bevel-dark);
                    box-shadow: 0 1px 0 var(--bevel-light);
                }
                .pe-ctx-submenu::after {
                    content: '▸'; float: right; margin-left: 12px;
                }
                .pe-ctx-item.pe-ctx-danger { color: var(--os-danger); }
                .pe-ctx-item.pe-ctx-danger:hover { background: var(--os-danger); color: #fff; }
                .pe-ctx-item.pe-ctx-danger:hover .pe-ctx-shortcut { color: rgba(255,255,255,0.7); }

                /* PE – Selected-process action bar */
                .pe-action-panel {
                    display: flex; align-items: center; gap: 6px;
                    padding: 4px 8px;
                    background: linear-gradient(to bottom, var(--os-chrome-light), var(--os-chrome));
                    border-top: 1px solid var(--bevel-light);
                    border-bottom: 1px solid var(--bevel-dark);
                    font-size: 11px; color: var(--os-text);
                }
                .pe-action-panel .pe-sel-label {
                    font-weight: 600; margin-right: auto;
                }

                /* --- File Manager --- */
                .fm-sidebar {
                    width: 160px; min-height: 220px;
                    border-right: 1px solid var(--bevel-dark);
                    background: var(--os-chrome-light);
                    overflow-y: auto;
                    padding: 4px 0;
                }
                .fm-sidebar-item {
                    display: flex; align-items: center; gap: 6px;
                    padding: 3px 8px; font-size: 11px; color: var(--os-text);
                    cursor: pointer;
                }
                .fm-sidebar-item:hover { background: rgba(43,91,181,0.15); }
                .fm-sidebar-item.fm-active { background: var(--os-accent); color: #fff; }
                .fm-sidebar-icon { font-size: 13px; width: 18px; text-align: center; }
                .fm-sidebar-section {
                    font-size: 10px; font-weight: 600; color: var(--os-text-secondary);
                    padding: 6px 8px 2px; text-transform: uppercase; letter-spacing: 0.5px;
                }
                .fm-content {
                    flex: 1; display: flex; flex-direction: column;
                }
                .fm-breadcrumb {
                    display: flex; align-items: center; gap: 2px;
                    padding: 4px 8px;
                    background: var(--os-chrome);
                    border-bottom: 1px solid var(--bevel-dark);
                    font-size: 11px; color: var(--os-text-secondary);
                }
                .fm-breadcrumb-seg {
                    color: var(--os-accent); cursor: pointer;
                }
                .fm-breadcrumb-seg:hover { text-decoration: underline; }
                .fm-breadcrumb-sep { color: var(--os-text-disabled); }
                .fm-detail-table { width: 100%; border-collapse: collapse; font-size: 11px; }
                .fm-detail-table th {
                    background: var(--os-chrome);
                    border: 1px solid;
                    border-color: var(--bevel-light) var(--bevel-dark) var(--bevel-dark) var(--bevel-light);
                    padding: 2px 8px; text-align: left;
                    color: var(--os-text); font-weight: normal;
                    white-space: nowrap; cursor: pointer;
                }
                .fm-detail-table td {
                    padding: 2px 8px; color: var(--os-text);
                    border-bottom: 1px solid rgba(148,152,172,0.15);
                }
                .fm-detail-table tr:hover td { background: rgba(43,91,181,0.12); }
                .fm-detail-table tr.fm-selected td { background: var(--os-accent); color: #fff; }
                .fm-info-bar {
                    display: flex; justify-content: space-between; align-items: center;
                    padding: 3px 8px;
                    background: var(--os-chrome);
                    border-top: 1px solid var(--bevel-dark);
                    font-size: 10px; color: var(--os-text-secondary);
                }

                /* --- Disk Manager --- */
                .dm-split {
                    display: flex; flex-direction: column;
                }
                .dm-top-pane {
                    border: 2px solid;
                    border-color: var(--bevel-dark) var(--bevel-light) var(--bevel-light) var(--bevel-dark);
                    background: var(--os-input-bg);
                    margin: 2px; overflow: auto; max-height: 140px;
                }
                .dm-disk-table { width: 100%; border-collapse: collapse; font-size: 11px; }
                .dm-disk-table th {
                    background: var(--os-chrome);
                    border: 1px solid;
                    border-color: var(--bevel-light) var(--bevel-dark) var(--bevel-dark) var(--bevel-light);
                    padding: 2px 8px; text-align: left;
                    color: var(--os-text); font-weight: normal;
                    white-space: nowrap; position: sticky; top: 0;
                }
                .dm-disk-table td {
                    padding: 2px 8px; color: var(--os-text);
                    border-bottom: 1px solid rgba(148,152,172,0.2);
                    white-space: nowrap;
                }
                .dm-disk-table tr:hover td { background: rgba(43,91,181,0.12); }
                .dm-disk-table tr.dm-selected td { background: var(--os-accent); color: #fff; }
                .dm-usage-bar {
                    width: 80px; height: 12px;
                    border: 1px solid var(--bevel-dark);
                    background: var(--os-surface-sunken);
                    display: inline-block; vertical-align: middle;
                }
                .dm-usage-fill {
                    height: 100%;
                }
                .dm-bottom-pane {
                    margin: 0 2px 2px;
                    border: 2px solid;
                    border-color: var(--bevel-dark) var(--bevel-light) var(--bevel-light) var(--bevel-dark);
                    background: var(--os-surface-sunken);
                    padding: 6px;
                }
                .dm-disk-row {
                    display: flex; align-items: center; gap: 8px;
                    margin-bottom: 6px; font-size: 11px; color: var(--os-text);
                }
                .dm-disk-label {
                    min-width: 60px; font-weight: 600; font-size: 10px; text-align: center;
                    padding: 2px 6px;
                    background: var(--os-chrome);
                    border: 1px solid var(--bevel-dark);
                }
                .dm-partition-bar {
                    flex: 1; height: 28px; display: flex;
                    border: 1px solid var(--bevel-dark);
                    overflow: hidden;
                }
                .dm-part {
                    display: flex; align-items: center; justify-content: center;
                    font-size: 9px; color: #fff; font-weight: 600;
                    border-right: 1px solid rgba(0,0,0,0.3);
                    white-space: nowrap; overflow: hidden; text-overflow: ellipsis;
                    padding: 0 4px;
                }
                .dm-part-primary { background: var(--os-accent); }
                .dm-part-logical { background: var(--os-teal); }
                .dm-part-swap { background: var(--os-warning); color: #111; }
                .dm-part-free { background: var(--os-chrome-dark); color: var(--os-text-secondary); }
                .dm-legend {
                    display: flex; gap: 12px; margin-top: 6px;
                    font-size: 10px; color: var(--os-text-secondary);
                }
                .dm-legend-item {
                    display: flex; align-items: center; gap: 4px;
                }
                .dm-legend-swatch {
                    width: 12px; height: 10px; border: 1px solid rgba(0,0,0,0.2);
                }
            </style>

            <!-- ===== Process Explorer ===== -->
            <div class="subsection-title" style="margin-top: 16px;">Process Explorer</div>
            <div class="preview-box" style="background: var(--os-surface-sunken); padding: 16px;">
                <div class="os-window" style="max-width: 600px;">
                    <div class="os-titlebar">
                        <span class="os-titlebar-icon">📊</span>
                        <span class="os-titlebar-text">MaahiOS Process Explorer</span>
                        <div class="os-titlebar-btns">
                            <div class="os-titlebar-btn">─</div>
                            <div class="os-titlebar-btn">□</div>
                            <div class="os-titlebar-btn">✕</div>
                        </div>
                    </div>
                    <div class="os-menubar">
                        <span class="os-menu-item"><u>F</u>ile</span>
                        <span class="os-menu-item"><u>O</u>ptions</span>
                        <span class="os-menu-item"><u>V</u>iew</span>
                        <span class="os-menu-item"><u>P</u>rocess</span>
                        <span class="os-menu-item"><u>H</u>elp</span>
                    </div>

                    <!-- Toolbar with process action buttons -->
                    <div class="pe-toolbar">
                        <span class="pe-toolbar-btn pe-tb-danger" title="Stop Process">
                            <span class="pe-tb-icon">⏹</span> Stop
                        </span>
                        <span class="pe-toolbar-btn" title="Restart Process">
                            <span class="pe-tb-icon">🔄</span> Restart
                        </span>
                        <span class="pe-toolbar-btn pe-tb-danger" title="End Process Tree">
                            <span class="pe-tb-icon">🛑</span> End Tree
                        </span>
                        <div class="pe-toolbar-sep"></div>
                        <span class="pe-toolbar-btn" title="Set Priority">
                            <span class="pe-tb-icon">📶</span> Priority ▾
                        </span>
                        <span class="pe-toolbar-btn" title="Set Affinity">
                            <span class="pe-tb-icon">🧮</span> Affinity
                        </span>
                        <div class="pe-toolbar-sep"></div>
                        <span class="pe-toolbar-btn" title="Properties">
                            <span class="pe-tb-icon">📋</span> Properties
                        </span>
                        <span class="pe-toolbar-btn" title="Open File Location">
                            <span class="pe-tb-icon">📂</span> Location
                        </span>
                        <div class="pe-toolbar-sep"></div>
                        <span class="pe-toolbar-btn pe-tb-disabled" title="Resume (process not suspended)">
                            <span class="pe-tb-icon">▶️</span> Resume
                        </span>
                        <span class="pe-toolbar-btn" title="Suspend Process">
                            <span class="pe-tb-icon">⏸️</span> Suspend
                        </span>
                    </div>

                    <div class="pe-header-bar">
                        <div class="pe-stat">CPU: <span class="pe-stat-value">23%</span></div>
                        <div class="pe-stat">Memory: <span class="pe-stat-value">1.2 GB / 4 GB</span></div>
                        <div class="pe-stat">Processes: <span class="pe-stat-value">47</span></div>
                        <div class="pe-stat">Threads: <span class="pe-stat-value">312</span></div>
                    </div>
                    <div class="pe-table-wrap">
                        <table class="pe-table">
                            <thead>
                                <tr>
                                    <th>Process Name</th>
                                    <th>PID</th>
                                    <th>CPU</th>
                                    <th>Memory</th>
                                    <th>Status</th>
                                    <th>User</th>
                                </tr>
                            </thead>
                            <tbody>
                                <tr>
                                    <td>⚙️ kernel</td>
                                    <td class="pe-pid">0</td>
                                    <td><span class="pe-cpu-bar" style="width:4px;"></span> 0.2%</td>
                                    <td>512 KB</td>
                                    <td><span class="os-badge os-badge-teal" style="padding:0 4px;font-size:9px;">Running</span></td>
                                    <td>SYSTEM</td>
                                </tr>
                                <tr>
                                    <td>🖥️ shell.exe</td>
                                    <td class="pe-pid">4</td>
                                    <td><span class="pe-cpu-bar" style="width:20px;"></span> 5.1%</td>
                                    <td>28 MB</td>
                                    <td><span class="os-badge os-badge-teal" style="padding:0 4px;font-size:9px;">Running</span></td>
                                    <td>maahi</td>
                                </tr>
                                <tr class="pe-selected">
                                    <td>📊 procexp.exe</td>
                                    <td class="pe-pid">156</td>
                                    <td><span class="pe-cpu-bar med" style="width:36px;"></span> 12.3%</td>
                                    <td>18 MB</td>
                                    <td><span class="os-badge os-badge-teal" style="padding:0 4px;font-size:9px;">Running</span></td>
                                    <td>maahi</td>
                                </tr>
                                <tr>
                                    <td>🌐 netstack.sys</td>
                                    <td class="pe-pid">8</td>
                                    <td><span class="pe-cpu-bar" style="width:6px;"></span> 0.8%</td>
                                    <td>4 MB</td>
                                    <td><span class="os-badge os-badge-teal" style="padding:0 4px;font-size:9px;">Running</span></td>
                                    <td>SYSTEM</td>
                                </tr>
                                <tr>
                                    <td>📁 filemanager.exe</td>
                                    <td class="pe-pid">204</td>
                                    <td><span class="pe-cpu-bar" style="width:14px;"></span> 3.4%</td>
                                    <td>22 MB</td>
                                    <td><span class="os-badge os-badge-teal" style="padding:0 4px;font-size:9px;">Running</span></td>
                                    <td>maahi</td>
                                </tr>
                                <tr>
                                    <td>🔊 snddrv.sys</td>
                                    <td class="pe-pid">12</td>
                                    <td><span class="pe-cpu-bar" style="width:2px;"></span> 0.1%</td>
                                    <td>2 MB</td>
                                    <td><span class="os-badge os-badge-default" style="padding:0 4px;font-size:9px;">Idle</span></td>
                                    <td>SYSTEM</td>
                                </tr>
                                <tr>
                                    <td>💿 diskio.sys</td>
                                    <td class="pe-pid">16</td>
                                    <td><span class="pe-cpu-bar high" style="width:50px;"></span> 18.7%</td>
                                    <td>8 MB</td>
                                    <td><span class="os-badge os-badge-warning" style="padding:0 4px;font-size:9px;">Busy</span></td>
                                    <td>SYSTEM</td>
                                </tr>
                                <tr>
                                    <td>⌨️ kbddrv.sys</td>
                                    <td class="pe-pid">20</td>
                                    <td><span class="pe-cpu-bar" style="width:2px;"></span> 0.0%</td>
                                    <td>1 MB</td>
                                    <td><span class="os-badge os-badge-default" style="padding:0 4px;font-size:9px;">Idle</span></td>
                                    <td>SYSTEM</td>
                                </tr>
                            </tbody>
                        </table>

                        <!-- Right-click context menu (mockup, positioned near selected row) -->
                        <div class="pe-context-menu" style="top: 72px; right: 40px;">
                            <div class="pe-ctx-item pe-ctx-danger">
                                <span class="pe-ctx-icon">⏹</span> Stop Process
                                <span class="pe-ctx-shortcut">Del</span>
                            </div>
                            <div class="pe-ctx-item">
                                <span class="pe-ctx-icon">🔄</span> Restart Process
                                <span class="pe-ctx-shortcut">Ctrl+R</span>
                            </div>
                            <div class="pe-ctx-item pe-ctx-danger">
                                <span class="pe-ctx-icon">🛑</span> End Process Tree
                                <span class="pe-ctx-shortcut">Shift+Del</span>
                            </div>
                            <div class="pe-ctx-sep"></div>
                            <div class="pe-ctx-item">
                                <span class="pe-ctx-icon">⏸️</span> Suspend
                            </div>
                            <div class="pe-ctx-item" style="opacity:0.5;">
                                <span class="pe-ctx-icon">▶️</span> Resume
                            </div>
                            <div class="pe-ctx-sep"></div>
                            <div class="pe-ctx-item pe-ctx-sub">
                                <span class="pe-ctx-icon">📶</span> Set Priority
                            </div>
                            <div class="pe-ctx-item pe-ctx-sub">
                                <span class="pe-ctx-icon">🧮</span> Set Affinity
                            </div>
                            <div class="pe-ctx-sep"></div>
                            <div class="pe-ctx-item">
                                <span class="pe-ctx-icon">🔍</span> Search Online
                            </div>
                            <div class="pe-ctx-item">
                                <span class="pe-ctx-icon">📂</span> Open File Location
                            </div>
                            <div class="pe-ctx-item">
                                <span class="pe-ctx-icon">📋</span> Properties
                                <span class="pe-ctx-shortcut">Alt+Enter</span>
                            </div>
                        </div>
                    </div>

                    <!-- Selected-process action panel (appears when a row is selected) -->
                    <div class="pe-action-panel">
                        <span class="pe-sel-label">📊 procexp.exe (PID 156)</span>
                        <button class="os-btn os-btn-sm" style="font-size:10px;padding:1px 8px;">⏹ Stop</button>
                        <button class="os-btn os-btn-sm" style="font-size:10px;padding:1px 8px;">🔄 Restart</button>
                        <button class="os-btn os-btn-sm" style="font-size:10px;padding:1px 8px;">⏸ Suspend</button>
                        <button class="os-btn os-btn-sm" style="font-size:10px;padding:1px 8px;">📋 Properties</button>
                    </div>
                    <div class="pe-graph-area">
                        <div style="flex:1;text-align:center;">
                            <div style="font-size:10px;color:var(--os-text-secondary);margin-bottom:2px;">CPU Usage</div>
                            <div class="pe-graph-box">
                                <span class="pe-graph-label">CPU</span>
                                <span class="pe-graph-value" style="color:#4DC7A5;">23%</span>
                                <div class="pe-graph-line">
                                    <svg viewBox="0 0 200 60" preserveAspectRatio="none">
                                        <polyline fill="none" stroke="#1E8A65" stroke-width="1.5"
                                            points="0,50 20,48 40,42 60,45 80,38 100,30 120,35 140,28 160,22 180,26 200,24"/>
                                        <polyline fill="rgba(30,138,101,0.2)" stroke="none"
                                            points="0,60 0,50 20,48 40,42 60,45 80,38 100,30 120,35 140,28 160,22 180,26 200,24 200,60"/>
                                    </svg>
                                </div>
                            </div>
                        </div>
                        <div style="flex:1;text-align:center;">
                            <div style="font-size:10px;color:var(--os-text-secondary);margin-bottom:2px;">Memory Usage</div>
                            <div class="pe-graph-box">
                                <span class="pe-graph-label">MEM</span>
                                <span class="pe-graph-value" style="color:#4A7BD5;">30%</span>
                                <div class="pe-graph-line">
                                    <svg viewBox="0 0 200 60" preserveAspectRatio="none">
                                        <polyline fill="none" stroke="#2B5BB5" stroke-width="1.5"
                                            points="0,44 20,44 40,42 60,42 80,40 100,38 120,38 140,36 160,36 180,35 200,34"/>
                                        <polyline fill="rgba(43,91,181,0.2)" stroke="none"
                                            points="0,60 0,44 20,44 40,42 60,42 80,40 100,38 120,38 140,36 160,36 180,35 200,34 200,60"/>
                                    </svg>
                                </div>
                            </div>
                        </div>
                    </div>
                    <div class="os-statusbar">
                        <span class="os-statusbar-section">Processes: 47 | Threads: 312 | Uptime: 02:14:33</span>
                        <span class="os-statusbar-section">CPU: 23%</span>
                        <span class="os-statusbar-section">Mem: 30%</span>
                    </div>
                </div>
            </div>

            <!-- ===== File Manager ===== -->
            <div class="subsection-title" style="margin-top: 24px;">File Manager</div>
            <div class="preview-box" style="background: var(--os-surface-sunken); padding: 16px;">
                <div class="os-window" style="max-width: 620px;">
                    <div class="os-titlebar">
                        <span class="os-titlebar-icon">📁</span>
                        <span class="os-titlebar-text">MaahiOS File Manager — C:\Users\maahi\Documents</span>
                        <div class="os-titlebar-btns">
                            <div class="os-titlebar-btn">─</div>
                            <div class="os-titlebar-btn">□</div>
                            <div class="os-titlebar-btn">✕</div>
                        </div>
                    </div>
                    <div class="os-menubar">
                        <span class="os-menu-item"><u>F</u>ile</span>
                        <span class="os-menu-item"><u>E</u>dit</span>
                        <span class="os-menu-item"><u>V</u>iew</span>
                        <span class="os-menu-item"><u>G</u>o</span>
                        <span class="os-menu-item"><u>T</u>ools</span>
                        <span class="os-menu-item"><u>H</u>elp</span>
                    </div>
                    <div class="os-toolbar">
                        <div class="os-toolbar-btn">⬅</div>
                        <div class="os-toolbar-btn">➡</div>
                        <div class="os-toolbar-btn">⬆</div>
                        <div class="os-toolbar-btn">🏠</div>
                        <div class="os-toolbar-sep"></div>
                        <div class="os-toolbar-btn">✂</div>
                        <div class="os-toolbar-btn">📋</div>
                        <div class="os-toolbar-btn">📄</div>
                        <div class="os-toolbar-sep"></div>
                        <div class="os-toolbar-btn">🗑</div>
                        <div class="os-toolbar-sep"></div>
                        <div class="os-toolbar-btn">📊</div>
                        <div class="os-toolbar-btn">📋</div>
                    </div>
                    <div class="os-addressbar">
                        <span class="os-addressbar-label">Address</span>
                        <input type="text" class="os-addressbar-input" value="C:\Users\maahi\Documents">
                        <button class="os-btn os-btn-sm" style="min-width:auto;padding:2px 8px;">Go</button>
                    </div>
                    <div style="display:flex; margin:2px;">
                        <!-- Sidebar Tree -->
                        <div class="fm-sidebar">
                            <div class="fm-sidebar-section">Favorites</div>
                            <div class="fm-sidebar-item"><span class="fm-sidebar-icon">🏠</span> Home</div>
                            <div class="fm-sidebar-item"><span class="fm-sidebar-icon">🖥️</span> Desktop</div>
                            <div class="fm-sidebar-item fm-active"><span class="fm-sidebar-icon">📄</span> Documents</div>
                            <div class="fm-sidebar-item"><span class="fm-sidebar-icon">⬇️</span> Downloads</div>
                            <div class="fm-sidebar-item"><span class="fm-sidebar-icon">🖼️</span> Pictures</div>
                            <div class="fm-sidebar-section">Devices</div>
                            <div class="fm-sidebar-item"><span class="fm-sidebar-icon">💿</span> C: (System)</div>
                            <div class="fm-sidebar-item"><span class="fm-sidebar-icon">💿</span> D: (Data)</div>
                            <div class="fm-sidebar-item"><span class="fm-sidebar-icon">💿</span> E: (USB)</div>
                            <div class="fm-sidebar-section">Network</div>
                            <div class="fm-sidebar-item"><span class="fm-sidebar-icon">🌐</span> Network</div>
                        </div>
                        <!-- File List -->
                        <div class="fm-content">
                            <div class="fm-breadcrumb">
                                <span class="fm-breadcrumb-seg">C:</span>
                                <span class="fm-breadcrumb-sep"> ▸ </span>
                                <span class="fm-breadcrumb-seg">Users</span>
                                <span class="fm-breadcrumb-sep"> ▸ </span>
                                <span class="fm-breadcrumb-seg">maahi</span>
                                <span class="fm-breadcrumb-sep"> ▸ </span>
                                <span style="color:var(--os-text);">Documents</span>
                            </div>
                            <div style="border:2px solid;border-color:var(--bevel-dark) var(--bevel-light) var(--bevel-light) var(--bevel-dark);background:var(--os-input-bg);flex:1;overflow:auto;max-height:180px;">
                                <table class="fm-detail-table">
                                    <thead>
                                        <tr>
                                            <th>Name <span style="font-size:8px;color:var(--os-text-secondary);">▲</span></th>
                                            <th>Size</th>
                                            <th>Type</th>
                                            <th>Modified</th>
                                        </tr>
                                    </thead>
                                    <tbody>
                                        <tr>
                                            <td>📁 Projects</td>
                                            <td>—</td>
                                            <td>Folder</td>
                                            <td>2025-03-10</td>
                                        </tr>
                                        <tr>
                                            <td>📁 Sources</td>
                                            <td>—</td>
                                            <td>Folder</td>
                                            <td>2025-03-08</td>
                                        </tr>
                                        <tr>
                                            <td>📁 Backups</td>
                                            <td>—</td>
                                            <td>Folder</td>
                                            <td>2025-02-28</td>
                                        </tr>
                                        <tr class="fm-selected">
                                            <td>📄 readme.txt</td>
                                            <td>2.4 KB</td>
                                            <td>Text File</td>
                                            <td>2025-03-12</td>
                                        </tr>
                                        <tr>
                                            <td>📄 notes.md</td>
                                            <td>8.1 KB</td>
                                            <td>Markdown</td>
                                            <td>2025-03-11</td>
                                        </tr>
                                        <tr>
                                            <td>📄 budget.csv</td>
                                            <td>14 KB</td>
                                            <td>CSV File</td>
                                            <td>2025-03-05</td>
                                        </tr>
                                        <tr>
                                            <td>🖼️ diagram.bmp</td>
                                            <td>320 KB</td>
                                            <td>Bitmap Image</td>
                                            <td>2025-02-20</td>
                                        </tr>
                                        <tr>
                                            <td>📦 archive.tar</td>
                                            <td>4.2 MB</td>
                                            <td>Archive</td>
                                            <td>2025-01-15</td>
                                        </tr>
                                    </tbody>
                                </table>
                            </div>
                            <div class="fm-info-bar">
                                <span>8 items (3 folders, 5 files)</span>
                                <span>Selected: readme.txt — 2.4 KB</span>
                            </div>
                        </div>
                    </div>
                    <div class="os-statusbar">
                        <span class="os-statusbar-section">8 object(s) | 4.5 MB total</span>
                        <span class="os-statusbar-section">Free: 12.8 GB</span>
                        <span class="os-statusbar-section">C: (System)</span>
                    </div>
                </div>
            </div>

            <!-- ===== Disk Manager ===== -->
            <div class="subsection-title" style="margin-top: 24px;">Disk Manager</div>
            <div class="preview-box" style="background: var(--os-surface-sunken); padding: 16px;">
                <div class="os-window" style="max-width: 620px;">
                    <div class="os-titlebar">
                        <span class="os-titlebar-icon">💿</span>
                        <span class="os-titlebar-text">MaahiOS Disk Manager</span>
                        <div class="os-titlebar-btns">
                            <div class="os-titlebar-btn">─</div>
                            <div class="os-titlebar-btn">□</div>
                            <div class="os-titlebar-btn">✕</div>
                        </div>
                    </div>
                    <div class="os-menubar">
                        <span class="os-menu-item"><u>A</u>ction</span>
                        <span class="os-menu-item"><u>V</u>iew</span>
                        <span class="os-menu-item"><u>H</u>elp</span>
                    </div>
                    <div class="dm-split">
                        <!-- Top pane: Volume list -->
                        <div class="dm-top-pane">
                            <table class="dm-disk-table">
                                <thead>
                                    <tr>
                                        <th>Volume</th>
                                        <th>Label</th>
                                        <th>File System</th>
                                        <th>Capacity</th>
                                        <th>Free Space</th>
                                        <th>Usage</th>
                                        <th>Status</th>
                                    </tr>
                                </thead>
                                <tbody>
                                    <tr class="dm-selected">
                                        <td>C:</td>
                                        <td>System</td>
                                        <td>MaahiFS</td>
                                        <td>32 GB</td>
                                        <td>12.8 GB</td>
                                        <td>
                                            <div class="dm-usage-bar"><div class="dm-usage-fill" style="width:60%;background:var(--os-accent);"></div></div>
                                            <span style="font-size:10px;"> 60%</span>
                                        </td>
                                        <td><span class="os-badge os-badge-teal" style="padding:0 4px;font-size:9px;">Healthy</span></td>
                                    </tr>
                                    <tr>
                                        <td>D:</td>
                                        <td>Data</td>
                                        <td>MaahiFS</td>
                                        <td>128 GB</td>
                                        <td>84 GB</td>
                                        <td>
                                            <div class="dm-usage-bar"><div class="dm-usage-fill" style="width:34%;background:var(--os-teal);"></div></div>
                                            <span style="font-size:10px;"> 34%</span>
                                        </td>
                                        <td><span class="os-badge os-badge-teal" style="padding:0 4px;font-size:9px;">Healthy</span></td>
                                    </tr>
                                    <tr>
                                        <td>E:</td>
                                        <td>USB Drive</td>
                                        <td>FAT32</td>
                                        <td>16 GB</td>
                                        <td>1.2 GB</td>
                                        <td>
                                            <div class="dm-usage-bar"><div class="dm-usage-fill" style="width:92%;background:var(--os-danger);"></div></div>
                                            <span style="font-size:10px;"> 92%</span>
                                        </td>
                                        <td><span class="os-badge os-badge-warning" style="padding:0 4px;font-size:9px;">Low Space</span></td>
                                    </tr>
                                </tbody>
                            </table>
                        </div>
                        <!-- Bottom pane: Graphical partition map -->
                        <div class="dm-bottom-pane">
                            <div style="font-size:10px;font-weight:600;color:var(--os-text-secondary);margin-bottom:6px;text-transform:uppercase;letter-spacing:0.5px;">Disk Layout</div>
                            <!-- Disk 0 -->
                            <div class="dm-disk-row">
                                <div class="dm-disk-label">Disk 0<br><span style="font-weight:normal;font-size:9px;">160 GB</span></div>
                                <div class="dm-partition-bar">
                                    <div class="dm-part dm-part-primary" style="width:20%;">C: System<br>32 GB</div>
                                    <div class="dm-part dm-part-logical" style="width:75%;">D: Data<br>128 GB</div>
                                </div>
                            </div>
                            <!-- Disk 1 -->
                            <div class="dm-disk-row">
                                <div class="dm-disk-label">Disk 1<br><span style="font-weight:normal;font-size:9px;">16 GB</span></div>
                                <div class="dm-partition-bar">
                                    <div class="dm-part dm-part-primary" style="width:100%;">E: USB Drive<br>16 GB (FAT32)</div>
                                </div>
                            </div>
                            <!-- Disk 2 (unallocated example) -->
                            <div class="dm-disk-row">
                                <div class="dm-disk-label">Disk 2<br><span style="font-weight:normal;font-size:9px;">64 GB</span></div>
                                <div class="dm-partition-bar">
                                    <div class="dm-part dm-part-swap" style="width:12%;">Swap<br>8 GB</div>
                                    <div class="dm-part dm-part-free" style="width:88%;">Unallocated<br>56 GB</div>
                                </div>
                            </div>
                            <div class="dm-legend">
                                <div class="dm-legend-item"><div class="dm-legend-swatch" style="background:var(--os-accent);"></div> Primary</div>
                                <div class="dm-legend-item"><div class="dm-legend-swatch" style="background:var(--os-teal);"></div> Logical</div>
                                <div class="dm-legend-item"><div class="dm-legend-swatch" style="background:var(--os-warning);"></div> Swap</div>
                                <div class="dm-legend-item"><div class="dm-legend-swatch" style="background:var(--os-chrome-dark);"></div> Unallocated</div>
                            </div>
                        </div>
                    </div>
                    <div class="os-statusbar">
                        <span class="os-statusbar-section">3 volumes | 2 physical disks | 1 removable</span>
                        <span class="os-statusbar-section">Selected: C: (System)</span>
                    </div>
                </div>
            </div>
        </div>

        <!-- ============================================================ -->
        <!-- CSS REFERENCE -->
        <!-- ============================================================ -->
        <div class="section">
            <h2 class="section-title">CSS Variable Reference</h2>
            <div class="section-desc">Quick reference for all CSS custom properties used in MaahiOS theme.</div>
            <style>
                .os-var-table { width:100%;border-collapse:collapse;font-size:12px; }
                .os-var-table th {
                    text-align:left;padding:6px 10px;
                    background:var(--os-chrome);color:var(--os-text);
                    border-bottom:2px solid var(--bevel-dark);
                }
                .os-var-table td {
                    padding:4px 10px;
                    border-bottom:1px solid rgba(90,93,118,0.2);
                    color:var(--os-text);
                }
                .os-var-table tr:hover td { background:rgba(43,91,181,0.1); }
                .os-var-name { font-family:var(--os-font-mono);color:var(--os-teal);font-size:11px; }
                .os-var-swatch { width:20px;height:14px;border:1px solid var(--bevel-dark);display:inline-block;vertical-align:middle;margin-right:6px; }
            </style>
            <div class="preview-box" style="background:var(--os-chrome);">
                <div style="border:2px solid;border-color:var(--bevel-dark) var(--bevel-light) var(--bevel-light) var(--bevel-dark);background:var(--os-surface-sunken);overflow:auto;max-height:320px;" class="os-scrollbar-demo">
                    <table class="os-var-table">
                        <thead>
                            <tr><th>Variable</th><th>Value</th><th>Usage</th></tr>
                        </thead>
                        <tbody>
                            <tr><td class="os-var-name">--os-chrome</td><td><span class="os-var-swatch" style="background:#D8DBE8;"></span>#D8DBE8</td><td>Primary chrome surface</td></tr>
                            <tr><td class="os-var-name">--os-chrome-light</td><td><span class="os-var-swatch" style="background:#E8EAF2;"></span>#E8EAF2</td><td>Lighter chrome variant</td></tr>
                            <tr><td class="os-var-name">--os-chrome-dark</td><td><span class="os-var-swatch" style="background:#C0C4D4;"></span>#C0C4D4</td><td>Darker chrome variant</td></tr>
                            <tr><td class="os-var-name">--bevel-light</td><td><span class="os-var-swatch" style="background:#F4F5FA;"></span>#F4F5FA</td><td>3D raised edge (light)</td></tr>
                            <tr><td class="os-var-name">--bevel-dark</td><td><span class="os-var-swatch" style="background:#9498AC;"></span>#9498AC</td><td>3D raised edge (dark)</td></tr>
                            <tr><td class="os-var-name">--os-accent</td><td><span class="os-var-swatch" style="background:#2B5BB5;"></span>#2B5BB5</td><td>Primary accent (selection, active)</td></tr>
                            <tr><td class="os-var-name">--os-accent-light</td><td><span class="os-var-swatch" style="background:#4A7BD5;"></span>#4A7BD5</td><td>Lighter accent variant</td></tr>
                            <tr><td class="os-var-name">--os-teal</td><td><span class="os-var-swatch" style="background:#1E8A65;"></span>#1E8A65</td><td>Secondary accent / success</td></tr>
                            <tr><td class="os-var-name">--os-cyan</td><td><span class="os-var-swatch" style="background:#18A080;"></span>#18A080</td><td>Highlight / glow</td></tr>
                            <tr><td class="os-var-name">--os-surface</td><td><span class="os-var-swatch" style="background:#FFFFFF;"></span>#FFFFFF</td><td>Window body surface</td></tr>
                            <tr><td class="os-var-name">--os-surface-sunken</td><td><span class="os-var-swatch" style="background:#C8CBD8;"></span>#C8CBD8</td><td>Sunken/inset surfaces</td></tr>
                            <tr><td class="os-var-name">--os-input-bg</td><td><span class="os-var-swatch" style="background:#FFFFFF;"></span>#FFFFFF</td><td>Input field background</td></tr>
                            <tr><td class="os-var-name">--os-text</td><td><span class="os-var-swatch" style="background:#1A1A2E;"></span>#1A1A2E</td><td>Primary text</td></tr>
                            <tr><td class="os-var-name">--os-text-secondary</td><td><span class="os-var-swatch" style="background:#5A5D76;"></span>#5A5D76</td><td>Secondary/muted text</td></tr>
                            <tr><td class="os-var-name">--os-danger</td><td><span class="os-var-swatch" style="background:#DC3545;"></span>#DC3545</td><td>Error state</td></tr>
                            <tr><td class="os-var-name">--os-warning</td><td><span class="os-var-swatch" style="background:#E8A317;"></span>#E8A317</td><td>Warning state</td></tr>
                            <tr><td class="os-var-name">--titlebar-start</td><td><span class="os-var-swatch" style="background:#1B3F8B;"></span>#1B3F8B</td><td>Titlebar gradient start</td></tr>
                            <tr><td class="os-var-name">--titlebar-end</td><td><span class="os-var-swatch" style="background:#2B5BB5;"></span>#2B5BB5</td><td>Titlebar gradient end</td></tr>
                        </tbody>
                    </table>
                </div>
            </div>
        </div>

    </div>
</body>
</html>
