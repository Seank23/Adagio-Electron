import { useState } from 'react';
import { useSelector } from 'react-redux';
import Segmented from '../controls/Segmented';
import { PreferenceRow, PreferenceSection } from './PreferenceRow';
import { usePreferenceActions } from '../../hooks/usePreferences';
import { selectFrameLength, selectHopSize, selectSampleRate } from '../../store/pipelineSlice';
import { formatAnalysisRange, formatFrame, formatHop } from '../../utils/format';
import { LIMITS } from '../../utils/protocol';

const SAMPLE_RATES = LIMITS.sampleRates.map(rate => ({ value: rate, label: `${rate / 1000} kHz` }));
const HOP_SIZES = LIMITS.hopSizes.map(size => ({ value: size, label: String(size) }));
const FRAME_LENGTHS = LIMITS.frameLengths.map(length => ({ value: length, label: String(length) }));

// The selected chips are the engine's values: they move on the transport event, not the click.
const AnalysisSection = () => {
    const { setAnalysisParams } = usePreferenceActions();
    const sampleRate = useSelector(selectSampleRate);
    const frameLength = useSelector(selectFrameLength);
    const hopSize = useSelector(selectHopSize);
    const connected = useSelector(state => state.app.connectionState === 'connected');
    // A new rate preprocesses the track before the engine answers, so its chips wait for the reply.
    const [resampling, setResampling] = useState(false);

    const known = connected && sampleRate !== null;
    const disabled = !known;

    const onSampleRate = async rate => {
        setResampling(true);
        try {
            await setAnalysisParams({ sampleRate: rate });
        } finally {
            setResampling(false);
        }
    };

    return (
        <PreferenceSection title="Analysis">
            <PreferenceRow
                label="Sample rate"
                description="The rate the track is resampled to for analysis. Higher reaches higher notes and costs more CPU."
                readout={known && formatAnalysisRange(sampleRate)}
            >
                <Segmented label="Sample rate" options={SAMPLE_RATES} value={sampleRate} mono
                    disabled={disabled || resampling} onChange={onSampleRate} />
            </PreferenceRow>
            <PreferenceRow
                label="Hop size"
                description="Samples between analysis frames. Smaller follows fast notes more closely and costs more CPU."
                readout={known && formatHop(hopSize, sampleRate)}
            >
                <Segmented label="Hop size" options={HOP_SIZES} value={hopSize} mono
                    disabled={disabled} onChange={size => setAnalysisParams({ hopSize: size })} />
            </PreferenceRow>
            <PreferenceRow
                label="Frame size"
                description="Samples in each FFT. Longer frames separate low notes better but react more slowly."
                readout={known && formatFrame(frameLength, sampleRate)}
            >
                <Segmented label="Frame size" options={FRAME_LENGTHS} value={frameLength} mono
                    disabled={disabled} onChange={length => setAnalysisParams({ frameLength: length })} />
            </PreferenceRow>
        </PreferenceSection>
    );
};
export default AnalysisSection;
