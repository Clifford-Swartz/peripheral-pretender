# Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
"""GDB/MI output parser."""
from harness.gdbmi import mi_quote, parse_line


def test_result_record_with_token_and_escapes():
    rec = parse_line('12^error,msg="No symbol \\"x\\" in current context."\r\n')
    assert (rec.kind, rec.token, rec.cls) == ("result", 12, "error")
    assert rec.payload == {"msg": 'No symbol "x" in current context.'}


def test_stopped_record_with_nested_values():
    rec = parse_line('*stopped,reason="breakpoint-hit",bkptno="1",frame={addr="0x0000001c",'
                     'func="step",args=[{name="n",value="1"}]},thread-id="1"')
    assert (rec.kind, rec.cls) == ("exec", "stopped")
    assert rec.payload["frame"]["func"] == "step"
    assert rec.payload["frame"]["args"] == [{"name": "n", "value": "1"}]


def test_memory_read_result():
    rec = parse_line('^done,memory=[{begin="0x20000000",offset="0x00000000",'
                     'end="0x20000004",contents="01000000"}]')
    assert rec.payload["memory"][0]["contents"] == "01000000"


def test_list_of_results_drops_names():
    rec = parse_line('^done,register-names=["r0","r1"],x=[a="1",b="2"]')
    assert rec.payload == {"register-names": ["r0", "r1"], "x": ["1", "2"]}


def test_stream_records_and_octal_utf8():
    assert parse_line('~"hello\\n"').payload == "hello\n"
    assert parse_line('~"\\302\\260C"').payload == "\u00b0C"
    assert parse_line('&"warning\\n"').kind == "log"


def test_prompt_and_blank_lines():
    assert parse_line("(gdb) \n") is None
    assert parse_line("\n") is None
    assert parse_line("garbage").kind == "raw"


def test_mi_quote():
    assert mi_quote('C:\\a "b"') == '"C:\\\\a \\"b\\""'
