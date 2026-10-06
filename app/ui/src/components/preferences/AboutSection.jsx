import Styled from '@emotion/styled';
import { ExternalLink } from 'lucide-react';
import BrandMark from '../controls/BrandMark';
import { PreferenceSection } from './PreferenceRow';
import { APP_VERSION, BUILD_DATE, REPO_URL, formatBuildDate } from '../../utils/appInfo';

// target="_blank" hands the link to main's window-open handler, which opens the browser. Without
// a target it would navigate the app window itself.
const AboutSection = () => (
    <PreferenceSection title="About">
        <About>
            <BrandMark size={36} />
            <Lines>
                <Name>Adagio</Name>
                <Version>
                    Version {APP_VERSION} <Dot>·</Dot> Updated {formatBuildDate(BUILD_DATE)}
                </Version>
                <Link href={REPO_URL} target="_blank" rel="noreferrer">
                    {REPO_URL.replace('https://', '')}
                    <ExternalLink size={12} />
                </Link>
            </Lines>
        </About>
    </PreferenceSection>
);
export default AboutSection;

const About = Styled.div`
    display: flex;
    align-items: center;
    gap: 14px;
    padding: 10px 0 14px;
`;

const Lines = Styled.div`
    display: flex;
    flex-direction: column;
    gap: 4px;
    min-width: 0;
`;

const Name = Styled.span`
    font: 600 13px/1.2 var(--font-ui);
    color: var(--text-primary);
`;

const Version = Styled.span`
    font: 400 11px/1.2 var(--font-ui);
    color: var(--text-secondary);
`;

const Dot = Styled.span`
    color: var(--text-muted);
`;

const Link = Styled.a`
    display: inline-flex;
    align-items: center;
    align-self: flex-start;
    gap: 5px;
    font: 500 11px/1.2 var(--font-ui);
    color: var(--accent-primary-text);
    text-decoration: none;
    border-radius: 3px;
    &:hover { text-decoration: underline; }
    &:focus-visible {
        outline: 2px solid var(--accent-primary);
        outline-offset: 2px;
    }
`;
