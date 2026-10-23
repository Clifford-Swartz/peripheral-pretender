/*
 * The Twin sidebar (its own icon in the activity bar). Showing it opens
 * the bench on the last workspace. It lists the bench, then the boards:
 * one click puts a board on the bench (the one there is marked); the
 * view's title bar has Debug, Run ELF, Stop and the engine log.
 *
 * Copyright (c) 2026 Clifford Swartz.
 */

import * as vscode from "vscode";
import type { Board } from "./extension";

export interface SidebarHooks {
    boards(): Board[];
    board(): string | undefined;
    running(): boolean;
    panelOpen(): boolean;
    open(): Promise<boolean>;
    changed: vscode.Event<void>;
}

/* The bench itself, then a board per entry */
type Entry = Board | "bench";

class Boards implements vscode.TreeDataProvider<Entry> {
    private emitter = new vscode.EventEmitter<void>();
    readonly onDidChangeTreeData = this.emitter.event;

    constructor(private hooks: SidebarHooks) {
        hooks.changed(() => this.emitter.fire());
    }

    getChildren(): Entry[] {
        return ["bench", ...this.hooks.boards()];
    }

    getTreeItem(b: Entry): vscode.TreeItem {
        if (b === "bench") {
            const item = new vscode.TreeItem("Workbench");
            item.id = "bench";
            item.description = this.hooks.running() ? "open" : "last workspace";
            item.iconPath = new vscode.ThemeIcon("layout");
            item.tooltip = "The bench: the board, Click boards, parts and " +
                "firmware from last time (add more from its palette)";
            item.command = { command: "peripheralPretender.open",
                             title: "Open the bench" };
            return item;
        }
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
        item.tooltip = running ? `${b.name}: on the bench. Click to show it.` :
            `Put ${b.name} on the bench`;
        item.command = {
            command: "peripheralPretender.open", title: "Open Board",
            arguments: [b.id],
        };
        return item;
    }
}

export function registerSidebar(ctx: vscode.ExtensionContext,
                                hooks: SidebarHooks): void {
    const view = vscode.window.createTreeView("peripheralPretender.boards", {
        treeDataProvider: new Boards(hooks),
    });
    /* Opening the sidebar opens the bench, as it was left */
    const show = () => {
        if (view.visible && !hooks.panelOpen()) {
            void hooks.open();
        }
    };
    ctx.subscriptions.push(
        view,
        view.onDidChangeVisibility(show),
        vscode.commands.registerCommand("peripheralPretender.debug", () =>
            vscode.debug.startDebugging(
                vscode.workspace.workspaceFolders?.[0],
                { type: "twin", request: "launch", name: "Debug on the twin" })));
    show();
}
