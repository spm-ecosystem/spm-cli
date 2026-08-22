import fs from 'fs';
import path from 'path';
import { JSDOM } from 'jsdom';

const args = process.argv.slice(2);
let manifestPath = '';
let snapshotPath = '';
let isJson = false;

for (let i = 0; i < args.length; i++) {
  if (args[i] === '--against' && i + 1 < args.length) {
    snapshotPath = args[++i];
  } else if (args[i] === '--json' || args[i] === '-j') {
    isJson = true;
  } else if (!args[i].startsWith('-')) {
    manifestPath = args[i];
  }
}

if (!manifestPath || !snapshotPath) {
  console.error('[Error] Usage: node validate.js <manifest.json> --against <snapshot.html> [--json]');
  process.exit(1);
}

try {
  const manifest = JSON.parse(fs.readFileSync(manifestPath, 'utf8'));
  const html = fs.readFileSync(snapshotPath, 'utf8');

  const dom = new JSDOM(html);
  const document = dom.window.document;

  const results = {
    reconstructs: [],
    components: []
  };

  function parseCleanNumber(val) {
    if (val === null || val === undefined) return null;
    const raw = String(val).trim();
    if (!raw) return null;

    let multiplier = 1;
    let workStr = raw;
    const suffixMatch = raw.match(/([0-9.,]+)\s*([kKmMbB])\b/);
    if (suffixMatch) {
      const unit = suffixMatch[2].toLowerCase();
      if (unit === 'k') multiplier = 1000;
      else if (unit === 'm') multiplier = 1000000;
      else if (unit === 'b') multiplier = 1000000000;
      workStr = suffixMatch[1];
    }

    const isNegative = /^\s*-\s*[\$€R£\d]/.test(raw) || /[\$€R£\s]-\s*[\d.]/.test(raw);

    let numStr = workStr.replace(/[^0-9.,]/g, '');
    if (!numStr) return null;

    if (numStr.includes(',') && numStr.includes('.')) {
      if (numStr.lastIndexOf(',') > numStr.lastIndexOf('.')) {
        numStr = numStr.replace(/\./g, '').replace(',', '.');
      } else {
        numStr = numStr.replace(/,/g, '');
      }
    } else if (numStr.includes(',')) {
      const parts = numStr.split(',');
      if (parts.length === 2 && parts[1].length <= 2) {
        numStr = parts[0] + '.' + parts[1];
      } else {
        numStr = numStr.replace(/,/g, '');
      }
    }

    const numVal = parseFloat(numStr);
    if (isNaN(numVal)) return null;

    const result = (isNegative ? -1 : 1) * numVal * multiplier;
    return String(result);
  }

  // Replicate browser runtime extractValue 100% exactly
  function extractValue(element, queryRule) {
    const parts = queryRule.split('|').map((s) => s.trim());
    const selector = parts[0];
    const extractor = parts[1];

    if (!selector || !extractor) return null;

    const targetEl = selector === 'self' ? element : element.querySelector(selector);
    if (!targetEl) return null;

    let val = null;

    if (extractor.startsWith('attr:')) {
      const attrName = extractor.substring(5);
      val = targetEl.getAttribute(attrName);
    } else if (extractor === 'text') {
      val = targetEl.textContent;
    } else if (extractor === 'html') {
      val = targetEl.innerHTML;
    } else if (extractor === 'hrefOrOnclick') {
      const href = targetEl.getAttribute('href');
      if (href && href !== '#' && !href.startsWith('javascript:')) {
        val = href;
      } else {
        const onclick = targetEl.getAttribute('onclick') || '';
        const match = onclick.match(/(?:document|window)\.location(?:\.href)?\s*=\s*['"]([^'"]+)['"]/i) || onclick.match(/document\.location\s*=\s*['"]([^'"]+)['"]/i);
        if (match) {
          val = match[1];
        } else {
          val = href || null;
        }
      }
    } else if (extractor === 'selector') {
      let spmId = targetEl.getAttribute('data-spm-id');
      if (!spmId) {
        spmId = 'spm-id-' + Math.random().toString(36).substring(2, 9);
        targetEl.setAttribute('data-spm-id', spmId);
      }
      val = `[data-spm-id="${spmId}"]`;
    } else if (extractor === 'nextSiblingText') {
      const next = targetEl.nextElementSibling;
      val = next ? next.textContent : null;
    } else if (extractor === 'hiddenInputs') {
      const inputs = targetEl.querySelectorAll('input[type="hidden"]');
      const list = [];
      inputs.forEach((input) => {
        const name = input.getAttribute('name');
        const value = input.getAttribute('value') || '';
        if (name) {
          list.push({ name, value });
        }
      });
      val = JSON.stringify(list);
    } else {
      return null;
    }

    // Process subsequent pipes
    for (let i = 2; i < parts.length; i++) {
      if (val === null || val === undefined) return null;
      const pipe = parts[i];
      if (pipe === 'number') {
        const trimmed = val.trim();
        const numVal = Number(trimmed);
        val = isNaN(numVal) || trimmed === '' ? null : String(numVal);
      } else if (pipe === 'cleanNumber') {
        val = parseCleanNumber(val);
      } else if (pipe === 'split') {
        val = JSON.stringify(val.split(/\s+/).filter(item => item.length > 0));
      } else if (pipe.startsWith('split:')) {
        const delim = pipe.substring(6);
        val = JSON.stringify(val.split(delim).map(s => s.trim()));
      } else {
        return null;
      }
    }

    return val;
  }

  // Validate reconstructs
  if (manifest.reconstructs) {
    for (const recon of manifest.reconstructs) {
      const container = document.querySelector(recon.containerSelector);
      const reconResult = {
        containerSelector: recon.containerSelector,
        status: container ? 'PASS' : 'FAIL',
        matched: container ? 1 : 0,
        children: [],
        binds: []
      };

      if (container) {
        // Validate static props
        // Validate binds
        if (recon.propsMap) {
          for (const [key, rule] of Object.entries(recon.propsMap)) {
            const extracted = extractValue(container, rule);
            reconResult.binds.push({
              key,
              rule,
              status: extracted !== null ? 'PASS' : 'FAIL',
              value: extracted
            });
          }
        }

        // Validate children
        if (recon.children) {
          for (const child of recon.children) {
            const childItems = child.scope === 'document' 
              ? document.querySelectorAll(child.selector)
              : container.querySelectorAll(child.selector);
            
            const childResult = {
              name: child.name,
              selector: child.selector,
              scope: child.scope || 'container',
              matched: childItems.length,
              status: childItems.length > 0 ? 'PASS' : 'FAIL',
              itemsBinds: []
            };

            childItems.forEach((item, index) => {
              const itemBinds = [];
              if (child.propsMap) {
                for (const [key, rule] of Object.entries(child.propsMap)) {
                  const extracted = extractValue(item, rule);
                  itemBinds.push({
                    key,
                    rule,
                    status: extracted !== null ? 'PASS' : 'FAIL',
                    value: extracted
                  });
                }
              }
              childResult.itemsBinds.push(itemBinds);
            });

            reconResult.children.push(childResult);
          }
        }
      }

      results.reconstructs.push(reconResult);
    }
  }

  // Validate components (selectors)
  if (manifest.components) {
    for (const comp of manifest.components) {
      const els = document.querySelectorAll(comp.selector);
      const compResult = {
        selector: comp.selector,
        action: comp.action,
        matched: els.length,
        status: (comp.action === 'hide' || els.length > 0) ? 'PASS' : 'FAIL',
        binds: []
      };

      if (els.length > 0 && comp.propsMap) {
        // Evaluate binds on the first matched element as validation proof
        for (const [key, rule] of Object.entries(comp.propsMap)) {
          const extracted = extractValue(els[0], rule);
          compResult.binds.push({
            key,
            rule,
            status: extracted !== null ? 'PASS' : 'FAIL',
            value: extracted
          });
        }
      }

      results.components.push(compResult);
    }
  }

  if (isJson) {
    console.log(JSON.stringify(results, null, 2));
  } else {
    // Human readable output
    console.log(`===========================================`);
    console.log(`SPM Validate Results`);
    console.log(`===========================================`);
    
    let totalPass = 0;
    let totalFail = 0;

    for (const recon of results.reconstructs) {
      const icon = recon.status === 'PASS' ? '✅' : '❌';
      console.log(`${icon} Reconstruct: ${recon.containerSelector} -> ${recon.status} (${recon.matched} match)`);
      if (recon.status === 'PASS') totalPass++; else totalFail++;

      for (const bind of recon.binds) {
        const bIcon = bind.status === 'PASS' ? '  ├─ ✅' : '  ├─ ❌';
        console.log(`${bIcon} Bind "${bind.key}": "${bind.rule}" -> "${bind.value}"`);
      }

      for (const child of recon.children) {
        const cIcon = child.status === 'PASS' ? '  ├─ ✅' : '  ├─ ❌';
        console.log(`${cIcon} Child "${child.name}": "${child.selector}" -> ${child.status} (${child.matched} matches)`);
        if (child.status === 'PASS') totalPass++; else totalFail++;
      }
    }

    for (const comp of results.components) {
      const icon = comp.status === 'PASS' ? '✅' : '❌';
      console.log(`${icon} Component Selector: "${comp.selector}" [${comp.action}] -> ${comp.status} (${comp.matched} matches)`);
      if (comp.status === 'PASS') totalPass++; else totalFail++;
    }

    console.log(`===========================================`);
    console.log(`Summary: ${totalPass} Passed, ${totalFail} Failed`);
    console.log(`===========================================`);
    if (totalFail > 0) {
      process.exit(1);
    }
  }

} catch (e) {
  console.error('[Error] Validation failed: ' + e.message);
  process.exit(1);
}
