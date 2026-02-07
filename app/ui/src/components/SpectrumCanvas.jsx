import React, { useEffect, useRef } from 'react';
import { useSelector } from 'react-redux';

const SpectrumCanvas = () => {
    const canvasRef = useRef(null);
    const spectrumData = useSelector(state => state.analysis.spectrumData);

    useEffect(() => {
        const canvas = canvasRef.current;
        const context = canvas.getContext('2d');
        let animationFrame;

        const drawSpectrum = () => {
            context.clearRect(0, 0, canvas.width, canvas.height);
            if (spectrumData && spectrumData.length > 0) {
                spectrumData.forEach((magnitude, index) => {
                    const x = index * (canvas.width / spectrumData.length);
                    const y = canvas.height * (1 - magnitude);
                    context.fillRect(x, y, 2, canvas.height);
                });
                animationFrame = requestAnimationFrame(drawSpectrum);
            } 
        }
        drawSpectrum();
        return () => cancelAnimationFrame(animationFrame);
    }, [spectrumData]);

    return <canvas ref={canvasRef} width={800} height={300} />;
}
export default SpectrumCanvas;