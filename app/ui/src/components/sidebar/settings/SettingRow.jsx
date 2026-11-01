import { useCallback } from 'react';
import Styled from '@emotion/styled';
import { Slider, Tooltip } from 'antd';
import { RotateCcw } from 'lucide-react';
import NumberField from '../../controls/NumberField';
import Segmented from '../../controls/Segmented';
import Select from '../../controls/Select';
import Switch from '../../controls/Switch';
import { useEngineSlider } from '../../../hooks/useEngineSlider';
import { useReport } from '../../../hooks/useReport';
import { useSettingCommit } from '../../../hooks/useSettingCommit';
import {
    controlKind, formatSetting, isChanged, normaliseSetting, parseSetting, sliderRange,
} from '../../../utils/settings';

// One setting, with the control its schema entry calls for (utils/settings' controlKind). Every value
// goes to the engine and shows only once it answers ok; a refusal goes to the status bar.
const SettingRow = ({ stage, setting, disabled }) => {
    const commit = useSettingCommit();
    const report = useReport();
    const send = useCallback(value => commit(stage, setting.key, value), [commit, stage, setting.key]);
    const changed = isChanged(setting);

    const resetButton = changed && (
        <ResetSetting
            label={setting.name}
            defaultText={formatSetting(setting, setting.default)}
            disabled={disabled}
            onClick={async () => report(await send(setting.default))}
        />
    );

    switch (controlKind(setting)) {
    case 'number':
        return <NumberSetting setting={setting} send={send} changed={changed} disabled={disabled} resetButton={resetButton} />;
    case 'switch':
        return (
            <Row compact>
                <Label>{setting.name}</Label>
                {resetButton}
                <Switch label={setting.name} checked={setting.value === 'Yes'} disabled={disabled}
                    onChange={async checked => report(await send(checked ? 'Yes' : 'No'))} />
            </Row>
        );
    case 'segmented':
        return (
            <Row>
                <Label>{setting.name}</Label>
                {resetButton}
                <Segmented label={setting.name} value={setting.value} disabled={disabled}
                    options={setting.options.map(option => ({ value: option, label: option }))}
                    onChange={async option => report(await send(option))} />
            </Row>
        );
    default:
        return (
            <Row>
                <Label>{setting.name}</Label>
                {resetButton}
                <Select label={setting.name} value={setting.value} disabled={disabled}
                    options={setting.options.map(option => ({ value: option, label: option }))}
                    onChange={async option => report(await send(option))} />
            </Row>
        );
    }
};
export default SettingRow;

const identity = value => value;

const NumberSetting = ({ setting, send, changed, disabled, resetButton }) => {
    const range = sliderRange(setting);
    const slider = useEngineSlider(setting.value, send, {
        toDisplay: identity,
        fromDisplay: value => normaliseSetting(setting, value),
    });
    const format = value => formatSetting(setting, value);

    return (
        <Column>
            <Row>
                <Label>{setting.name}</Label>
                {resetButton}
                <NumberField
                    label={setting.name}
                    value={slider.value}
                    unit={setting.unit}
                    step={range.step}
                    min={setting.min}
                    max={setting.max}
                    format={format}
                    parse={text => parseSetting(setting, text)}
                    changed={changed}
                    disabled={disabled}
                    onCommit={slider.onChangeComplete}
                />
            </Row>
            <Track
                min={range.min}
                max={range.max}
                step={range.step}
                value={slider.value}
                disabled={disabled}
                tooltip={{ open: false }}
                ariaLabelForHandle={setting.name}
                ariaValueTextFormatterForHandle={format}
                onChange={slider.onChange}
                onChangeComplete={slider.onChangeComplete}
            />
        </Column>
    );
};

const ResetSetting = ({ label, defaultText, disabled, onClick }) => (
    <Tooltip title={`Reset to ${defaultText}`} mouseEnterDelay={0.5}>
        <ResetButton type="button" aria-label={`Reset ${label}`} disabled={disabled} onClick={onClick}>
            <RotateCcw size={12} aria-hidden />
        </ResetButton>
    </Tooltip>
);

const Column = Styled.div`
    display: flex;
    flex-direction: column;
    gap: 8px;
`;

const Row = Styled('div', { shouldForwardProp: prop => prop !== 'compact' })`
    display: flex;
    align-items: center;
    gap: 8px;
    min-height: 24px;
    padding: ${({ compact }) => compact ? '4px 0' : 0};
`;

const Label = Styled.span`
    flex: 1;
    min-width: 0;
    font: 400 12px/1.3 var(--font-ui);
    color: var(--text-primary);
`;

const ResetButton = Styled.button`
    display: inline-flex;
    align-items: center;
    justify-content: center;
    flex-shrink: 0;
    width: 20px;
    height: 20px;
    padding: 0;
    border: none;
    border-radius: 4px;
    background: none;
    color: var(--text-secondary);
    cursor: pointer;
    &:hover:not(:disabled) { background: var(--bg-surface); color: var(--text-primary); }
    &:disabled { opacity: .5; cursor: default; }
    &:focus-visible { outline: 2px solid var(--accent-primary); outline-offset: 1px; }
`;

const Track = Styled(Slider)`
    margin: 0 6px;
`;
