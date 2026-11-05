import pathlib
import struct
import sys

data = pathlib.Path(sys.argv[1]).read_bytes()
expected_name = sys.argv[2].encode() + b"\0"
assert data[:4] == b"MTLB"
public_offset, public_size = struct.unpack_from("<QQ", data, 40)
assert public_offset + public_size <= len(data)

blocks = []
cursor = public_offset
while cursor < public_offset + public_size:
    (length,) = struct.unpack_from("<I", data, cursor)
    end = cursor + 4 + length
    assert end <= public_offset + public_size
    tags = {}
    tag_cursor = cursor + 4
    while tag_cursor < end:
        tag = data[tag_cursor:tag_cursor + 4]
        tag_cursor += 4
        if tag == b"ENDT":
            break
        (tag_size,) = struct.unpack_from("<H", data, tag_cursor)
        tag_cursor += 2
        tags.setdefault(tag, []).append(data[tag_cursor:tag_cursor + tag_size])
        tag_cursor += tag_size
    assert tag_cursor == end
    blocks.append(tags)
    cursor = end

assert len(blocks) == 2, len(blocks)
assert len(blocks[0].get(b"CNST", [])) == 1, blocks[0]
cnst = blocks[0][b"CNST"][0]
assert struct.unpack_from("<H", cnst)[0] == 1
assert cnst[2:].startswith(expected_name), cnst
assert b"CNST" not in blocks[1], blocks[1]
