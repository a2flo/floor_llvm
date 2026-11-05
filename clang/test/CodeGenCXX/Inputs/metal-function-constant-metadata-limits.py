import pathlib
import struct
import sys

if sys.argv[1] == "generate":
    source, destination, length = sys.argv[2:]
    text = pathlib.Path(source).read_text()
    pathlib.Path(destination).write_text(text.replace("@LONG_NAME@", "c" * int(length)))
elif sys.argv[1] == "invalid":
    source, destination, kind = sys.argv[2:]
    text = pathlib.Path(source).read_text()
    original = next(line for line in text.splitlines() if line.startswith("!2 ="))
    replacements = {
        "empty": "!2 = !{}",
        "short": "!2 = !{i32 addrspace(2)* @fc}",
        "wide": original.replace("i32 0, i1 true", "i128 18446744073709551616, i1 true"),
    }
    pathlib.Path(destination).write_text(text.replace(original, replacements[kind]))
elif sys.argv[1] == "check":
    data = pathlib.Path(sys.argv[2]).read_bytes()
    assert data[:4] == b"MTLB", "expected a complete metallib"
    offset = data.index(b"CNST")
    length, count = struct.unpack_from("<HH", data, offset + 4)
    assert length == 65535 and count == 1, (length, count)
    end = offset + 6 + length
    assert data[end:end + 4] == b"ENDT", "CNST length must reach the following tag"
else:
    raise AssertionError("unknown test mode")
