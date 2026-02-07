#pragma once
#include <JuceHeader.h>

struct ColorTheme
{
    juce::String name;
    juce::Colour backgroundDark;
    juce::Colour backgroundMid;
    juce::Colour backgroundLight;
    juce::Colour textColor;
    juce::Colour accentColor;
    juce::Colour curveColor;
    std::vector<juce::Colour> bandColors;
    
    // Keine get-Methoden nötig - direkt auf Member zugreifen
};

class ThemeManager
{
public:
    enum ThemeID
    {
        NeonMagenta = 0,
        CyberBlue,
        DeepPurple,
        ClassicDark,
        MinimalWhite,
        RetroOrange,
        ForestGreen
    };
    
    static ThemeManager& getInstance()
    {
        static ThemeManager instance;
        return instance;
    }
    
    void setTheme(ThemeID id)
    {
        currentThemeID = id;
        currentTheme = themes[id];
        
        // Speichere Theme-Auswahl
        juce::PropertiesFile::Options options;
        options.applicationName = "Aura";
        options.filenameSuffix = ".settings";
        options.osxLibrarySubFolder = "Application Support";
        options.folderName = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                            .getChildFile("Aura").getFullPathName();
        
        juce::PropertiesFile settings(options);
        settings.setValue("theme", static_cast<int>(id));
        settings.save();
        
        // Rufe Listener auf (wird in PluginEditor gesetzt)
        if (onThemeChanged)
            onThemeChanged(id);
    }
    
    // Callback für Theme-Änderungen
    std::function<void(ThemeID)> onThemeChanged;
    
    ThemeID getCurrentThemeID() const { return currentThemeID; }
    const ColorTheme& getCurrentTheme() const { return currentTheme; }
    const std::vector<ColorTheme>& getAllThemes() const { return themes; }
    
    void loadSavedTheme()
    {
        juce::PropertiesFile::Options options;
        options.applicationName = "Aura";
        options.filenameSuffix = ".settings";
        options.osxLibrarySubFolder = "Application Support";
        options.folderName = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                            .getChildFile("Aura").getFullPathName();
        
        juce::PropertiesFile settings(options);
        int savedTheme = settings.getIntValue("theme", 0);
        
        if (savedTheme >= 0 && savedTheme < themes.size())
            setTheme(static_cast<ThemeID>(savedTheme));
    }
    
private:
    ThemeManager()
    {
        initializeThemes();
        loadSavedTheme();
    }
    
    void initializeThemes()
    {
        // 1. Neon Magenta/Cyan (Aktuelles Design)
        themes.push_back({
            "Neon Magenta",
            juce::Colour(0xff0a0a0a),  // Deep Black
            juce::Colour(0xff151515),
            juce::Colour(0xff202020),
            juce::Colour(0xffffffff),
            juce::Colour(0xffff00ff),  // Magenta
            juce::Colour(0xff00ffff),  // Cyan
            {
                juce::Colour(0xffff00ff), // Magenta
                juce::Colour(0xffff1493), // Hot Pink
                juce::Colour(0xff00ffff), // Cyan
                juce::Colour(0xff00ff9f), // Mint
                juce::Colour(0xffb19cd9), // Purple
                juce::Colour(0xff87ceeb), // Sky Blue
                juce::Colour(0xffff6b9d), // Rose
                juce::Colour(0xff39ff14)  // Green Neon
            }
        });
        
        // 2. Cyber Blue (Blaues Tech-Theme)
        themes.push_back({
            "Cyber Blue",
            juce::Colour(0xff0a0e1a),
            juce::Colour(0xff151922),
            juce::Colour(0xff1f2533),
            juce::Colour(0xffe0f0ff),
            juce::Colour(0xff00d9ff),  // Electric Blue
            juce::Colour(0xff00ffcc),  // Türkis
            {
                juce::Colour(0xff00d9ff),
                juce::Colour(0xff0099ff),
                juce::Colour(0xff00ffcc),
                juce::Colour(0xff66ffff),
                juce::Colour(0xff3366ff),
                juce::Colour(0xff00ccff),
                juce::Colour(0xff0088ff),
                juce::Colour(0xff33ddff)
            }
        });
        
        // 3. Deep Purple (Lila/Violett Theme)
        themes.push_back({
            "Deep Purple",
            juce::Colour(0xff0f0a14),
            juce::Colour(0xff1a101f),
            juce::Colour(0xff251829),
            juce::Colour(0xfff0e0ff),
            juce::Colour(0xffbb86fc),  // Light Purple
            juce::Colour(0xffcf6fff),  // Medium Purple
            {
                juce::Colour(0xffbb86fc),
                juce::Colour(0xff9d4edd),
                juce::Colour(0xffcf6fff),
                juce::Colour(0xffe0aaff),
                juce::Colour(0xff8b5cf6),
                juce::Colour(0xffa855f7),
                juce::Colour(0xffc084fc),
                juce::Colour(0xff9333ea)
            }
        });
        
        // 4. Classic Dark (Klassisches dunkles Theme)
        themes.push_back({
            "Classic Dark",
            juce::Colour(0xff1a1a1a),
            juce::Colour(0xff2d2d2d),
            juce::Colour(0xff3d3d3d),
            juce::Colour(0xffe0e0e0),
            juce::Colour(0xff4fc3f7),  // Blue
            juce::Colour(0xffffd54f),  // Yellow
            {
                juce::Colour(0xfffe6b8b),
                juce::Colour(0xffffb347),
                juce::Colour(0xffffd54f),
                juce::Colour(0xff77dd77),
                juce::Colour(0xff89cff0),
                juce::Colour(0xffb19cd9),
                juce::Colour(0xffffb6c1),
                juce::Colour(0xffc4e17f)
            }
        });
        
        // 5. Minimal White (Helles minimalistisches Theme)
        themes.push_back({
            "Minimal White",
            juce::Colour(0xfffafafa),
            juce::Colour(0xfff0f0f0),
            juce::Colour(0xffe8e8e8),
            juce::Colour(0xff1a1a1a),
            juce::Colour(0xff2196f3),  // Material Blue
            juce::Colour(0xffff5722),  // Deep Orange
            {
                juce::Colour(0xffef5350),
                juce::Colour(0xffff7043),
                juce::Colour(0xffffca28),
                juce::Colour(0xff66bb6a),
                juce::Colour(0xff42a5f5),
                juce::Colour(0xff7e57c2),
                juce::Colour(0xffec407a),
                juce::Colour(0xff26c6da)
            }
        });
        
        // 6. Retro Orange (Warmes Retro-Theme)
        themes.push_back({
            "Retro Orange",
            juce::Colour(0xff1a0f0a),
            juce::Colour(0xff2a1810),
            juce::Colour(0xff3a2218),
            juce::Colour(0xfffff5e0),
            juce::Colour(0xffff8c42),  // Orange
            juce::Colour(0xffffcd3c),  // Gold
            {
                juce::Colour(0xffff8c42),
                juce::Colour(0xffffaa00),
                juce::Colour(0xffffcd3c),
                juce::Colour(0xffffa500),
                juce::Colour(0xffff7f50),
                juce::Colour(0xffffb84d),
                juce::Colour(0xffff9966),
                juce::Colour(0xffffc04d)
            }
        });
        
        // 7. Forest Green (Grünes natürliches Theme)
        themes.push_back({
            "Forest Green",
            juce::Colour(0xff0a140f),
            juce::Colour(0xff101f15),
            juce::Colour(0xff18291d),
            juce::Colour(0xffe0ffe8),
            juce::Colour(0xff00ff7f),  // Spring Green
            juce::Colour(0xff7fffd4),  // Aquamarine
            {
                juce::Colour(0xff00ff7f),
                juce::Colour(0xff3cb371),
                juce::Colour(0xff7fffd4),
                juce::Colour(0xff00fa9a),
                juce::Colour(0xff20b2aa),
                juce::Colour(0xff66cdaa),
                juce::Colour(0xff8fbc8f),
                juce::Colour(0xff3de882)
            }
        });
        
        currentTheme = themes[0];
    }
    
    ThemeID currentThemeID = NeonMagenta;
    ColorTheme currentTheme;
    std::vector<ColorTheme> themes;
};
