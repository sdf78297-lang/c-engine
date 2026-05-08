#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace Exo {

struct StoryChoice {
    std::string label;
    std::string nextNode;
    int identityDelta = 0;
};

struct StoryNode {
    std::string id;
    std::string title;
    std::string location;
    std::vector<std::string> body;
    std::vector<StoryChoice> choices;
    int identityOverride = -1;
    bool ending = false;
};

class StoryRuntime {
public:
    [[nodiscard]] bool loadFromFile(const std::filesystem::path& path);
    [[nodiscard]] bool loaded() const;
    [[nodiscard]] const std::string& title() const;
    [[nodiscard]] const std::string& lastError() const;
    [[nodiscard]] const StoryNode& currentNode() const;
    [[nodiscard]] const std::string& currentNodeId() const;
    [[nodiscard]] const std::vector<StoryNode>& nodes() const;
    [[nodiscard]] int identity() const;

    bool choose(std::size_t choiceIndex);
    bool jumpTo(const std::string& nodeId);
    bool restoreState(const std::string& nodeId, int identity);
    void applyIdentityDelta(int delta);

private:
    [[nodiscard]] const StoryNode* findNode(const std::string& id) const;
    bool enterNode(const std::string& id);

    std::string title_ = "Untitled story";
    std::string startNode_;
    std::string currentNode_;
    std::string lastError_;
    std::vector<StoryNode> nodes_;
    int identity_ = 100;
    bool loaded_ = false;
};

} // namespace Exo
