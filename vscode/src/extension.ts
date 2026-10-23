/*
 * Peripheral Pretender for VS Code: the twin's board view in a panel.
 *
 * The extension is a thin shell. The engine (tools/pp_web, Python, with
 * the pp-qemu binaries) runs as a child process on a free loopback port,
 * and the panel shows its page: the workspace, with the board, Click
 * boards, parts and firmware it had last time (or an empty bench with
 * templates). Picking a board switches the running engine to it. The platform VSIX bundles the engine
 * (engine/python, engine/app, engine/qemu: scripts/build_vsix.py), so
 * installing the extension is all a user does. For development,
 * peripheralPretender.engineRoot runs a checkout with a system Python.
 *
 * Copyright (c) 2026 Clifford Swartz.
 */

import * as cp from "child_process";
import * as fs from "fs";
import * as net from "net";
import * as os from "os";
import * as path from "path";
import * as vscode from "vscode";
import { boardArch, elfArch, registerDebug } from "./debug";
import { registerSidebar } from "./sidebar";

interface Engine {
    proc?: cp.ChildProcess;     /* none: an engine already running, adopted */
    port: number;
    board?: string;             /* as the engine last said (/api/state) */
    name?: string;
    poll?: NodeJS.Timeout;
}

export interface Board {
    id: string;
    name: string;
    machine: string;            /* QEMU machine: says Arm or AVR */
}

let engine: Engine | undefined;
let starting: Promise<boolean> | undefined;   /* an engine on its way */
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
        if (e.proc && e.proc.exitCode !== null) {
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
        clearInterval(e.poll);
        if (!e.proc) {
            /* Adopted: whoever started it stops it */
        } else if (process.platform === "win32" && e.proc.pid) {
            /* The whole tree: the engine's QEMU would outlive it */
            e.proc.removeAllListeners("exit");
            cp.spawnSync("taskkill", ["/T", "/F", "/PID", String(e.proc.pid)],
                         { windowsHide: true });
        } else {
            e.proc.removeAllListeners("exit");
            e.proc.kill();
        }
    }
    refresh();
}

/*
 * An engine already running on this workspace (the MCP server starts one
 * when an assistant uses the bench first): each engine says where it
 * listens in ~/.pp_web/engines
 */
async function runningEngine(lay: Layout): Promise<Engine | undefined> {
    const dir = path.join(os.homedir(), ".pp_web", "engines");
    const mine = path.resolve(lay.env.PP_WEB_BENCH_DIR as string,
                              "workspace.json").toLowerCase();

    for (const f of fs.existsSync(dir) ? fs.readdirSync(dir) : []) {
        try {
            const info = JSON.parse(fs.readFileSync(path.join(dir, f), "utf8"));
            if (path.resolve(info.workspace).toLowerCase() !== mine) {
                continue;
            }
            const r = await fetch(`http://127.0.0.1:${info.port}/api/state`);
            if (r.ok) {
                return { port: info.port };
            }
        } catch {
            /* gone, or not an engine file */
        }
    }
    return undefined;
}

/* Engine started or stopped: the status bar and the sidebar follow */
const changed = new vscode.EventEmitter<void>();

function refresh(): void {
    if (engine) {
        const name = engine.name?.replace(/\s*\(.*$/, "") || "empty bench";
        status.text = `$(circuit-board) Twin: ${name}`;
        status.tooltip = `Peripheral Pretender: ${engine.name || "no board"} ` +
            `on port ${engine.port}. Click to show the bench.`;
        status.command = "peripheralPretender.open";
    } else {
        status.text = "$(circuit-board) Twin";
        status.tooltip = "Peripheral Pretender: open a board";
        status.command = "peripheralPretender.open";
    }
    status.show();
    changed.fire();
}

/* Which board the engine has, for the status bar and the sidebar */
async function poll(e: Engine): Promise<void> {
    try {
        const r = await fetch(`http://127.0.0.1:${e.port}/api/state`);
        const s = await r.json() as { board: string | null; name: string | null };
        if (s.board !== (e.board ?? null) || s.name !== (e.name ?? null)) {
            e.board = s.board ?? undefined;
            e.name = s.name ?? undefined;
            if (panel) {
                panel.title = `Twin: ${e.name?.replace(/\s*\(.*$/, "") || "bench"}`;
            }
            refresh();
        }
    } catch {
        /* restarting */
    }
}

/* The engine on the last workspace, or on @board when given */
async function startEngine(ctx: vscode.ExtensionContext,
                           board?: string): Promise<Engine> {
    const lay = layout(ctx);
    const cfg = vscode.workspace.getConfiguration("peripheralPretender");
    const port = await freePort();
    const args = ["-m", "tools.pp_web", "--port", String(port),
                  ...(board ? ["--board", board] : [])];
    const folder = vscode.workspace.workspaceFolders?.[0]?.uri.fsPath;

    if (folder && cfg.get<boolean>("watch", true)) {
        args.push("--watch", folder);
    }
    if (!fs.existsSync(lay.root)) {
        throw new Error(`no engine at ${lay.root}`);
    }
    const running = await runningEngine(lay);
    if (running) {
        log.appendLine(`[twin] the engine on port ${running.port} runs this ` +
                       "workspace already: showing it");
        await poll(running);
        running.poll = setInterval(() => poll(running), 2000);
        return running;
    }
    fs.mkdirSync(lay.env.PP_WEB_BENCH_DIR as string, { recursive: true });
    log.appendLine(`[twin] ${lay.python} ${args.join(" ")} (in ${lay.root})`);
    const proc = cp.spawn(lay.python, args, {
        cwd: lay.root, env: lay.env, windowsHide: true,
    });
    proc.stdout?.on("data", (d) => log.append(d.toString()));
    proc.stderr?.on("data", (d) => log.append(d.toString()));
    proc.on("error", (err) => log.appendLine(`[twin] ${err.message}`));
    const e: Engine = { proc, port };
    proc.on("exit", (code) => {
        log.appendLine(`[twin] engine exited (${code})`);
        if (engine === e) {
            engine = undefined;
            refresh();
        }
    });
    await waitReady(e, 30000);
    await poll(e);
    e.poll = setInterval(() => poll(e), 2000);
    return e;
}

/* Switch the running engine to @board */
async function switchTo(e: Engine, board: string): Promise<void> {
    const r = await fetch(`http://127.0.0.1:${e.port}/api/board`, {
        method: "POST", headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ board }),
    });
    if (!r.ok) {
        throw new Error(await r.text());
    }
    await poll(e);
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

async function showPanel(port: number, board?: string): Promise<void> {
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
    panel.title = `Twin: ${board?.replace(/\s*\(.*$/, "") || "bench"}`;
    panel.webview.html = await pageHtml(port);
    panel.reveal();
}

/*
 * Show the bench: the running engine (switched to @choice, if given), or
 * a new one on the last workspace (the "board" setting, or @choice, picks
 * a board instead)
 */
async function open(ctx: vscode.ExtensionContext,
                    choice?: string): Promise<boolean> {
    if (starting) {
        /* The sidebar and a command can both ask at once: one engine */
        await starting;
    }
    if (engine) {
        if (choice && choice !== engine.board) {
            try {
                await switchTo(engine, choice);
            } catch (err) {
                vscode.window.showErrorMessage(
                    `Peripheral Pretender: ${(err as Error).message}`);
                return false;
            }
        }
        await showPanel(engine.port, engine.name);
        return true;
    }
    const cfg = vscode.workspace.getConfiguration("peripheralPretender");
    const board = choice || cfg.get<string>("board", "") || undefined;
    starting = launch(ctx, board);
    try {
        return await starting;
    } finally {
        starting = undefined;
    }
}

async function launch(ctx: vscode.ExtensionContext,
                      board?: string): Promise<boolean> {
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
    await showPanel(engine.port, engine.name);
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
    /* An empty bench, or a board of another architecture: pick one */
    const arch = elfArch(uri.fsPath);
    const here = boards(layout(ctx).root).find((b) => b.id === engine?.board);
    if (!here || (arch && boardArch(here) !== arch)) {
        const board = await pickBoard(layout(ctx).root, true,
                                      (b) => !arch || boardArch(b) === arch);
        if (!board || !(await open(ctx, board)) || !engine) {
            return;
        }
    }
    const data = await vscode.workspace.fs.readFile(uri);
    const name = encodeURIComponent(path.basename(uri.fsPath));
    const r = await fetch(`http://127.0.0.1:${engine.port}/api/firmware?name=${name}`, {
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
        running: () => !!engine,
        panelOpen: () => !!panel,
        open: () => open(ctx),
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
    registerMcp(ctx);
    return { port: () => engine?.port };
}

/*
 * The MCP server (tools/pp_mcp), for the assistants in VS Code: they drive
 * the bench the user sees. VS Code 1.101 brought the provider API; older
 * ones go without.
 */
function registerMcp(ctx: vscode.ExtensionContext): void {
    const lm = (vscode as any).lm;
    const Def = (vscode as any).McpStdioServerDefinition;
    if (!lm?.registerMcpServerDefinitionProvider || !Def) {
        return;
    }
    ctx.subscriptions.push(lm.registerMcpServerDefinitionProvider(
        "peripheralPretender.mcp", {
            provideMcpServerDefinitions: () => {
                const lay = layout(ctx);
                const env: Record<string, string> = {};
                for (const k of ["PP_WEB_BENCH_DIR", "PP_QEMU", "PP_QEMU_AVR",
                                 "PP_MSYS2_BIN", "PYTHONNOUSERSITE",
                                 "PYTHONUNBUFFERED"]) {
                    if (lay.env[k]) {
                        env[k] = lay.env[k] as string;
                    }
                }
                const def = new Def("Peripheral Pretender", lay.python,
                                    ["-m", "tools.pp_mcp"], env,
                                    ctx.extension.packageJSON.version);
                def.cwd = vscode.Uri.file(lay.root);
                return [def];
            },
        }));
}

export function deactivate(): void {
    stopEngine();
}
