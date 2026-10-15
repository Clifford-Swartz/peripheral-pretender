/*
 * The Twin sidebar (its own icon in the activity bar): the boards, one
 * click to open one (or switch to it), the running one marked; the view's
 * title bar has Debug, Run ELF, Stop and the engine log.
 *
 * Copyright (c) 2026 Clifford Swartz.
 */

import * as vscode from "vscode";
import type { Board } from "./extension";

export interface SidebarHooks {
    boards(): Board[];
    board(): string | undefined;
    changed: vscode.Event<void>;
}

class Boards implements vscode.TreeDataProvider<Board> {
    private emitter = new vscode.EventEmitter<void>();
    readonly onDidChangeTreeData = this.emitter.event;

    constructor(private hooks: SidebarHooks) {
        hooks.changed(() => this.emitter.fire());
    }

    getChildren(): Board[] {
        return this.hooks.boards();
    }

    getTreeItem(b: Board): vscode.TreeItem {
        const running = b.id === this.hooks.board();
        /* "PIC32CM MC00 Curiosity Pro (EV15N46A)": the name, then the kit */
        const kit = /\(([^)]*)\)/.exec(b.name)?.[1];
        const item = new vscode.TreeItem(b.name.replace(/\s*\(.*$/, ""));
        item.id = b.id;
        item.description = [running ? "running" : "", kit].filter(Boolean)
            .join(", ");
        item.iconPath = new vscode.ThemeIcon(
            running ? "circle-filled" : "circuit-board",
            running ? new vscode.ThemeColor("testing.iconPassed") : undefined);
        item.tooltip = running ? `${b.name}: running. Click to show it.` :
            `Open ${b.name} on the twin`;
        item.command = {
            command: "peripheralPretender.open", title: "Open Board",
            arguments: [b.id],
        };
        return item;
    }
}

export function registerSidebar(ctx: vscode.ExtensionContext,
                                hooks: SidebarHooks): void {
    ctx.subscriptions.push(
        vscode.window.createTreeView("peripheralPretender.boards", {
            treeDataProvider: new Boards(hooks),
        }),
        vscode.commands.registerCommand("peripheralPretender.debug", () =>
            vscode.debug.startDebugging(
                vscode.workspace.workspaceFolders?.[0],
                { type: "twin", request: "launch", name: "Debug on the twin" })));
}
