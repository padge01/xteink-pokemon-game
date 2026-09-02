/** Remove complete XML comments without scanning protected constructs. */
function stripComments(xml) {
  const encoder = new TextEncoder();
  const out = [];
  let cursor = 0;
  let count = 0;
  let bytes = 0;

  const copyProtected = (start, opener, closer) => {
    const close = xml.indexOf(closer, start + opener.length);
    const end = close < 0 ? xml.length : close + closer.length;
    out.push(xml.slice(start, end));
    return end;
  };

  const copyDoctype = (start) => {
    let quote = "";
    let subsetDepth = 0;

    for (let i = start + 9; i < xml.length; i++) {
      const ch = xml[i];
      if (quote) {
        if (ch === quote) quote = "";
        continue;
      }
      if (xml.startsWith("<!--", i)) {
        const close = xml.indexOf("-->", i + 4);
        if (close < 0) {
          out.push(xml.slice(start));
          return xml.length;
        }
        i = close + 2;
        continue;
      }
      if (xml.startsWith("<?", i)) {
        const close = xml.indexOf("?>", i + 2);
        if (close < 0) {
          out.push(xml.slice(start));
          return xml.length;
        }
        i = close + 1;
        continue;
      }
      if (ch === '"' || ch === "'") {
        quote = ch;
      } else if (ch === "[") {
        subsetDepth++;
      } else if (ch === "]" && subsetDepth > 0) {
        subsetDepth--;
      } else if (ch === ">" && subsetDepth === 0) {
        const end = i + 1;
        out.push(xml.slice(start, end));
        return end;
      }
    }

    out.push(xml.slice(start));
    return xml.length;
  };

  while (cursor < xml.length) {
    const next = xml.indexOf("<", cursor);
    if (next < 0) {
      out.push(xml.slice(cursor));
      break;
    }
    if (next > cursor) out.push(xml.slice(cursor, next));

    if (xml.startsWith("<!--", next)) {
      const close = xml.indexOf("-->", next + 4);
      if (close < 0) {
        out.push(xml.slice(next));
        break;
      }
      const end = close + 3;
      count++;
      bytes += encoder.encode(xml.slice(next, end)).length;
      cursor = end;
      continue;
    }

    if (xml.startsWith("<![CDATA[", next)) {
      cursor = copyProtected(next, "<![CDATA[", "]]>");
      continue;
    }

    if (xml.startsWith("<?", next)) {
      cursor = copyProtected(next, "<?", "?>");
      continue;
    }

    if (xml.startsWith("<!DOCTYPE", next)) {
      cursor = copyDoctype(next);
      continue;
    }

    out.push("<");
    cursor = next + 1;
  }

  return { text: out.join(""), count, bytes };
}
