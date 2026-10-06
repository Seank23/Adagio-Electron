import Styled from '@emotion/styled';

export const PreferenceSection = ({ title, children }) => (
    <section aria-label={title}>
        <SectionTitle>{title}</SectionTitle>
        {children}
    </section>
);

export const PreferenceRow = ({ label, description, readout, children }) => (
    <Row>
        <Text>
            <Label>{label}</Label>
            <Description>{description}</Description>
        </Text>
        <Control>
            {children}
            {readout && <Readout>{readout}</Readout>}
        </Control>
    </Row>
);

const SectionTitle = Styled.h3`
    margin: 0 0 4px;
    padding-top: 18px;
    font: 600 10px/1 var(--font-ui);
    letter-spacing: 1px;
    text-transform: uppercase;
    color: var(--text-secondary);
`;

const Row = Styled.div`
    display: flex;
    align-items: center;
    gap: 24px;
    padding: 12px 0;
    &:not(:last-child) {
        border-bottom: 1px solid var(--border-subtle);
    }
`;

const Text = Styled.div`
    display: flex;
    flex: 1;
    flex-direction: column;
    gap: 3px;
    min-width: 0;
`;

const Label = Styled.span`
    font: 500 12px/1.2 var(--font-ui);
    color: var(--text-primary);
`;

const Description = Styled.span`
    font: 400 11px/15px var(--font-ui);
    color: var(--text-muted);
`;

const Control = Styled.div`
    display: flex;
    flex-direction: column;
    flex-shrink: 0;
    align-items: flex-end;
    gap: 5px;
`;

const Readout = Styled.span`
    font: 400 10px/1 var(--font-mono);
    font-variant-numeric: tabular-nums;
    color: var(--text-muted);
`;
