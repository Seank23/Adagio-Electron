import { useRef } from 'react';
import { useDispatch, useSelector } from 'react-redux';
import Styled from '@emotion/styled';
import { Tooltip } from 'antd';
import { Panel } from '../Panel';
import AnalysisTab from './AnalysisTab';
import { setSidebarTab } from '../../store/settingsSlice';

const SIDEBAR_TABS = [
    { key: 'analysis', label: 'Analysis', Panel: AnalysisTab },
    { key: 'settings', label: 'Settings', disabled: true, hint: 'Coming later' },
];

const tabId = key => `sidebar-tab-${key}`;
const panelId = key => `sidebar-panel-${key}`;

const Sidebar = () => {
    const dispatch = useDispatch();
    const stored = useSelector(state => state.settings.sidebarTab);
    const enabled = SIDEBAR_TABS.filter(tab => !tab.disabled);
    const selected = enabled.find(tab => tab.key === stored) ?? enabled[0];
    const tabRefs = useRef({});

    const select = key => {
        dispatch(setSidebarTab(key));
        tabRefs.current[key]?.focus();
    };

    // The tab list owns the arrow keys, so they move between enabled tabs instead of seeking.
    const onKeyDown = event => {
        const index = enabled.indexOf(selected);
        const target = {
            ArrowLeft: enabled[(index - 1 + enabled.length) % enabled.length],
            ArrowRight: enabled[(index + 1) % enabled.length],
            Home: enabled[0],
            End: enabled[enabled.length - 1],
        }[event.key];
        if (!target)
            return;
        event.preventDefault();
        event.stopPropagation();
        select(target.key);
    };

    const { Panel: SelectedPanel } = selected;

    return (
        <Panel>
            <TabBar role="tablist" aria-label="Sidebar" onKeyDown={onKeyDown}>
                {SIDEBAR_TABS.map(tab => {
                    const isSelected = tab.key === selected.key;
                    const button = (
                        <Tab
                            key={tab.key}
                            ref={element => { tabRefs.current[tab.key] = element; }}
                            type="button"
                            role="tab"
                            id={tabId(tab.key)}
                            aria-selected={isSelected}
                            aria-controls={isSelected ? panelId(tab.key) : undefined}
                            aria-disabled={tab.disabled || undefined}
                            disabled={tab.disabled}
                            tabIndex={isSelected ? 0 : -1}
                            selected={isSelected}
                            onClick={() => select(tab.key)}
                        >
                            {tab.label}
                        </Tab>
                    );
                    // A disabled button gets no pointer events, so its tooltip hangs on a wrapper.
                    return tab.hint
                        ? (
                            <Tooltip key={tab.key} title={tab.hint} mouseEnterDelay={0.3}>
                                <TabSlot>{button}</TabSlot>
                            </Tooltip>
                        )
                        : button;
                })}
            </TabBar>
            <TabPanel role="tabpanel" id={panelId(selected.key)} aria-labelledby={tabId(selected.key)}>
                <SelectedPanel />
            </TabPanel>
        </Panel>
    );
};
export default Sidebar;

const TabBar = Styled.div`
    display: flex;
    align-items: stretch;
    flex-shrink: 0;
    gap: 18px;
    height: 36px;
    padding: 0 16px;
    border-bottom: 1px solid var(--border-subtle);
`;

const TabSlot = Styled.span`
    display: flex;
`;

// The underline sits on the bar's own border: the tab overlaps it by a pixel.
const Tab = Styled('button', { shouldForwardProp: prop => prop !== 'selected' })`
    margin-bottom: -1px;
    padding: 0 2px;
    border: 0;
    border-bottom: 2px solid ${({ selected }) => selected ? 'var(--accent-primary-fg)' : 'transparent'};
    background: none;
    font: ${({ selected }) => selected ? 600 : 500} 12px/1 var(--font-ui);
    color: ${({ selected }) => selected ? 'var(--text-primary)' : 'var(--text-secondary)'};
    cursor: pointer;
    &:hover:not(:disabled) {
        color: var(--text-primary);
    }
    &:disabled {
        color: var(--text-muted);
        cursor: default;
    }
    &:focus-visible {
        outline: 2px solid var(--accent-primary);
        outline-offset: -2px;
    }
`;

const TabPanel = Styled.div`
    flex: 1;
    min-height: 0;
    overflow-y: auto;
`;
