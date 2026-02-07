#pragma once

#include <JuceHeader.h>
#include "../Presets/PresetManager.h"
#include "../Parameters/ParameterIDs.h"

// Forward declaration
class AuraAudioProcessor;

/**
 * PresetComponent: UI für Preset-Auswahl
 */
class PresetComponent : public juce::Component
{
public:
    class Listener
    {
    public:
        virtual ~Listener() = default;
        virtual void presetSelected(const PresetManager::PresetData& preset) = 0;
    };

    PresetComponent(juce::AudioProcessor* processor) : audioProcessor(processor)
    {
        presetButton.setButtonText("Presets");
        presetButton.onClick = [this]() { showPresetMenu(); };
        addAndMakeVisible(presetButton);

        saveButton.setButtonText("Save");
        saveButton.onClick = [this]() { savePreset(); };
        addAndMakeVisible(saveButton);

        deleteButton.setButtonText("Delete");
        deleteButton.onClick = [this]() { deletePreset(); };
        addAndMakeVisible(deleteButton);

        categoryLabel.setText("Category: All", juce::dontSendNotification);
        categoryLabel.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(categoryLabel);

        loadUserPresets();
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xFF2A2A2A));
        g.setColour(juce::Colour(0xFF404040));
        g.drawRect(getLocalBounds(), 1);
    }

    void resized() override
    {
        auto bounds = getLocalBounds().reduced(5);
        presetButton.setBounds(bounds.removeFromLeft(80));
        bounds.removeFromLeft(5);
        saveButton.setBounds(bounds.removeFromLeft(60));
        bounds.removeFromLeft(5);
        deleteButton.setBounds(bounds.removeFromLeft(60));
        bounds.removeFromLeft(10);
        categoryLabel.setBounds(bounds);
    }

    void addListener(Listener* listener)
    {
        listeners.push_back(listener);
    }

    void removeListener(Listener* listener)
    {
        listeners.erase(std::remove(listeners.begin(), listeners.end(), listener), listeners.end());
    }

private:
    juce::TextButton presetButton;
    juce::TextButton saveButton;
    juce::TextButton deleteButton;
    juce::Label categoryLabel;
    std::vector<Listener*> listeners;
    juce::AudioProcessor* audioProcessor;
    juce::String currentPresetName;
    juce::File getUserPresetsFolder()
    {
        auto userFolder = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                            .getChildFile("Aura").getChildFile("Presets");
        if (!userFolder.exists())
            userFolder.createDirectory();
        return userFolder;
    }
    
    juce::Array<PresetManager::PresetData> userPresets;
    
    void loadUserPresets()
    {
        userPresets.clear();
        auto presetsFolder = getUserPresetsFolder();
        auto files = presetsFolder.findChildFiles(juce::File::findFiles, false, "*.xml");
        
        for (auto& file : files)
        {
            auto xml = juce::parseXML(file);
            if (xml != nullptr)
            {
                PresetManager::PresetData preset;
                preset.name = xml->getStringAttribute("name");
                preset.category = xml->getStringAttribute("category", "User");
                
                for (int i = 0; i < ParameterIDs::MAX_BANDS; ++i)
                {
                    auto bandElement = xml->getChildByName("Band" + juce::String(i));
                    if (bandElement != nullptr)
                    {
                        preset.bands[i].frequency = (float)bandElement->getDoubleAttribute("frequency", 1000.0);
                        preset.bands[i].gain = (float)bandElement->getDoubleAttribute("gain", 0.0);
                        preset.bands[i].q = (float)bandElement->getDoubleAttribute("q", 0.71);
                        preset.bands[i].type = (ParameterIDs::FilterType)bandElement->getIntAttribute("type", 0);
                        preset.bands[i].active = bandElement->getBoolAttribute("active", false);
                        preset.bands[i].bypass = bandElement->getBoolAttribute("bypass", false);
                    }
                }
                
                userPresets.add(preset);
            }
        }
    }
    
    void savePreset()
    {
        // Verwende einen Text-Input Dialog
        auto* window = new juce::AlertWindow("Save Preset", 
                                             "Enter a name for your preset:", 
                                             juce::AlertWindow::NoIcon, 
                                             this);
        window->addTextEditor("presetName", "", "Preset Name:");
        window->addButton("Save", 1);
        window->addButton("Cancel", 0);
        
        window->enterModalState(true, 
            juce::ModalCallbackFunction::create([this, window](int result)
            {
                if (result == 1)
                {
                    auto presetName = window->getTextEditorContents("presetName");
                    if (presetName.isNotEmpty())
                    {
                        saveCurrentStateAsPreset(presetName);
                    }
                }
                delete window;
            }));
    }
    
    void saveCurrentStateAsPreset(const juce::String& presetName)
    {
        if (audioProcessor == nullptr)
            return;
            
        // Get APVTS from AudioProcessor
        auto* processor = dynamic_cast<AuraAudioProcessor*>(audioProcessor);
        if (processor == nullptr)
            return;
            
        auto& apvts = processor->getAPVTS();
        
        auto xml = std::make_unique<juce::XmlElement>("Preset");
        xml->setAttribute("name", presetName);
        xml->setAttribute("category", "User");
        
        for (int i = 0; i < ParameterIDs::MAX_BANDS; ++i)
        {
            auto bandElement = xml->createNewChildElement("Band" + juce::String(i));
            
            auto* freqParam = apvts.getRawParameterValue(ParameterIDs::getBandFreqID(i));
            auto* gainParam = apvts.getRawParameterValue(ParameterIDs::getBandGainID(i));
            auto* qParam = apvts.getRawParameterValue(ParameterIDs::getBandQID(i));
            auto* typeParam = apvts.getRawParameterValue(ParameterIDs::getBandTypeID(i));
            auto* activeParam = apvts.getRawParameterValue(ParameterIDs::getBandActiveID(i));
            auto* bypassParam = apvts.getRawParameterValue(ParameterIDs::getBandBypassID(i));
            
            if (freqParam) bandElement->setAttribute("frequency", freqParam->load());
            if (gainParam) bandElement->setAttribute("gain", gainParam->load());
            if (qParam) bandElement->setAttribute("q", qParam->load());
            if (typeParam) bandElement->setAttribute("type", (int)typeParam->load());
            if (activeParam) bandElement->setAttribute("active", activeParam->load() > 0.5f);
            if (bypassParam) bandElement->setAttribute("bypass", bypassParam->load() > 0.5f);
        }
        
        auto presetsFolder = getUserPresetsFolder();
        auto file = presetsFolder.getChildFile(presetName + ".xml");
        
        if (xml->writeTo(file))
        {
            currentPresetName = presetName;
            loadUserPresets();
            juce::AlertWindow::showMessageBoxAsync(
                juce::AlertWindow::InfoIcon,
                "Success",
                "Preset '" + presetName + "' saved successfully!");
        }
        else
        {
            juce::AlertWindow::showMessageBoxAsync(
                juce::AlertWindow::WarningIcon,
                "Error",
                "Failed to save preset!");
        }
    }
    
    void deletePreset()
    {
        if (userPresets.isEmpty())
        {
            juce::AlertWindow::showMessageBoxAsync(
                juce::AlertWindow::InfoIcon,
                "No Presets",
                "No user presets to delete.");
            return;
        }
        
        juce::PopupMenu menu;
        for (int i = 0; i < userPresets.size(); ++i)
        {
            menu.addItem(i + 1, userPresets[i].name);
        }
        
        menu.showMenuAsync(
            juce::PopupMenu::Options().withTargetComponent(&deleteButton),
            [this](int selectedId)
            {
                if (selectedId > 0 && selectedId <= userPresets.size())
                {
                    const auto& preset = userPresets.getReference(selectedId - 1);
                    
                    juce::AlertWindow::showOkCancelBox(
                        juce::AlertWindow::QuestionIcon,
                        "Delete Preset",
                        "Are you sure you want to delete '" + preset.name + "'?",
                        "Delete",
                        "Cancel",
                        this,
                        juce::ModalCallbackFunction::create([this, preset](int result)
                        {
                            if (result == 1)
                            {
                                auto presetsFolder = getUserPresetsFolder();
                                auto file = presetsFolder.getChildFile(preset.name + ".xml");
                                
                                if (file.deleteFile())
                                {
                                    loadUserPresets();
                                    juce::AlertWindow::showMessageBoxAsync(
                                        juce::AlertWindow::InfoIcon,
                                        "Success",
                                        "Preset deleted successfully!");
                                }
                            }
                        }));
                }
            });
    }

    void showPresetMenu()
    {
        auto presets = PresetManager::getBuiltInPresets();
        juce::PopupMenu mainMenu;

        // User Presets zuerst
        if (!userPresets.isEmpty())
        {
            juce::PopupMenu userMenu;
            int itemId = 1;
            for (const auto& preset : userPresets)
            {
                userMenu.addItem(itemId++, preset.name);
            }
            mainMenu.addSubMenu("User Presets", userMenu);
            mainMenu.addSeparator();
        }

        // Gruppiere Built-In Presets nach Kategorie
        juce::StringArray categories;
        for (const auto& preset : presets)
        {
            if (!categories.contains(preset.category))
                categories.add(preset.category);
        }

        for (const auto& category : categories)
        {
            juce::PopupMenu categoryMenu;
            int itemId = 1000;  // Offset für Built-In Presets

            for (const auto& preset : presets)
            {
                if (preset.category == category)
                {
                    categoryMenu.addItem(itemId, preset.name);
                    itemId++;
                }
            }

            mainMenu.addSubMenu(category, categoryMenu);
        }

        mainMenu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&presetButton),
            [this, presets](int selectedId) mutable
            {
                if (selectedId > 0)
                {
                    // User Preset ausgewählt
                    if (selectedId < 1000)
                    {
                        int index = selectedId - 1;
                        if (index < userPresets.size())
                        {
                            const auto& preset = userPresets.getReference(index);
                            currentPresetName = preset.name;
                            categoryLabel.setText("Category: " + preset.category, juce::dontSendNotification);

                            for (auto* listener : listeners)
                                listener->presetSelected(preset);
                        }
                    }
                    // Built-In Preset ausgewählt
                    else
                    {
                        int index = 0;
                        for (const auto& preset : presets)
                        {
                            index++;
                            if (index + 999 == selectedId)
                            {
                                currentPresetName = preset.name;
                                categoryLabel.setText("Category: " + preset.category, juce::dontSendNotification);

                                for (auto* listener : listeners)
                                    listener->presetSelected(preset);

                                break;
                            }
                        }
                    }
                }
            });
    }
};
