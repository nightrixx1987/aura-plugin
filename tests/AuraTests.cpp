#include <JuceHeader.h>
#include "TestBiquadFilter.cpp"
#include "TestSVFFilter.cpp"
#include "TestEQBand.cpp"
#include "TestEQProcessor.cpp"
#include "TestWebSearchEngine.cpp"

class AuraTestRunner : public juce::UnitTestRunner
{
public:
    void runAll()
    {
        setAssertOnFailure(false);
        runAllTests();
    }
};

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    AuraTestRunner runner;
    runner.runAll();
    return juce::UnitTest::getNumAllTestsFailed();
}
