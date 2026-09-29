import protocol from '@protocol/protocol.json';

// The one description of the wire, shared with the engine: CMake generates the C++
// constants from this same file.
const byJsName = entries => Object.fromEntries(entries.map(entry => [entry.js, entry.name]));

export const EVENT_TYPE = byJsName(protocol.events);
export const COMMAND = byJsName(protocol.commands);
export const TRANSPORT_STATE = byJsName(protocol.states);

const byJsValue = entries => Object.fromEntries(entries.map(entry => [entry.js, entry.value]));

// Binary frames: a fixed little-endian header, then the data at HEADER_SIZE.
export const BINARY_FRAME = {
    HEADER_SIZE: protocol.binaryFrames.headerSize,
    KIND: byJsValue(protocol.binaryFrames.kinds),
    ELEMENT_TYPE: byJsValue(protocol.binaryFrames.elementTypes),
    OFFSET: Object.fromEntries(protocol.binaryFrames.header.map(field => [field.name, field.byteOffset])),
};

export const LIMITS = protocol.limits;
export const PROTOCOL_VERSION = protocol.version;

const { host, port, tokenParam } = protocol.transport;
export const ENGINE_WS_URL = `ws://${host}:${port}`;
export const TOKEN_PARAM = tokenParam;
