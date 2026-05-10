#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace {

using Json = nlohmann::json;

struct Diagnostic {
    std::string severity;
    std::filesystem::path path;
    std::string message;
};

class Validator {
public:
    explicit Validator(std::filesystem::path root)
        : root_(std::move(root)) {}

    int run() {
        validateProjectManifest();
        validateAiContext();
        validateHtmlMenuRuntime();
        validateCatalog(root_ / "data" / "items.json", "items");
        validateCatalog(root_ / "data" / "puzzles.json", "puzzles");
        validateRooms();
        validateStoryGraph(storyPath());
        printReport();
        return hasErrors_ ? 1 : 0;
    }

private:
    [[nodiscard]] std::filesystem::path resolve(std::filesystem::path path) const {
        return path.is_absolute() ? path : root_ / path;
    }

    [[nodiscard]] std::string readFile(const std::filesystem::path& path) {
        std::ifstream input(path, std::ios::in | std::ios::binary);
        if (!input) {
            error(path, "unable to open file");
            return {};
        }

        std::ostringstream buffer;
        buffer << input.rdbuf();
        std::string text = buffer.str();
        validateTextEncoding(path, text);
        return text;
    }

    [[nodiscard]] Json parseJsonFile(const std::filesystem::path& path) {
        const std::string text = readFile(path);
        if (text.empty()) {
            return Json();
        }

        try {
            return Json::parse(text);
        } catch (const std::exception& e) {
            error(path, std::string("JSON parse failed: ") + e.what());
            return Json();
        }
    }

    void validateTextEncoding(const std::filesystem::path& path, std::string_view text) {
        if (text.find("\xEF\xBF\xBD") != std::string_view::npos) {
            error(path, "contains Unicode replacement characters");
        }
        if (text.find("Рќ") != std::string_view::npos || text.find("Рџ") != std::string_view::npos) {
            warn(path, "contains common mojibake markers; confirm this file is UTF-8 and not double-encoded");
        }
    }

    void validateProjectManifest() {
        const std::filesystem::path path = root_ / "data" / "project_manifest.json";
        Json root = parseJsonFile(path);
        if (!root.is_object()) {
            return;
        }

        requireString(root, path, "schema");
        requireObject(root, path, "project");
        requireObject(root, path, "entry");
        requireObject(root, path, "canonicalFiles");

        const Json* canonical = find(root, "canonicalFiles");
        if (canonical != nullptr && canonical->is_object()) {
            validateManifestFileRef(*canonical, path, "aiBible");
            validateManifestFileRef(*canonical, path, "aiWorkflow");
            validateManifestFileRef(*canonical, path, "aiContext");
            validateManifestFileRef(*canonical, path, "items");
            validateManifestFileRef(*canonical, path, "puzzles");

            const Json* rooms = find(*canonical, "rooms");
            if (rooms != nullptr && rooms->is_array()) {
                bool hasAmbulanceRoom = false;
                for (const Json& room : *rooms) {
                    if (room.is_string()) {
                        const std::string roomPath = room.get<std::string>();
                        if (roomPath == "data/rooms/ambulance_patient_compartment/room.json") {
                            hasAmbulanceRoom = true;
                        }
                        requireExists(resolve(roomPath), path, "canonical room file missing");
                    }
                }
                if (!hasAmbulanceRoom) {
                    error(path, "canonicalFiles.rooms must include data/rooms/ambulance_patient_compartment/room.json");
                }
            }
        }
    }

    void validateAiContext() {
        const std::filesystem::path path = root_ / "data" / "ai_context.json";
        Json root = parseJsonFile(path);
        if (root.is_object()) {
            requireString(root, path, "schema");
        }
    }

    void validateHtmlMenuRuntime() {
        const std::filesystem::path path = root_ / "assets" / "ui" / "main_menu" / "web" / "index.html";
        const std::string text = readFile(path);
        if (text.empty()) {
            return;
        }

        for (const std::string_view marker : {
                 "__bundler/manifest",
                 "__bundler/template",
                 "__bundler_loading",
                 "__bundler_thumbnail",
                 "DecompressionStream",
                 "createObjectURL",
                 "DOMParser",
                 "atob(",
             }) {
            if (text.find(marker) != std::string::npos) {
                error(path, "HTML menu runtime still contains standalone bundle marker: " + std::string(marker));
            }
        }

        if (text.find(".story-mode .screen:not(.active)") == std::string::npos) {
            error(path, "story-mode must pause inactive screen animations");
        }
        if (text.find("scrollTop") != std::string::npos) {
            error(path, "story typewriter must not force scrollTop on every character");
        }
        if (text.find("insertAdjacentHTML") != std::string::npos) {
            error(path, "story typewriter must not rebuild HTML fragments per character");
        }
    }

    void validateCatalog(const std::filesystem::path& path, std::string_view arrayField) {
        Json root = parseJsonFile(path);
        if (!root.is_object()) {
            return;
        }
        requireString(root, path, "schema");
        const Json* entries = find(root, arrayField);
        if (entries == nullptr || !entries->is_array() || entries->empty()) {
            error(path, "missing or empty catalog array: " + std::string(arrayField));
            return;
        }
        validateIds(*entries, path);
    }

    void validateRooms() {
        const std::filesystem::path roomsRoot = root_ / "data" / "rooms";
        if (!std::filesystem::exists(roomsRoot)) {
            error(roomsRoot, "data/rooms directory missing");
            return;
        }

        for (const std::filesystem::directory_entry& entry : std::filesystem::recursive_directory_iterator(roomsRoot)) {
            if (!entry.is_regular_file() || entry.path().filename() != "room.json") {
                continue;
            }
            validateRoom(entry.path());
        }
    }

    void validateRoom(const std::filesystem::path& path) {
        Json root = parseJsonFile(path);
        if (!root.is_object()) {
            return;
        }

        requireString(root, path, "id");
        requireString(root, path, "name");
        validateAsciiId(root.value("id", ""), path, "id");
        requireNonEmptyArray(root, path, "staticMeshes");
        requireNonEmptyArray(root, path, "pointLights");
        requireNonEmptyArray(root, path, "fixedCameras");

        const Json* meshes = find(root, "staticMeshes");
        if (meshes != nullptr && meshes->is_array()) {
            std::unordered_set<std::string> meshNames;
            int missingMeshSources = 0;
            for (const Json& mesh : *meshes) {
                if (!mesh.is_object()) {
                    continue;
                }
                requireString(mesh, path, "name");
                requireString(mesh, path, "meshAsset");
                if (const Json* name = find(mesh, "name"); name != nullptr && name->is_string()) {
                    const std::string value = name->get<std::string>();
                    if (!meshNames.insert(value).second) {
                        error(path, "duplicate staticMesh name: " + value);
                    }
                }
                const Json* source = find(mesh, "meshSource");
                if (source != nullptr && source->is_string()) {
                    requireExists(resolve(source->get<std::string>()), path, "meshSource missing");
                    if (!std::filesystem::exists(resolve(source->get<std::string>()))) {
                        ++missingMeshSources;
                    }
                } else {
                    ++missingMeshSources;
                }
                validateOptionalFlag(mesh, path, "visibleWhenFlag");
                validateOptionalFlag(mesh, path, "hiddenWhenFlag");
                validateStaticMeshRenderOverrides(mesh, path);
            }
            if (root.value("id", "") == "ambulance_patient_compartment"
                && !meshes->empty()
                && missingMeshSources == static_cast<int>(meshes->size())) {
                error(path, "ambulance_patient_compartment has no existing meshSource assets; scene would render empty");
            }
        }
        validateCollisionBoxes(root, path);
        validateRenderEnvironment(root, path);
        const std::unordered_set<std::string> audioCueIds = validateAudioCues(root, path);
        const std::unordered_set<std::string> sequenceIds = validateSequences(root, path, audioCueIds);
        validateCollapseConfig(root, path, audioCueIds, sequenceIds);
        validateRoomTriggers(root, path, audioCueIds, sequenceIds);
        validateRoomEnterEvents(root, path, audioCueIds);

        const bool hasDoors = hasNonEmptyArray(root, "doors");
        const bool hasInteractables = hasNonEmptyArray(root, "interactables") || hasNonEmptyArray(root, "interactions");
        const bool hasSpawns = hasNonEmptyArray(root, "spawnPoints") || hasNonEmptyArray(root, "spawns");
        if (!hasDoors) {
            error(path, "room must define at least one door");
        }
        if (!hasInteractables) {
            error(path, "room must define at least one interactable or interaction");
        }
        if (!hasSpawns) {
            error(path, "room must define spawnPoints or spawns");
        }
    }

    [[nodiscard]] std::unordered_set<std::string> validateAudioCues(
        const Json& room,
        const std::filesystem::path& path) {
        std::unordered_set<std::string> ids;
        const Json* audio = find(room, "audio");
        if (audio == nullptr) {
            return ids;
        }
        if (!audio->is_object()) {
            error(path, "audio must be an object");
            return ids;
        }

        const Json* musicPath = find(*audio, "musicPath");
        if (musicPath != nullptr) {
            if (!musicPath->is_string()) {
                error(path, "audio.musicPath must be a string");
            } else {
                requireExists(resolve(musicPath->get<std::string>()), path, "music file missing");
            }
        }

        const Json* cues = find(*audio, "cues");
        if (cues == nullptr) {
            return ids;
        }
        if (!cues->is_array()) {
            error(path, "audio.cues must be an array");
            return ids;
        }

        for (const Json& cue : *cues) {
            if (!cue.is_object()) {
                error(path, "audio cue must be an object");
                continue;
            }

            const Json* id = find(cue, "id");
            if (id == nullptr || !id->is_string()) {
                error(path, "audio cue is missing id");
                continue;
            }
            const std::string value = id->get<std::string>();
            validateAsciiId(value, path, "audio cue id");
            if (!ids.insert(value).second) {
                error(path, "duplicate audio cue id: " + value);
            }

            const Json* audioPath = find(cue, "path");
            if (audioPath == nullptr || !audioPath->is_string()) {
                error(path, "audio cue is missing path");
                continue;
            }
            requireExists(resolve(audioPath->get<std::string>()), path, "audio cue file missing");
        }

        return ids;
    }

    void validateRoomTriggers(
        const Json& room,
        const std::filesystem::path& path,
        const std::unordered_set<std::string>& audioCueIds,
        const std::unordered_set<std::string>& sequenceIds) {
        const Json* triggers = find(room, "triggers");
        if (triggers == nullptr) {
            return;
        }
        if (!triggers->is_array()) {
            error(path, "triggers must be an array");
            return;
        }

        for (const Json& trigger : *triggers) {
            if (!trigger.is_object()) {
                error(path, "trigger must be an object");
                continue;
            }
            validateOptionalFlag(trigger, path, "setFlag");
            validateAudioCueRef(trigger, path, audioCueIds);
            const Json* sequenceId = find(trigger, "sequenceId");
            if (sequenceId == nullptr) {
                sequenceId = find(trigger, "startSequence");
            }
            if (sequenceId != nullptr) {
                if (!sequenceId->is_string()) {
                    error(path, "trigger sequenceId must be a string");
                } else {
                    const std::string value = sequenceId->get<std::string>();
                    validateAsciiId(value, path, "trigger sequenceId");
                    if (!sequenceIds.contains(value)) {
                        error(path, "trigger sequenceId points to missing sequence: " + value);
                    }
                }
            }
            const Json* bounds = find(trigger, "bounds");
            if (bounds == nullptr || !bounds->is_object()) {
                error(path, "trigger is missing bounds");
            } else {
                validateBounds(*bounds, path, "trigger bounds");
            }
        }
    }

    [[nodiscard]] std::unordered_set<std::string> validateSequences(
        const Json& room,
        const std::filesystem::path& path,
        const std::unordered_set<std::string>& audioCueIds) {
        std::unordered_set<std::string> ids;
        const Json* sequences = find(room, "sequences");
        if (sequences == nullptr) {
            return ids;
        }
        if (!sequences->is_array()) {
            error(path, "sequences must be an array");
            return ids;
        }

        for (const Json& sequence : *sequences) {
            if (!sequence.is_object()) {
                error(path, "sequence must be an object");
                continue;
            }
            const Json* id = find(sequence, "id");
            if (id == nullptr || !id->is_string()) {
                error(path, "sequence is missing id");
                continue;
            }
            const std::string value = id->get<std::string>();
            validateAsciiId(value, path, "sequence id");
            if (!ids.insert(value).second) {
                error(path, "duplicate sequence id: " + value);
            }

            const Json* steps = find(sequence, "steps");
            if (steps == nullptr || !steps->is_array()) {
                error(path, "sequence steps must be an array");
                continue;
            }
            for (const Json& step : *steps) {
                if (!step.is_object()) {
                    error(path, "sequence step must be an object");
                    continue;
                }
                const Json* time = find(step, "time");
                if (time != nullptr && !time->is_number()) {
                    error(path, "sequence step time must be a number");
                }
                const Json* actions = find(step, "actions");
                if (actions == nullptr || !actions->is_array()) {
                    error(path, "sequence step actions must be an array");
                    continue;
                }
                for (const Json& action : *actions) {
                    if (!action.is_object()) {
                        error(path, "sequence action must be an object");
                        continue;
                    }
                    const Json* type = find(action, "type");
                    if (type == nullptr || !type->is_string()) {
                        error(path, "sequence action type must be a string");
                    } else {
                        const std::string actionType = type->get<std::string>();
                        static const std::unordered_set<std::string> knownActions {
                            "setFlag",
                            "playAudio",
                            "stopAudio",
                            "fadeScreen",
                            "setCameraMode",
                            "lockPlayerControl",
                            "unlockPlayerControl",
                            "transitionRoom",
                            "setEntityVisible",
                            "setMusicVolume",
                            "adjustIdentity",
                            "setIdentity",
                            "addItem",
                            "removeItem",
                            "setObjective",
                            "completeObjective"
                        };
                        if (!knownActions.contains(actionType)) {
                            error(path, "unknown sequence action type: " + actionType);
                        }
                        if (actionType == "transitionRoom") {
                            const Json* roomId = find(action, "roomId");
                            if (roomId == nullptr) {
                                roomId = find(action, "targetRoomId");
                            }
                            if (roomId == nullptr) {
                                roomId = find(action, "targetRoom");
                            }
                            if (roomId == nullptr || !roomId->is_string()) {
                                error(path, "transitionRoom action must define roomId");
                            } else {
                                requireExists(root_ / "data" / "rooms" / roomId->get<std::string>() / "room.json",
                                    path,
                                    "transitionRoom target room missing");
                            }
                        }
                    }
                    validateOptionalFlag(action, path, "flag");
                    validateAudioCueRef(action, path, audioCueIds);
                }
            }
        }
        return ids;
    }

    void validateRoomEnterEvents(
        const Json& room,
        const std::filesystem::path& path,
        const std::unordered_set<std::string>& audioCueIds) {
        const Json* events = find(room, "roomEnterEvents");
        if (events == nullptr) {
            return;
        }
        if (!events->is_array()) {
            error(path, "roomEnterEvents must be an array");
            return;
        }

        for (const Json& event : *events) {
            if (!event.is_object()) {
                error(path, "room enter event must be an object");
                continue;
            }
            validateOptionalFlag(event, path, "setFlag");
            validateAudioCueRef(event, path, audioCueIds);
        }
    }

    void validateAudioCueRef(
        const Json& object,
        const std::filesystem::path& path,
        const std::unordered_set<std::string>& audioCueIds) {
        const Json* cue = find(object, "audioCue");
        if (cue == nullptr) {
            return;
        }
        if (!cue->is_string()) {
            error(path, "audioCue must be a string");
            return;
        }

        const std::string value = cue->get<std::string>();
        validateAsciiId(value, path, "audioCue");
        if (!audioCueIds.contains(value)) {
            error(path, "audioCue points to missing audio.cues id: " + value);
        }
    }

    void validateCollapseConfig(
        const Json& room,
        const std::filesystem::path& path,
        const std::unordered_set<std::string>& audioCueIds,
        const std::unordered_set<std::string>& sequenceIds) {
        validateOptionalFlag(room, path, "collapseAfterFlag");
        validateOptionalFlag(room, path, "collapseTargetEnteredFlag");

        const Json* sequenceId = find(room, "collapseSequenceId");
        if (sequenceId != nullptr) {
            if (!sequenceId->is_string()) {
                error(path, "collapseSequenceId must be a string");
            } else {
                const std::string value = sequenceId->get<std::string>();
                validateAsciiId(value, path, "collapseSequenceId");
                if (!sequenceIds.contains(value)) {
                    error(path, "collapseSequenceId points to missing sequence: " + value);
                }
            }
        }

        const Json* cue = find(room, "collapseAudioCue");
        if (cue != nullptr) {
            if (!cue->is_string()) {
                error(path, "collapseAudioCue must be a string");
            } else {
                const std::string value = cue->get<std::string>();
                validateAsciiId(value, path, "collapseAudioCue");
                if (!audioCueIds.contains(value)) {
                    error(path, "collapseAudioCue points to missing audio.cues id: " + value);
                }
            }
        }

        const Json* heartbeatCue = find(room, "collapseHeartbeatAudioCue");
        if (heartbeatCue != nullptr) {
            if (!heartbeatCue->is_string()) {
                error(path, "collapseHeartbeatAudioCue must be a string");
            } else {
                const std::string value = heartbeatCue->get<std::string>();
                validateAsciiId(value, path, "collapseHeartbeatAudioCue");
                if (!audioCueIds.contains(value)) {
                    error(path, "collapseHeartbeatAudioCue points to missing audio.cues id: " + value);
                }
            }
        }

        const Json* targetRoom = find(room, "collapseTargetRoom");
        if (targetRoom != nullptr) {
            if (!targetRoom->is_string()) {
                error(path, "collapseTargetRoom must be a string");
            } else {
                const std::string value = targetRoom->get<std::string>();
                validateAsciiId(value, path, "collapseTargetRoom");
                requireExists(root_ / "data" / "rooms" / value / "room.json", path, "collapse target room missing");
            }
        }

        const Json* targetSpawn = find(room, "collapseTargetSpawn");
        if (targetSpawn != nullptr) {
            if (!targetSpawn->is_string()) {
                error(path, "collapseTargetSpawn must be a string");
            } else {
                validateAsciiId(targetSpawn->get<std::string>(), path, "collapseTargetSpawn");
            }
        }
    }

    void validateOptionalFlag(const Json& object, const std::filesystem::path& path, std::string_view field) {
        const Json* flag = find(object, field);
        if (flag == nullptr) {
            return;
        }
        if (!flag->is_string()) {
            error(path, std::string(field) + " must be a string");
            return;
        }
        validateAsciiId(flag->get<std::string>(), path, field);
    }

    void validateCollisionBoxes(const Json& room, const std::filesystem::path& path) {
        const Json* boxes = find(room, "collisionBoxes");
        if (boxes == nullptr) {
            return;
        }
        if (!boxes->is_array()) {
            error(path, "collisionBoxes must be an array");
            return;
        }

        std::unordered_set<std::string> ids;
        for (const Json& box : *boxes) {
            if (!box.is_object()) {
                error(path, "collision box must be an object");
                continue;
            }
            const Json* id = find(box, "id");
            if (id == nullptr || !id->is_string()) {
                error(path, "collision box is missing id");
            } else {
                const std::string value = id->get<std::string>();
                validateAsciiId(value, path, "collision box id");
                if (!ids.insert(value).second) {
                    error(path, "duplicate collision box id: " + value);
                }
            }

            const Json* bounds = find(box, "bounds");
            if (bounds == nullptr || !bounds->is_object()) {
                error(path, "collision box is missing bounds");
                continue;
            }
            validateBounds(*bounds, path, "collision box bounds");
        }
    }

    void validateRenderEnvironment(const Json& room, const std::filesystem::path& path) {
        const Json* environment = find(room, "renderEnvironment");
        if (environment == nullptr) {
            return;
        }
        if (!environment->is_object()) {
            error(path, "renderEnvironment must be an object");
            return;
        }

        for (const std::string_view field : {
                 "clearColor",
                 "ambientColor",
                 "keyLightDirection",
                 "keyLightColor",
                 "fogColor",
             }) {
            const Json* value = find(*environment, field);
            if (value != nullptr && !isVec3(value)) {
                error(path, "renderEnvironment." + std::string(field) + " must be a vec3");
            }
        }

        for (const std::string_view field : {
                 "ambientIntensity",
                 "keyLightIntensity",
                 "fogStart",
                 "fogDensity",
                 "exposure",
                 "contrast",
                 "saturation",
                 "vignetteStrength",
             }) {
            const Json* value = find(*environment, field);
            if (value != nullptr && (!value->is_number() || !std::isfinite(value->get<double>()))) {
                error(path, "renderEnvironment." + std::string(field) + " must be a finite number");
            }
        }

        const Json* vignette = find(*environment, "vignetteStrength");
        if (vignette != nullptr && vignette->is_number()) {
            const double value = vignette->get<double>();
            if (value < 0.0 || value > 1.0) {
                error(path, "renderEnvironment.vignetteStrength must be between 0 and 1");
            }
        }
    }

    void validateStaticMeshRenderOverrides(const Json& mesh, const std::filesystem::path& path) {
        const Json* overrides = find(mesh, "renderOverrides");
        if (overrides == nullptr) {
            return;
        }
        if (!overrides->is_object()) {
            error(path, "static mesh renderOverrides must be an object");
            return;
        }

        for (const std::string_view field : {
                 "colorTint",
                 "emissiveColor",
             }) {
            const Json* value = find(*overrides, field);
            if (value != nullptr && !isVec3(value)) {
                error(path, "static mesh renderOverrides." + std::string(field) + " must be a vec3");
            }
        }

        const Json* emissiveIntensity = find(*overrides, "emissiveIntensity");
        if (emissiveIntensity != nullptr
            && (!emissiveIntensity->is_number() || !std::isfinite(emissiveIntensity->get<double>()))) {
            error(path, "static mesh renderOverrides.emissiveIntensity must be a finite number");
        }
    }

    void validateBounds(const Json& bounds, const std::filesystem::path& path, std::string_view label) {
        const Json* min = find(bounds, "min");
        const Json* max = find(bounds, "max");
        if (!isVec3(min) || !isVec3(max)) {
            error(path, std::string(label) + " must define min/max vec3");
            return;
        }
        for (std::size_t i = 0; i < 3; ++i) {
            if ((*min)[i].get<double>() > (*max)[i].get<double>()) {
                error(path, std::string(label) + " min must be <= max");
                return;
            }
        }
    }

    [[nodiscard]] bool isVec3(const Json* value) const {
        if (value == nullptr || !value->is_array() || value->size() != 3) {
            return false;
        }
        for (const Json& component : *value) {
            if (!component.is_number()) {
                return false;
            }
            if (!std::isfinite(component.get<double>())) {
                return false;
            }
        }
        return true;
    }

    void validateStoryGraph(const std::filesystem::path& path) {
        Json root = parseJsonFile(path);
        if (!root.is_object()) {
            return;
        }
        const Json* nodes = find(root, "nodes");
        if (nodes == nullptr || !nodes->is_array() || nodes->empty()) {
            error(path, "story nodes array is missing or empty");
            return;
        }

        std::unordered_set<std::string> ids;
        for (const Json& node : *nodes) {
            if (!node.is_object()) {
                error(path, "story node must be an object");
                continue;
            }
            const Json* id = find(node, "id");
            if (id == nullptr || !id->is_string()) {
                error(path, "story node is missing id");
                continue;
            }
            const std::string value = id->get<std::string>();
            validateAsciiId(value, path, "story node id");
            if (!ids.insert(value).second) {
                error(path, "duplicate story node id: " + value);
            }
        }

        const std::string startNode = root.value("startNode", "");
        if (startNode.empty() || !ids.contains(startNode)) {
            error(path, "startNode is missing or points to a missing node");
        }

        for (const Json& node : *nodes) {
            if (!node.is_object()) {
                continue;
            }
            const std::string from = node.value("id", "<missing>");
            const Json* choices = find(node, "choices");
            if (choices == nullptr || !choices->is_array()) {
                continue;
            }
            for (const Json& choice : *choices) {
                if (!choice.is_object()) {
                    error(path, "story choice from " + from + " must be an object");
                    continue;
                }
                const std::string nextNode = choice.value("nextNode", "");
                if (nextNode.empty() || !ids.contains(nextNode)) {
                    error(path, "story choice from " + from + " points to missing node: " + nextNode);
                }
            }
        }
    }

    [[nodiscard]] std::filesystem::path storyPath() const {
        const std::filesystem::path dataStory = root_ / "data" / "story" / "zero_patient.story.json";
        if (std::filesystem::exists(dataStory)) {
            return dataStory;
        }
        return root_ / "samples" / "zero_patient_story.json";
    }

    void validateManifestFileRef(const Json& object, const std::filesystem::path& source, std::string_view field) {
        const Json* value = find(object, field);
        if (value == nullptr || !value->is_string()) {
            error(source, "canonicalFiles." + std::string(field) + " must be a file path string");
            return;
        }
        requireExists(resolve(value->get<std::string>()), source, "canonical file missing");
    }

    void validateIds(const Json& entries, const std::filesystem::path& path) {
        std::unordered_set<std::string> ids;
        for (const Json& entry : entries) {
            if (!entry.is_object()) {
                error(path, "catalog entry must be an object");
                continue;
            }
            const Json* id = find(entry, "id");
            if (id == nullptr || !id->is_string()) {
                error(path, "catalog entry is missing id");
                continue;
            }
            const std::string value = id->get<std::string>();
            validateAsciiId(value, path, "id");
            if (!ids.insert(value).second) {
                error(path, "duplicate id: " + value);
            }
        }
    }

    void validateAsciiId(const std::string& value, const std::filesystem::path& path, std::string_view label) {
        static const std::regex idPattern("^[a-z][a-z0-9_]*$");
        if (!std::regex_match(value, idPattern)) {
            error(path, std::string(label) + " must be snake_case ASCII: " + value);
        }
    }

    [[nodiscard]] static const Json* find(const Json& object, std::string_view field) {
        if (!object.is_object()) {
            return nullptr;
        }
        const auto it = object.find(std::string(field));
        return it == object.end() ? nullptr : &(*it);
    }

    void requireObject(const Json& object, const std::filesystem::path& path, std::string_view field) {
        const Json* value = find(object, field);
        if (value == nullptr || !value->is_object()) {
            error(path, "missing object field: " + std::string(field));
        }
    }

    void requireString(const Json& object, const std::filesystem::path& path, std::string_view field) {
        const Json* value = find(object, field);
        if (value == nullptr || !value->is_string() || value->get<std::string>().empty()) {
            error(path, "missing string field: " + std::string(field));
        }
    }

    void requireNonEmptyArray(const Json& object, const std::filesystem::path& path, std::string_view field) {
        if (!hasNonEmptyArray(object, field)) {
            error(path, "missing or empty array field: " + std::string(field));
        }
    }

    [[nodiscard]] static bool hasNonEmptyArray(const Json& object, std::string_view field) {
        const Json* value = find(object, field);
        return value != nullptr && value->is_array() && !value->empty();
    }

    void requireExists(const std::filesystem::path& path, const std::filesystem::path& source, std::string_view message) {
        if (!std::filesystem::exists(path)) {
            error(source, std::string(message) + ": " + path.string());
        }
    }

    void warn(const std::filesystem::path& path, std::string message) {
        diagnostics_.push_back({"warning", path, std::move(message)});
    }

    void error(const std::filesystem::path& path, std::string message) {
        hasErrors_ = true;
        diagnostics_.push_back({"error", path, std::move(message)});
    }

    void printReport() const {
        if (diagnostics_.empty()) {
            std::cout << "Content validation passed.\n";
            return;
        }

        for (const Diagnostic& diagnostic : diagnostics_) {
            std::cout << diagnostic.severity << ": " << diagnostic.path.string() << ": " << diagnostic.message << "\n";
        }
        if (hasErrors_) {
            std::cout << "Content validation failed.\n";
        } else {
            std::cout << "Content validation passed with warnings.\n";
        }
    }

    std::filesystem::path root_;
    std::vector<Diagnostic> diagnostics_;
    bool hasErrors_ = false;
};

} // namespace

int main(int argc, char** argv) {
    std::filesystem::path root = EXO_ENGINE_ROOT;
    if (argc > 1) {
        root = argv[1];
    }

    Validator validator(root);
    return validator.run();
}
