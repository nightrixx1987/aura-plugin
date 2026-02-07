#pragma once

#include <JuceHeader.h>
#include <ctime>
#include <mutex>
#include <functional>
#include "OnlineLicenseValidator.h"

/**
 * LicenseManager v2: Gehaertetes Lizenz-System
 *
 * Sicherheits-Features:
 * - MD5-HMAC basierte Key-Validierung (128-Bit statt 16-Bit)
 * - Hardware-Binding (Machine ID aus Volume Serial + Computer Name)
 * - Obfuscated Secret (XOR-fragmentiert, kein String-Literal)
 * - Anti-Tampering fuer Trial-Daten (MD5-Integritaetspruefung)
 * - Uhrzurueckstellung-Erkennung (Last-Seen Timestamp)
 *
 * Key-Format: AURA-CCCC-MMMM-SSSSSSSS (23 Zeichen)
 *   CCCC = Kundennummer (4 Zeichen alphanumerisch)
 *   MMMM = Machine-ID Prefix (erste 4 Zeichen der Machine-ID)
 *   SSSSSSSS = MD5-HMAC Signatur (8 Hex-Zeichen)
 */
class LicenseManager
{
public:
    enum class LicenseStatus
    {
        Trial,           // Trial-Periode aktiv (30 Tage)
        TrialExpired,    // Trial abgelaufen
        Licensed,        // Gueltige Lizenz (machine-bound)
        Unlicensed       // Keine Lizenz / Invalid
    };

    static LicenseManager& getInstance();

    // Singleton darf nicht kopiert/verschoben werden
    LicenseManager(const LicenseManager&) = delete;
    LicenseManager& operator=(const LicenseManager&) = delete;

    // === Lizenz-Status (thread-safe, cached) ===
    LicenseStatus getLicenseStatus();
    bool isFullyLicensed();      // Licensed == true
    bool shouldNagUser();        // Trial oder Expired

    // === Trial-Infos ===
    int getTrialDaysRemaining();
    bool isTrialPeriod();
    bool isTrialExpired();

    // === Lizenz-Management (Offline) ===
    bool activateLicense(const juce::String& licenseKey);
    juce::String getLicenseKey();
    bool clearLicense();

    // === Online-Lizenz-Management ===
    using OnlineCallback = std::function<void(bool success, const juce::String& message)>;

    /** Aktiviert Lizenz online beim Server. Callback auf Message-Thread. */
    void activateOnline(const juce::String& licenseKey, OnlineCallback callback);

    /** Deaktiviert Lizenz online (gibt Seat frei fuer Rechnerwechsel). */
    void deactivateOnline(OnlineCallback callback);

    /** Periodischer Online-Re-Check (alle 7 Tage wenn Internet vorhanden). */
    void performPeriodicValidation(OnlineCallback callback = nullptr);

    /** Validiert Lizenz bei jedem Plugin-Start gegen den Server. */
    void validateOnStartup(OnlineCallback callback = nullptr);

    /** Prueft ob ein Online-Recheck faellig ist. */
    bool isOnlineRecheckDue() const;

    /** True wenn Online-Aktivierung vorliegt (vs. Legacy-Offline). */
    bool isOnlineActivated() const;

    /** Gibt die Tage seit letztem erfolgreichen Online-Check zurueck. */
    int getDaysSinceLastOnlineCheck() const;

    /** Gibt den Aktivierungs-Token zurueck (leer wenn offline). */
    juce::String getActivationToken() const;

    /** Offline-Grace-Period: Tage die noch offline genutzt werden kann. */
    int getOfflineGraceDaysRemaining() const;

    // === Konstanten ===
    static constexpr int ONLINE_RECHECK_INTERVAL_DAYS = 7;
    static constexpr int OFFLINE_GRACE_PERIOD_DAYS = 30;
    static constexpr int OFFLINE_GRACE_EXTRA_DAYS = 7;  // Kulanz nach Ablauf

    // === Machine ID (fuer den Benutzer sichtbar) ===
    juce::String getMachineID();

    // === Display-Text ===
    juce::String getStatusText();

    // === Sekundaerer Lizenz-Check (fuer verteiltes Enforcement) ===
    // Gibt 1.0f zurueck wenn lizenziert/trial, 0.0f wenn expired
    // Wird von DSP-Code aufgerufen fuer subtile Degradierung
    float getEnforcementFactor();

private:
    LicenseManager();
    ~LicenseManager() = default;

    // Cached Status (atomic fuer thread-safety)
    std::atomic<int> cachedStatus { 0 };
    std::atomic<float> cachedEnforcementFactor { 1.0f };
    void updateCachedStatus();

    // Online-Validator
    OnlineLicenseValidator onlineValidator;

    // Online-Aktivierungs-Daten aus PropertiesFile lesen/schreiben
    void storeOnlineActivation(const juce::String& token, const juce::String& expiresAt);
    void clearOnlineActivation();
    time_t getLastOnlineCheckTime() const;
    void updateLastOnlineCheckTime();

    // Thread-Safety: Recursive-Mutex fuer Properties-Zugriffe
    // (recursive weil z.B. clearLicense() -> updateCachedStatus() -> getLicenseStatus() denselben Mutex braucht)
    std::recursive_mutex propertiesMutex;

    // Persistierung
    juce::PropertiesFile& getPropertiesFile();
    std::unique_ptr<juce::PropertiesFile> propertiesFile;

    // === Key-Validierung (MD5-HMAC + Machine Binding) ===
    bool validateLicenseKey(const juce::String& key);
    juce::String computeKeySignature(const juce::String& customerID,
                                      const juce::String& machineID);

    // === Hardware-Fingerprint ===
    juce::String computeMachineFingerprint();
    juce::String cachedMachineID;

    // === Obfuscated Secret (XOR-fragmentiert) ===
    static juce::String assembleSecret();

    // === Trial-Datum Management ===
    void initializeTrialDate();
    time_t getTrialStartDate();

    // === Anti-Tampering ===
    juce::String computeIntegrityHash(const juce::String& data);
    bool verifyTrialIntegrity();
    void updateLastSeen();

    // === Offline-Grace-Period ===
    bool isOfflineGracePeriodExpired() const;

};
