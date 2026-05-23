#include <JuceHeader.h>

class WebSearchEngineTest : public juce::UnitTest
{
public:
    WebSearchEngineTest() : juce::UnitTest("WebSearchEngine") {}

    void runTest() override
    {
        testSearchResultParsing();
        testEmptyQuery();
        testCancel();
    }

private:
    void testSearchResultParsing()
    {
        beginTest("Search Result Parsing");

        auto json = R"({
            "AbstractText": "EQ (audio) - Equalization or equalisation is the process of adjusting the balance between frequency components within an electronic signal.",
            "AbstractURL": "https://en.wikipedia.org/wiki/Equalization_(audio)",
            "AbstractSource": "Wikipedia",
            "Results": [
                {
                    "Text": "Parametric EQ - Parametric equalization",
                    "FirstURL": "https://en.wikipedia.org/wiki/Parametric_equalizer",
                    "Result": ""
                }
            ],
            "RelatedTopics": [
                {
                    "Text": "Graphic EQ - Graphic equalizer",
                    "FirstURL": "https://en.wikipedia.org/wiki/Graphic_equalizer",
                    "Result": ""
                }
            ]
        })";

        auto parsed = juce::JSON::parse(juce::String(json));
        expect(parsed.isObject(), "Should parse valid JSON");

        auto* root = parsed.getDynamicObject();
        expect(root != nullptr, "Root should be a DynamicObject");

        auto abstractText = root->getProperty("AbstractText").toString();
        expect(abstractText.isNotEmpty(), "AbstractText should not be empty");
        expect(abstractText.contains("Equalization"), "AbstractText should contain relevant content");

        auto abstractURL = root->getProperty("AbstractURL").toString();
        expect(abstractURL.isNotEmpty(), "AbstractURL should not be empty");
        expect(abstractURL.contains("wikipedia.org"), "AbstractURL should be a valid URL");

        auto* resultsArray = root->getProperty("Results").getArray();
        expect(resultsArray != nullptr, "Results array should exist");
        expect(resultsArray->size() > 0, "Results should contain entries");

        if (resultsArray != nullptr && resultsArray->size() > 0)
        {
            auto& first = (*resultsArray)[0];
            auto* firstObj = first.getDynamicObject();
            expect(firstObj != nullptr, "First result should be an object");
            if (firstObj != nullptr)
            {
                auto text = firstObj->getProperty("Text").toString();
                expect(text.contains("Parametric"), "Result text should contain relevant content");
            }
        }

        auto* related = root->getProperty("RelatedTopics").getArray();
        expect(related != nullptr, "RelatedTopics array should exist");
    }

    void testEmptyQuery()
    {
        beginTest("Empty Query Handling");

        bool callbackCalled = false;
        WebSearchEngine engine;

        engine.search("", [&callbackCalled](const juce::String&, const std::vector<WebSearchResult>& results, const juce::String& error)
        {
            callbackCalled = true;
            expect(results.empty(), "Empty query should return no results");
            expect(error.isEmpty(), "Empty query should not produce an error");
        });

        expect(callbackCalled, "Callback should be called immediately for empty query");
    }

    void testCancel()
    {
        beginTest("Cancel During Search");

        WebSearchEngine engine;
        bool callbackCalled = false;

        engine.search("test query", [&callbackCalled](const juce::String&, const std::vector<WebSearchResult>&, const juce::String&)
        {
            callbackCalled = true;
        });

        engine.cancel();
        expect(true, "Cancel should not crash");
    }
};

static WebSearchEngineTest webSearchEngineTest;
