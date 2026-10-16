import { readFileSync, readdirSync, writeFileSync } from 'fs';
import path from 'path';

const RULE = '-'.repeat(80);
const LICENCE_FILE = /^(licen[cs]e|copying)/i;

const HEADER = `Adagio third-party notices
==========================

Adagio is free software under the GNU General Public License, version 3 or (at
your option) any later version; its text is in LICENSE.txt. It is built with the
third-party software below, each under the licence given with it.

Electron, and the Chromium it is built on, carry their own notices beside
Adagio.exe: LICENSE.electron.txt and LICENSES.chromium.html.
`;

const UI_HEADER = `The interface (resources/app.asar)
==================================

The interface bundles these npm packages: every one the build took code, styles
or fonts from.
`;

// Writes the notices electron-builder puts beside Adagio.exe: a header, the engine's own
// (kept by hand beside the CMakeLists that pins its libraries), then one for every npm
// package in the bundle, which only the build knows. Walking package.json's dependencies
// instead would list Babel and the rest of what @emotion/react only needs to compile.
export const thirdPartyNotices = ({ engineNotices, output }) => ({
    name: 'adagio-third-party-notices',
    apply: 'build',
    writeBundle(options, bundle) {
        const rendered = new Set();
        for (const file of Object.values(bundle)) {
            if (file.type !== 'chunk')
                continue;
            for (const [id, module] of Object.entries(file.modules))
                if (module.renderedLength > 0)
                    rendered.add(id);
        }
        // A stylesheet renders no JavaScript, but its rules and the fonts it names still ship.
        const used = [...this.getModuleIds()].filter(id => rendered.has(id) || /\.css($|\?)/.test(id));

        const packages = new Map();
        for (const root of new Set(used.map(packageRoot).filter(Boolean))) {
            const manifest = JSON.parse(readFileSync(path.join(root, 'package.json'), 'utf8'));
            packages.set(`${manifest.name}@${manifest.version}`, { root, manifest });
        }
        const sections = [...packages.values()]
            .sort((a, b) => a.manifest.name.localeCompare(b.manifest.name) || a.manifest.version.localeCompare(b.manifest.version))
            .map(describePackage);

        const engine = readFileSync(engineNotices, 'utf8').replace(/\r\n/g, '\n');
        writeFileSync(output, [HEADER, engine, UI_HEADER, ...sections].join('\n'));
    },
});

// The package directory a module id lies in, or null for our own source. Ids can carry a
// leading \0 and a query (CommonJS proxies), and nest one node_modules inside another.
const packageRoot = id => {
    const parts = id.replace(/^\0/, '').split('?')[0].split(/[\\/]/);
    const at = parts.lastIndexOf('node_modules');
    if (at < 0 || at + 1 >= parts.length)
        return null;
    const nameLength = parts[at + 1].startsWith('@') ? 2 : 1;
    return parts.slice(0, at + 1 + nameLength).join('/');
};

const licenceName = manifest => {
    const licence = manifest.license ?? manifest.licenses;
    if (Array.isArray(licence))
        return licence.map(entry => entry.type ?? entry).join(' OR ');
    return licence?.type ?? licence ?? 'no licence given';
};

const describePackage = ({ root, manifest }) => {
    const licence = licenceName(manifest);
    const file = readdirSync(root).filter(name => LICENCE_FILE.test(name)).sort()[0];
    const author = typeof manifest.author === 'string' ? manifest.author : manifest.author?.name;
    const text = file
        ? readFileSync(path.join(root, file), 'utf8')
        : `The package carries no licence text. Its package.json gives the licence as ${licence}`
            + (author ? ` and the author as ${author}.` : '.');
    return `${RULE}\n${manifest.name} ${manifest.version} (${licence})\n${RULE}\n\n${text.replace(/\r\n/g, '\n').trim()}\n`;
};
