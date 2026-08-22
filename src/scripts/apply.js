import fs from 'fs';
import path from 'path';
import { JSDOM } from 'jsdom';

const args = process.argv.slice(2);
let manifestPath = '';
let inputPath = '';
let outputPath = '';

for (let i = 0; i < args.length; i++) {
  if (args[i] === '--input' && i + 1 < args.length) {
    inputPath = args[++i];
  } else if (args[i] === '-o' && i + 1 < args.length) {
    outputPath = args[++i];
  } else if (!args[i].startsWith('-')) {
    manifestPath = args[i];
  }
}

if (!manifestPath || !inputPath || !outputPath) {
  console.error('[Error] Usage: node apply.js <manifest.json> --input <input.html> -o <output.html>');
  process.exit(1);
}

try {
  const manifest = JSON.parse(fs.readFileSync(manifestPath, 'utf8'));
  const html = fs.readFileSync(inputPath, 'utf8');

  const dom = new JSDOM(html);
  const document = dom.window.document;

  // 1. Inject CSS Variables and custom styles
  if (manifest.theme) {
    const head = document.head || document.documentElement;
    const styleEl = document.createElement('style');
    styleEl.id = 'spm-injected-theme-styles';

    let cssVars = '';
    if (manifest.theme.cssVariables) {
      cssVars = `:root {\n`;
      for (const [name, val] of Object.entries(manifest.theme.cssVariables)) {
        cssVars += `  ${name}: ${val};\n`;
      }
      cssVars += `}\n`;
    }

    const customStyles = manifest.theme.customStyles || '';
    styleEl.textContent = `${cssVars}\n${customStyles}`;
    head.appendChild(styleEl);
  }

  // 2. Process Components (selectors)
  if (manifest.components) {
    for (const comp of manifest.components) {
      const els = document.querySelectorAll(comp.selector);
      els.forEach((el) => {
        if (comp.action === 'hide') {
          el.style.display = 'none';
          el.setAttribute('data-spm-hidden', 'true');
        } else if (comp.action === 'replace') {
          // Replace with modern component placeholder
          const placeholder = document.createElement('spm-modern-component');
          placeholder.setAttribute('name', comp.name);
          placeholder.setAttribute('selector', comp.selector);
          el.replaceWith(placeholder);
        }
      });
    }
  }

  // 3. Process Reconstructs
  if (manifest.reconstructs) {
    for (const recon of manifest.reconstructs) {
      const container = document.querySelector(recon.containerSelector);
      if (container) {
        // Hide existing children
        Array.from(container.children).forEach((child) => {
          if (child.tagName !== 'SCRIPT' && child.tagName !== 'STYLE') {
            child.style.display = 'none';
          }
        });

        // Insert reconstruct host placeholder before container so hiding container doesn't hide host
        const host = document.createElement('spm-reconstruct-host');
        host.setAttribute('layout', recon.layoutComponent);
        host.setAttribute('selector', recon.containerSelector);
        if (container.parentNode) {
          container.parentNode.insertBefore(host, container);
        } else {
          container.appendChild(host);
        }
      }
    }
  }

  // 4. Inject Standalone Modernizer Engine Script
  const body = document.body || document.documentElement;
  
  const manifestScript = document.createElement('script');
  manifestScript.textContent = `window.__spm_dev_manifest = ${JSON.stringify(manifest)};`;
  body.appendChild(manifestScript);

  const engineScript = document.createElement('script');
  engineScript.src = 'file:///home/watashi/Projects/extension/dist/src/content/index.iife.js';
  body.appendChild(engineScript);

  fs.writeFileSync(outputPath, dom.serialize(), 'utf8');
  console.log(`[Apply] Successfully applied manifest transformations to ${outputPath}`);

} catch (e) {
  console.error('[Error] Apply failed: ' + e.message);
  process.exit(1);
}
