# Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
"""Smoke test: start ``py -m tools.pp_mcp`` over stdio and call its tools."""

import os
import sys
from pathlib import Path

import anyio
import pytest
from mcp import ClientSession, StdioServerParameters
from mcp.client.stdio import get_default_environment, stdio_client

PROJECT_ROOT = Path(__file__).resolve().parents[2]
SESSION_TIMEOUT_S = 120


def server_params(qemu_path=None):
    env = get_default_environment()
    for name in ("PP_QEMU", "PP_MSYS2_BIN"):
        if name in os.environ:
            env[name] = os.environ[name]
    if qemu_path is not None:
        env["PP_QEMU"] = str(qemu_path)
    return StdioServerParameters(command=sys.executable,
                                 args=["-m", "tools.pp_mcp"],
                                 cwd=PROJECT_ROOT, env=env)


def field(obj, name):
    """Read a result field: mcp 1.x spells it camelCase (structuredContent),
    mcp 2.x snake_case (structured_content)."""
    snake = "".join("_" + c.lower() if c.isupper() else c for c in name)
    return getattr(obj, snake) if hasattr(obj, snake) else getattr(obj, name)


def call_server(calls, qemu_path=None):
    """Run [(tool, arguments)] in one server session; return the tool list
    and the results."""
    async def session():
        with anyio.fail_after(SESSION_TIMEOUT_S):
            async with stdio_client(server_params(qemu_path)) as streams:
                async with ClientSession(*streams) as client:
                    await client.initialize()
                    tools = await client.list_tools()
                    results = [await client.call_tool(name, args)
                               for name, args in calls]
                    return tools, results
    return anyio.run(session)


def test_tools_are_listed():
    tools, _ = call_server([])
    by_name = {t.name: t for t in tools.tools}
    assert set(by_name) == {"twin_run", "twin_machines", "bench_status",
                            "bench_add_board", "bench_remove_board",
                            "bench_template", "bench_plug", "bench_run",
                            "bench_press", "bench_control", "bench_uart",
                            "bench_timeline", "bench_faults", "bench_record",
                            "bench_replay", "bench_recordings"}
    schema = field(by_name["twin_run"], "inputSchema")
    assert list(schema["properties"])[:4] == ["elf_path", "timeout_s",
                                              "input_text", "icount_shift"]
    assert {"machine_options", "input_after", "trace"} <= \
        set(schema["properties"])
    assert schema["required"] == ["elf_path"]
    output = field(by_name["twin_run"], "outputSchema")
    assert {"uart_truncated", "uart_length", "detail"} <= \
        set(output["properties"])


def test_uart_is_bounded_head_and_tail():
    from tools.pp_mcp.server import bound_uart
    assert bound_uart("short", limit=10) == ("short", False)
    text = "".join(f"line {i}\n" for i in range(20000))
    cut, truncated = bound_uart(text, limit=1000)
    assert truncated and len(cut) < 1100
    assert cut.startswith("line 0\n") and cut.endswith("line 19999\n")
    assert f"[... {len(text) - 1000} characters omitted ...]" in cut


def test_machine_options_take_properties_only():
    from tools.pp_mcp.server import ToolError, machine_extra_args
    assert machine_extra_args(None) == []
    assert machine_extra_args("sw0-pressed=on") == \
        ["-machine", "sw0-pressed=on"]
    for bad in ("mps2-an385", "type=mps2-an385", "mps2-an385,x=1"):
        with pytest.raises(ToolError, match="only key=value"):
            machine_extra_args(bad)


@pytest.mark.qemu
def test_twin_run_and_machines(qemu_path, mps2_images):
    relative_echo = mps2_images["echo"].relative_to(PROJECT_ROOT)
    _, (machines, exited, echoed, missing, traced, burst) = call_server([
        ("twin_machines", {}),
        ("twin_run", {"elf_path": str(mps2_images["exit"]),
                      "machine": "mps2-an385"}),
        ("twin_run", {"elf_path": str(relative_echo),
                      "machine": "mps2-an385", "timeout_s": 20,
                      "input_text": "from mcp\r\nexit 9\r\n",
                      "input_after": "ready"}),
        ("twin_run", {"elf_path": "no/such.elf", "machine": "mps2-an385"}),
        ("twin_run", {"elf_path": str(mps2_images["exit"]),
                      "machine": "mps2-an385",
                      "trace": ["cmsdk_apb_uart_tx", "no_such_event_x"]}),
        ("twin_run", {"elf_path": str(mps2_images["burst"]),
                      "machine": "mps2-an385", "timeout_s": 30}),
    ], qemu_path)

    info = field(machines, "structuredContent")
    assert info["default_machine"] == "pic32cm-jh01-cpro"
    assert "mps2-an385" in {m["name"] for m in info["machines"]}

    result = field(exited, "structuredContent")
    assert not field(exited, "isError")
    assert (result["exit_code"], result["status"]) == (0, "exit")
    assert result["uart"].endswith("PP:EXIT 0\n")
    assert result["unimplemented_summary"]["total"] == 6
    assert set(result) >= {"exit_code", "uart", "unimplemented_summary",
                           "qemu_stderr_tail", "duration_s"}

    result = field(echoed, "structuredContent")
    assert result["exit_code"] == 9
    assert "echo: from mcp\n" in result["uart"]

    assert field(missing, "isError")
    assert "ELF not found" in missing.content[0].text

    result = field(traced, "structuredContent")
    assert result["trace"]["events"] == {"cmsdk_apb_uart_tx": 55}
    assert len(result["warnings"]) == 1 and \
        "no_such_event_x" in result["warnings"][0]
    assert (result["uart_truncated"], result["uart_length"]) == \
        (False, len(result["uart"]))

    result = field(burst, "structuredContent")      # ~150 KB transcript
    assert result["exit_code"] == 0 and result["uart_truncated"]
    assert result["uart_length"] > 140_000 and len(result["uart"]) < 70_000
    assert result["uart"].endswith("PP:EXIT 0\n")
