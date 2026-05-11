#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <ExoEngine/Animation/AnimationSystem.h>
#include <ExoEngine/Audio/AudioSystem.h>
#include <ExoEngine/Debug/DebugOverlay.h>
#include <ExoEngine/Game/CameraDirector.h>
#include <ExoEngine/Game/GameState.h>
#include <ExoEngine/Game/PlayerMotor.h>
#include <ExoEngine/Game/RoomManager.h>
#include <ExoEngine/Platform/Window.h>
#include <ExoEngine/Renderer/Renderer.h>
#include <ExoEngine/Scene/Scene.h>
#include <ExoEngine/Story/StoryRuntime.h>

namespace Exo {

struct ApplicationConfig {
    std::string name = "ExoEngine";
    std::uint32_t width = 1280;
    std::uint32_t height = 720;
    std::uint32_t maxFrames = 0;
    bool headless = false;
    bool showMenu = false;
    std::string startupOverlay = "main_menu";
    bool cycleWorkstationOverlay = false;
    std::string startupRoomId;
    std::string startupSpawnId;
    std::vector<std::string> startupFlags;
    std::string inspectEntityId;
};

class Application {
public:
    explicit Application(ApplicationConfig config = {});

    int run();

private:
    enum class WorkstationSequenceState {
        None,
        Entering,
        AtWorkstation,
        Exiting,
    };

    enum class CollapseSequenceState {
        None,
        ArmedAfterWorkstation,
        Dizzy,
        Falling,
        Blackout,
    };

    enum class CameraMode {
        FreeFirstPerson,
        SeatedComputer,
        CollapseCutscene,
        LyingLimitedLook,
    };

    int runHeadless();
    int runWindowed();
    [[nodiscard]] bool loadStartupScene(bool required);
    [[nodiscard]] bool loadStartupStory(bool required);
    [[nodiscard]] bool loadRoomScene(const std::string& roomId, const std::string& spawnId, bool required);
    [[nodiscard]] RenderView makeCurrentView() const;
    void updatePlayer(float deltaSeconds, float mouseDeltaX, float mouseDeltaY);
    void updateLyingLimitedLook(float deltaSeconds, float mouseDeltaX, float mouseDeltaY);
    void reloadSceneMeshes();
    void activateCurrentFocus();
    void beginWorkstationEntrySequence();
    void beginWorkstationExitSequence();
    [[nodiscard]] bool updateWorkstationSequence(float deltaSeconds);
    [[nodiscard]] bool workstationSequenceBlocksPlayer() const;
    void updateSequenceRuntime(float deltaSeconds);
    void executeSequenceAction(const SequenceAction& action);
    [[nodiscard]] bool sequenceBlocksPlayer() const;
    [[nodiscard]] ScreenOverlay currentScreenOverlay() const;
    void armCollapseAfterWorkstation();
    void skipToCollapseShortcut();
    void beginCollapseDizzy();
    void setCollapseCameraStage(CollapseSequenceState stage);
    void resetCollapseRuntime();
    void enterLyingLimitedLook(const std::string& anchorId, bool inputEnabled);
    [[nodiscard]] bool lyingLimitedLookActive() const;
    [[nodiscard]] Vec3 roomAnchorPosition(const std::string& anchorId, Vec3 fallback) const;
    void updateCollapseSequence(float deltaSeconds);
    void completeCollapseTransition();
    [[nodiscard]] bool collapseSequenceBlocksPlayer() const;
    [[nodiscard]] float collapseControlMultiplier() const;
    [[nodiscard]] ScreenOverlay collapseScreenOverlay() const;
    [[nodiscard]] Transform animatedStaticMeshTransform(const StaticMeshInstance& instance) const;
    [[nodiscard]] Transform applyCharacterPerformance(const std::string& entityId, Transform base) const;
    [[nodiscard]] const StaticMeshInstance* findStaticMeshInstance(const std::string& entityId) const;
    [[nodiscard]] Transform currentEntityTransform(const StaticMeshInstance& instance) const;
    [[nodiscard]] Transform actionTargetTransform(const SequenceAction& action, Transform current) const;
    void setEntityTransformOverride(const SequenceAction& action);
    void animateEntityTransformOverride(const SequenceAction& action);
    void clearEntityTransformOverride(const SequenceAction& action);
    void startCharacterPerformance(const SequenceAction& action);
    void stopCharacterPerformance(const SequenceAction& action);
    void updateCharacterPerformances(float deltaSeconds);
    void evaluateCurrentTrigger();
    void processRoomEnterEvents();
    void playAudioCue(const std::string& cueId, bool loop = false);
    void playRoomMusic();
    [[nodiscard]] bool isStaticMeshVisible(const StaticMeshInstance& instance) const;
    void syncStoryToGameState();
    void syncGameStateToStory();
    [[nodiscard]] std::filesystem::path engineRoot() const;
    [[nodiscard]] std::filesystem::path dataRoot() const;
    [[nodiscard]] std::filesystem::path savePath() const;

    ApplicationConfig config_;
    Scene scene_;
    Window window_;
    Renderer renderer_;
    AudioSystem audioSystem_;
    DebugOverlay debugOverlay_;
    StoryRuntime story_;
    RoomManager roomManager_;
    SequenceManager sequenceManager_;
    PlayerMotor playerMotor_;
    CameraDirector cameraDirector_;
    AnimationSystem animationSystem_;
    GameState gameState_;

    struct ActiveCharacterPerformance {
        std::string cueId;
        float duration = 0.0f;
        float elapsed = 0.0f;
        float intensity = 1.0f;
    };

    float playerPitch_ = 0.0f;
    float visualTime_ = 0.0f;
    float walkCameraPhase_ = 0.0f;
    float walkCameraAmount_ = 0.0f;
    std::string currentFocusPrompt_;
    std::string currentFocusInteractionId_;
    std::string pendingUiOverlay_;
    std::vector<std::int32_t> sceneMeshHandles_;
    WorkstationSequenceState workstationSequenceState_ = WorkstationSequenceState::None;
    float workstationSequenceTimer_ = 0.0f;
    Vec3 workstationStandPosition_ {};
    Vec3 workstationSeatedPosition_ {-1.24f, 1.28f, -0.78f};
    float workstationStandYaw_ = 0.0f;
    float workstationSeatedYaw_ = 0.0f;
    float workstationStandPitch_ = 0.0f;
    float workstationSeatedPitch_ = -0.12f;
    CameraMode cameraMode_ = CameraMode::FreeFirstPerson;
    CollapseSequenceState collapseSequenceState_ = CollapseSequenceState::None;
    float collapseSequenceTimer_ = 0.0f;
    float collapseControlMultiplier_ = 1.0f;
    float collapseBlackFade_ = 0.0f;
    float collapseNoiseIntensity_ = 0.0f;
    float collapseCameraRoll_ = 0.0f;
    float collapseStoredMusicVolume_ = 1.0f;
    Vec3 collapseArmPosition_ {};
    Vec3 collapseStartPosition_ {};
    float collapseStartYaw_ = 0.0f;
    float collapseStartPitch_ = 0.0f;
    bool collapseDrivenBySequence_ = false;
    Vec3 lyingAnchorPosition_ {};
    float lyingBaseYaw_ = 0.0f;
    float lyingBasePitch_ = 0.0f;
    float lyingYawOffset_ = 0.0f;
    float lyingPitchOffset_ = 0.0f;
    float lyingYawTarget_ = 0.0f;
    float lyingPitchTarget_ = 0.0f;
    float lyingLookTimer_ = 0.0f;
    bool lyingLookInputEnabled_ = false;
    bool sequencePlayerControlLocked_ = false;
    ScreenOverlay sequenceOverlay_ {};
    float sequenceFadeStart_ = 0.0f;
    float sequenceFadeTarget_ = 0.0f;
    float sequenceFadeDuration_ = 0.0f;
    float sequenceFadeTimer_ = 0.0f;
    std::unordered_set<std::string> sequenceHiddenEntities_;
    std::unordered_map<std::string, ActiveCharacterPerformance> characterPerformances_;
};

} // namespace Exo
