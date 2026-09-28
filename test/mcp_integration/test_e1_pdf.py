"""E1: text of an annotated PDF background."""

import tempfile

import xoai


def make_pdf(path, lines):
    """Writes a one-page A4 PDF with the given text lines (Helvetica 18pt, 30pt apart, from y=100)."""
    content = "BT /F1 18 Tf " + " ".join(
        f"1 0 0 1 72 {842 - 100 - 30 * i} Tm ({line}) Tj" for i, line in enumerate(lines)) + " ET"
    objects = [
        "<< /Type /Catalog /Pages 2 0 R >>",
        "<< /Type /Pages /Kids [3 0 R] /Count 1 >>",
        "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 595 842] /Contents 4 0 R "
        "/Resources << /Font << /F1 5 0 R >> >> >>",
        f"<< /Length {len(content)} >>\nstream\n{content}\nendstream",
        "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>",
    ]
    out = "%PDF-1.4\n"
    offsets = []
    for i, obj in enumerate(objects, 1):
        offsets.append(len(out))
        out += f"{i} 0 obj\n{obj}\nendobj\n"
    xref = len(out)
    out += f"xref\n0 {len(objects) + 1}\n0000000000 65535 f \n"
    out += "".join(f"{o:010d} 00000 n \n" for o in offsets)
    out += f"trailer\n<< /Size {len(objects) + 1} /Root 1 0 R >>\nstartxref\n{xref}\n%%EOF\n"
    with open(path, "w") as f:
        f.write(out)


PDF = tempfile.NamedTemporaryFile(prefix="xoai-", suffix=".pdf", delete=False).name
make_pdf(PDF, ["Hello xournalai", "Second line of text"])
APP_ARGS = [PDF]


def test_pdf_text(app):
    c = app.client()
    info = c.call("doc_info")
    assert info["pages"][0]["background"]["type"] == "pdf"
    r = c.call("pdf_text", page=1)
    assert "Hello xournalai" in r["text"] and "Second line" in r["text"]


def test_pdf_text_positions_and_region(app):
    c = app.client()
    r = c.call("pdf_text", page=1, positions=True)
    lines = [l for l in r["lines"] if l["text"].strip()]
    assert len(lines) >= 2
    first = next(l for l in lines if "Hello" in l["text"])
    x, y, w, h = first["bbox"]
    assert 60 < x < 90 and 70 < y < 110, first["bbox"]  # 100pt from the top, top-left origin
    top = c.call("pdf_text", page=1, region=[0, 0, 595, 110])
    assert "Hello" in top["text"] and "Second" not in top["text"]
