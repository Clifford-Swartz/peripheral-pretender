# Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
"""PP:EXIT detection in a chunked UART stream."""

import pytest

from tools.pp.protocol import ExitScanner, process_exit_status


def feed_all(*chunks):
    scanner = ExitScanner()
    results = [scanner.feed(c) for c in chunks]
    return scanner, results


@pytest.mark.parametrize("terminator", [b"\r\n", b"\n", b"\r"])
def test_exit_line_with_any_terminator(terminator):
    scanner, results = feed_all(b"hello\r\nPP:EXIT 0" + terminator)
    assert results == [0]
    assert scanner.exit_code == 0


def test_code_split_across_chunks_is_read_whole():
    scanner, results = feed_all(b"PP:EX", b"IT 1", b"2", b"\r\n")
    assert results == [None, None, None, 12]


def test_unterminated_exit_line_is_pending_only():
    scanner, results = feed_all(b"boot\r\nPP:EXIT 5")
    assert results == [None]
    assert scanner.exit_code is None
    assert scanner.pending_exit() == 5


def test_text_before_marker_and_trailing_blanks():
    _, results = feed_all(b"[test] done PP:EXIT 7 \t\n")
    assert results == [7]


def test_negative_code():
    _, results = feed_all(b"PP:EXIT -1\n")
    assert results == [-1]


@pytest.mark.parametrize("line", [b"PP:EXIT\n", b"PP:EXIT abc\n",
                                  b"PP:EXIT 1 2\n", b"PP:EXIT 3x\n",
                                  b"pp:exit 0\n"])
def test_non_exit_lines(line):
    scanner, results = feed_all(line)
    assert results == [None]
    assert scanner.pending_exit() is None


def test_first_exit_line_wins():
    scanner, results = feed_all(b"PP:EXIT 1\nPP:EXIT 2\n", b"PP:EXIT 3\n")
    assert results == [1, 1]


def test_long_unterminated_line_keeps_its_tail():
    scanner, _ = feed_all(b"x" * 5000, b"PP:EXIT 9")
    assert scanner.pending_exit() == 9


def test_pending_cleared_by_more_text():
    scanner, _ = feed_all(b"PP:EXIT 9", b" not really")
    assert scanner.pending_exit() is None


@pytest.mark.parametrize("value, status", [(0, 0), (3, 3), (255, 255),
                                           (256, 255), (-1, 255)])
def test_process_exit_status(value, status):
    assert process_exit_status(value) == status
