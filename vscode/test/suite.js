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

    await vscode.commands.executeCommand("peripheralPretender.stop");
    assert.strictEqual(api.port(), undefined);
    await until("QEMU gone after stop", () => qemusUnder(qemuDir).length === 0,
                10000);
    console.log("[suite] stopped; no QEMU left");
}

module.exports = { run };
