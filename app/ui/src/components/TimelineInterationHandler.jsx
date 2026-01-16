import _ from 'lodash';

/* eslint-disable react-hooks/refs */
const TimelineInterationHandler = ({ containerRef, waveSurferRef, updateZoomLevel }) => {
    let isDragging = false;
    let startX = 0;
    let startY = 0;
    let startScroll = 0;
    let currentScroll = 0;
    let startZoom = Math.max(waveSurferRef.current.getWidth() / waveSurferRef.current.getDuration(), waveSurferRef.current?.options.minPxPerSec);
    let visualZoom = 1;
    let zoomAnchorX = 0;
    let wrapper = null;
    const width = waveSurferRef.current.getWidth();
    let hasZoomed = false;
    let aggVisualZoom = 1;

    const applyVisualZoom = (wrapper, scale) => {
        visualZoom = scale;
        const scaledAnchor = zoomAnchorX * scale;
        const delta = (scaledAnchor - zoomAnchorX) / aggVisualZoom;

        wrapper.style.transform = `translateX(${-delta}px) scaleX(${scale})`;
        wrapper.scrollLeft = currentScroll + delta;
    }

    containerRef.current.addEventListener("mousedown", (e) => {
        isDragging = true;
        startX = e.clientX;
        startY = e.clientY;
        startScroll = waveSurferRef.current?.getScroll();
        startZoom = Math.max(width / waveSurferRef.current.getDuration(), waveSurferRef.current?.options.minPxPerSec);
        
        wrapper = waveSurferRef.current?.getWrapper();
        zoomAnchorX = (e.clientX - containerRef.current.getBoundingClientRect().left) - width * 0.5;
    });

    window.addEventListener("mousemove", (e) => {
        if (!isDragging) return;

        const dx = e.clientX - startX;
        const dy = e.clientY - startY;

        // --- PAN ---
        if (Math.abs(dx) > 5) {
            currentScroll = startScroll + dx;
        }

        // --- ZOOM ---
        if (Math.abs(dy) > 20) {
            hasZoomed = true;
            const scale = Math.max(0.5, Math.min(8, 1 + dy * 0.005));
            applyVisualZoom(wrapper, scale);
        }
    });

    window.addEventListener("mouseup", () => {
        if (!isDragging) return;
        isDragging = false;
        waveSurferRef.current?.setScroll(currentScroll);
        if (hasZoomed) {
            hasZoomed = false;
            const newPxPerSec = startZoom * visualZoom;
            updateZoomLevel(newPxPerSec);
            const width = waveSurferRef.current.getWidth();
            const anchorRatio = ((zoomAnchorX + width / 2) * visualZoom) / wrapper.scrollWidth;
            const newScroll = anchorRatio * wrapper.scrollWidth - width / 2;
            waveSurferRef.current.setScroll(newScroll);
            aggVisualZoom += visualZoom;
            visualZoom = 1;
            wrapper.style.transform = '';
        }
    });

    return (
        <></>
    );
};

export default TimelineInterationHandler;