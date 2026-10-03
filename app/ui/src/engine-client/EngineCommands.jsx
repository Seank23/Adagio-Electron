import { COMMAND } from '../utils/protocol';

// Every call answers { ok, value, error } once the engine has actually run the command.
export const createEngineCommands = engine => ({
    load: path => engine.request(COMMAND.LOAD, path),
    play: () => engine.request(COMMAND.PLAY),
    pause: () => engine.request(COMMAND.PAUSE),
    stop: () => engine.request(COMMAND.STOP),
    clear: () => engine.request(COMMAND.CLEAR),
    seek: seconds => engine.request(COMMAND.SEEK, seconds),
    setVolume: volume => engine.request(COMMAND.SET_VOLUME, volume),
    setSpeed: speed => engine.request(COMMAND.SET_SPEED, speed),
    analyseFrame: () => engine.request(COMMAND.ANALYSE_FRAME),
    stepFrame: () => engine.request(COMMAND.STEP_FRAME),
    resetAnalysis: () => engine.request(COMMAND.RESET_ANALYSIS),
    getAnalysisSchema: () => engine.request(COMMAND.GET_ANALYSIS_SCHEMA),
    setAnalysisSetting: (stage, key, value) => engine.request(COMMAND.SET_ANALYSIS_SETTING, { stage, key, value }),
    status: () => engine.request(COMMAND.STATUS),
    getWaveform: () => engine.request(COMMAND.GET_WAVEFORM),
});
