/*
 * Peripheral Pretender for VS Code: the twin's board view in a panel.
 *
 * The extension is a thin shell. The engine (tools/pp_web, Python, with
 * the pp-qemu binaries) runs as a child process on a free loopback port,
 * and the panel shows its page. The platform VSIX bundles the engine
 * (engine/python, engine/app, engine/qemu: scripts/build_vsix.py), so
 * installing the extension is all a user does. For development,
 * peripheralPretender.engineRoot runs a checkout with a system Python.
 *
 * Copyright (c) 2026 Clifford Swartz.
 */

import * as cp from "child_process";
import * as fs from "fs";
import * as net from "net";
import * as path from "path";
import * as vscode from "vscode";
import { registerDebug } from "./debug";
import { registerSidebar } from "./sidebar";

interface Engine {
    proc: cp.ChildProcess;
    port: number;
    board: string;
}

export interface Board {
    id: string;
    name: string;
    machine: string;            /* QEMU machine: says Arm or AVR */
}

let engine: Engine | undefined;
let panel: vscode.WebviewPanel | undefined;
let log: vscode.LogOutputChannel;     /* also saved under the exthost logs */
let status: vscode.StatusBarItem;

/* Where the engine lives and how to start it */
interface Layout {
    python: string;
    root: string;               /* the project root: tools/, devices/ */
    env: NodeJS.ProcessEnv;
}

function layout(ctx: vscode.ExtensionContext): Layout {
    const cfg = vscode.workspace.getConfiguration("peripheralPretender");
    const env: NodeJS.ProcessEnv = { ...process.env };
    const dev = cfg.get<string>("engineRoot", "");

    env.PP_WEB_BENCH_DIR = path.join(ctx.globalStorageUri.fsPath, "bench");
    env.PYTHONUNBUFFERED = "1";
    if (dev) {
        const py = cfg.get<string>("python", "") ||
            (process.platform === "win32" ? "py" : "python3");
        return { python: py, root: dev, env };
    }
    const base = path.join(ctx.extensionPath, "engine");
    const qemu = path.join(base, "qemu");
    const exe = process.platform === "win32" ? ".exe" : "";

    env.PP_QEMU = path.join(qemu, `qemu-system-arm${exe}`);
    env.PP_QEMU_AVR = path.join(qemu, `qemu-system-avr${exe}`);
    env.PP_MSYS2_BIN = qemu;    /* the DLLs sit beside the binaries */
    env.PYTHONNOUSERSITE = "1";
    return {
        python: path.join(base, "python",
                          process.platform === "win32" ? "python.exe"
                                                       : "bin/python3"),
        root: path.join(base, "app"),
        env,
    };
}

/* The bundled gdb-multiarch (Arm and AVR), or a configured one */
function gdbPath(ctx: vscode.ExtensionContext): string {
    const cfg = vscode.workspace.getConfiguration("peripheralPretender");
    const exe = process.platform === "win32" ? ".exe" : "";

    return cfg.get<string>("gdbPath", "") ||
        (cfg.get<string>("engineRoot", "") ?
            (process.platform === "win32" ?
                "C:/msys64/ucrt64/bin/gdb-multiarch.exe" : "gdb-multiarch") :
            path.join(ctx.extensionPath, "engine", "gdb", "bin",
                      `gdb-multiarch${exe}`));
}

function boards(root: string): Board[] {
    const dir = path.join(root, "tools", "pp_web", "boards");
    const out: Board[] = [];

    for (const id of fs.existsSync(dir) ? fs.readdirSync(dir) : []) {
        const file = path.join(dir, id, "board.json");
        if (!fs.existsSync(file)) {
            continue;
        }
        try {
            const doc = JSON.parse(fs.readFileSync(file, "utf8"));
            out.push({ id: doc.id || id, name: doc.name || id,
                       machine: doc.machine || "" });
        } catch {
            /* not a board */
        }
    }
    return out.sort((a, b) => a.name.localeCompare(b.name));
}

function freePort(): Promise<number> {
    return new Promise((resolve, reject) => {
        const srv = net.createServer();
        srv.once("error", reject);
        srv.listen(0, "127.0.0.1", () => {
            const port = (srv.address() as net.AddressInfo).port;
            srv.close(() => resolve(port));
        });
    });
}

/* Resolves once the engine answers HTTP, rejects if it exits first */
async function waitReady(e: Engine, timeoutMs: number): Promise<void> {
    const deadline = Date.now() + timeoutMs;

    while (Date.now() < deadline) {
        if (e.proc.exitCode !== null) {
            throw new Error(`engine exited (${e.proc.exitCode})`);
        }
        try {
            const r = await fetch(`http://127.0.0.1:${e.port}/`);
            if (r.ok) {
                return;
            }
        } catch {
            /* not listening yet */
        }
        await new Promise((r) => setTimeout(r, 200));
    }
    throw new Error("engine did not start in time");
}

async function pickBoard(root: string, ask = false,
                         only?: (b: Board) => boolean)
                         : Promise<string | undefined> {
    const cfg = vscode.workspace.getConfiguration("peripheralPretender");
    const fixed = cfg.get<string>("board", "");
    const all = boards(root).filter(only || (() => true));

    if (fixed && !ask && all.some((b) => b.id === fixed)) {
        return fixed;
    }
    if (all.length === 1 && only) {
        return all[0].id;
    }
    const items = all.map((b) => ({ label: b.name, id: b.id }));
    if (!items.length) {
        vscode.window.showErrorMessage(
            `Peripheral Pretender: no boards found under ${root}`);
        return undefined;
    }
    const pick = await vscode.window.showQuickPick(items, {
        placeHolder: "Board to run the twin of",
    });
    return pick?.id;
}

function stopEngine(): void {
    if (engine) {
        const e = engine;
        engine = undefined;
        e.proc.removeAllListeners("exit");
        if (process.platform === "win32" && e.proc.pid) {
            /* The whole tree: the engine's QEMU would outlive it */
            cp.spawnSync("taskkill", ["/T", "/F", "/PID", String(e.proc.pid)],
                         { windowsHide: true });
        } else {
            e.proc.kill();
        }
    }
    refresh();
}

/* Engine started or stopped: the status bar and the sidebar follow */
const changed = new vscode.EventEmitter<void>();

function refresh(): void {
    if (engine) {
        status.text = `$(circuit-board) Twin: ${engine.board}`;
        status.tooltip = `Peripheral Pretender: ${engine.board} on port ` +
            `${engine.port}. Click to switch boards.`;
        status.command = "peripheralPretender.switchBoard";
    } else {
        status.text = "$(circuit-board) Twin";
        status.tooltip = "Peripheral Pretender: open a board";
        status.command = "peripheralPretender.open";
    }
    status.show();
    changed.fire();
}

async function startEngine(ctx: vscode.ExtensionContext,
                           board: string): Promise<Engine> {
    const lay = layout(ctx);
    const cfg = vscode.workspace.getConfiguration("peripheralPretender");
    const port = await freePort();
    const args = ["-m", "tools.pp_web", "--port", String(port),
                  "--board", board];
    const folder = vscode.workspace.workspaceFolders?.[0]?.uri.fsPath;

    if (folder && cfg.get<boolean>("watch", true)) {
        args.push("--watch", folder);
    }
    if (!fs.existsSync(lay.root)) {
        throw new Error(`no engine at ${lay.root}`);
    }
    fs.mkdirSync(lay.env.PP_WEB_BENCH_DIR as string, { recursive: true });
    log.appendLine(`[twin] ${lay.python} ${args.join(" ")} (in ${lay.root})`);
    const proc = cp.spawn(lay.python, args, {
        cwd: lay.root, env: lay.env, windowsHide: true,
    });
    proc.stdout?.on("data", (d) => log.append(d.toString()));
    proc.stderr?.on("data", (d) => log.append(d.toString()));
    proc.on("error", (err) => log.appendLine(`[twin] ${err.message}`));
    const e: Engine = { proc, port, board };
    proc.on("exit", (code) => {
        log.appendLine(`[twin] engine exited (${code})`);
        if (engine === e) {
            engine = undefined;
            refresh();
        }
    });
    await waitReady(e, 30000);
    return e;
}

/*
 * The engine's page itself, loaded straight into the webview with <base>
 * pointing at the engine. Not in an iframe: native dropdowns, keyboard
 * focus and drag and drop misbehave in an iframe nested in a webview.
 */
export function injectPage(html: string, port: number): string {
    const engine = `http://127.0.0.1:${port}`;
    const head = `<base href="${engine}/">
<meta http-equiv="Content-Security-Policy" content="default-src 'none'; ` +
        `script-src ${engine} 'unsafe-inline'; style-src ${engine} 'unsafe-inline'; ` +
        `img-src ${engine} data: blob:; font-src ${engine} data:; ` +
        `media-src ${engine} blob:; ` +
        `connect-src ${engine} ws://127.0.0.1:${port};">`;

    return html.replace(/<head>/i, `<head>\n${head}`);
}

async function pageHtml(port: number): Promise<string> {
    const r = await fetch(`http://127.0.0.1:${port}/`);
    return injectPage(await r.text(), port);
}

async function showPanel(port: number, board: string): Promise<void> {
    if (!panel) {
        panel = vscode.window.createWebviewPanel(
            "peripheralPretender", "Twin", vscode.ViewColumn.Beside, {
                enableScripts: true,
                retainContextWhenHidden: true,
                portMapping: [{ webviewPort: port, extensionHostPort: port }],
            });
        panel.onDidDispose(() => {
            panel = undefined;
        });
    }
    panel.title = `Twin: ${board}`;
    panel.webview.html = await pageHtml(port);
    panel.reveal();
}

async function open(ctx: vscode.ExtensionContext,
                    choice?: string): Promise<boolean> {
    if (engine && (!choice || choice === engine.board)) {
        await showPanel(engine.port, engine.board);
        return true;
    }
    stopEngine();
    const board = choice || await pickBoard(layout(ctx).root);
    if (!board) {
        return false;
    }
    try {
        engine = await vscode.window.withProgress({
            location: vscode.ProgressLocation.Notification,
            title: "Starting the twin",
        }, () => startEngine(ctx, board));
    } catch (err) {
        stopEngine();
        log.show(true);
        vscode.window.showErrorMessage(
            `Peripheral Pretender: ${(err as Error).message}`);
        return false;
    }
    refresh();
    await showPanel(engine.port, board);
    return true;
}

async function runElf(ctx: vscode.ExtensionContext,
                      uri?: vscode.Uri): Promise<void> {
    if (!uri) {
        const picked = await vscode.window.showOpenDialog({
            filters: { "ELF": ["elf"] }, canSelectMany: false,
        });
        uri = picked?.[0];
    }
    if (!uri || !(await open(ctx)) || !engine) {
        return;
    }
    const data = await vscode.workspace.fs.readFile(uri);
    const r = await fetch(`http://127.0.0.1:${engine.port}/api/firmware`, {
        method: "POST", body: data,
    });
    if (!r.ok) {
        vscode.window.showErrorMessage(
            `Peripheral Pretender: ${await r.text()}`);
    }
}

/* For tests and other extensions: the running engine's port, if any */
export interface Api {
    port(): number | undefined;
}

export function activate(ctx: vscode.ExtensionContext): Api {
    log = vscode.window.createOutputChannel("Peripheral Pretender",
                                            { log: true });
    status = vscode.window.createStatusBarItem(
        vscode.StatusBarAlignment.Left, 50);
    ctx.subscriptions.push(
        log, status,
        vscode.commands.registerCommand("peripheralPretender.open",
                                        (board?: string) => open(ctx, board)),
        vscode.commands.registerCommand(
            "peripheralPretender.switchBoard", async () => {
                const board = await pickBoard(layout(ctx).root, true);
                if (board) {
                    await open(ctx, board);
                }
            }),
        vscode.commands.registerCommand("peripheralPretender.runElf",
                                        (uri?: vscode.Uri) => runElf(ctx, uri)),
        vscode.commands.registerCommand("peripheralPretender.stop", () => {
            stopEngine();
            panel?.dispose();
        }),
        vscode.commands.registerCommand("peripheralPretender.showLog",
                                        () => log.show()),
        { dispose: stopEngine }, changed);
    registerSidebar(ctx, {
        boards: () => boards(layout(ctx).root),
        board: () => engine?.board,
        changed: changed.event,
    });
    refresh();
    registerDebug(ctx, {
        log,
        port: () => engine?.port,
        board: () => engine?.board,
        boards: () => boards(layout(ctx).root),
        pickBoard: (only) => pickBoard(layout(ctx).root, false, only),
        open: (board) => open(ctx, board),
        gdb: () => gdbPath(ctx),
    });
    return { port: () => engine?.port };
}

export function deactivate(): void {
    stopEngine();
}
