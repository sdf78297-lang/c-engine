#include <ExoEngine/Story/StoryRuntime.h>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <unordered_set>
#include <utility>

namespace Exo {
namespace {

using Json = nlohmann::json;

[[nodiscard]] std::string childPath(std::string_view parent, std::string_view key) {
    std::string path(parent);
    path += ".";
    path += key;
    return path;
}

[[nodiscard]] std::string childPath(std::string_view parent, std::size_t index) {
    std::string path(parent);
    path += "[";
    path += std::to_string(index);
    path += "]";
    return path;
}

class StoryJsonReader {
public:
    explicit StoryJsonReader(const Json& root)
        : root_(root) {}

    [[nodiscard]] std::string title() const {
        return requiredString(root_, "title", "$");
    }

    [[nodiscard]] std::string startNode() const {
        return requiredString(root_, "startNode", "$");
    }

    [[nodiscard]] int identity() const {
        const Json* value = optionalField(root_, "identity");
        if (value == nullptr) {
            return 100;
        }
        return std::clamp(number<int>(*value, "$.identity"), 0, 100);
    }

    [[nodiscard]] std::vector<StoryNode> nodes() const {
        const Json& nodesJson = requiredField(root_, "nodes", "$");
        if (!nodesJson.is_array()) {
            fail("$.nodes", "expected array");
        }

        std::vector<StoryNode> nodes;
        nodes.reserve(nodesJson.size());
        for (std::size_t i = 0; i < nodesJson.size(); ++i) {
            nodes.push_back(readNode(nodesJson[i], childPath("$.nodes", i)));
        }

        if (nodes.empty()) {
            fail("$.nodes", "must contain at least one story node");
        }

        return nodes;
    }

private:
    [[noreturn]] void fail(std::string_view path, std::string_view message) const {
        throw std::runtime_error("Story JSON error at " + std::string(path) + ": " + std::string(message));
    }

    [[nodiscard]] const Json* optionalField(const Json& object, std::string_view field) const {
        if (!object.is_object()) {
            return nullptr;
        }
        const auto it = object.find(std::string(field));
        return it == object.end() ? nullptr : &(*it);
    }

    [[nodiscard]] const Json& requiredField(
        const Json& object,
        std::string_view field,
        std::string_view path) const {
        if (!object.is_object()) {
            fail(path, "expected object");
        }

        const Json* value = optionalField(object, field);
        if (value == nullptr) {
            fail(childPath(path, field), "missing required field");
        }

        return *value;
    }

    [[nodiscard]] std::string requiredString(
        const Json& object,
        std::string_view field,
        std::string_view path) const {
        const Json& value = requiredField(object, field, path);
        const std::string fieldPath = childPath(path, field);
        if (!value.is_string()) {
            fail(fieldPath, "expected string");
        }

        const std::string result = value.get<std::string>();
        if (result.empty()) {
            fail(fieldPath, "must not be empty");
        }
        return result;
    }

    [[nodiscard]] std::string optionalString(
        const Json& object,
        std::string_view field,
        std::string_view path,
        std::string fallback) const {
        const Json* value = optionalField(object, field);
        if (value == nullptr) {
            return fallback;
        }

        if (!value->is_string()) {
            fail(childPath(path, field), "expected string");
        }
        return value->get<std::string>();
    }

    template <typename T>
    [[nodiscard]] T number(const Json& value, std::string_view path) const {
        if (!value.is_number_integer()) {
            fail(path, "expected integer");
        }
        return value.get<T>();
    }

    [[nodiscard]] int optionalInt(
        const Json& object,
        std::string_view field,
        std::string_view path,
        int fallback) const {
        const Json* value = optionalField(object, field);
        if (value == nullptr) {
            return fallback;
        }
        return number<int>(*value, childPath(path, field));
    }

    [[nodiscard]] bool optionalBool(
        const Json& object,
        std::string_view field,
        std::string_view path,
        bool fallback) const {
        const Json* value = optionalField(object, field);
        if (value == nullptr) {
            return fallback;
        }
        if (!value->is_boolean()) {
            fail(childPath(path, field), "expected boolean");
        }
        return value->get<bool>();
    }

    [[nodiscard]] std::vector<std::string> readBody(const Json& node, std::string_view path) const {
        const Json& bodyJson = requiredField(node, "body", path);
        const std::string bodyPath = childPath(path, "body");
        if (!bodyJson.is_array()) {
            fail(bodyPath, "expected array");
        }

        std::vector<std::string> body;
        body.reserve(bodyJson.size());
        for (std::size_t i = 0; i < bodyJson.size(); ++i) {
            if (!bodyJson[i].is_string()) {
                fail(childPath(bodyPath, i), "expected string");
            }
            body.push_back(bodyJson[i].get<std::string>());
        }
        return body;
    }

    [[nodiscard]] std::vector<StoryChoice> readChoices(const Json& node, std::string_view path) const {
        const Json* choicesJson = optionalField(node, "choices");
        if (choicesJson == nullptr) {
            return {};
        }

        const std::string choicesPath = childPath(path, "choices");
        if (!choicesJson->is_array()) {
            fail(choicesPath, "expected array");
        }

        std::vector<StoryChoice> choices;
        choices.reserve(choicesJson->size());
        for (std::size_t i = 0; i < choicesJson->size(); ++i) {
            const Json& choiceJson = (*choicesJson)[i];
            const std::string choicePath = childPath(choicesPath, i);

            StoryChoice choice;
            choice.label = requiredString(choiceJson, "label", choicePath);
            choice.nextNode = requiredString(choiceJson, "nextNode", choicePath);
            choice.identityDelta = optionalInt(choiceJson, "identityDelta", choicePath, 0);
            choices.push_back(std::move(choice));
        }

        return choices;
    }

    [[nodiscard]] StoryNode readNode(const Json& nodeJson, std::string_view path) const {
        if (!nodeJson.is_object()) {
            fail(path, "expected object");
        }

        StoryNode node;
        node.id = requiredString(nodeJson, "id", path);
        node.title = requiredString(nodeJson, "title", path);
        node.location = optionalString(nodeJson, "location", path, "");
        node.body = readBody(nodeJson, path);
        node.choices = readChoices(nodeJson, path);
        node.identityOverride = optionalInt(nodeJson, "identityOverride", path, -1);
        node.ending = optionalBool(nodeJson, "ending", path, false);
        return node;
    }

    const Json& root_;
};

} // namespace

bool StoryRuntime::loadFromFile(const std::filesystem::path& path) {
    loaded_ = false;
    lastError_.clear();

    std::ifstream input(path, std::ios::in | std::ios::binary);
    if (!input) {
        lastError_ = "Unable to open story JSON file: " + path.string();
        return false;
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();

    Json root;
    try {
        root = Json::parse(buffer.str());
        StoryJsonReader reader(root);
        title_ = reader.title();
        startNode_ = reader.startNode();
        nodes_ = reader.nodes();
        identity_ = reader.identity();

        std::unordered_set<std::string> ids;
        for (const StoryNode& node : nodes_) {
            if (!ids.insert(node.id).second) {
                lastError_ = "Duplicate story node id: " + node.id;
                return false;
            }
        }

        for (const StoryNode& node : nodes_) {
            for (const StoryChoice& choice : node.choices) {
                if (findNode(choice.nextNode) == nullptr) {
                    lastError_ = "Story choice from " + node.id + " points to missing node: " + choice.nextNode;
                    return false;
                }
            }
        }

        if (!enterNode(startNode_)) {
            lastError_ = "Story startNode not found: " + startNode_;
            return false;
        }
    } catch (const std::exception& error) {
        lastError_ = error.what();
        return false;
    }

    loaded_ = true;
    return true;
}

bool StoryRuntime::loaded() const {
    return loaded_;
}

const std::string& StoryRuntime::title() const {
    return title_;
}

const std::string& StoryRuntime::lastError() const {
    return lastError_;
}

const StoryNode& StoryRuntime::currentNode() const {
    const StoryNode* node = findNode(currentNode_);
    if (node != nullptr) {
        return *node;
    }
    return nodes_.front();
}

const std::vector<StoryNode>& StoryRuntime::nodes() const {
    return nodes_;
}

int StoryRuntime::identity() const {
    return identity_;
}

bool StoryRuntime::choose(std::size_t choiceIndex) {
    if (!loaded_) {
        return false;
    }

    const StoryNode& node = currentNode();
    if (choiceIndex >= node.choices.size()) {
        return false;
    }

    const StoryChoice& choice = node.choices[choiceIndex];
    if (findNode(choice.nextNode) == nullptr) {
        return false;
    }

    identity_ = std::clamp(identity_ + choice.identityDelta, 0, 100);
    return enterNode(choice.nextNode);
}

const StoryNode* StoryRuntime::findNode(const std::string& id) const {
    const auto it = std::find_if(nodes_.begin(), nodes_.end(), [&id](const StoryNode& node) {
        return node.id == id;
    });
    return it == nodes_.end() ? nullptr : &(*it);
}

bool StoryRuntime::enterNode(const std::string& id) {
    const StoryNode* node = findNode(id);
    if (node == nullptr) {
        return false;
    }

    currentNode_ = id;
    if (node->identityOverride >= 0) {
        identity_ = std::clamp(node->identityOverride, 0, 100);
    }
    return true;
}

} // namespace Exo
