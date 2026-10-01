import { createContext } from 'react';
import { PALETTES } from './tokens';

// For code that needs plain colour strings, it changes only when the theme does.
export const PaletteContext = createContext(null);
