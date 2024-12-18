#include <iostream>
#include <fstream>
#include <functional>
#include <chrono>
#include <string>
#include <map>
#include <sstream>
#include <queue>
#include <algorithm>
#include <utility>
#include <numeric>


#define MY_INPUT_PATH R"(..\inputDay20.txt)"
#define TEST_INPUT1_PART1_PATH R"(..\testInput1Part1Day20.txt)"
#define TEST_INPUT2_PART1_PATH R"(..\testInput2Part1Day20.txt)"


class Module {
public:
    Module(const std::string& name) : name{ name } {}

    virtual ~Module() = default;

    virtual void processInputs(const std::string& module, bool pulse) = 0;

    void addOutput(const std::string& moduleName) {
        moduleNetwork.at(name)->outputModules.push_back(moduleName);
    }

    static void addModuleToNetwork(
            const std::string& name,
            const std::shared_ptr<Module>& module
    ) {
        moduleNetwork[name] = module;
    }

    friend class ButtonModule;

    bool containsOutputModule(const std::string& outputModule) {
        return std::find(
                outputModules.begin(),
                outputModules.end(),
                outputModule
        ) != outputModules.end();
    }

    static void clearNetwork() {
        moduleNetwork.clear();
        numOfPulses.clear();
        while (!pulseQueue.empty()) {
            pulseQueue.pop();
        }
    }

    static std::map<bool, long long> getPulseCount() {
        return numOfPulses;
    }

    static std::map<std::string, std::shared_ptr<Module>>& getModuleNetwork() {
        return moduleNetwork;
    }

protected:
    std::string name;
    std::vector<std::string> outputModules;
    static std::map<std::string, std::shared_ptr<Module>> moduleNetwork;
    static std::queue<std::function<void()>> pulseQueue;
    static std::map<bool, long long> numOfPulses;
    static std::pair<std::string, bool> lastActiveModule;
};

std::map<std::string, std::shared_ptr<Module>> Module::moduleNetwork;
std::queue<std::function<void()>> Module::pulseQueue;
std::map<bool, long long> Module::numOfPulses;
std::pair<std::string, bool> Module::lastActiveModule;

class FlipFlipModule final : public Module {
public:
    FlipFlipModule(const std::string& name) : Module{ name } {}

    void processInputs(const std::string& module, bool pulse) override {
        if (pulse) {
            return;
        }

        state = !state;

        lastActiveModule = std::make_pair(this->name, state);

        for (const std::string& outputModule : outputModules) {
            numOfPulses[state]++;
            if (moduleNetwork.contains(outputModule)) {
                pulseQueue.emplace(
                    [outputModule, this]() {
                        moduleNetwork.at(outputModule)->processInputs(name, state);
                    }
                );
            }
        }
    }

private:
    bool state{ false };

};

class ConjunctionModule final : public Module {
public:
    ConjunctionModule(const std::string& name) : Module{ name } {}

    ConjunctionModule(const Module& module) : Module{ module } {}

    void processInputs(const std::string& module, bool pulse) override {
        memory[module] = pulse;

        bool output{ false };
        for (auto&& mem : memory) {
            output = output || !mem.second;
        }

        lastActiveModule = std::make_pair(this->name, output);

        for (const std::string& outputModule : outputModules) {
            numOfPulses[output]++;
            if (moduleNetwork.contains(outputModule)) {
                pulseQueue.emplace(
                    [outputModule, output, this]() {
                        moduleNetwork.at(outputModule)->processInputs(name, output);
                    }
                );
            }
        }
    }

    static void allocateMemorySlots() {
        for (auto&& mybConjunctionModule : Module::moduleNetwork) {
            if (std::shared_ptr<ConjunctionModule> conjunctionModule{
                    std::dynamic_pointer_cast<ConjunctionModule>(mybConjunctionModule.second)
                }
            ) {
                for (auto&& module : Module::moduleNetwork) {
                    if (module.second->containsOutputModule(conjunctionModule->name)) {
                        conjunctionModule->memory[module.first] = false;
                    }
                }
            }
        }
    }

private:
    std::map<std::string, bool> memory;

};

class BroadcastModule final : public Module {
public:
    BroadcastModule(const std::string& name) : Module{ name } {}

    void processInputs(const std::string& module, bool pulse) override {
        lastActiveModule = std::make_pair(this->name, pulse);

        for (const std::string& outputModule : outputModules) {
            numOfPulses[pulse]++;
            if (moduleNetwork.contains(outputModule)) {
                pulseQueue.emplace(
                    [outputModule, pulse, this]() {
                        moduleNetwork.at(outputModule)->processInputs(name, pulse);
                    }
                );
            }
        }
    }
};

class ButtonModule final {
public:
    ButtonModule(
            const std::string& name,
            const std::string& broadcaster
    ) : name{ name }, broadcaster{ broadcaster } {}

    std::map<std::string, int> push(std::vector<std::string> trackModules = {}) {
        std::map<std::string, int> trackedNumOfPushes;

        numOfPushes++;
        Module::lastActiveModule = std::make_pair(name, false);
        Module::numOfPulses[false]++;
        Module::moduleNetwork.at(broadcaster)->processInputs(name, false);
        while (!Module::pulseQueue.empty()) {
            Module::pulseQueue.front()();

            if (auto found {
                    std::find(
                        trackModules.begin(),
                        trackModules.end(),
                        Module::lastActiveModule.first
                    )
                }; found != trackModules.end() &&  Module::lastActiveModule.second
            ) {

                if (!trackedNumOfPushes.contains( Module::lastActiveModule.first)) {
                    trackedNumOfPushes[Module::lastActiveModule.first] = numOfPushes;
                }
            }

            Module::pulseQueue.pop();
        }

        return trackedNumOfPushes;
    }

private:
    std::string broadcaster;
    std::string name;
    int numOfPushes{};
};


long long solutionPart1(const char* inputPath) {
    std::ifstream input(inputPath);

    Module::clearNetwork();

    for (std::string line; std::getline(input, line); ) {
        std::stringstream ss(line);

        std::string nameAndType, arrow;
        ss >> nameAndType >> arrow;

        std::string name, type;
        if (nameAndType.front() == '&' | nameAndType.front() == '%') {
            type = nameAndType.front();
            nameAndType.erase(nameAndType.begin());
        }
        name = nameAndType;

        std::vector<std::string> outputModules;
        for (std::string outputModule; ss >> outputModule; ) {
            if (outputModule.back() == ',') {
                outputModule.erase(outputModule.end() - 1);
            }

            outputModules.push_back(outputModule);
        }

        std::shared_ptr<Module> module;
        if (type == "%") {
            module = std::make_shared<FlipFlipModule>(name);
            Module::addModuleToNetwork(name, module);
        } else if (type == "&") {
            module = std::make_shared<ConjunctionModule>(name);
            Module::addModuleToNetwork(name, module);
        } else {
            module = std::make_shared<BroadcastModule>(name);
            Module::addModuleToNetwork(name, module);
        }

        for (auto&& outputModule : outputModules) {
            module->addOutput(outputModule);
        }
    }

    ConjunctionModule::allocateMemorySlots();

    ButtonModule button("button", "broadcaster");

    for (int i{}; i < 1000; ++i) {
        button.push();
    }

    std::map<bool, long long> pulseCount{ Module::getPulseCount() };
    return pulseCount[false] * pulseCount[true];
}


long long solutionPart2(const char* inputPath) {
    std::ifstream input(inputPath);

    Module::clearNetwork();

    for (std::string line; std::getline(input, line); ) {
        std::stringstream ss(line);

        std::string nameAndType, arrow;
        ss >> nameAndType >> arrow;

        std::string name, type;
        if (nameAndType.front() == '&' | nameAndType.front() == '%') {
            type = nameAndType.front();
            nameAndType.erase(nameAndType.begin());
        }
        name = nameAndType;

        std::vector<std::string> outputModules;
        for (std::string outputModule; ss >> outputModule; ) {
            if (outputModule.back() == ',') {
                outputModule.erase(outputModule.end() - 1);
            }

            outputModules.push_back(outputModule);
        }

        std::shared_ptr<Module> module;
        if (type == "%") {
            module = std::make_shared<FlipFlipModule>(name);
            Module::addModuleToNetwork(name, module);
        } else if (type == "&") {
            module = std::make_shared<ConjunctionModule>(name);
            Module::addModuleToNetwork(name, module);
        } else {
            module = std::make_shared<BroadcastModule>(name);
            Module::addModuleToNetwork(name, module);
        }

        for (auto&& outputModule : outputModules) {
            module->addOutput(outputModule);
        }
    }

    ConjunctionModule::allocateMemorySlots();

    ButtonModule button("button", "broadcaster");

    std::vector<std::string> trackModules;
    auto&& moduleNetwork{ Module::getModuleNetwork() };
    for (auto&& moduleWithRxOutput : moduleNetwork) {
        if (moduleWithRxOutput.second->containsOutputModule("rx")) {
            for (auto&& module : moduleNetwork) {
                if (module.second->containsOutputModule(moduleWithRxOutput.first)) {
                    trackModules.push_back(module.first);
                }
            }
        }
    }

    std::vector<int> numOfPushes;

    while (!trackModules.empty()) {
        std::map<std::string, int> trackedNumOfPushes{ button.push(trackModules) };

        for (auto&& num : trackedNumOfPushes) {
            if (auto found{
                    std::find(
                        trackModules.begin(),
                        trackModules.end(),
                        num.first
                    )
                }; found != trackModules.end()
            ) {

                numOfPushes.push_back(num.second);
                trackModules.erase(found);
            }
        }
    }

    long long numOfPushesUntilRx{ 1 };

    for (auto&& n : numOfPushes) {
        numOfPushesUntilRx = std::lcm(numOfPushesUntilRx, n);
    }

    return numOfPushesUntilRx;
}


double measureTime(const std::function<void()>& func, int numOfRuns) {
    auto startTime = std::chrono::steady_clock::now();
    for (int i{}; i < numOfRuns; i++) {
        func();
    }
    auto endTime = std::chrono::steady_clock::now();

    return std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count() / (double)numOfRuns;
}


int main() {
    std::cout << "Test inputs:" << std::endl;
    std::cout << "\tInput 1 Part 1: " << solutionPart1(TEST_INPUT1_PART1_PATH) << std::endl;
    std::cout << "\tInput 2 Part 1: " << solutionPart1(TEST_INPUT2_PART1_PATH) << std::endl;
    std::cout << "My input:" << std::endl;
    std::cout << "\tPart 1: " << solutionPart1(MY_INPUT_PATH) << std::endl;
    std::cout << "\tPart 2: " << solutionPart2(MY_INPUT_PATH) << std::endl;
    std::cout << "My input runtime [ms]:" << std::endl;
    std::cout << "\tPart 1: " << measureTime([](){ solutionPart1(MY_INPUT_PATH); }, 1000) << std::endl;
    std::cout << "\tPart 2: " << measureTime([](){ solutionPart2(MY_INPUT_PATH); }, 1000) << std::endl;

    return 0;
}