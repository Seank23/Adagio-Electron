import { useState } from 'react';
import { useSelector } from 'react-redux';
import NumberStepper from '../controls/NumberStepper';
import Segmented from '../controls/Segmented';
import { PreferenceRow, PreferenceSection } from './PreferenceRow';
import { usePreferenceActions } from '../../hooks/usePreferences';
import { selectFrameLength, selectFrameSmoothing, selectHopSize, selectSampleRate } from '../../store/pipelineSlice';
import { formatAnalysisRange, formatFrame, formatHop, formatSmoothing } from '../../utils/format';
import { LIMITS } from '../../utils/protocol';

const SAMPLE_RATES = LIMITS.sampleRates.map(rate => ({ value: rate, label: `${rate / 1000} kHz` }));
const HOP_SIZES = LIMITS.hopSizes.map(size => ({ value: size, label: String(size) }));
const FRAME_LENGTHS = LIMITS.frameLengths.map(length => ({ value: length, label: String(length) }));

const AnalysisSection = () => {
    const { setEngineParams } = usePreferenceActions();
    const sampleRate = useSelector(selectSampleRate);
    const frameLength = useSelector(selectFrameLength);
    const hopSize = useSelector(selectHopSize);
    const frameSmoothing = useSelector(selectFrameSmoothing);
    const connected = useSelector(state => state.app.connectionState === 'connected');

    const [resampling, setResampling] = useState(false);

    const known = connected && sampleRate !== null;
    const disabled = !known;

    const onSampleRate = async rate => {
        setResampling(true);
        try {
            await setEngineParams({ sampleRate: rate });
        } finally {
            setResampling(false);
        }
    };

    return (
        <PreferenceSection title="Analysis">
            <PreferenceRow
                label="Sample rate"
                description="The rate the track is resampled to for analysis. Higher values can reach higher notes and costs more CPU."
                readout={known && formatAnalysisRange(sampleRate)}
            >
                <Segmented label="Sample rate" options={SAMPLE_RATES} value={sampleRate} font="--font-mono"
                    disabled={disabled || resampling} onChange={onSampleRate} />
            </PreferenceRow>
            <PreferenceRow
                label="Hop size"
                description="Samples between analysis frames. Smaller values can help distinguish fast notes and costs more CPU."
                readout={known && formatHop(hopSize, sampleRate)}
            >
                <Segmented label="Hop size" options={HOP_SIZES} value={hopSize} font="--font-mono"
                    disabled={disabled} onChange={size => setEngineParams({ hopSize: size })} />
            </PreferenceRow>
            <PreferenceRow
                label="Frame size"
                description="Samples in each analysis frame. Longer frames separate low notes better but react more slowly and cost more CPU."
                readout={known && formatFrame(frameLength, sampleRate)}
            >
                <Segmented label="Frame size" options={FRAME_LENGTHS} value={frameLength} font="--font-mono"
                    disabled={disabled} onChange={length => setEngineParams({ frameLength: length })} />
            </PreferenceRow>
            <PreferenceRow
                label="Spectrum smoothing"
                description="Number of frames in the rolling average. More frames give a steadier spectrum but follow changes more slowly; 1 turns it off."
                readout={known && formatSmoothing(frameSmoothing, hopSize, sampleRate)}
            >
                <NumberStepper label="Spectrum smoothing" value={frameSmoothing}
                    min={LIMITS.frameSmoothingMin} max={LIMITS.frameSmoothingMax}
                    disabled={disabled} onChange={count => setEngineParams({ frameSmoothing: count })} />
            </PreferenceRow>
        </PreferenceSection>
    );
};
export default AnalysisSection;
