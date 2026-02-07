#include "LicenseManager.h"
#include "../Utils/VersionInfo.h"
#include <ctime>

#if JUCE_WINDOWS
  #include <windows.h>
#endif

LicenseManager& LicenseManager::getInstance()
{
    static LicenseManager instance;  // Meyer's Singleton — initialisiert beim ersten Aufruf
    return instance;
}

LicenseManager::LicenseManager()
{
    try
    {
        cachedMachineID = computeMachineFingerprint();
        getPropertiesFile();
        initializeTrialDate();
        updateCachedStatus();
    }
    catch (const std::exception&)
    {
        // Fehler ignorieren — Trial-Modus funktioniert trotzdem
    }
}

// ============================================================================
// Obfuscated Secret Assembly
// Das Geheimnis wird zur Laufzeit aus XOR-verschluesselten Fragmenten
// zusammengesetzt. Ein `strings Aura.exe` findet nichts Lesbares.
// ============================================================================

juce::String LicenseManager::assembleSecret()
{
    // Secret: "AuRa_Eq_2026_LiCeNsE_kEy_SeCrEt_V2" (34 Zeichen)
    // In 4 Fragmente aufgeteilt, jeweils mit unterschiedlichem XOR-Key

    // Fragment 1: "AuRa_Eq_2" XOR 0xA7
    const uint8_t f1[] = { 0xe6, 0xd2, 0xf5, 0xc6, 0xf8, 0xe2, 0xd6, 0xf8, 0x95 };
    // Fragment 2: "026_LiCeN" XOR 0x5B
    const uint8_t f2[] = { 0x6b, 0x69, 0x6d, 0x04, 0x17, 0x32, 0x18, 0x3e, 0x15 };
    // Fragment 3: "sE_kEy_Se" XOR 0xD3
    const uint8_t f3[] = { 0xa0, 0x96, 0x8c, 0xb8, 0x96, 0xaa, 0x8c, 0x80, 0xb6 };
    // Fragment 4: "CrEt_V2" XOR 0x8F
    const uint8_t f4[] = { 0xcc, 0xfd, 0xca, 0xfb, 0xd0, 0xd9, 0xbd };

    const uint8_t k1 = 0xa7, k2 = 0x5b, k3 = 0xd3, k4 = 0x8f;

    juce::String result;
    result.preallocateBytes(40);

    for (auto b : f1) result += static_cast<char>(b ^ k1);
    for (auto b : f2) result += static_cast<char>(b ^ k2);
    for (auto b : f3) result += static_cast<char>(b ^ k3);
    for (auto b : f4) result += static_cast<char>(b ^ k4);

    return result;
}

// ============================================================================
// Hardware-Fingerprint (Machine ID)
// Kombiniert Computer-Name + Volume-Seriennummer → MD5 → 8 Hex-Zeichen
// ============================================================================

juce::String LicenseManager::computeMachineFingerprint()
{
    juce::String fingerprint;

    // 1. Computer-Name (stabil ueber Reboots)
    fingerprint += juce::SystemStats::getComputerName();
    fingerprint += "|";

    // 2. Volume Serial Number (stabil bis zur Neuformatierung)
#if JUCE_WINDOWS
    DWORD serialNumber = 0;
    GetVolumeInformationW(L"C:\\", nullptr, 0, &serialNumber,
                          nullptr, nullptr, nullptr, 0);
    fingerprint += juce::String(static_cast<int64_t>(serialNumber));
#elif JUCE_MAC
    // macOS: Hardware-UUID via IOKit (stabil, ueberlebt OS-Updates)
    // Fallback auf ComputerName + UserName falls IOKit fehlschlaegt
    {
        juce::ChildProcess proc;
        if (proc.start("ioreg -rd1 -c IOPlatformExpertDevice") && proc.waitForProcessToFinish(3000))
        {
            juce::String output = proc.readAllProcessOutput();
            // Extrahiere IOPlatformUUID
            int uuidIdx = output.indexOf("IOPlatformUUID");
            if (uuidIdx >= 0)
            {
                int quoteStart = output.indexOf(uuidIdx, "\"");
                if (quoteStart >= 0)
                {
                    quoteStart++;  // nach dem ersten Quote
                    // Naechstes Quote nach dem Key-Value =
                    quoteStart = output.indexOf(quoteStart, "\"");
                    if (quoteStart >= 0)
                    {
                        quoteStart++;
                        int quoteEnd = output.indexOf(quoteStart, "\"");
                        if (quoteEnd > quoteStart)
                        {
                            fingerprint += output.substring(quoteStart, quoteEnd);
                        }
                    }
                }
            }
        }

        // Falls UUID nicht gefunden, Fallback
        if (fingerprint.endsWith("|"))
        {
            fingerprint += juce::SystemStats::getComputerName();
            fingerprint += juce::SystemStats::getUserLanguage();
        }
    }
#else
    fingerprint += juce::SystemStats::getOperatingSystemName();
#endif

    // 3. Zusaetzlicher Salt (erschwert Reproduktion)
    fingerprint += "|AuRa_HW_v2";

    // Hash zu konsistentem 8-Zeichen Hex-String
    juce::MD5 md5(fingerprint.toUTF8());
    return md5.toHexString().substring(0, 8).toUpperCase();
}

juce::String LicenseManager::getMachineID()
{
    if (cachedMachineID.isEmpty())
        cachedMachineID = computeMachineFingerprint();
    return cachedMachineID;
}

// ============================================================================
// Key-Signatur (MD5-HMAC basiert)
// Input: Secret + CustomerID + MachineID → MD5 → erste 8 Hex-Zeichen
// ============================================================================

juce::String LicenseManager::computeKeySignature(const juce::String& customerID,
                                                   const juce::String& machineID)
{
    juce::String secret = assembleSecret();
    juce::String input = secret + "-" + customerID + "-" + machineID;
    juce::MD5 md5(input.toUTF8());
    return md5.toHexString().substring(0, 8).toUpperCase();
}

// ============================================================================
// Anti-Tampering: MD5-basierte Integritaetspruefung fuer Trial-Daten
// ============================================================================

juce::String LicenseManager::computeIntegrityHash(const juce::String& data)
{
    juce::String salted = assembleSecret() + "|" + data + "|integrity_v2";
    juce::MD5 md5(salted.toUTF8());
    return md5.toHexString().substring(0, 8);
}

bool LicenseManager::verifyTrialIntegrity()
{
    auto& props = getPropertiesFile();

    juce::String startDateStr = props.getValue("trial_start_date", "");
    juce::String storedHash = props.getValue("trial_integrity", "");

    if (startDateStr.isEmpty())
        return true;   // Noch nicht initialisiert — OK

    if (storedHash.isEmpty())
        return false;  // Daten ohne Hash = manipuliert

    juce::String expectedHash = computeIntegrityHash(startDateStr);
    return storedHash == expectedHash;
}

void LicenseManager::updateLastSeen()
{
    auto& props = getPropertiesFile();
    time_t now = std::time(nullptr);
    props.setValue("last_seen", juce::String(static_cast<long long>(now)));
    props.save();
}

// ============================================================================
// Persistierung
// ============================================================================

juce::PropertiesFile& LicenseManager::getPropertiesFile()
{
    if (!propertiesFile)
    {
        juce::PropertiesFile::Options options;
        options.applicationName = "Aura";
        options.filenameSuffix = ".license";
        options.folderName = juce::File::getSpecialLocation(
            juce::File::userApplicationDataDirectory)
            .getChildFile("Aura").getFullPathName();

        propertiesFile = std::make_unique<juce::PropertiesFile>(options);
    }

    return *propertiesFile;
}

// ============================================================================
// Trial-Datum Management
// ============================================================================

void LicenseManager::initializeTrialDate()
{
    auto& props = getPropertiesFile();

    if (props.getValue("trial_start_date").isEmpty())
    {
        time_t now = std::time(nullptr);
        juce::String dateStr = juce::String(static_cast<long long>(now));

        props.setValue("trial_start_date", dateStr);
        props.setValue("trial_integrity", computeIntegrityHash(dateStr));
        props.save();
    }

    updateLastSeen();
}

time_t LicenseManager::getTrialStartDate()
{
    auto& props = getPropertiesFile();
    juce::String dateStr = props.getValue("trial_start_date", "0");
    long long timestamp = dateStr.getLargeIntValue();
    return static_cast<time_t>(timestamp);
}

// ============================================================================
// Lizenz-Status
// ============================================================================

LicenseManager::LicenseStatus LicenseManager::getLicenseStatus()
{
    std::lock_guard<std::recursive_mutex> lock(propertiesMutex);
    auto& props = getPropertiesFile();

    // 1. Pruefe gueltige Lizenz (mit Machine Binding)
    juce::String licenseKey = props.getValue("license_key", "");

    if (!licenseKey.isEmpty())
    {
        // Online-aktivierte Lizenz: Pruefe Offline-Grace-Period
        if (props.getBoolValue("online_activated", false))
        {
            if (isOfflineGracePeriodExpired())
            {
                // Grace-Period abgelaufen — braucht Online-Recheck
                return LicenseStatus::Unlicensed;
            }
            // Online-Lizenz gueltig (innerhalb Grace-Period oder kuerzlich geprueft)
            return LicenseStatus::Licensed;
        }

        // Legacy-Offline-Lizenz: Validiere mit Machine Binding
        if (validateLicenseKey(licenseKey))
        {
            return LicenseStatus::Licensed;
        }
    }

    // 2. Anti-Tampering: Pruefe ob Trial-Daten manipuliert wurden
    if (!verifyTrialIntegrity())
    {
        return LicenseStatus::TrialExpired;  // Manipulation erkannt
    }

    // 3. Anti-Tampering: Pruefe ob Systemuhr zurueckgestellt wurde
    juce::String lastSeenStr = props.getValue("last_seen", "0");
    time_t lastSeen = static_cast<time_t>(lastSeenStr.getLargeIntValue());
    time_t now = std::time(nullptr);

    if (lastSeen > 0 && now < lastSeen - 86400)
    {
        return LicenseStatus::TrialExpired;
    }

    updateLastSeen();

    // 4. Pruefe Trial-Ablauf
    if (isTrialExpired())
    {
        return LicenseStatus::TrialExpired;
    }

    return LicenseStatus::Trial;
}

void LicenseManager::updateCachedStatus()
{
    auto status = getLicenseStatus();
    cachedStatus.store(static_cast<int>(status));

    // Enforcement-Faktor: 1.0 = OK, 0.0 = degradiert
    if (status == LicenseStatus::Licensed || status == LicenseStatus::Trial)
        cachedEnforcementFactor.store(1.0f);
    else
        cachedEnforcementFactor.store(0.0f);
}

bool LicenseManager::isFullyLicensed()
{
    return static_cast<LicenseStatus>(cachedStatus.load()) == LicenseStatus::Licensed;
}

bool LicenseManager::shouldNagUser()
{
    auto status = static_cast<LicenseStatus>(cachedStatus.load());
    return status == LicenseStatus::Trial || status == LicenseStatus::TrialExpired;
}

float LicenseManager::getEnforcementFactor()
{
    return cachedEnforcementFactor.load();
}

int LicenseManager::getTrialDaysRemaining()
{
    const int TRIAL_DAYS = 30;

    time_t startDate = getTrialStartDate();
    time_t now = std::time(nullptr);

    long secondsDiff = static_cast<long>(std::difftime(now, startDate));
    int daysPassed = secondsDiff / (24 * 3600);

    int remaining = TRIAL_DAYS - daysPassed;
    return remaining > 0 ? remaining : 0;
}

bool LicenseManager::isTrialPeriod()
{
    return !isTrialExpired() && !isFullyLicensed();
}

bool LicenseManager::isTrialExpired()
{
    return getTrialDaysRemaining() <= 0;
}

// ============================================================================
// Key-Validierung (MD5 + Machine Binding)
// Format: "AURA-CCCC-MMMM-SSSSSSSS" (23 Zeichen)
//   CCCC = Kundennummer
//   MMMM = Machine-ID Prefix (erste 4 Zeichen)
//   SSSSSSSS = MD5-HMAC Signatur (8 Hex-Zeichen)
// ============================================================================

bool LicenseManager::validateLicenseKey(const juce::String& key)
{
    // Format-Check
    if (!key.startsWith("AURA-") || key.length() != 23)
        return false;

    if (key[4] != '-' || key[9] != '-' || key[14] != '-')
        return false;

    // Felder extrahieren
    juce::String customerID   = key.substring(5, 9);     // CCCC
    juce::String machinePrefix = key.substring(10, 14);   // MMMM
    juce::String signature     = key.substring(15, 23);   // SSSSSSSS

    // Machine Binding pruefen
    juce::String currentMachineID = getMachineID();
    if (machinePrefix != currentMachineID.substring(0, 4))
        return false;

    // MD5-Signatur pruefen
    juce::String expectedSignature = computeKeySignature(customerID, currentMachineID);
    return signature == expectedSignature;
}

bool LicenseManager::activateLicense(const juce::String& licenseKey)
{
    juce::String trimmedKey = licenseKey.trim().toUpperCase();

    if (!validateLicenseKey(trimmedKey))
        return false;

    std::lock_guard<std::recursive_mutex> lock(propertiesMutex);
    auto& props = getPropertiesFile();
    props.setValue("license_key", trimmedKey);
    props.save();

    updateCachedStatus();
    return true;
}

juce::String LicenseManager::getLicenseKey()
{
    auto& props = getPropertiesFile();
    return props.getValue("license_key", "");
}

bool LicenseManager::clearLicense()
{
    std::lock_guard<std::recursive_mutex> lock(propertiesMutex);
    auto& props = getPropertiesFile();
    props.removeValue("license_key");
    props.save();

    updateCachedStatus();
    return true;
}

// ============================================================================
// Display
// ============================================================================

juce::String LicenseManager::getStatusText()
{
    auto status = getLicenseStatus();

    switch (status)
    {
        case LicenseStatus::Licensed:
        {
            if (isOnlineActivated())
            {
                int graceDays = getOfflineGraceDaysRemaining();
                if (graceDays < OFFLINE_GRACE_PERIOD_DAYS)
                    return juce::String::formatted("Lizenziert (Offline: %d Tage verbleibend)", graceDays);
            }
            return "Lizenziert - Vielen Dank!";
        }

        case LicenseStatus::Trial:
        {
            int daysLeft = getTrialDaysRemaining();
            return juce::String::formatted("Testversion: %d Tag%s verbleibend",
                                           daysLeft, daysLeft == 1 ? "" : "e");
        }

        case LicenseStatus::TrialExpired:
            return "Testversion abgelaufen - Bitte Lizenz aktivieren";

        case LicenseStatus::Unlicensed:
        default:
            return "Nicht lizenziert";
    }
}

// ============================================================================
// Online-Aktivierung
// ============================================================================

void LicenseManager::activateOnline(const juce::String& licenseKey, OnlineCallback callback)
{
    juce::String trimmedKey = licenseKey.trim().toUpperCase();
    juce::String machineID = getMachineID();
    juce::String version = VersionInfo::getCurrentVersion();

    onlineValidator.activateOnline(trimmedKey, machineID, version,
        [this, trimmedKey, callback](const OnlineLicenseValidator::Result& result)
        {
            if (result.success)
            {
                // Lizenz-Key und Online-Token speichern
                {
                    std::lock_guard<std::recursive_mutex> lock(propertiesMutex);
                    auto& props = getPropertiesFile();
                    props.setValue("license_key", trimmedKey);
                    props.save();
                }

                storeOnlineActivation(result.activationToken, result.expiresAt);
                updateLastOnlineCheckTime();
                updateCachedStatus();

                DBG("LicenseManager: Online-Aktivierung erfolgreich. Token: "
                    + result.activationToken.substring(0, 8) + "...");
            }

            if (callback)
                callback(result.success, result.message);
        });
}

void LicenseManager::deactivateOnline(OnlineCallback callback)
{
    juce::String licenseKey = getLicenseKey();
    juce::String machineID = getMachineID();
    juce::String token = getActivationToken();

    if (licenseKey.isEmpty() || token.isEmpty())
    {
        // Offline-Lizenz: einfach lokal loeschen
        clearLicense();
        clearOnlineActivation();
        if (callback)
            callback(true, "Lizenz lokal deaktiviert.");
        return;
    }

    onlineValidator.deactivateOnline(licenseKey, machineID, token,
        [this, callback](const OnlineLicenseValidator::Result& result)
        {
            if (result.success || result.networkError)
            {
                // Auch bei Netzwerkfehler lokal deaktivieren
                // (Server wird beim naechsten Check aufgeraeumt)
                clearLicense();
                clearOnlineActivation();
                updateCachedStatus();
            }

            if (callback)
            {
                if (result.networkError)
                    callback(true, "Lizenz lokal deaktiviert (Server nicht erreichbar).");
                else
                    callback(result.success, result.message);
            }
        });
}

void LicenseManager::validateOnStartup(OnlineCallback callback)
{
    if (!isOnlineActivated())
    {
        if (callback) callback(true, "Keine Online-Lizenz vorhanden.");
        return;
    }

    juce::String licenseKey = getLicenseKey();
    juce::String machineID = getMachineID();
    juce::String token = getActivationToken();

    onlineValidator.validateOnline(licenseKey, machineID, token,
        [this, callback](const OnlineLicenseValidator::Result& result)
        {
            if (result.success)
            {
                updateLastOnlineCheckTime();
                DBG("LicenseManager: Startup-Validierung erfolgreich.");
            }
            else if (!result.networkError)
            {
                // Server hat aktiv abgelehnt (Lizenz gesperrt/deaktiviert)
                DBG("LicenseManager: Lizenz vom Server abgelehnt: " + result.message);
                clearLicense();
                clearOnlineActivation();
                updateCachedStatus();
            }
            // Bei Netzwerkfehler: Offline-Grace-Period greift

            if (callback)
                callback(result.success, result.message);
        });
}

void LicenseManager::performPeriodicValidation(OnlineCallback callback)
{
    if (!isOnlineActivated())
    {
        if (callback) callback(true, "Keine Online-Lizenz vorhanden.");
        return;
    }

    if (!isOnlineRecheckDue())
    {
        if (callback) callback(true, "Kein Recheck noetig.");
        return;
    }

    juce::String licenseKey = getLicenseKey();
    juce::String machineID = getMachineID();
    juce::String token = getActivationToken();

    onlineValidator.validateOnline(licenseKey, machineID, token,
        [this, callback](const OnlineLicenseValidator::Result& result)
        {
            if (result.success)
            {
                updateLastOnlineCheckTime();
                DBG("LicenseManager: Periodische Validierung erfolgreich.");
            }
            else if (!result.networkError)
            {
                // Server hat aktiv abgelehnt (z.B. Lizenz gesperrt)
                DBG("LicenseManager: Lizenz vom Server abgelehnt: " + result.message);
                clearLicense();
                clearOnlineActivation();
                updateCachedStatus();
            }
            // Bei Netzwerkfehler: Offline-Grace-Period greift, nichts aendern

            if (callback)
                callback(result.success, result.message);
        });
}

// ============================================================================
// Online-Aktivierungs-Daten Persistierung
// ============================================================================

void LicenseManager::storeOnlineActivation(const juce::String& token,
                                             const juce::String& expiresAt)
{
    std::lock_guard<std::recursive_mutex> lock(propertiesMutex);
    auto& props = getPropertiesFile();
    props.setValue("activation_token", token);
    props.setValue("online_activated", true);
    props.setValue("activation_expires_at", expiresAt);
    props.save();
}

void LicenseManager::clearOnlineActivation()
{
    std::lock_guard<std::recursive_mutex> lock(propertiesMutex);
    auto& props = getPropertiesFile();
    props.removeValue("activation_token");
    props.removeValue("online_activated");
    props.removeValue("activation_expires_at");
    props.removeValue("last_online_check");
    props.save();
}

juce::String LicenseManager::getActivationToken() const
{
    auto* self = const_cast<LicenseManager*>(this);
    auto& props = self->getPropertiesFile();
    return props.getValue("activation_token", "");
}

bool LicenseManager::isOnlineActivated() const
{
    auto* self = const_cast<LicenseManager*>(this);
    auto& props = self->getPropertiesFile();
    return props.getBoolValue("online_activated", false);
}

time_t LicenseManager::getLastOnlineCheckTime() const
{
    auto* self = const_cast<LicenseManager*>(this);
    auto& props = self->getPropertiesFile();
    juce::String str = props.getValue("last_online_check", "0");
    return static_cast<time_t>(str.getLargeIntValue());
}

void LicenseManager::updateLastOnlineCheckTime()
{
    std::lock_guard<std::recursive_mutex> lock(propertiesMutex);
    auto& props = getPropertiesFile();
    time_t now = std::time(nullptr);
    props.setValue("last_online_check", juce::String(static_cast<long long>(now)));
    props.save();
}

int LicenseManager::getDaysSinceLastOnlineCheck() const
{
    time_t lastCheck = getLastOnlineCheckTime();
    if (lastCheck == 0) return 9999;  // Noch nie geprueft

    time_t now = std::time(nullptr);
    long secondsDiff = static_cast<long>(std::difftime(now, lastCheck));
    return secondsDiff / (24 * 3600);
}

bool LicenseManager::isOnlineRecheckDue() const
{
    return getDaysSinceLastOnlineCheck() >= ONLINE_RECHECK_INTERVAL_DAYS;
}

int LicenseManager::getOfflineGraceDaysRemaining() const
{
    int daysSince = getDaysSinceLastOnlineCheck();
    int totalGrace = OFFLINE_GRACE_PERIOD_DAYS + OFFLINE_GRACE_EXTRA_DAYS;
    int remaining = totalGrace - daysSince;
    return remaining > 0 ? remaining : 0;
}

bool LicenseManager::isOfflineGracePeriodExpired() const
{
    if (!isOnlineActivated()) return false;
    return getOfflineGraceDaysRemaining() <= 0;
}
