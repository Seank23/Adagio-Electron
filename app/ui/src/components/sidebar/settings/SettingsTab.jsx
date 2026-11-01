import { useSelector } from 'react-redux';
import PipelineBlock from './PipelineBlock';
import StageSection from './StageSection';
import { selectStages } from '../../../store/pipelineSlice';

// A form generated from the engine's schema: the pipeline, then a section per stage in pipeline order.
const SettingsTab = () => {
    const stages = useSelector(selectStages);
    const connected = useSelector(state => state.app.connectionState === 'connected');
    const disabled = !connected || stages.length === 0;

    return (
        <>
            <PipelineBlock stageCount={stages.length} disabled={disabled} />
            {stages.map((stage, index) => (
                <StageSection key={stage.name} stage={stage} index={index} disabled={disabled} />
            ))}
        </>
    );
};
export default SettingsTab;
