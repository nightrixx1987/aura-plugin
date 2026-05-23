#pragma once

#include <JuceHeader.h>
#include "../DSP/WebSearchEngine.h"
#include "CustomLookAndFeel.h"

class WebSearchPanel : public juce::Component,
                       private juce::Timer,
                       private juce::ListBoxModel
{
public:
    WebSearchPanel(WebSearchEngine& engine)
        : searchEngine(engine)
    {
        titleLabel.setText("Web Search", juce::dontSendNotification);
        titleLabel.setJustificationType(juce::Justification::centredLeft);
        titleLabel.setColour(juce::Label::textColourId, CustomLookAndFeel::getTextColor());
        addAndMakeVisible(titleLabel);

        searchBox.setMultiLine(false);
        searchBox.setReturnKeyStartsNewLine(false);
        searchBox.setTextToShowWhenEmpty("Search mixing tips, frequencies...", CustomLookAndFeel::getTextColor().withAlpha(0.4f));
        searchBox.setColour(juce::TextEditor::textColourId, CustomLookAndFeel::getTextColor());
        searchBox.setColour(juce::TextEditor::backgroundColourId, CustomLookAndFeel::getBackgroundDark());
        searchBox.setColour(juce::TextEditor::outlineColourId, CustomLookAndFeel::getAccentColor().withAlpha(0.3f));
        searchBox.setColour(juce::TextEditor::focusedOutlineColourId, CustomLookAndFeel::getAccentColor());
        searchBox.onTextChange = [this]()
        {
            if (!searchBox.getText().isEmpty())
                searchBox.setColour(juce::TextEditor::outlineColourId, CustomLookAndFeel::getAccentColor().withAlpha(0.3f));
        };
        searchBox.onReturnKey = [this]() { performSearch(); };
        addAndMakeVisible(searchBox);

        searchButton.setButtonText("Search");
        searchButton.setTooltip("Search the web for mixing and audio production information");
        searchButton.onClick = [this]() { performSearch(); };
        addAndMakeVisible(searchButton);

        statusLabel.setJustificationType(juce::Justification::centred);
        statusLabel.setColour(juce::Label::textColourId, CustomLookAndFeel::getTextColor().withAlpha(0.5f));
        statusLabel.setFont(juce::Font(juce::FontOptions(11.0f)));
        addAndMakeVisible(statusLabel);

        resultsListBox.setModel(this);
        resultsListBox.setColour(juce::ListBox::backgroundColourId, CustomLookAndFeel::getBackgroundDark());
        resultsListBox.setColour(juce::ListBox::outlineColourId, CustomLookAndFeel::getAccentColor().withAlpha(0.2f));
        resultsListBox.setRowHeight(52);
        addAndMakeVisible(resultsListBox);
    }

    ~WebSearchPanel() override
    {
        stopTimer();
    }

    void resized() override
    {
        auto bounds = getLocalBounds().reduced(5);

        auto topRow = bounds.removeFromTop(24);
        titleLabel.setBounds(topRow.removeFromLeft(80));
        topRow.removeFromLeft(5);

        searchButton.setBounds(topRow.removeFromRight(60));
        topRow.removeFromRight(3);

        searchBox.setBounds(topRow);
        bounds.removeFromTop(5);

        statusLabel.setBounds(bounds.removeFromTop(18));
        bounds.removeFromTop(2);

        resultsListBox.setBounds(bounds);
    }

    void paint(juce::Graphics& g) override
    {
        g.setColour(CustomLookAndFeel::getBackgroundMid());
        g.fillRoundedRectangle(getLocalBounds().toFloat(), 5.0f);

        g.setColour(CustomLookAndFeel::getAccentColor().withAlpha(0.3f));
        g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 5.0f, 1.0f);
    }

    void performSearch()
    {
        auto query = searchBox.getText().trim();
        if (query.isEmpty())
        {
            searchBox.setColour(juce::TextEditor::outlineColourId, juce::Colours::red.withAlpha(0.5f));
            return;
        }

        statusLabel.setText("Searching...", juce::dontSendNotification);
        searchButton.setEnabled(false);
        searchResults.clear();
        resultsListBox.updateContent();
        resultsListBox.repaint();

        searchEngine.search(query, [this](const juce::String&, const std::vector<WebSearchResult>& results, const juce::String& error)
        {
            searchButton.setEnabled(true);

            if (error.isNotEmpty())
            {
                statusLabel.setText("Error: " + error, juce::dontSendNotification);
                return;
            }

            searchResults = results;

            if (searchResults.empty())
            {
                statusLabel.setText("No results found", juce::dontSendNotification);
            }
            else
            {
                statusLabel.setText(juce::String(searchResults.size()) + " results", juce::dontSendNotification);
            }

            resultsListBox.updateContent();
            resultsListBox.repaint();
        });
    }

    int getNumRows() override
    {
        return static_cast<int>(searchResults.size());
    }

    void paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected) override
    {
        if (rowNumber < 0 || rowNumber >= static_cast<int>(searchResults.size()))
            return;

        const auto& result = searchResults[static_cast<size_t>(rowNumber)];

        if (rowIsSelected)
        {
            g.setColour(CustomLookAndFeel::getAccentColor().withAlpha(0.15f));
            g.fillRect(0, 0, width, height);
        }

        auto bounds = juce::Rectangle<int>(4, 2, width - 8, height - 4);

        auto titleBounds = bounds.removeFromTop(18);
        g.setColour(CustomLookAndFeel::getAccentColor());
        g.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
        g.drawText(result.title, titleBounds, juce::Justification::centredLeft);

        auto snippetBounds = bounds;
        g.setColour(CustomLookAndFeel::getTextColor().withAlpha(0.8f));
        g.setFont(juce::Font(juce::FontOptions(10.0f)));
        g.drawText(result.snippet.substring(0, 200), snippetBounds, juce::Justification::centredLeft);

        auto urlBounds = juce::Rectangle<int>(4, height - 14, width - 8, 12);
        g.setColour(CustomLookAndFeel::getTextColor().withAlpha(0.4f));
        g.setFont(juce::Font(juce::FontOptions(9.0f)));
        g.drawText(result.url, urlBounds, juce::Justification::centredLeft);
    }

    void listBoxItemClicked(int row, const juce::MouseEvent&) override
    {
        if (row >= 0 && row < static_cast<int>(searchResults.size()))
        {
            WebSearchEngine::openURLInBrowser(searchResults[static_cast<size_t>(row)].url);
        }
    }

private:
    WebSearchEngine& searchEngine;
    std::vector<WebSearchResult> searchResults;

    juce::Label titleLabel;
    juce::TextEditor searchBox;
    juce::TextButton searchButton;
    juce::Label statusLabel;
    juce::ListBox resultsListBox;

    void timerCallback() override {}

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WebSearchPanel)
};
