// Main owns the preferences file. Outside Electron there is no main, so nothing is saved and the
// caller applies the change to this window alone.
export const loadPreferences = async () =>
    window.api?.getPreferences ? window.api.getPreferences() : { ok: false, unsaved: true };

export const savePreferences = async patch =>
    window.api?.setPreferences ? window.api.setPreferences(patch) : { ok: false, unsaved: true };
