#include "AnalysisService.h"
#include "../Debug/Instrumentation.h"

#include "AnalysisProcessor.h"
#include "FFTProcessor.h"

#include "../Core/AudioUtils.h"

#include <kfr/base.hpp>
#include <kfr/dsp.hpp>
#include <kfr/io.hpp>

namespace Adagio
{
    AnalysisService::~AnalysisService()
    {

    }

    int AnalysisService::Init(AudioData& audioData, AnalysisParams& params)
    {
        ADAGIO_PROFILE_FUNCTION();
        m_AudioData = &audioData;
        m_Params = &params;
        float inRate = m_AudioData->PlaybackSampleRate;
        float outRate = m_Params->SampleRate;
        int sampleCount = m_AudioData->PlaybackStream[0].size();
        if (inRate != outRate)
        {
            auto r = kfr::resampler<float>(kfr::resample_quality::normal, m_Params->SampleRate, m_AudioData->PlaybackSampleRate);
            kfr::univector<float>* resampled = new kfr::univector<float>(sampleCount * outRate / inRate + r.get_delay());
            r.process(*resampled, AudioUtils::StereoToMono(m_AudioData->PlaybackStream));
            memcpy(&m_AudioData->AnalysisStream, resampled, sizeof(*resampled));
        }
        else
        {
            kfr::univector<float> monoStream = AudioUtils::StereoToMono(m_AudioData->PlaybackStream);
            memcpy(&m_AudioData->AnalysisStream, &monoStream, sizeof(monoStream));
        }
        m_AudioData->AnalysisSampleRate = outRate;
        m_Processors.push_back(new FFTProcessor());
        return 1;
    }

    kfr::univector<float> AnalysisService::AnalyseFrame(long startSample)
    {
        ADAGIO_PROFILE_FUNCTION();
        long endSample = startSample + m_Params->FrameLength;
        if (endSample > m_AudioData->AnalysisStream.size())
            startSample -= endSample - m_AudioData->AnalysisStream.size();
        std::vector<float> audioFrame(&m_AudioData->AnalysisStream[startSample], &m_AudioData->AnalysisStream[endSample]);
        kfr::univector<float> spectrum = GenerateSpectrum(kfr::make_univector(audioFrame));
        return spectrum;
    }

    kfr::univector<float> AnalysisService::GenerateSpectrum(kfr::univector<float> audioFrame)
    {
        ADAGIO_PROFILE_FUNCTION();
        void* data = (void*)&audioFrame;

        for (int i = 0; i < m_Processors.size(); i++) 
        {
            ADAGIO_PROFILE_SCOPE("FFTProcessor::Execute()");
            data = m_Processors[i]->Execute(data, nullptr);
        }
        kfr::univector<float>& cast = *reinterpret_cast<kfr::univector<float>*>(data);
        return cast;
    }
}
