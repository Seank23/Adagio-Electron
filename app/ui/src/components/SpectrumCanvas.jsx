import React, { useEffect, useRef, useState } from 'react';
import { useSelector } from 'react-redux';
import { theme } from 'antd';
import _ from 'lodash';

const SpectrumCanvas = () => {
    const { token } = theme.useToken();
    const canvasRef = useRef(null);
    const spectrumData = useSelector(state => state.analysis.spectrumData);
    const spectrumSR = useSelector(state => state.analysis.spectrumSR);
    const isFileOpen = useSelector(state => state.app.isFileOpen);
    const maxSpectrumValue = useSelector(state => state.analysis.maxSpectrumValue);
    const showLogScale = useSelector(state => state.settings.showLogScale);

    const [maxValueArray, setMaxValueArray] = useState([]);
    const [meanMaxValue, setMeanMaxValue] = useState(1);
    
    const MAX_VAL_ARRAY_SIZE = 100;
    const MIN_FREQ = 50;
    const X_AXIS_PADDING = 20;
    const MAX_Y_VALUES = [50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000];

    const width = canvasRef.current?.parentElement?.clientWidth || 1000;

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

    const binToFreq = (binIndex) => {
        return binIndex * (spectrumSR / spectrumData.length);
    };

    const magToY = (mag, height) => {
        return (height - X_AXIS_PADDING) - ((mag / meanMaxValue) * height);
    };

    useEffect(() => {
        const canvas = canvasRef.current;
        const context = canvas?.getContext('2d');
        let animationFrame;

        const draw = () => {
            animationFrame = requestAnimationFrame(draw);
            context.clearRect(0, 0, canvas.width, canvas.height);

            drawSpectrum();
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
        if (canvas && context) {
            draw();
        }
        return () => cancelAnimationFrame(animationFrame);
    }, [spectrumData]);

    useEffect(() => {
        setMaxValueArray(prev => {
            const newArray = [...prev, maxSpectrumValue];
            if (newArray.length > MAX_VAL_ARRAY_SIZE) {
                newArray.shift();
            }
            return newArray;
        });
        setMeanMaxValue(_.mean(maxValueArray) || 1);
    }, [maxSpectrumValue]);

    return (
        <>
            {isFileOpen && <canvas ref={canvasRef} width={width} height={300} />}
        </>
    );
}
export default SpectrumCanvas;