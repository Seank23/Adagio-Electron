// Baked in by Vite's define (vite.config.js): app/package.json's version and the local build date.
export const APP_VERSION = __APP_VERSION__;
export const BUILD_DATE = __BUILD_DATE__;
export const REPO_URL = 'https://github.com/Seank23/Adagio-Electron';

// '2026-10-05' → '5 October 2026'. Read as UTC midnight and printed in UTC, so no time zone moves the day.
export const formatBuildDate = iso => new Intl.DateTimeFormat('en-GB', {
    day: 'numeric', month: 'long', year: 'numeric', timeZone: 'UTC',
}).format(new Date(`${iso}T00:00:00Z`));
