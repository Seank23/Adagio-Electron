import React, { useEffect, useRef, useState } from 'react';
import { useSelector } from 'react-redux';
import { theme } from 'antd';
 
const MIN_FREQ = 50;
const X_AXIS_PADDING = 20;
const MAX_Y_VALUES = [50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000, 50000, 100000, 200000, 500000, 1000000];

const SpectrumCanvas = () => {
    const { token } = theme.useToken();
    const canvasRef = useRef(null);
    const spectrumData = useSelector(state => state.analysis.spectrumData);
    const spectrumSR = useSelector(state => state.analysis.spectrumSR);
    const isFileOpen = useSelector(state => state.app.isFileOpen);
    const maxSpectrumValue = useSelector(state => state.analysis.maxSpectrumValue);
    const localMedianData = useSelector(state => state.analysis.localMedianData);
    const showLogScale = useSelector(state => state.settings.showLogScale);

    const peaks = useSelector(state => state.analysis.spectrumPeaks);

    const meanMaxValue = maxSpectrumValue || 1;
    const [canvasWidth, setCanvasWidth] = useState(1000);

    useEffect(() => {
        const canvas = canvasRef.current;
        const parent = canvas?.parentElement;

        if (!parent) {
            return;
        }

        const updateWidth = () => {
            setCanvasWidth(parent.clientWidth || 1000);
        };

        updateWidth();

        const observer = new ResizeObserver(updateWidth);
        observer.observe(parent);

        return () => observer.disconnect();
    }, [isFileOpen]);

    useEffect(() => {
        const canvas = canvasRef.current;
        const context = canvas?.getContext('2d');
        let animationFrame;

        const freqToXLog = (freq, width) => {
            const minLog = Math.log10(MIN_FREQ);
            const maxLog = Math.log10(spectrumSR / 2);
            const logFreq = Math.log10(freq);

            return (
                (logFreq - minLog) /
                (maxLog - minLog)
            ) * width;
        };

        const freqToX = (freq, width) => {
            return (freq / (spectrumSR / 2)) * width;
        };

        const binToFreq = (binIndex, dataLength = spectrumData.length) => {
            return binIndex * (spectrumSR / dataLength);
        };

        const magToY = (mag, height) => {
            return (height - X_AXIS_PADDING) - ((mag / meanMaxValue) * height) * 0.9;
        };

        const draw = () => {
            animationFrame = requestAnimationFrame(draw);
            context.clearRect(0, 0, canvas.width, canvas.height);

            drawSpectrum();
            drawLocalMedian();
            drawPeaks();
            drawXAxis();
            drawYAxis();
        };

        const drawSpectrum = () => {
            if (spectrumData && spectrumData.length > 0) {
                context.lineWidth = 2;
                context.strokeStyle = token.colorPrimary;
                context.beginPath();

                for (let i = 0; i < spectrumData.length; i++) {
                    const freq = binToFreq(i);
                    if (freq < MIN_FREQ || freq > spectrumSR / 2) {
                        continue; // Skip frequencies outside the range
                    }
                    const x = showLogScale ? freqToXLog(freq, canvas.width) : freqToX(freq, canvas.width);
                    const y = magToY(spectrumData[i], canvas.height); 
                    if (i === 0) {
                        context.moveTo(x, y);
                    } else {
                        context.lineTo(x, y);
                    }
                }
                context.lineTo(canvas.width, canvas.height - X_AXIS_PADDING);
                context.stroke();
            } 
        };

        const drawLocalMedian = () => {
            if (!localMedianData || localMedianData.length === 0) {
                return;
            }

            context.lineWidth = 2;
            context.strokeStyle = token.colorError;
            context.beginPath();

            let started = false;

            for (let i = 0; i < localMedianData.length; i++) {
                const freq = binToFreq(localMedianData[i].bin);
                if (freq < MIN_FREQ || freq > spectrumSR / 2) {
                    continue;
                }

                const x = showLogScale ? freqToXLog(freq, canvas.width) : freqToX(freq, canvas.width);
                const y = magToY(localMedianData[i].magnitude, canvas.height);

                if (!started) {
                    context.moveTo(x, y);
                    started = true;
                } else {
                    context.lineTo(x, y);
                }
            }

            if (started) {
                context.stroke();
            }
        };

        const drawPeaks = () => {
            if (!peaks || peaks.length === 0 || !spectrumData || spectrumData.length === 0) {
                return;
            }

            context.fillStyle = token.colorError;

            peaks.forEach(peak => {
                let freq;
                let mag;

                if (typeof peak === 'number') {
                    const bin = peak;
                    freq = binToFreq(bin);
                    mag = spectrumData[bin];
                } else if (Array.isArray(peak)) {
                    const [peakX, peakY] = peak;
                    freq = peakX;
                    mag = peakY;
                } else if (typeof peak === 'object' && peak !== null) {
                    const bin = peak.bin ?? peak.index;
                    freq = peak.freq ?? peak.frequency ?? peak.hz;
                    mag = peak.mag ?? peak.magnitude ?? peak.value;

                    if (freq === undefined && bin !== undefined) {
                        freq = binToFreq(bin);
                    }

                    if (mag === undefined && bin !== undefined) {
                        mag = spectrumData[bin];
                    }
                }

                if (freq === undefined || mag === undefined || Number.isNaN(freq) || Number.isNaN(mag)) {
                    return;
                }

                if (freq < MIN_FREQ || freq > spectrumSR / 2) {
                    return;
                }

                const x = showLogScale ? freqToXLog(freq, canvas.width) : freqToX(freq, canvas.width);
                const y = magToY(mag, canvas.height);

                context.beginPath();
                context.arc(x, y, 3, 0, Math.PI * 2);
                context.fill();
            });
        };

        const drawXAxis = () => {
            context.fillStyle = '#fff';
            context.fillRect(0, canvas.height - X_AXIS_PADDING, canvas.width, X_AXIS_PADDING);

            context.strokeStyle = "#666";
            context.fillStyle = "#888";
            context.font = "12px sans-serif";

            context.beginPath();
            context.moveTo(0, canvas.height - X_AXIS_PADDING);
            context.lineTo(canvas.width, canvas.height - X_AXIS_PADDING);
            context.stroke();

            for (let freq = 0; freq <= spectrumSR / 2; freq += 500) {
                const x = showLogScale ? freqToXLog(freq, canvas.width) : freqToX(freq, canvas.width);

                context.beginPath();
                context.moveTo(x, canvas.height - X_AXIS_PADDING);
                context.lineTo(x, canvas.height - X_AXIS_PADDING + 6);
                context.stroke();

                context.fillText(
                    freq >= 1000 ? `${freq / 1000}k` : `${freq}`,
                    x + 2,
                    canvas.height - 4
                );
            }
        };

        const drawYAxis = () => {
            context.strokeStyle = "#666";
            context.fillStyle = "#aaa";
            context.font = "12px sans-serif";

            context.beginPath();
            context.moveTo(0, 0);
            context.lineTo(0, canvas.height);
            context.stroke();
            const ticksCount = 10;
            const roundedMax = MAX_Y_VALUES.find(val => val >= meanMaxValue) || meanMaxValue;

            for (let mag = 0; mag <= roundedMax; mag += roundedMax / ticksCount) {
                const y = magToY(mag, canvas.height);

                context.beginPath();
                context.moveTo(0, y);
                context.lineTo(6, y);
                context.stroke();

                context.fillText(mag, 8, y + 4);
            }
        };
        if (canvas && context && spectrumSR > 0) {
            draw();
        }
        return () => cancelAnimationFrame(animationFrame);
    }, [spectrumData, localMedianData, peaks, spectrumSR, showLogScale, meanMaxValue, token.colorPrimary, token.colorError, token.colorErrorBg, canvasWidth]);

    return (
        <>
            {isFileOpen && <canvas ref={canvasRef} width={canvasWidth} height={300} />}
        </>
    );
}
export default SpectrumCanvas;