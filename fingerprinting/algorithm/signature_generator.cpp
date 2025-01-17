#include "signature_generator.h"
#include "../utils/array.h"
#include "../utils/hanning.h"
#include "../utils/debug.h"

#include <algorithm>
#include <numeric>

SignatureGenerator::SignatureGenerator()
    : mInputPendingProcessing()
    , mSampleProcessed(0)
    , mMaxTimeSeconds(3.1)
    , mNextSignature(16000, 0)
    , mRingBufferOfSamples(2048, 0)
    , mFFTOutputs(256, FFT::RealArray(1025, 0.0))
    , mSpreadFFTsOutput(256, FFT::RealArray(1025, 0.0))
{
}

void SignatureGenerator::FeedInput(const Raw16bitPCM& input)
{
    mInputPendingProcessing.reserve(mInputPendingProcessing.size() + input.size());
    mInputPendingProcessing.insert(mInputPendingProcessing.end(), input.begin(), input.end());
}

Signature SignatureGenerator::GetNextSignature()
{
    if (mInputPendingProcessing.size() - mSampleProcessed < 128)
    {
        throw std::runtime_error("Not enough input to generate signature");
    }

    while (mInputPendingProcessing.size() - mSampleProcessed >= 128 &&
        ((double)mNextSignature.NumberOfSamples() / mNextSignature.SampleRate() < mMaxTimeSeconds ||
        mNextSignature.SumOfPeaksLength() < MAX_PEAKS))
    {
        Raw16bitPCM input(mInputPendingProcessing.begin() + mSampleProcessed,
            mInputPendingProcessing.begin() + mSampleProcessed + 128);
        
        processInput(input);
        mSampleProcessed += 128;

    }

    Signature result = std::move(mNextSignature);
    
    mNextSignature = Signature(16000, 0);
    mRingBufferOfSamples = RingBuffer<std::int16_t>(2048, 0);
    mFFTOutputs = RingBuffer<FFT::RealArray>(256, FFT::RealArray(1025, 0.0));
    mSpreadFFTsOutput = RingBuffer<FFT::RealArray>(256, FFT::RealArray(1025, 0.0));

    return result; // RVO
}

void SignatureGenerator::processInput(const Raw16bitPCM& input)
{
    mNextSignature.AddNumberOfSamples(input.size());
    for (std::size_t chunk = 0; chunk < input.size(); chunk += 128)
    {
        Raw16bitPCM chunk_input(input.begin() + chunk, input.begin() + chunk + 128);

        doFFT(chunk_input);
        doPeakSpreadingAndRecoginzation();
    }
}

void SignatureGenerator::doFFT(const Raw16bitPCM& input)
{
    std::copy(input.begin(), input.end(),
        mRingBufferOfSamples.begin() + mRingBufferOfSamples.Position());

    mRingBufferOfSamples.Position() += input.size();
    mRingBufferOfSamples.Position() %= 2048;
    mRingBufferOfSamples.NumWritten() += input.size();

    FFT::RealArray excerpt_from_ring_buffer(2048, 0.0);

    std::copy(mRingBufferOfSamples.begin() + mRingBufferOfSamples.Position(), mRingBufferOfSamples.end(),
        excerpt_from_ring_buffer.begin());

    std::copy(mRingBufferOfSamples.begin(), mRingBufferOfSamples.begin() + mRingBufferOfSamples.Position(),
        excerpt_from_ring_buffer.begin() + 2048 - mRingBufferOfSamples.Position());

    array::multiply(excerpt_from_ring_buffer, HANNIG_MATRIX);

    FFT::RealArray real;
    FFT::RealArray imag;
    FFT::RFFT(excerpt_from_ring_buffer, real, imag);

    // do max((real^2 + imag^2) / (1 << 17), 0.0000000001)

    for (int i = 0; i < 1025; ++i)
    {
        real[i] *= real[i];
        imag[i] *= imag[i];

        real[i] += imag[i];
        real[i] /= (1 << 17);
        real[i] = real[i] < 0.0000000001 ? 0.0000000001 : real[i];
    }

    mFFTOutputs.Append(real);
}

void SignatureGenerator::doPeakSpreadingAndRecoginzation()
{
    doPeakSpreading();

    if (mSpreadFFTsOutput.NumWritten() >= 47)
    {
        doPeakRecognition();
    }
}

void SignatureGenerator::doPeakSpreading()
{
    auto spread_last_fft = mFFTOutputs[mFFTOutputs.Position() - 1];
    
    for (auto position = 0u; position < 1025; ++position)
    {
        if (position < 1023)
        {
            spread_last_fft[position] = *std::max_element(spread_last_fft.begin() + position,
                spread_last_fft.begin() + position + 3);
        }
            
        auto max_value = spread_last_fft[position];
        for (auto former_fft_num : {-1, -3, -6})
        {
            auto& former_fft_ouput = mSpreadFFTsOutput[(mSpreadFFTsOutput.Position() + former_fft_num) % mSpreadFFTsOutput.Size()];
            former_fft_ouput[position] = max_value = std::max(max_value, former_fft_ouput[position]);
        }
    }
    mSpreadFFTsOutput.Append(spread_last_fft);
}

void SignatureGenerator::doPeakRecognition()
{
    const auto& fft_minus_46 = mFFTOutputs[(mFFTOutputs.Position() - 46) % mFFTOutputs.Size()];
    const auto& fft_minus_49 = mSpreadFFTsOutput[(mSpreadFFTsOutput.Position() - 49) % mSpreadFFTsOutput.Size()];

    for (auto bin_position = 10u; bin_position < 1025; ++bin_position)
    {
        if (fft_minus_46[bin_position] >= 1.0 / 64.0 &&
            fft_minus_46[bin_position] >= fft_minus_49[bin_position])
        {
            auto max_neighbor_in_fft_minus_49 = 0.0l;
            for (auto neighbor_offset : {-10, -7, -4, -3, 1, 2, 5, 8})
            {
                max_neighbor_in_fft_minus_49 = std::max(max_neighbor_in_fft_minus_49,
                    fft_minus_49[bin_position + neighbor_offset]);
            }

            if (fft_minus_46[bin_position] > max_neighbor_in_fft_minus_49)
            {
                auto max_neighbor_in_other_adjacent_ffts = max_neighbor_in_fft_minus_49;
                for (auto other_offset : {-53, -45, 165, 172, 179, 186, 193, 200, 214, 221, 228, 235, 242, 249})
                {
                    max_neighbor_in_other_adjacent_ffts = std::max(
                        max_neighbor_in_other_adjacent_ffts,
                        mSpreadFFTsOutput[(mSpreadFFTsOutput.Position() + other_offset) % mSpreadFFTsOutput.Size()][bin_position -1]);
                }

                if (fft_minus_46[bin_position] > max_neighbor_in_other_adjacent_ffts)
                {
                    auto fft_number = mSpreadFFTsOutput.NumWritten() - 46;
                    auto peak_magnitude = std::log(std::max(1.0l / 64, fft_minus_46[bin_position])) * 1477.3 + 6144;
                    auto peak_magnitude_before = std::log(std::max(1.0l / 64, fft_minus_46[bin_position - 1])) * 1477.3 + 6144;
                    auto peak_magnitude_after = std::log(std::max(1.0l / 64, fft_minus_46[bin_position + 1])) * 1477.3 + 6144;

                    auto peak_variation_1 = peak_magnitude * 2 - peak_magnitude_before - peak_magnitude_after;
                    auto peak_variation_2 = (peak_magnitude_after - peak_magnitude_before) * 32 / peak_variation_1;

                    auto corrected_peak_frequency_bin = bin_position * 64.0 + peak_variation_2;
                    auto frequency_hz = corrected_peak_frequency_bin * (16000.0l / 2. / 1024. / 64.);

                    auto band = FrequancyBand();
                    if (frequency_hz < 250)
                        continue;
                    else if (frequency_hz < 520)
                        band = FrequancyBand::_250_520;
                    else if (frequency_hz < 1450)
                        band = FrequancyBand::_520_1450;
                    else if (frequency_hz < 3500)
                        band = FrequancyBand::_1450_3500;
                    else if (frequency_hz <= 5500)
                        band = FrequancyBand::_3500_5500;
                    else
                        continue;

                    auto& band_to_sound_peaks = mNextSignature.FrequancyBandToPeaks();
                    if (band_to_sound_peaks.find(band) == band_to_sound_peaks.end())
                    {
                        band_to_sound_peaks[band] = std::list<FrequancyPeak>();
                    }

                    band_to_sound_peaks[band].push_back(
                        FrequancyPeak(fft_number, int(peak_magnitude), int(corrected_peak_frequency_bin), LOW_QUALITY_SAMPLE_RATE));
                }
            }
        }
    }
}

void SignatureGenerator::prepareInput()
{
    mNextSignature.Reset(16000, 0);
    mRingBufferOfSamples = RingBuffer<std::int16_t>(2048, 0);
    mFFTOutputs = RingBuffer<FFT::RealArray>(256, FFT::RealArray(1025, 0.0));
    mSpreadFFTsOutput = RingBuffer<FFT::RealArray>(256, FFT::RealArray(1025, 0.0));
}