import { BINARY_FRAME } from '../utils/protocol';

const { HEADER_SIZE, ELEMENT_TYPE, OFFSET } = BINARY_FRAME;

// Reads a binary frame's header and views its data in place. The header is
// naturally aligned, so the typed array needs no copy.
export const decodeFrame = buffer => {
    if (!(buffer instanceof ArrayBuffer) || buffer.byteLength < HEADER_SIZE)
        return null;

    const view = new DataView(buffer);
    const elementType = view.getUint16(OFFSET.elementType, true);
    const count = view.getUint32(OFFSET.count, true);

    let data;
    if (elementType === ELEMENT_TYPE.UINT16 && buffer.byteLength >= HEADER_SIZE + count * 2)
        data = new Uint16Array(buffer, HEADER_SIZE, count);
    else if (elementType === ELEMENT_TYPE.FLOAT32 && buffer.byteLength >= HEADER_SIZE + count * 4)
        data = new Float32Array(buffer, HEADER_SIZE, count);
    else
        return null;

    return {
        kind: view.getUint8(OFFSET.kind),
        version: view.getUint8(OFFSET.version),
        elementType,
        seekGeneration: view.getUint32(OFFSET.seekGeneration, true),
        timestamp: view.getFloat64(OFFSET.timestamp, true),
        resolution: view.getFloat32(OFFSET.resolution, true),
        maxMagnitude: view.getFloat32(OFFSET.maxMagnitude, true),
        count,
        data,
    };
};
