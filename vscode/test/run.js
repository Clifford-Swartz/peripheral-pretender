/*
 * End-to-end test: a VS Code with a throwaway profile loads the extension
 * from this folder (with the engine staged by scripts/build_vsix.py
 * --skip-package) and runs test/suite.js in it.
 *
 *   npm test            (VSCODE_EXE: the Code.exe to use; default: download)
 */

const os = require("os");
const path = require("path");
const fs = require("fs");
const cp = require("child_process");
const {
    downloadAndUnzipVSCode, resolveCliArgsFromVSCodeExecutablePath, runTests,
} = require("@vscode/test-electron");

/* Kept between runs (.vscode-test is not in git): the C/C++ extension,
   whose debugger the twin's debug sessions use */
const EXTENSIONS = path.resolve(__dirname, "..", ".vscode-test", "extensions");
const CPPTOOLS = "ms-vscode.cpptools";

/*
 * The test runner's module path may not contain spaces (VS Code requires
 * it unquoted): on Windows use the 8.3 name
 */
function noSpaces(p) {
    if (process.platform !== "win32" || !p.includes(" ")) {
        return p;
    }
    return require("child_process").execSync(
        `for %I in ("${p}") do @echo %~sI`, { shell: "cmd.exe" })
        .toString().trim();
}

async function main() {
    const ext = path.resolve(__dirname, "..");
    const profile = fs.mkdtempSync(path.join(os.tmpdir(), "pp-vscode-"));

    if (!fs.existsSync(path.join(ext, "engine", "app"))) {
        throw new Error("no staged engine: py scripts/build_vsix.py --skip-package");
    }
    const exe = process.env.VSCODE_EXE || await downloadAndUnzipVSCode();
    if (!fs.existsSync(EXTENSIONS) ||
        !fs.readdirSync(EXTENSIONS).some((d) => d.startsWith(CPPTOOLS))) {
        const [cli, ...args] = resolveCliArgsFromVSCodeExecutablePath(exe);
        const q = (s) => `"${s}"`;          /* paths with spaces */
        const env = { ...process.env };
        delete env.ELECTRON_RUN_AS_NODE;
        cp.execSync([cli, ...args, "--extensions-dir", EXTENSIONS,
                     "--install-extension", CPPTOOLS].map(q).join(" "),
                    { stdio: "inherit", env });
    }
    try {
        await runTests({
            vscodeExecutablePath: exe,
            extensionDevelopmentPath: noSpaces(ext),
            extensionTestsPath: noSpaces(path.join(__dirname, "suite.js")),
            launchArgs: [
                "--user-data-dir", path.join(profile, "user"),
                "--extensions-dir", noSpaces(EXTENSIONS),
                "--disable-workspace-trust",
                "--skip-welcome",
                ...(process.env.PP_CDP_PORT ?
                    [`--remote-debugging-port=${process.env.PP_CDP_PORT}`] : []),
                profile,
            ],
        });
    } finally {
        fs.rmSync(profile, { recursive: true, force: true });
    }
}

main().catch((err) => {
    console.error(err);
    process.exit(1);
});
