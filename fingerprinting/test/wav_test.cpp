#include <iostream>
#include "../audio/wav.h"

int main(int argc, char* argv[])
{
    if (argc != 2)
    {
        std::cout << "Usage: wav_test <wav_file_path>" << std::endl;
        return 0;
    }

    Wav wav(argv[1]);
    std::cout << "Audio format: " << wav.GetAudioFormat() << std::endl;
    std::cout << "Channel: " << wav.GetChannel() << std::endl;
    std::cout << "Sample rate: " << wav.GetSampleRate() << std::endl;
    std::cout << "Bit per sample: " << wav.GetBitPerSample() << std::endl;
    std::cout << "Data size: " << wav.GetDataSize() << std::endl;
    std::cout << "File size: " << wav.GetFileSize() << std::endl;


    Raw16bitPCM raw_pcm;
    wav.GetLowQualityPCM(raw_pcm);
    std::cout << "Raw PCM size: " << raw_pcm.size() << std::endl;
    wav.SaveWavFile(argv[1] + std::string("_changed.wav"), raw_pcm, 16000, 16, 1);
    return 0;
}