import Styled from '@emotion/styled';

export const Bar = ({ height, color, fillRef }) => (
    <Track height={height}>
        <Fill ref={fillRef} height={height} color={color} />
    </Track>
);

const withoutStyleProps = { shouldForwardProp: prop => prop !== 'height' && prop !== 'color' };

const Track = Styled('div', withoutStyleProps)`
    flex: 1;
    min-width: 0;
    height: ${({ height }) => height}px;
    border-radius: ${({ height }) => height / 2}px;
    background: var(--bg-inset);
    overflow: hidden;
`;

const Fill = Styled('div', withoutStyleProps)`
    width: 0;
    height: 100%;
    border-radius: ${({ height }) => height / 2}px;
    background: ${({ color }) => color};
`;

export const Percent = Styled('span', withoutStyleProps)`
    flex-shrink: 0;
    text-align: right;
    font: 400 10px/1 var(--font-mono);
    font-variant-numeric: tabular-nums;
    color: ${({ color }) => color};
`;

// The panel-title style, as the first item of a section rather than a header row.
export const SectionTitle = Styled.span`
    font: 600 10px/1 var(--font-ui);
    letter-spacing: 1px;
    text-transform: uppercase;
    color: var(--text-secondary);
`;
