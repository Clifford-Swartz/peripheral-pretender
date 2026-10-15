/*
 * Runs inside VS Code (test/run.js): open the MC00 board, check the page
 * answers, run the bundled blinky through the firmware API, check the
 * twin's QEMU runs from the bundle, stop, and check nothing is left.
 */

const assert = require("assert");
const cp = require("child_process");
const fs = require("fs");
const path = require("path");
const vscode = require("vscode");

const ID = "cliffordswartz.peripheral-pretender";

/* Running QEMUs started from @dir (Windows) */
function qemusUnder(dir) {
    const out = cp.execFileSync("powershell", ["-NoProfile", "-Command",
        "Get-Process qemu-system-* -ErrorAction SilentlyContinue | " +
        "ForEach-Object { $_.Path }"], { encoding: "utf8" });
    return out.split(/\r?\n/).filter(
        (p) => p && path.resolve(p).toLowerCase()
            .startsWith(path.resolve(dir).toLowerCase()));
}

async function until(what, fn, ms) {
    const deadline = Date.now() + ms;
    for (;;) {
        if (await fn()) {
            return;
        }
        if (Date.now() > deadline) {
            throw new Error(`timed out: ${what}`);
        }
        await new Promise((r) => setTimeout(r, 250));
    }
}

/*
 * Debug on the twin: a "twin" launch stops at main (the C/C++ extension's
 * gdb session on the bundled gdb-multiarch), a step moves on, and when the
 * session ends the firmware runs on undebugged
 */
async function debugBlinky(elf, qemuDir) {
    const stops = [];
    const tracker = vscode.debug.registerDebugAdapterTrackerFactory("cppdbg", {
        createDebugAdapterTracker: () => ({
            onDidSendMessage: (m) => {
                if (m.type === "event" && m.event === "stopped") {
                    stops.push(m.body);
                }
            },
        }),
    });
    try {
        const started = await vscode.debug.startDebugging(undefined, {
            type: "twin", request: "launch", name: "blinky on the twin",
            program: elf, board: "ev15n46a",
        });
        assert.ok(started, "debug session did not start");
        await until("stopped at main", () => stops.length > 0, 60000);
        const session = vscode.debug.activeDebugSession;
        const frame = async () => {
            const st = await session.customRequest("stackTrace", {
                threadId: stops[stops.length - 1].threadId, levels: 1 });
            return st.stackFrames[0];
        };
        const top = await frame();
        assert.strictEqual(top.name.replace(/\(.*$/, "").trim(), "main", top.name);
        assert.match(top.source.path, /blinky\.c$/);
        console.log(`[suite] debugger stopped in ${top.name} at ` +
                    `${path.basename(top.source.path)}:${top.line}`);
        const before = stops.length;
        await session.customRequest("next", { threadId: stops[before - 1].threadId });
        await until("step done", () => stops.length > before, 20000);
        const next = await frame();
        assert.ok(next.line > top.line, `step went from ${top.line} to ${next.line}`);
        console.log(`[suite] stepped to line ${next.line}`);
        await vscode.debug.stopDebugging(session);
        await until("debug session over",
                    () => vscode.debug.activeDebugSession === undefined, 20000);
        await until("firmware runs on after debugging",
                    () => qemusUnder(qemuDir).length > 0, 20000);
        console.log("[suite] session ended; the twin runs the firmware on");
    } finally {
        tracker.dispose();
    }
}

async function run() {
    const ext = vscode.extensions.getExtension(ID);
    assert.ok(ext, `${ID} not loaded`);
    const cfg = vscode.workspace.getConfiguration("peripheralPretender");
    await cfg.update("board", "ev15n46a", vscode.ConfigurationTarget.Global);
    await cfg.update("watch", false, vscode.ConfigurationTarget.Global);
    const api = await ext.activate();
    const qemuDir = path.join(ext.extensionPath, "engine", "qemu");

    await vscode.commands.executeCommand("peripheralPretender.open");
    const port = api.port();
    assert.ok(port, "engine not started");
    const page = await fetch(`http://127.0.0.1:${port}/`);
    assert.strictEqual(page.status, 200);
    assert.match(await page.text(), /<html/i);
    console.log(`[suite] engine on ${port}, page served`);

    /* PP_SUITE_HOLD=<file>: keep the panel open until <file> exists, for
       poking at the webview from outside (test/run.js PP_CDP_PORT) */
    if (process.env.PP_SUITE_HOLD) {
        console.log("[suite] holding");
        await until("hold released",
                    () => fs.existsSync(process.env.PP_SUITE_HOLD), 600000);
    }

    const elf = path.join(ext.extensionPath, "engine", "app", "firmware",
                          "hello", "build", "pic32cm_mc00_cpro", "blinky.elf");
    const r = await fetch(`http://127.0.0.1:${port}/api/firmware`, {
        method: "POST", body: fs.readFileSync(elf),
    });
    assert.strictEqual(r.status, 200, await r.text());
    await until("bundled QEMU running", () => qemusUnder(qemuDir).length > 0,
                15000);
    console.log(`[suite] firmware posted; ${qemusUnder(qemuDir).length} ` +
                "bundled QEMU running");

    await debugBlinky(elf, qemuDir);

    await vscode.commands.executeCommand("peripheralPretender.stop");
    assert.strictEqual(api.port(), undefined);
    await until("QEMU gone after stop", () => qemusUnder(qemuDir).length === 0,
                10000);
    console.log("[suite] stopped; no QEMU left");
}

module.exports = { run };
