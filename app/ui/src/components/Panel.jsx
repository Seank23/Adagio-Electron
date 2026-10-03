import Styled from '@emotion/styled';

export const Panel = Styled.div`
    display: flex;
    flex-direction: column;
    min-width: 0;
    min-height: 0;
    overflow: hidden;
    background: var(--bg-panel);
`;

export const PanelHeader = ({ title, height = 34, children }) => (
    <HeaderRow style={{ height }}>
        <PanelTitle>{title}</PanelTitle>
        {children}
    </HeaderRow>
);

export const Spacer = Styled.div`
    flex: 1;
`;

const HeaderRow = Styled.div`
    display: flex;
    align-items: center;
    flex-shrink: 0;
    gap: 10px;
    padding: 0 12px;
    border-bottom: 1px solid var(--border-subtle);
`;

const PanelTitle = Styled.span`
    font: 600 10px/1 var(--font-ui);
    letter-spacing: 1px;
    text-transform: uppercase;
    color: var(--text-secondary);
`;
