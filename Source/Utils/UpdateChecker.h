#pragma once

#include <JuceHeader.h>
#include "../Utils/VersionInfo.h"

/**
 * UpdateChecker: Prüft asynchron auf neue Plugin-Versionen.
 * 
 * - Läuft auf einem Background-Thread (blockiert nicht die GUI oder Audio)
 * - Rate-Limiting: maximal 1x pro Tag
 * - Ergebnis wird per Listener ans UI gemeldet
 * - Settings (lastChecked, skippedVersion) in PropertiesFile
 */
class UpdateChecker : private juce::Thread
{
public:
    struct UpdateInfo
    {
        juce::String latestVersion;
        juce::String downloadURL;
        juce::String changelog;
        juce::String message;
        bool updateAvailable = false;
    };
    
    class Listener
    {
    public:
        virtual ~Listener() = default;
        virtual void updateCheckCompleted(const UpdateInfo& info) = 0;
    };
    
    UpdateChecker()
        : juce::Thread("UpdateChecker")
    {
    }
    
    ~UpdateChecker() override
    {
        aliveFlag->store(false);  // Signalisiert pending callAsync-Lambdas, dass wir tot sind
        stopThread(3000);
    }
    
    void addListener(Listener* l) { listeners.add(l); }
    void removeListener(Listener* l) { listeners.remove(l); }
    
    /**
     * Startet den Update-Check (asynchron).
     * @param force  Wenn true, wird auch geprüft wenn heute schon geprüft wurde
     */
    void checkForUpdates(bool force = false)
    {
        if (isThreadRunning())
            return;
        
        forceCheck = force;
        startThread(juce::Thread::Priority::low);
    }
    
    /**
     * Markiert eine Version als "übersprungen" — wird nicht mehr angezeigt.
     */
    void skipVersion(const juce::String& version)
    {
        auto settings = getSettings();
        if (settings != nullptr)
        {
            settings->setValue("update_skipped_version", version);
            settings->save();
        }
    }
    
    /**
     * Gibt die zuletzt gecachte UpdateInfo zurück.
     */
    UpdateInfo getLastResult() const { return lastResult; }
    
private:
    juce::ListenerList<Listener> listeners;
    UpdateInfo lastResult;
    bool forceCheck = false;
    std::atomic<bool> hasCheckedThisSession { false };
    
    // Shared Flag: überlebt den UpdateChecker und schützt callAsync-Lambdas vor Dangling Pointer
    std::shared_ptr<std::atomic<bool>> aliveFlag = std::make_shared<std::atomic<bool>>(true);
    
    void run() override
    {
        DBG("UpdateChecker: Thread gestartet");
        
        // Rate-Limiting: maximal 1x pro Tag (außer force oder erster Check der Session)
        if (!forceCheck && hasCheckedThisSession.load() && !shouldCheck())
        {
            DBG("UpdateChecker: Rate-Limited, ueberspringe Check");
            return;
        }
        
        juce::String url = VersionInfo::getUpdateURL();
        DBG("UpdateChecker: Pruefe URL: " + url);
        
        // HTTP-Request an den Update-Server
        juce::URL updateURL(url);
        
        auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                        .withConnectionTimeoutMs(8000)
                        .withResponseHeaders(nullptr);
        
        std::unique_ptr<juce::InputStream> stream;
        
        try
        {
            stream = updateURL.createInputStream(options);
        }
        catch (...)
        {
            DBG("UpdateChecker: Exception beim Verbindungsaufbau");
            return;
        }
        
        if (stream == nullptr)
        {
            DBG("UpdateChecker: Konnte Server nicht erreichen (stream == null)");
            return;
        }
        
        if (threadShouldExit())
            return;
        
        juce::String response = stream->readEntireStreamAsString();
        stream.reset();  // Stream schliessen
        
        DBG("UpdateChecker: Antwort erhalten (" + juce::String(response.length()) + " Zeichen)");
        
        if (response.isEmpty())
        {
            DBG("UpdateChecker: Leere Antwort vom Server");
            return;
        }
        
        // JSON parsen
        auto json = juce::JSON::parse(response);
        
        if (auto* obj = json.getDynamicObject())
        {
            UpdateInfo info;
            info.latestVersion = obj->getProperty("latest_version").toString();
            info.downloadURL = obj->getProperty("download_url").toString();
            info.changelog = obj->getProperty("changelog").toString();
            info.message = obj->getProperty("message").toString();
            
            DBG("UpdateChecker: Server Version = " + info.latestVersion 
                + ", Aktuelle Version = " + VersionInfo::getCurrentVersion());
            
            // Prüfe ob die Version neuer ist
            info.updateAvailable = (VersionInfo::compareVersions(
                VersionInfo::getCurrentVersion(), info.latestVersion) < 0);
            
            // Prüfe ob diese Version übersprungen wurde
            auto settings = getSettings();
            if (settings != nullptr)
            {
                juce::String skipped = settings->getValue("update_skipped_version", "");
                if (skipped == info.latestVersion && !forceCheck)
                {
                    DBG("UpdateChecker: Version " + info.latestVersion + " wurde uebersprungen");
                    info.updateAvailable = false;
                }
                
                // Timestamp NUR bei erfolgreichem Check speichern
                settings->setValue("update_last_checked", juce::String(juce::Time::currentTimeMillis()));
                settings->save();
            }
            
            lastResult = info;
            hasCheckedThisSession.store(true);
            
            DBG("UpdateChecker: Update verfuegbar = " + juce::String(info.updateAvailable ? "JA" : "NEIN"));
            
            if (threadShouldExit())
                return;
            
            // Ergebnis auf dem Message-Thread an Listener melden
            // Kopie von info für Lambda-Capture
            UpdateInfo infoCopy = info;
            auto alive = aliveFlag;  // shared_ptr Kopie hält Flag am Leben
            juce::MessageManager::callAsync([this, infoCopy, alive]()
            {
                // Safety: Prüfe ob UpdateChecker noch existiert bevor this zugegriffen wird
                if (!alive->load())
                    return;
                listeners.call([&infoCopy](Listener& l) { l.updateCheckCompleted(infoCopy); });
            });
        }
        else
        {
            DBG("UpdateChecker: JSON Parse-Fehler: " + response.substring(0, 200));
        }
    }
    
    bool shouldCheck() const
    {
        auto settings = getSettings();
        if (settings == nullptr)
            return true;
        
        juce::String lastCheckedStr = settings->getValue("update_last_checked", "0");
        juce::int64 lastChecked = lastCheckedStr.getLargeIntValue();
        juce::int64 now = juce::Time::currentTimeMillis();
        
        // Mindestens 24 Stunden seit dem letzten Check
        constexpr juce::int64 oneDay = 24 * 60 * 60 * 1000;
        return (now - lastChecked) > oneDay;
    }
    
    std::unique_ptr<juce::PropertiesFile> getSettings() const
    {
        juce::PropertiesFile::Options opts;
        opts.applicationName = "Aura";
        opts.filenameSuffix = ".settings";
        opts.folderName = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                            .getChildFile("Aura").getFullPathName();
        return std::make_unique<juce::PropertiesFile>(opts);
    }
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UpdateChecker)
};
