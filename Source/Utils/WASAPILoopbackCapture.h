#pragma once

#include <JuceHeader.h>

#ifdef JUCE_WINDOWS
// WICHTIG: NOMINMAX vor Windows-Headers definieren um min/max Makro-Konflikte zu vermeiden
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <functiondiscoverykeys_devpkey.h>
#include <vector>
#include <atomic>
#include <thread>
#include <mutex>

/**
 * WASAPILoopbackCapture: Native Windows WASAPI Loopback Audio Capture
 * 
 * Ermöglicht das Capturen von ALLEM Windows Audio-Output ohne virtuelle Geräte.
 * Verwendet WASAPI im Loopback-Modus (AUDCLNT_STREAMFLAGS_LOOPBACK).
 * 
 * Funktionsweise:
 * 1. Verbindet sich mit dem Standard-Audio-Ausgabegerät
 * 2. Öffnet einen Loopback-Stream (captured was abgespielt wird)
 * 3. Liefert Audio-Daten in Echtzeit an den Buffer
 * 
 * WICHTIG: Erfordert Windows Vista oder neuer!
 * 
 * Vorteile gegenüber virtuellen Geräten:
 * - Keine zusätzliche Software nötig
 * - Captured ALLES (nicht nur was an virtuelle Geräte geroutet wird)
 * - Geringere Latenz
 * 
 * Nachteile:
 * - Nur Windows
 * - Shared-Mode (Samplerate des System-Outputs)
 */
class WASAPILoopbackCapture
{
public:
    //==========================================================================
    // Callback für empfangene Audio-Daten
    //==========================================================================
    using AudioCallback = std::function<void(const float* leftChannel, 
                                              const float* rightChannel, 
                                              int numSamples)>;
    
    //==========================================================================
    // Konstruktor / Destruktor
    //==========================================================================
    WASAPILoopbackCapture()
    {
        // COM initialisieren (falls noch nicht geschehen)
        HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        comInitialized = SUCCEEDED(hr) || hr == S_FALSE;  // S_FALSE = bereits initialisiert
    }
    
    ~WASAPILoopbackCapture()
    {
        stop();
        
        if (comInitialized)
        {
            CoUninitialize();
        }
    }
    
    //==========================================================================
    // Capture starten
    //==========================================================================
    bool start()
    {
        if (isRunning)
            return true;
        
        if (!initializeWASAPI())
            return false;
        
        isRunning = true;
        
        // Capture-Thread starten
        captureThread = std::thread([this]() { captureLoop(); });
        
        return true;
    }
    
    //==========================================================================
    // Capture stoppen
    //==========================================================================
    void stop()
    {
        isRunning = false;
        
        if (captureThread.joinable())
        {
            captureThread.join();
        }
        
        cleanupWASAPI();
    }
    
    //==========================================================================
    // Status
    //==========================================================================
    bool isCapturing() const { return isRunning; }
    
    double getSampleRate() const { return capturedSampleRate; }
    int getNumChannels() const { return capturedChannels; }
    
    //==========================================================================
    // Audio-Daten abrufen (für processBlock)
    //==========================================================================
    void setAudioCallback(AudioCallback callback)
    {
        std::lock_guard<std::mutex> lock(callbackMutex);
        audioCallback = callback;
    }
    
    /**
     * Holt die neuesten gecapturten Samples für die Verarbeitung (Lock-free!)
     * Gibt die Anzahl der tatsächlich kopierten Samples zurück
     */
    int getLatestSamples(juce::AudioBuffer<float>& destBuffer)
    {
        int available = ringFifo.getNumReady();
        if (available == 0)
            return 0;
        
        int numSamples = juce::jmin(available, destBuffer.getNumSamples());
        int numChannels = juce::jmin(ringBuffer.getNumChannels(), destBuffer.getNumChannels());
        
        auto scope = ringFifo.read(numSamples);
        
        for (int ch = 0; ch < numChannels; ++ch)
        {
            if (scope.blockSize1 > 0)
                destBuffer.copyFrom(ch, 0, ringBuffer, ch, scope.startIndex1, scope.blockSize1);
            if (scope.blockSize2 > 0)
                destBuffer.copyFrom(ch, scope.blockSize1, ringBuffer, ch, scope.startIndex2, scope.blockSize2);
        }
        
        return numSamples;
    }
    
    //==========================================================================
    // Verfügbare Ausgabegeräte auflisten
    //==========================================================================
    struct OutputDevice
    {
        juce::String name;
        juce::String id;
        bool isDefault = false;
    };
    
    std::vector<OutputDevice> getAvailableOutputDevices()
    {
        std::vector<OutputDevice> devices;
        
        IMMDeviceEnumerator* pDevEnum = nullptr;
        HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr,
                                       CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
                                       reinterpret_cast<void**>(&pDevEnum));
        
        if (FAILED(hr) || pDevEnum == nullptr)
            return devices;
        
        // Standard-Gerät abrufen
        IMMDevice* pDefaultDevice = nullptr;
        LPWSTR defaultId = nullptr;
        
        hr = pDevEnum->GetDefaultAudioEndpoint(eRender, eConsole, &pDefaultDevice);
        if (SUCCEEDED(hr) && pDefaultDevice != nullptr)
        {
            pDefaultDevice->GetId(&defaultId);
            pDefaultDevice->Release();
        }
        
        // Alle Geräte auflisten
        IMMDeviceCollection* pCollection = nullptr;
        hr = pDevEnum->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &pCollection);
        
        if (SUCCEEDED(hr) && pCollection != nullptr)
        {
            UINT count = 0;
            pCollection->GetCount(&count);
            
            for (UINT i = 0; i < count; ++i)
            {
                IMMDevice* pDevItem = nullptr;
                hr = pCollection->Item(i, &pDevItem);
                
                if (SUCCEEDED(hr) && pDevItem != nullptr)
                {
                    OutputDevice device;
                    
                    // ID abrufen
                    LPWSTR deviceId = nullptr;
                    pDevItem->GetId(&deviceId);
                    if (deviceId != nullptr)
                    {
                        device.id = juce::String(deviceId);
                        
                        // Prüfen ob Default
                        if (defaultId != nullptr && wcscmp(deviceId, defaultId) == 0)
                        {
                            device.isDefault = true;
                        }
                        
                        CoTaskMemFree(deviceId);
                    }
                    
                    // Name abrufen
                    IPropertyStore* pProps = nullptr;
                    hr = pDevItem->OpenPropertyStore(STGM_READ, &pProps);
                    if (SUCCEEDED(hr) && pProps != nullptr)
                    {
                        PROPVARIANT varName;
                        PropVariantInit(&varName);
                        
                        hr = pProps->GetValue(PKEY_Device_FriendlyName, &varName);
                        if (SUCCEEDED(hr))
                        {
                            device.name = juce::String(varName.pwszVal);
                            PropVariantClear(&varName);
                        }
                        
                        pProps->Release();
                    }
                    
                    devices.push_back(device);
                    pDevItem->Release();
                }
            }
            
            pCollection->Release();
        }
        
        if (defaultId != nullptr)
            CoTaskMemFree(defaultId);
        
        pDevEnum->Release();
        
        return devices;
    }
    
private:
    //==========================================================================
    // WASAPI Initialisierung
    //==========================================================================
    bool initializeWASAPI()
    {
        HRESULT hr;
        
        // Device Enumerator erstellen
        hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr,
                              CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
                              reinterpret_cast<void**>(&pEnumerator));
        if (FAILED(hr))
            return false;
        
        // Standard-Ausgabegerät abrufen (für Loopback)
        hr = pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pDevice);
        if (FAILED(hr))
            return false;
        
        // Audio Client erstellen
        hr = pDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL,
                               nullptr, reinterpret_cast<void**>(&pAudioClient));
        if (FAILED(hr))
            return false;
        
        // Mix-Format abrufen (System-Samplerate)
        WAVEFORMATEX* pwfx = nullptr;
        hr = pAudioClient->GetMixFormat(&pwfx);
        if (FAILED(hr))
            return false;
        
        capturedSampleRate = static_cast<double>(pwfx->nSamplesPerSec);
        capturedChannels = static_cast<int>(pwfx->nChannels);
        
        // Format-Info speichern für spätere Konvertierung
        if (pwfx->wFormatTag == WAVE_FORMAT_EXTENSIBLE)
        {
            WAVEFORMATEXTENSIBLE* pwfex = reinterpret_cast<WAVEFORMATEXTENSIBLE*>(pwfx);
            isFloatFormat = (pwfex->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT);
            bitsPerSample = pwfx->wBitsPerSample;
        }
        else
        {
            isFloatFormat = (pwfx->wFormatTag == WAVE_FORMAT_IEEE_FLOAT);
            bitsPerSample = pwfx->wBitsPerSample;
        }
        
        // Buffer-Dauer (100ms)
        REFERENCE_TIME hnsRequestedDuration = 1000000;  // 100ms in 100-ns Einheiten
        
        // Audio Client im LOOPBACK-Modus initialisieren!
        hr = pAudioClient->Initialize(
            AUDCLNT_SHAREMODE_SHARED,
            AUDCLNT_STREAMFLAGS_LOOPBACK,  // DER MAGISCHE FLAG!
            hnsRequestedDuration,
            0,
            pwfx,
            nullptr);
        
        if (FAILED(hr))
        {
            CoTaskMemFree(pwfx);
            return false;
        }
        
        // Capture Client erstellen
        hr = pAudioClient->GetService(__uuidof(IAudioCaptureClient),
                                       reinterpret_cast<void**>(&pCaptureClient));
        if (FAILED(hr))
        {
            CoTaskMemFree(pwfx);
            return false;
        }
        
        // Interne Buffer initialisieren (Lock-free Ring-Buffer)
        ringBuffer.setSize(juce::jmin(capturedChannels, 2), RING_BUFFER_SIZE);
        ringBuffer.clear();
        ringFifo.reset();
        
        // Pre-allokierte Callback-Buffer
        callbackLeftBuffer.resize(static_cast<size_t>(RING_BUFFER_SIZE));
        callbackRightBuffer.resize(static_cast<size_t>(RING_BUFFER_SIZE));
        
        CoTaskMemFree(pwfx);
        
        // Stream starten
        hr = pAudioClient->Start();
        return SUCCEEDED(hr);
    }
    
    //==========================================================================
    // WASAPI Cleanup
    //==========================================================================
    void cleanupWASAPI()
    {
        if (pAudioClient != nullptr)
        {
            pAudioClient->Stop();
        }
        
        if (pCaptureClient != nullptr)
        {
            pCaptureClient->Release();
            pCaptureClient = nullptr;
        }
        
        if (pAudioClient != nullptr)
        {
            pAudioClient->Release();
            pAudioClient = nullptr;
        }
        
        if (pDevice != nullptr)
        {
            pDevice->Release();
            pDevice = nullptr;
        }
        
        if (pEnumerator != nullptr)
        {
            pEnumerator->Release();
            pEnumerator = nullptr;
        }
    }
    
    //==========================================================================
    // Capture-Thread Hauptschleife
    //==========================================================================
    void captureLoop()
    {
        // COM pro Thread initialisieren (WASAPI erfordert dies)
        HRESULT comHr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        bool threadComInit = SUCCEEDED(comHr) || comHr == S_FALSE;
        
        // Thread-Priorität erhöhen für Echtzeit
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
        
        while (isRunning)
        {
            UINT32 packetLength = 0;
            HRESULT hr = pCaptureClient->GetNextPacketSize(&packetLength);
            
            if (FAILED(hr))
            {
                // Fehler - kurz warten und erneut versuchen
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }
            
            while (packetLength != 0 && isRunning)
            {
                BYTE* pData = nullptr;
                UINT32 numFramesAvailable = 0;
                DWORD flags = 0;
                
                hr = pCaptureClient->GetBuffer(&pData, &numFramesAvailable, &flags, nullptr, nullptr);
                
                if (SUCCEEDED(hr))
                {
                    bool isSilent = (flags & AUDCLNT_BUFFERFLAGS_SILENT) != 0;
                    
                    if (!isSilent && pData != nullptr && numFramesAvailable > 0)
                    {
                        // Audio-Daten verarbeiten (Float-Format angenommen)
                        processAudioData(reinterpret_cast<float*>(pData), 
                                         static_cast<int>(numFramesAvailable));
                    }
                    
                    pCaptureClient->ReleaseBuffer(numFramesAvailable);
                }
                
                hr = pCaptureClient->GetNextPacketSize(&packetLength);
                if (FAILED(hr))
                    break;
            }
            
            // Kurz warten um CPU nicht zu überlasten
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        
        // COM für diesen Thread aufräumen
        if (threadComInit)
            CoUninitialize();
    }
    
    //==========================================================================
    // Audio-Daten verarbeiten
    //==========================================================================
    void processAudioData(const float* data, int numFrames)
    {
        // Normalisierungsfaktor berechnen
        float maxSample = 0.0f;
        for (int frame = 0; frame < numFrames; ++frame)
        {
            for (int ch = 0; ch < capturedChannels; ++ch)
            {
                float sample = std::abs(data[frame * capturedChannels + ch]);
                if (sample > maxSample) maxSample = sample;
            }
        }
        float normFactor = (maxSample > 1.0f) ? (1.0f / maxSample) : 1.0f;
        
        // In Lock-free Ring-Buffer schreiben
        int channels = juce::jmin(capturedChannels, ringBuffer.getNumChannels());
        auto scope = ringFifo.write(numFrames);
        
        auto writeBlock = [&](int startIndex, int blockSize, int srcOffset)
        {
            for (int ch = 0; ch < channels; ++ch)
            {
                float* dest = ringBuffer.getWritePointer(ch, startIndex);
                for (int i = 0; i < blockSize; ++i)
                {
                    float sample = data[(srcOffset + i) * capturedChannels + ch] * normFactor;
                    dest[i] = juce::jlimit(-1.0f, 1.0f, sample);
                }
            }
        };
        
        if (scope.blockSize1 > 0)
            writeBlock(scope.startIndex1, scope.blockSize1, 0);
        if (scope.blockSize2 > 0)
            writeBlock(scope.startIndex2, scope.blockSize2, scope.blockSize1);
        
        // Callback aufrufen (mit pre-allozierten Buffern, kein Heap im Capture-Thread)
        {
            std::lock_guard<std::mutex> lock(callbackMutex);
            if (audioCallback && capturedChannels >= 2)
            {
                int cbSize = juce::jmin(numFrames, static_cast<int>(callbackLeftBuffer.size()));
                for (int i = 0; i < cbSize; ++i)
                {
                    callbackLeftBuffer[static_cast<size_t>(i)] = data[i * capturedChannels] * normFactor;
                    callbackRightBuffer[static_cast<size_t>(i)] = data[i * capturedChannels + 1] * normFactor;
                }
                audioCallback(callbackLeftBuffer.data(), callbackRightBuffer.data(), cbSize);
            }
        }
    }
    
    //==========================================================================
    // Member-Variablen
    //==========================================================================
    bool comInitialized = false;
    std::atomic<bool> isRunning{false};
    std::thread captureThread;
    
    // WASAPI Interfaces
    IMMDeviceEnumerator* pEnumerator = nullptr;
    IMMDevice* pDevice = nullptr;
    IAudioClient* pAudioClient = nullptr;
    IAudioCaptureClient* pCaptureClient = nullptr;
    
    // Captured Audio Info
    double capturedSampleRate = 48000.0;
    int capturedChannels = 2;
    bool isFloatFormat = true;
    int bitsPerSample = 32;
    
    // Audio Buffer (Lock-free Ring-Buffer)
    static constexpr int RING_BUFFER_SIZE = 8192;
    juce::AudioBuffer<float> ringBuffer { 2, RING_BUFFER_SIZE };
    juce::AbstractFifo ringFifo { RING_BUFFER_SIZE };
    
    // Pre-allokierte Buffer für Callback (kein Heap im Capture-Thread)
    std::vector<float> callbackLeftBuffer;
    std::vector<float> callbackRightBuffer;
    
    // Callback
    AudioCallback audioCallback;
    std::mutex callbackMutex;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WASAPILoopbackCapture)
};

#else
// Dummy-Klasse für nicht-Windows Plattformen
class WASAPILoopbackCapture
{
public:
    using AudioCallback = std::function<void(const float*, const float*, int)>;
    
    bool start() { return false; }
    void stop() {}
    bool isCapturing() const { return false; }
    double getSampleRate() const { return 44100.0; }
    int getNumChannels() const { return 2; }
    void setAudioCallback(AudioCallback) {}
    int getLatestSamples(juce::AudioBuffer<float>&) { return 0; }
    
    struct OutputDevice { juce::String name, id; bool isDefault = false; };
    std::vector<OutputDevice> getAvailableOutputDevices() { return {}; }
};
#endif // JUCE_WINDOWS


/**
 * SystemAudioCapture: High-Level Wrapper für System-Audio Capture
 * 
 * Kombiniert WASAPI Loopback (nativ) mit VirtualDeviceDetector (Fallback).
 * Wählt automatisch die beste verfügbare Methode.
 */
class SystemAudioCapture
{
public:
    enum class CaptureMethod
    {
        None,
        WASAPILoopback,     // Natives Windows Loopback
        VirtualDevice       // VB-Cable, Voicemeeter etc.
    };
    
    SystemAudioCapture() = default;
    
    /**
     * Startet System-Audio Capture mit der besten verfügbaren Methode
     */
    bool startCapture()
    {
        #ifdef JUCE_WINDOWS
        // Versuche zuerst natives WASAPI Loopback
        if (wasapiCapture.start())
        {
            currentMethod = CaptureMethod::WASAPILoopback;
            return true;
        }
        #endif
        
        // Fallback: Virtuelles Gerät (muss extern konfiguriert werden)
        currentMethod = CaptureMethod::VirtualDevice;
        return false;  // Virtuelles Gerät muss vom User eingerichtet werden
    }
    
    void stopCapture()
    {
        #ifdef JUCE_WINDOWS
        wasapiCapture.stop();
        #endif
        currentMethod = CaptureMethod::None;
    }
    
    bool isCapturing() const
    {
        #ifdef JUCE_WINDOWS
        return wasapiCapture.isCapturing();
        #else
        return false;
        #endif
    }
    
    CaptureMethod getCurrentMethod() const { return currentMethod; }
    
    juce::String getMethodName() const
    {
        switch (currentMethod)
        {
            case CaptureMethod::WASAPILoopback: return "WASAPI Loopback (Native)";
            case CaptureMethod::VirtualDevice:  return "Virtual Audio Device";
            default: return "None";
        }
    }
    
    /**
     * Holt gecapturte Samples (nur für WASAPI Loopback)
     */
    int getLatestSamples(juce::AudioBuffer<float>& buffer)
    {
        #ifdef JUCE_WINDOWS
        return wasapiCapture.getLatestSamples(buffer);
        #else
        juce::ignoreUnused(buffer);
        return 0;
        #endif
    }
    
    double getCapturedSampleRate() const
    {
        #ifdef JUCE_WINDOWS
        return wasapiCapture.getSampleRate();
        #else
        return 44100.0;
        #endif
    }
    
private:
    WASAPILoopbackCapture wasapiCapture;
    CaptureMethod currentMethod = CaptureMethod::None;
};
