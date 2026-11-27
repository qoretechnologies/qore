"""Check a synthetic Gmail or SMTP capture using an independent MIME parser."""
import base64
import sys
from email import policy
from email.parser import BytesParser

encoded, layout = sys.argv[1:]
message = BytesParser(policy=policy.default).parsebytes(base64.urlsafe_b64decode(encoded + "==="))
assert all(not part.defects for part in message.walk())
body = message
if layout == "mixed":
    assert message.get_content_type() == "multipart/mixed"
    body, attachment = message.iter_parts()
    assert attachment.get_content_disposition() == "attachment"
    assert attachment.get_filename() == "sample.bin"
    assert attachment.get_payload(decode=True) == bytes.fromhex("00ff")
else:
    assert layout == "alternative"
assert body.get_content_type() == "multipart/alternative"
plain, html = body.iter_parts()
assert plain.get_content_type() == "text/plain"
assert html.get_content_type() == "text/html"
assert plain["Content-Transfer-Encoding"] == "quoted-printable"
assert html["Content-Transfer-Encoding"] == "base64"
assert plain.get_content() == "Plain café"
assert html.get_content() == "<p>Růžová</p>"
