#pragma once
#include <JuceHeader.h>
#include "ThemeManager.h"

class ThemeSelector : public juce::Component
{
public:
    ThemeSelector()
    {
        themeCombo.setTextWhenNothingSelected("Wähle Design");
        
        auto& themes = ThemeManager::getInstance().getAllThemes();
        for (size_t i = 0; i < themes.size(); ++i)
        {
            themeCombo.addItem(themes[i].name, static_cast<int>(i + 1));
        }
        
        themeCombo.setSelectedId(static_cast<int>(ThemeManager::getInstance().getCurrentThemeID()) + 1, 
                                  juce::dontSendNotification);
        
        themeCombo.onChange = [this]
        {
            int selectedIndex = themeCombo.getSelectedId() - 1;
            size_t numThemes = ThemeManager::getInstance().getAllThemes().size();
            
            if (selectedIndex >= 0 && selectedIndex < static_cast<int>(numThemes))
            {
                ThemeManager::getInstance().setTheme(static_cast<ThemeManager::ThemeID>(selectedIndex));
            }
        };
        
        addAndMakeVisible(themeCombo);
        
        label.setText("Theme:", juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centredRight);
        addAndMakeVisible(label);
    }
    
    void paint(juce::Graphics& /*g*/) override
    {
        // Transparent - wird vom Parent gezeichnet
    }
    
    void resized() override
    {
        auto bounds = getLocalBounds();
        label.setBounds(bounds.removeFromLeft(50));
        themeCombo.setBounds(bounds.reduced(2));
    }
    
private:
    juce::ComboBox themeCombo;
    juce::Label label;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ThemeSelector)
};
