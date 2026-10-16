// Main owns the preferences file. Outside Electron there is no main, so nothing is saved and the
// caller applies the change to this window alone.
export const loadPreferences = async () =>
    window.api?.getPreferences ? window.api.getPreferences() : { ok: false, unsaved: true };

export const savePreferences = async patch =>
    window.api?.setPreferences ? window.api.setPreferences(patch) : { ok: false, unsaved: true };

// The saved analysis values the engine doesn't hold, or null when it holds them all.
export const engineParamsToRestore = (saved, current) => {
    const changes = Object.fromEntries(Object.entries(saved ?? {}).filter(([key, value]) => current?.[key] !== value));
    return Object.keys(changes).length > 0 ? changes : null;
};
