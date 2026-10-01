import { useContext } from 'react';
import { PaletteContext } from '../theme/PaletteContext';

export const usePalette = () => useContext(PaletteContext);
