#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <functional>
#include <vector>

struct WebSearchResult
{
    juce::String title;
    juce::String url;
    juce::String snippet;
};

class WebSearchEngine
{
public:
    using ResultsCallback = std::function<void (const juce::String& query, const std::vector<WebSearchResult>& results, const juce::String& error)>;

    WebSearchEngine()
        : sharedState(std::make_shared<SharedState>()) {}

    ~WebSearchEngine()
    {
        cancel();
    }

    void search(const juce::String& query, ResultsCallback callback)
    {
        auto ss = sharedState;
        auto gen = ++ss->generation;

        if (query.trim().isEmpty())
        {
            if (callback)
                juce::MessageManager::callAsync([callback, query]()
                {
                    callback(query, {}, {});
                });
            return;
        }

        std::thread([ss, gen, query, callback]()
        {
            performSearch(ss, gen, query, callback);
        }).detach();
    }

    void cancel()
    {
        if (sharedState)
            ++sharedState->generation;
    }

    static void openURLInBrowser(const juce::String& url)
    {
        if (url.isNotEmpty())
            juce::URL(url).launchInDefaultBrowser();
    }

private:
    struct SharedState
    {
        std::atomic<uint64_t> generation { 0 };
    };

    std::shared_ptr<SharedState> sharedState;

    static void performSearch(std::shared_ptr<SharedState> ss, uint64_t gen, const juce::String& query, ResultsCallback callback)
    {
        std::vector<WebSearchResult> results;

        auto url = juce::URL("https://api.duckduckgo.com")
            .withParameter("q", query)
            .withParameter("format", "json")
            .withParameter("no_html", "1")
            .withParameter("skip_disambig", "1")
            .withParameter("t", "aura_plugin");

        auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
            .withConnectionTimeoutMs(10000)
            .withResponseHeaders(nullptr);

        auto is = url.createInputStream(options);
        if (is == nullptr)
        {
            if (gen != ss->generation.load())
                return;
            if (callback)
                juce::MessageManager::callAsync([ss, gen, callback, query]()
                {
                    if (gen == ss->generation.load())
                        callback(query, {}, "Connection failed");
                });
            return;
        }

        if (is->failed())
        {
            if (gen != ss->generation.load())
                return;
            if (callback)
                juce::MessageManager::callAsync([ss, gen, callback, query, code = is->getStatusCode()]()
                {
                    if (gen == ss->generation.load())
                        callback(query, {}, "HTTP error: " + juce::String(code));
                });
            return;
        }

        auto jsonString = is->readEntireStreamAsString();
        is = nullptr;
        if (gen != ss->generation.load()) return;

        auto parsed = juce::JSON::parse(jsonString);
        if (auto* root = parsed.getDynamicObject())
        {
            auto abstractText = root->getProperty("AbstractText").toString();
            auto abstractURL = root->getProperty("AbstractURL").toString();
            auto abstractSource = root->getProperty("AbstractSource").toString();

            if (abstractText.isNotEmpty())
            {
                WebSearchResult r;
                r.title = abstractSource.isNotEmpty() ? abstractSource : "Abstract";
                r.url = abstractURL;
                r.snippet = abstractText;
                results.push_back(r);
            }

            if (auto* related = root->getProperty("RelatedTopics").getArray())
            {
                for (auto& item : *related)
                {
                    if (gen != ss->generation.load()) return;

                    if (auto* obj = item.getDynamicObject())
                    {
                        if (obj->hasProperty("Topics"))
                        {
                            if (auto* topics = obj->getProperty("Topics").getArray())
                            {
                                for (auto& sub : *topics)
                                {
                                    if (gen != ss->generation.load()) return;
                                    if (auto* subObj = sub.getDynamicObject())
                                        addResult(results, subObj);
                                }
                            }
                        }
                        else
                        {
                            addResult(results, obj);
                        }
                    }
                }
            }

            if (auto* resultsArray = root->getProperty("Results").getArray())
            {
                for (auto& item : *resultsArray)
                {
                    if (gen != ss->generation.load()) return;
                    if (auto* obj = item.getDynamicObject())
                        addResult(results, obj);
                }
            }
        }

        if (gen != ss->generation.load()) return;

        if (results.empty() && jsonString.isNotEmpty())
        {
            WebSearchResult r;
            r.title = "Raw API Response";
            r.snippet = jsonString.substring(0, 500);
            results.push_back(r);
        }

        juce::MessageManager::callAsync([ss, gen, callback, query, results]()
        {
            if (gen == ss->generation.load())
                callback(query, results, {});
        });
    }

    static void addResult(std::vector<WebSearchResult>& results, const juce::DynamicObject* obj)
    {
        auto text = obj->getProperty("Text").toString();
        auto url = obj->getProperty("FirstURL").toString();
        auto result = obj->getProperty("Result").toString();

        if (text.isEmpty())
            return;

        WebSearchResult r;
        r.title = text.upToFirstOccurrenceOf(" - ", false, false).trim();
        if (r.title.isEmpty())
            r.title = text;
        r.url = url;
        r.snippet = result.isNotEmpty() ? result : text;
        results.push_back(r);
    }
};
