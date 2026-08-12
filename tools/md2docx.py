"""Converte un sottoinsieme di Markdown in .docx, senza dipendenze esterne.

Un .docx e' uno zip con tre parti obbligatorie: [Content_Types].xml, _rels/.rels
e word/document.xml. Qui ne generiamo uno minimale ma valido, con stili definiti
in word/styles.xml, cosi' titoli, tabelle e codice restano leggibili in Word.

Supporta: # .. ####, paragrafi, liste - e 1., tabelle |...|, blocchi ```,
citazioni >, separatori ---, e inline **grassetto**, `codice`, [testo](link).
"""
import html
import re
import sys
import zipfile

W = 'http://schemas.openxmlformats.org/wordprocessingml/2006/main'


def esc(s):
    return html.escape(s, quote=False)


MONO = '<w:rFonts w:ascii="Consolas" w:hAnsi="Consolas"/><w:sz w:val="18"/>'


def _run(props, body):
    rpr = f'<w:rPr>{props}</w:rPr>' if props else ''
    return f'<w:r>{rpr}<w:t xml:space="preserve">{esc(body)}</w:t></w:r>'


def _inline(text, bold=False, ital=False):
    """Ricorsiva, perche' nel testo capita **grassetto con `codice` dentro**."""
    base = ('<w:b/>' if bold else '') + ('<w:i/>' if ital else '')
    out = []
    # Lo split tiene i delimitatori, cosi' possiamo alternare i formati.
    # Il grassetto viene prima del corsivo: ** deve vincere su *.
    for part in re.split(r'(\*\*.+?\*\*|\*[^*]+\*|`[^`]+`)', text):
        if not part:
            continue
        if part.startswith('**') and part.endswith('**') and len(part) > 4:
            out.append(_inline(part[2:-2], bold=True, ital=ital))
        elif part.startswith('*') and part.endswith('*') and len(part) > 2:
            out.append(_inline(part[1:-1], bold=bold, ital=True))
        elif part.startswith('`') and part.endswith('`'):
            out.append(_run(MONO + base, part[1:-1]))
        else:
            out.append(_run(base, part))
    return ''.join(out)


def runs(text):
    """Testo inline -> sequenza di <w:r>, gestendo **bold**, `code`, [x](y)."""
    text = re.sub(r'\[([^\]]+)\]\([^)]+\)', r'\1', text)
    return _inline(text) or '<w:r><w:t/></w:r>'


def para(text, style=None, ind=False):
    props = ''
    if style:
        props += f'<w:pStyle w:val="{style}"/>'
    if ind:
        props += '<w:ind w:left="360"/>'
    ppr = f'<w:pPr>{props}</w:pPr>' if props else ''
    return f'<w:p>{ppr}{runs(text)}</w:p>'


def code_para(line):
    return ('<w:p><w:pPr><w:pStyle w:val="Code"/></w:pPr>'
            f'<w:r><w:t xml:space="preserve">{esc(line)}</w:t></w:r></w:p>')


def row(cells, header=False):
    tcs = []
    for c in cells:
        shd = '<w:shd w:val="clear" w:fill="E8E8E8"/>' if header else ''
        body = f'<w:p>{runs(("**%s**" % c) if header and c else c)}</w:p>'
        tcs.append(f'<w:tc><w:tcPr>{shd}</w:tcPr>{body}</w:tc>')
    return f'<w:tr>{"".join(tcs)}</w:tr>'


def table(rows):
    borders = ''.join(
        f'<w:{e} w:val="single" w:sz="4" w:color="999999"/>'
        for e in ('top', 'left', 'bottom', 'right', 'insideH', 'insideV'))
    body = row(rows[0], True) + ''.join(row(r) for r in rows[1:])
    return (f'<w:tbl><w:tblPr><w:tblW w:w="5000" w:type="pct"/>'
            f'<w:tblBorders>{borders}</w:tblBorders></w:tblPr>{body}</w:tbl>'
            '<w:p/>')


def split_row(line):
    return [c.strip() for c in line.strip().strip('|').split('|')]


# Costrutti che iniziano un blocco nuovo, e quindi interrompono un paragrafo.
# La recinzione ``` ammette spazi davanti: puo' stare dentro una voce di elenco.
SPECIAL = re.compile(r'^(#{1,4}\s|\s*[-*]\s|\s*\d+\.\s|>|\||\s*```|---$|\*\*\*$)')


def unwrap(lines):
    """Riunisce le righe di uno stesso paragrafo.

    Il sorgente e' formattato a 80 colonne, ma in Word un paragrafo deve essere
    un paragrafo: altrimenti il testo va a capo dove capita invece che al bordo
    della pagina, e soprattutto un **grassetto** spezzato su due righe non
    verrebbe riconosciuto. Dentro i blocchi di codice non si tocca niente.
    """
    out, i, in_code = [], 0, False
    while i < len(lines):
        ln = lines[i]
        # Le recinzioni ``` possono essere rientrate, se il blocco sta dentro
        # una voce di elenco: quindi si guarda la riga senza spazi iniziali.
        if ln.strip().startswith('```'):
            in_code = not in_code
            out.append(ln)
            i += 1
            continue
        if in_code or not ln.strip() or ln.strip() in ('---', '***'):
            out.append(ln)
            i += 1
            continue
        if ln.startswith('>'):
            # Le citazioni sono righe consecutive che iniziano con '>': vanno
            # riunite come qualunque altro paragrafo, altrimenti un grassetto a
            # cavallo di due righe non viene riconosciuto.
            buf = []
            while i < len(lines) and lines[i].startswith('>'):
                buf.append(lines[i].lstrip('> ').strip())
                i += 1
            out.append('> ' + ' '.join(x for x in buf if x))
            continue
        # Una riga di continuazione non e' vuota e non apre un nuovo costrutto.
        j = i + 1
        buf = ln.rstrip()
        while (j < len(lines) and lines[j].strip()
               and not SPECIAL.match(lines[j]) and not ln.startswith('|')
               and not ln.startswith('```')):
            buf += ' ' + lines[j].strip()
            j += 1
        out.append(buf)
        i = j
    return out


def convert(md):
    out, lines, i = [], unwrap(md.split('\n')), 0
    while i < len(lines):
        ln = lines[i]

        if ln.strip().startswith('```'):
            # Se la recinzione era rientrata (blocco dentro un elenco), si
            # toglie lo stesso rientro dal corpo, conservando pero' quello
            # relativo interno al codice.
            pad = len(ln) - len(ln.lstrip())
            i += 1
            while i < len(lines) and not lines[i].strip().startswith('```'):
                body = lines[i]
                out.append(code_para(body[pad:] if body[:pad].isspace() else body.lstrip()))
                i += 1
            i += 1
            continue

        if ln.startswith('|') and i + 1 < len(lines) and re.match(r'^\|[\s:|-]+\|$', lines[i + 1]):
            rows = [split_row(ln)]
            i += 2
            while i < len(lines) and lines[i].startswith('|'):
                rows.append(split_row(lines[i]))
                i += 1
            out.append(table(rows))
            continue

        m = re.match(r'^(#{1,4})\s+(.*)', ln)
        if m:
            out.append(para(m.group(2), f'Heading{len(m.group(1))}'))
        elif re.match(r'^\s*[-*]\s+', ln):
            # Il pallino e' un carattere letterale: una vera lista puntata di
            # Word richiederebbe numbering.xml, che per questo documento non
            # vale la complessita'.
            out.append(para('• ' + re.sub(r'^\s*[-*]\s+', '', ln),
                            'ListParagraph', ind=True))
        elif re.match(r'^\s*\d+\.\s+', ln):
            out.append(para(ln.strip(), 'ListParagraph', ind=True))
        elif ln.startswith('>'):
            out.append(para(ln.lstrip('> ').strip(), 'Quote', ind=True))
        elif ln.strip() in ('---', '***'):
            out.append('<w:p><w:pPr><w:pBdr><w:bottom w:val="single" w:sz="6" '
                       'w:color="AAAAAA"/></w:pBdr></w:pPr></w:p>')
        elif ln.strip():
            out.append(para(ln.strip()))
        else:
            out.append('<w:p/>')
        i += 1
    return ''.join(out)


STYLES = """<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<w:styles xmlns:w="{w}">
<w:docDefaults><w:rPrDefault><w:rPr>
<w:rFonts w:ascii="Calibri" w:hAnsi="Calibri"/><w:sz w:val="22"/>
</w:rPr></w:rPrDefault></w:docDefaults>
<w:style w:type="paragraph" w:styleId="Heading1"><w:name w:val="heading 1"/>
<w:pPr><w:spacing w:before="360" w:after="140"/><w:outlineLvl w:val="0"/></w:pPr>
<w:rPr><w:b/><w:sz w:val="36"/><w:color w:val="1F3864"/></w:rPr></w:style>
<w:style w:type="paragraph" w:styleId="Heading2"><w:name w:val="heading 2"/>
<w:pPr><w:spacing w:before="320" w:after="120"/><w:outlineLvl w:val="1"/></w:pPr>
<w:rPr><w:b/><w:sz w:val="30"/><w:color w:val="2E5496"/></w:rPr></w:style>
<w:style w:type="paragraph" w:styleId="Heading3"><w:name w:val="heading 3"/>
<w:pPr><w:spacing w:before="260" w:after="100"/><w:outlineLvl w:val="2"/></w:pPr>
<w:rPr><w:b/><w:sz w:val="26"/><w:color w:val="2E5496"/></w:rPr></w:style>
<w:style w:type="paragraph" w:styleId="Heading4"><w:name w:val="heading 4"/>
<w:pPr><w:spacing w:before="220" w:after="80"/><w:outlineLvl w:val="3"/></w:pPr>
<w:rPr><w:b/><w:i/><w:sz w:val="23"/></w:rPr></w:style>
<w:style w:type="paragraph" w:styleId="ListParagraph"><w:name w:val="List Paragraph"/>
<w:pPr><w:spacing w:after="60"/></w:pPr></w:style>
<w:style w:type="paragraph" w:styleId="Quote"><w:name w:val="Quote"/>
<w:pPr><w:spacing w:before="120" w:after="120"/>
<w:pBdr><w:left w:val="single" w:sz="18" w:color="B0B0B0" w:space="8"/></w:pBdr></w:pPr>
<w:rPr><w:i/></w:rPr></w:style>
<w:style w:type="paragraph" w:styleId="Code"><w:name w:val="Code"/>
<w:pPr><w:spacing w:after="0"/><w:ind w:left="360"/><w:shd w:val="clear" w:fill="F2F2F2"/></w:pPr>
<w:rPr><w:rFonts w:ascii="Consolas" w:hAnsi="Consolas"/><w:sz w:val="18"/></w:rPr></w:style>
</w:styles>""".replace('{w}', W)

CONTENT_TYPES = """<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">
<Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/>
<Default Extension="xml" ContentType="application/xml"/>
<Override PartName="/word/document.xml" ContentType="application/vnd.openxmlformats-officedocument.wordprocessingml.document.main+xml"/>
<Override PartName="/word/styles.xml" ContentType="application/vnd.openxmlformats-officedocument.wordprocessingml.styles+xml"/>
</Types>"""

RELS = """<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">
<Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument" Target="word/document.xml"/>
</Relationships>"""

DOC_RELS = """<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">
<Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/styles" Target="styles.xml"/>
</Relationships>"""


def main(src, dst):
    with open(src, encoding='utf-8') as f:
        body = convert(f.read())
    doc = ('<?xml version="1.0" encoding="UTF-8" standalone="yes"?>'
           f'<w:document xmlns:w="{W}"><w:body>{body}'
           '<w:sectPr><w:pgSz w:w="11906" w:h="16838"/>'
           '<w:pgMar w:top="1134" w:right="1134" w:bottom="1134" w:left="1134"/>'
           '</w:sectPr></w:body></w:document>')
    with zipfile.ZipFile(dst, 'w', zipfile.ZIP_DEFLATED) as z:
        z.writestr('[Content_Types].xml', CONTENT_TYPES)
        z.writestr('_rels/.rels', RELS)
        z.writestr('word/_rels/document.xml.rels', DOC_RELS)
        z.writestr('word/styles.xml', STYLES)
        z.writestr('word/document.xml', doc)
    print('scritto', dst)


if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2])
