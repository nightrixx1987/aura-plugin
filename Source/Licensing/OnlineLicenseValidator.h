#pragma once

#include <JuceHeader.h>
#include <functional>
#include <memory>

/**
 * OnlineLicenseValidator: Asynchrone Server-seitige Lizenz-Validierung
 *
 * Nutzt juce::URL::createInputStream() auf einem Background-Thread
 * (analog zu UpdateChecker). Ergebnisse werden per Callback auf dem
 * Message-Thread zurueckgeliefert.
 *
 * API-Endpunkte:
 *   POST /api/activate.php    { license_key, machine_id, plugin_version }
 *   POST /api/deactivate.php  { license_key, machine_id, activation_token }
 *   POST /api/validate.php    { license_key, machine_id, activation_token }
 *
 * Antwort-Format (JSON):
 *   { "success": true/false, "message": "...", "activation_token": "...",
 *     "expires_at": "2027-01-01", "max_activations": 2, "current_activations": 1 }
 */
class OnlineLicenseValidator : private juce::Thread
{
public:
    // === Ergebnis-Struktur ===
    struct Result
    {
        bool success = false;
        bool networkError = false;      // true wenn Server nicht erreichbar
        juce::String message;           // Menschenlesbarer Status-Text
        juce::String activationToken;   // Server-generiertes Token
        juce::String expiresAt;         // Ablaufdatum (leer = unbefristet)
        int maxActivations = 1;
        int currentActivations = 0;
    };

    // === Callback-Typen ===
    using ResultCallback = std::function<void(const Result&)>;

    // === Request-Typen ===
    enum class RequestType
    {
        Activate,
        Deactivate,
        Validate
    };

    OnlineLicenseValidator()
        : juce::Thread("OnlineLicenseValidator")
    {
    }

    ~OnlineLicenseValidator() override
    {
        aliveFlag->store(false);
        stopThread(5000);
    }

    // === API-Basis-URL ===
    static juce::String getAPIBaseURL()
    {
        // Prüfe ob eine lokale Test-URL existiert (für Entwicklung)
        auto localTestFile = juce::File::getSpecialLocation(
                juce::File::userApplicationDataDirectory)
            .getChildFile("Aura")
            .getChildFile("license_api_url.txt");

        if (localTestFile.existsAsFile())
        {
            juce::String customURL = localTestFile.loadFileAsString().trim();
            if (customURL.isNotEmpty())
            {
                DBG("OnlineLicenseValidator: Verwende Test-URL: " + customURL);
                return customURL;
            }
        }

        return "https://www.unproved-audio.de/api";
    }

    // =================================================================
    // Asynchrone Requests (laufen auf Background-Thread)
    // =================================================================

    /**
     * Aktiviert eine Lizenz online.
     * @param licenseKey     Der Lizenz-Key (z.B. "AURA-XXXX-...")
     * @param machineID      Die Machine-ID des aktuellen Rechners
     * @param pluginVersion  Aktuelle Plugin-Version
     * @param callback       Wird auf dem Message-Thread aufgerufen
     */
    void activateOnline(const juce::String& licenseKey,
                        const juce::String& machineID,
                        const juce::String& pluginVersion,
                        ResultCallback callback)
    {
        if (isThreadRunning())
        {
            Result busy;
            busy.success = false;
            busy.message = "Eine Anfrage laeuft bereits. Bitte warten.";
            if (callback) callback(busy);
            return;
        }

        pendingRequest = RequestType::Activate;
        pendingLicenseKey = licenseKey;
        pendingMachineID = machineID;
        pendingPluginVersion = pluginVersion;
        pendingToken = "";
        pendingCallback = std::move(callback);

        startThread(juce::Thread::Priority::low);
    }

    /**
     * Deaktiviert eine Lizenz online (gibt Seat frei).
     */
    void deactivateOnline(const juce::String& licenseKey,
                          const juce::String& machineID,
                          const juce::String& activationToken,
                          ResultCallback callback)
    {
        if (isThreadRunning())
        {
            Result busy;
            busy.success = false;
            busy.message = "Eine Anfrage laeuft bereits. Bitte warten.";
            if (callback) callback(busy);
            return;
        }

        pendingRequest = RequestType::Deactivate;
        pendingLicenseKey = licenseKey;
        pendingMachineID = machineID;
        pendingToken = activationToken;
        pendingCallback = std::move(callback);

        startThread(juce::Thread::Priority::low);
    }

    /**
     * Validiert eine bestehende Aktivierung (periodischer Re-Check).
     */
    void validateOnline(const juce::String& licenseKey,
                        const juce::String& machineID,
                        const juce::String& activationToken,
                        ResultCallback callback)
    {
        if (isThreadRunning())
        {
            Result busy;
            busy.success = false;
            busy.message = "Eine Anfrage laeuft bereits. Bitte warten.";
            if (callback) callback(busy);
            return;
        }

        pendingRequest = RequestType::Validate;
        pendingLicenseKey = licenseKey;
        pendingMachineID = machineID;
        pendingToken = activationToken;
        pendingCallback = std::move(callback);

        startThread(juce::Thread::Priority::low);
    }

    /** Gibt zurueck ob gerade ein Request laeuft */
    bool isBusy() const { return isThreadRunning(); }

private:
    // === Pending Request State ===
    RequestType pendingRequest = RequestType::Validate;
    juce::String pendingLicenseKey;
    juce::String pendingMachineID;
    juce::String pendingPluginVersion;
    juce::String pendingToken;
    ResultCallback pendingCallback;

    // Shared alive-Flag (schuetzt callAsync-Lambdas vor Dangling Pointer)
    std::shared_ptr<std::atomic<bool>> aliveFlag =
        std::make_shared<std::atomic<bool>>(true);

    // =================================================================
    // Thread-Run: HTTP-Request ausfuehren
    // =================================================================
    void run() override
    {
        DBG("OnlineLicenseValidator: Thread gestartet fuer "
            + juce::String(pendingRequest == RequestType::Activate ? "Activate" :
                           pendingRequest == RequestType::Deactivate ? "Deactivate" : "Validate"));

        Result result = executeRequest();

        if (threadShouldExit())
            return;

        // Ergebnis auf dem Message-Thread zurueckliefern
        auto alive = aliveFlag;
        auto cb = pendingCallback;
        juce::MessageManager::callAsync([result, cb, alive]()
        {
            if (!alive->load())
                return;
            if (cb)
                cb(result);
        });
    }

    Result executeRequest()
    {
        Result result;

        // Endpunkt bestimmen
        juce::String endpoint;
        switch (pendingRequest)
        {
            case RequestType::Activate:   endpoint = "/activate.php";   break;
            case RequestType::Deactivate: endpoint = "/deactivate.php"; break;
            case RequestType::Validate:   endpoint = "/validate.php";   break;
        }

        juce::String fullURL = getAPIBaseURL() + endpoint;
        DBG("OnlineLicenseValidator: POST " + fullURL);

        // POST-Body als JSON zusammenbauen
        auto* jsonObj = new juce::DynamicObject();
        jsonObj->setProperty("license_key", pendingLicenseKey);
        jsonObj->setProperty("machine_id", pendingMachineID);

        if (pendingRequest == RequestType::Activate)
        {
            jsonObj->setProperty("plugin_version", pendingPluginVersion);
        }
        else
        {
            jsonObj->setProperty("activation_token", pendingToken);
        }

        // HMAC-Signatur fuer Request-Authentizitaet
        juce::String requestBody = juce::JSON::toString(juce::var(jsonObj));

        // HTTP-Request senden
        juce::URL url(fullURL);
        url = url.withPOSTData(requestBody);

        auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inPostData)
                          .withConnectionTimeoutMs(10000)
                          .withExtraHeaders("Content-Type: application/json")
                          .withResponseHeaders(nullptr);

        std::unique_ptr<juce::InputStream> stream;

        try
        {
            stream = url.createInputStream(options);
        }
        catch (...)
        {
            DBG("OnlineLicenseValidator: Exception beim Verbindungsaufbau");
            result.networkError = true;
            result.message = "Verbindung zum Lizenz-Server fehlgeschlagen.\n"
                             "Bitte Internetverbindung pruefen.";
            return result;
        }

        if (stream == nullptr)
        {
            DBG("OnlineLicenseValidator: Server nicht erreichbar (stream == null)");
            result.networkError = true;
            result.message = "Lizenz-Server nicht erreichbar.\n"
                             "Bitte Internetverbindung pruefen.";
            return result;
        }

        if (threadShouldExit())
            return result;

        juce::String response = stream->readEntireStreamAsString();
        stream.reset();

        DBG("OnlineLicenseValidator: Antwort (" + juce::String(response.length()) + " Zeichen): "
            + response.substring(0, 300));

        if (response.isEmpty())
        {
            result.networkError = true;
            result.message = "Leere Antwort vom Server.";
            return result;
        }

        // JSON parsen
        auto json = juce::JSON::parse(response);
        if (auto* obj = json.getDynamicObject())
        {
            result.success = obj->getProperty("success");
            result.message = obj->getProperty("message").toString();
            result.activationToken = obj->getProperty("activation_token").toString();
            result.expiresAt = obj->getProperty("expires_at").toString();
            result.maxActivations = obj->getProperty("max_activations");
            result.currentActivations = obj->getProperty("current_activations");
        }
        else
        {
            result.networkError = true;
            result.message = "Ungueltige Antwort vom Server.";
            DBG("OnlineLicenseValidator: JSON-Parse-Fehler: " + response.substring(0, 200));
        }

        return result;
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OnlineLicenseValidator)
};
