/*
 * Debug on the twin: a "twin" launch configuration (or plain F5 on a
 * workspace with a build) runs the ELF on the twin paused, with QEMU's
 * gdbstub on a free port, and hands VS Code a cppdbg configuration for
 * the bundled gdb-multiarch to attach. The board view keeps running
 * beside the debugger; when the session ends, the firmware runs on.
 *
 * Copyright (c) 2026 Clifford Swartz.
 */

import * as fs from "fs";
import * as net from "net";
import * as path from "path";
import * as vscode from "vscode";
import type { Board } from "./extension";

export interface DebugHooks {
    log: vscode.LogOutputChannel;
    port(): number | undefined;
    board(): string | undefined;
    boards(): Board[];
    pickBoard(only: (b: Board) => boolean): Promise<string | undefined>;
    open(board: string): Promise<boolean>;
    gdb(): string;
}

const CPPTOOLS = "ms-vscode.cpptools";
const EM_ARM = 40;
const EM_AVR = 83;

/* The ELF's architecture (e_machine), or undefined if not an ELF */
export function elfArch(file: string): "arm" | "avr" | undefined {
    const buf = Buffer.alloc(20);
    const fd = fs.openSync(file, "r");
    try {
        fs.readSync(fd, buf, 0, 20, 0);
    } finally {
        fs.closeSync(fd);
    }
    if (buf.readUInt32BE(0) !== 0x7f454c46) {
        return undefined;
    }
    const machine = buf.readUInt16LE(18);
    return machine === EM_ARM ? "arm" : machine === EM_AVR ? "avr"
                                                           : undefined;
}

export function boardArch(b: Board): "arm" | "avr" {
    return b.machine.startsWith("avr") ? "avr" : "arm";
}

/* The newest build in the workspace: out/** (MPLAB for VS Code), dist/** */
async function newestElf(): Promise<string | undefined> {
    const found = await vscode.workspace.findFiles(
        "{**/out/**/*.elf,**/dist/**/*.elf}", "**/node_modules/**", 200);
    let best: string | undefined;
    let bestTime = -1;
    for (const uri of found) {
        const t = fs.statSync(uri.fsPath).mtimeMs;
        if (t > bestTime) {
            best = uri.fsPath;
            bestTime = t;
        }
    }
    return best;
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

async function post(port: number, elf: string,
                    query: Record<string, string>): Promise<void> {
    const qs = new URLSearchParams({ name: path.basename(elf), ...query });
    const r = await fetch(`http://127.0.0.1:${port}/api/firmware?${qs}`, {
        method: "POST", body: fs.readFileSync(elf),
    });
    if (!r.ok) {
        throw new Error(await r.text());
    }
}

class TwinDebug implements vscode.DebugConfigurationProvider {
    constructor(private hooks: DebugHooks) {}

    provideDebugConfigurations(): vscode.DebugConfiguration[] {
        return [{
            type: "twin", request: "launch", name: "Debug on the twin",
            program: "${workspaceFolder}/out/${workspaceFolderBasename}/default.elf",
        }];
    }

    async resolveDebugConfigurationWithSubstitutedVariables(
        folder: vscode.WorkspaceFolder | undefined,
        config: vscode.DebugConfiguration,
    ): Promise<vscode.DebugConfiguration | undefined> {
        const h = this.hooks;

        const cpptools = vscode.extensions.getExtension(CPPTOOLS);
        if (cpptools && !cpptools.isActive) {
            /* Its cppdbg adapter exists only once it is active */
            await cpptools.activate();
        }
        if (!cpptools) {
            const pick = await vscode.window.showErrorMessage(
                "Debugging on the twin uses the C/C++ extension's debugger.",
                "Install C/C++");
            if (pick) {
                await vscode.commands.executeCommand(
                    "workbench.extensions.installExtension", CPPTOOLS);
            }
            return undefined;
        }
        let program: string | undefined = config.program;
        if (!program) {
            /* F5 with no launch.json: the workspace's newest build */
            program = await newestElf();
        }
        if (!program || !fs.existsSync(program)) {
            vscode.window.showErrorMessage(
                `Twin: no firmware to debug${program ? ` (${program})` : ""}: ` +
                "build the project, or set \"program\" to its ELF.");
            return undefined;
        }
        const arch = elfArch(program);
        if (!arch) {
            vscode.window.showErrorMessage(`Twin: ${program} is not an ELF ` +
                                           "for Arm or AVR.");
            return undefined;
        }
        /* The board: as configured, the one showing if it fits, or ask */
        const fits = (b: Board) => boardArch(b) === arch;
        const all = h.boards();
        let board: string | undefined = config.board;
        const showing = all.find((b) => b.id === h.board());
        if (!board && showing && fits(showing)) {
            board = showing.id;
        }
        board = board || await h.pickBoard(fits);
        if (!board || !(await h.open(board))) {
            return undefined;
        }
        const port = h.port();
        if (!port) {
            return undefined;
        }
        const gdbPort = await freePort();
        try {
            await post(port, program, { debug: String(gdbPort) });
        } catch (err) {
            vscode.window.showErrorMessage(
                `Twin: the twin did not start: ${(err as Error).message}`);
            return undefined;
        }
        h.log.info(`[twin] debugging ${program} on ${board}, gdbstub ` +
                   `127.0.0.1:${gdbPort}`);
        const stopAtEntry = config.stopAtEntry !== false;
        return {
            type: "cppdbg",
            request: "launch",
            name: config.name || "Debug on the twin",
            program,
            cwd: folder?.uri.fsPath || path.dirname(program),
            MIMode: "gdb",
            miDebuggerPath: config.gdbPath || h.gdb(),
            miDebuggerServerAddress: `127.0.0.1:${gdbPort}`,
            ...(arch === "arm" ? { targetArchitecture: "arm" } : {}),
            stopAtConnect: false,
            launchCompleteCommand: "exec-continue",
            postRemoteConnectCommands: stopAtEntry ?
                [{ text: "-break-insert -t main", ignoreFailures: true }] : [],
            externalConsole: false,
            /* Ours: on termination the firmware runs on without gdb */
            ppTwin: { program },
        };
    }
}

export function registerDebug(ctx: vscode.ExtensionContext,
                              hooks: DebugHooks): void {
    const provider = new TwinDebug(hooks);

    ctx.subscriptions.push(
        vscode.debug.registerDebugConfigurationProvider("twin", provider),
        vscode.debug.registerDebugConfigurationProvider(
            "twin", provider, vscode.DebugConfigurationProviderTriggerKind.Dynamic),
        vscode.debug.onDidTerminateDebugSession(async (s) => {
            const mine = s.configuration.ppTwin as { program: string } | undefined;
            const port = hooks.port();
            if (!mine || !port || !fs.existsSync(mine.program)) {
                return;
            }
            /* gdb's kill ends QEMU: run the firmware again, undebugged */
            try {
                await post(port, mine.program, {});
            } catch (err) {
                hooks.log.warn(`[twin] rerun after debugging: ${err}`);
            }
        }));
}
