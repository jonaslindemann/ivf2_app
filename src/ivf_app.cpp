/**
 * @file ivf_app.cpp
 * @brief Timeline-driven flow-field particle example.
 *
 * Controls:
 *   Space  - pause / resume global time and audio
 *   ESC    - quit
 */

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "audio_player.h"
#include "particle_scene.h"

#include <ivf/gl.h>
#include <ivf/nodes.h>
#include <ivf/scene_timeline.h>
#include <ivfui/scene_timeline_player.h>
#include <ivfui/time_control_panel.h>
#include <ivfui/ui.h>

#include <ivf/tint_effect.h>
#include <ivf/filmgrain_effect.h>
#include <ivf/chromatic_effect.h>
#include <ivf/vignette_effect.h>
#include <ivf/bloom_effect.h>
#include <ivf/dithering_effect.h>
#include <ivf/pixelation_effect.h>
#include <ivf/wave_distortion_effect.h>
#include <ivf/swirl_effect.h>
#include <ivf/kaleidoscope_effect.h>
#include <ivf/glitch_effect.h>
#include <ivf/scanline_effect.h>
#include <ivf/posterize_effect.h>
#include <ivf/color_grading_effect.h>
#include <ivf/night_vision_effect.h>
#include <ivf/blur_effect.h>

#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

using namespace ivf;
using namespace ivfui;

class ExampleWindow : public GLFWSceneWindow {
private:
    SceneTimelinePtr m_timeline;
    SceneTimelinePlayerPtr m_timelinePlayer;
    std::shared_ptr<ParticleTimelineScene> m_particleScene;
    AudioPlayer m_audioPlayer;
    std::filesystem::path m_settingsPath{"scene_settings.json"};

public:
    ExampleWindow(int w, int h, std::string title) : GLFWSceneWindow(w, h, title) {}

    static std::shared_ptr<ExampleWindow> create(int w, int h, std::string title)
    {
        return std::make_shared<ExampleWindow>(w, h, title);
    }

    int onSetup() override
    {
        setClearColor(glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
        enableHeadlight();
        this->setRenderToTexture(true);

        // Create and configure post-processing effects

        // Blur effect

        auto blurEffect = BlurEffect::create();
        blurEffect->setBlurRadius(2.0);
        blurEffect->load();

        // Tint effect

        auto tintEffect = TintEffect::create();

        tintEffect->setTintColor(glm::vec3(1.2, 0.9, 0.7));
        tintEffect->setTintStrength(0.5);
        tintEffect->setGrayScaleWeights(glm::vec3(0.299, 0.587, 0.114));
        tintEffect->load();

        // Film grain effect

        auto filmgrainEffect = FilmgrainEffect::create();
        filmgrainEffect->setNoiseIntensity(0.5);
        filmgrainEffect->setGrainBlending(0.5);
        filmgrainEffect->load();

        // Chromatic aberration effect

        auto chromaticEffect = ChromaticEffect::create();
        chromaticEffect->setOffset(0.01);
        chromaticEffect->load();

        // Vignette effect

        auto vignetteEffect = VignetteEffect::create();
        vignetteEffect->setSize(1.0);
        vignetteEffect->setSmoothness(0.7);
        vignetteEffect->load();

        // Bloom effect

        auto bloomEffect = BloomEffect::create();
        bloomEffect->setThreshold(1.0);
        bloomEffect->setIntensity(1.0);
        bloomEffect->load();

        // Dithering effect

        auto ditheringEffect = DitheringEffect::create();
        ditheringEffect->load();

        // Pixelation effect

        auto pixelationEffect = PixelationEffect::create();
        pixelationEffect->setPixelSize(4.0);
        pixelationEffect->load();

        // Wave distortion effect

        auto waveDistortionEffect = WaveDistortionEffect::create();
        waveDistortionEffect->setFrequency(10.0f);
        waveDistortionEffect->setAmplitude(0.01f);
        waveDistortionEffect->setSpeed(1.0f);
        waveDistortionEffect->load();

        // Swirl effect

        auto swirlEffect = SwirlEffect::create();
        swirlEffect->setRadius(0.5f);
        swirlEffect->setAngle(3.14159f);
        swirlEffect->load();

        // Kaleidoscope effect

        auto kaleidoscopeEffect = KaleidoscopeEffect::create();
        kaleidoscopeEffect->setSegments(6.0f);
        kaleidoscopeEffect->setRotation(0.0f);
        kaleidoscopeEffect->load();

        // Glitch effect

        auto glitchEffect = GlitchEffect::create();
        glitchEffect->setIntensity(0.05f);
        glitchEffect->setBlockSize(0.05f);
        glitchEffect->setSpeed(4.0f);
        glitchEffect->load();

        // Scanline effect

        auto scanlineEffect = ScanlineEffect::create();
        scanlineEffect->setLineSpacing(4.0f);
        scanlineEffect->setLineIntensity(0.4f);
        scanlineEffect->setScrollSpeed(0.0f);
        scanlineEffect->load();

        // Posterize effect

        auto posterizeEffect = PosterizeEffect::create();
        posterizeEffect->setLevels(4.0f);
        posterizeEffect->load();

        // Color grading effect

        auto colorGradingEffect = ColorGradingEffect::create();
        colorGradingEffect->setShadows(glm::vec3(0.6f, 0.5f, 0.5f));
        colorGradingEffect->setMidtones(glm::vec3(0.5f, 0.5f, 0.5f));
        colorGradingEffect->setHighlights(glm::vec3(0.5f, 0.5f, 0.6f));
        colorGradingEffect->setContrast(1.1f);
        colorGradingEffect->setSaturation(1.2f);
        colorGradingEffect->load();

        // Night vision effect

        auto nightVisionEffect = NightVisionEffect::create();
        nightVisionEffect->setNoiseIntensity(0.05f);
        nightVisionEffect->setGlowStrength(0.3f);
        nightVisionEffect->setPhosphorColor(glm::vec3(0.1f, 1.0f, 0.1f));
        nightVisionEffect->load();

		m_particleScene = ParticleTimelineScene::create();

		// Add effects to the timeline scene (SceneTimelinePlayer will apply scene effects)

		m_particleScene->effect(blurEffect);            //  0
		m_particleScene->effect(tintEffect);            //  1
		m_particleScene->effect(chromaticEffect);       //  2
		m_particleScene->effect(ditheringEffect);       //  3
		m_particleScene->effect(bloomEffect);           //  4
		m_particleScene->effect(pixelationEffect);      //  5
		m_particleScene->effect(vignetteEffect);        //  6
		m_particleScene->effect(filmgrainEffect);       //  7
		m_particleScene->effect(waveDistortionEffect);  //  8
		m_particleScene->effect(swirlEffect);           //  9
		m_particleScene->effect(kaleidoscopeEffect);    // 10
		m_particleScene->effect(glitchEffect);          // 11
		m_particleScene->effect(scanlineEffect);        // 12
		m_particleScene->effect(posterizeEffect);       // 13
		m_particleScene->effect(colorGradingEffect);    // 14
		m_particleScene->effect(nightVisionEffect);     // 15

		// Disable all effects by default

		for (int i = 0; i < static_cast<int>(m_particleScene->effects().size()); ++i)
			m_particleScene->effects()[static_cast<size_t>(i)]->program()->setEnabled(false);

        m_timeline = SceneTimeline::create();
        m_timeline->setLoop(true);
        m_timeline->addScene(m_particleScene->name(), m_particleScene->duration()) = *m_particleScene;
        m_timeline->play();

        m_timelinePlayer = SceneTimelinePlayer::create(this, m_timeline);
        m_timelinePlayer->update(0.0);

        loadSceneSettings();
        m_audioPlayer.load(findAsset("assets/slow_beats.wav"));

        addUiWindow(TimeControlPanel::create());

        TimeController::instance()->setScale(1.0f);

        this->showEffectInspector();

        return 0;
    }

    void onUpdate() override
    {
        const double dt = TimeController::instance()->delta();

        if (m_particleScene)
            m_particleScene->setAudioInput(m_audioPlayer.energy(), m_audioPlayer.isPlaying());

        if (m_timelinePlayer)
            m_timelinePlayer->update(dt);

        if (m_particleScene)
            applyCamera(m_particleScene->camera());
    }

    void onKey(int key, int /*sc*/, int action, int /*mods*/) override
    {
        if (action == GLFW_PRESS && key == GLFW_KEY_SPACE) {
            TimeController::instance()->togglePause();
            m_audioPlayer.setPaused(TimeController::instance()->isPaused());
        }
    }

    void onDrawUi() override
    {
        if (m_particleScene)
            m_particleScene->drawControls();

        m_audioPlayer.drawControls();
    }

	void onClose() override
	{
		saveSceneSettings();
		if (m_audioPlayer.isPlaying())
			m_audioPlayer.setPaused(true);
	}

private:
    void applyCamera(const TimelineCamera& camera)
    {
        auto manipulator = cameraManipulator();
        if (!manipulator)
            return;

        manipulator->setCameraPosition(camera.position);
        manipulator->setCameraTarget(camera.target);
        manipulator->setFov(camera.fov);
        manipulator->setNearZ(camera.nearZ);
        manipulator->setFarZ(camera.farZ);
    }

    void loadSceneSettings()
    {
        if (!m_particleScene)
            return;

        std::ifstream in(m_settingsPath);
        if (!in)
            return;

        nlohmann::json json;
        in >> json;

        if (const auto it = json.find(m_particleScene->name()); it != json.end())
            m_particleScene->fromJson(*it);
    }

    void saveSceneSettings()
    {
        if (!m_particleScene)
            return;

        nlohmann::json json;

        std::ifstream in(m_settingsPath);
        if (in)
            in >> json;

        if (!json.is_object())
            json = nlohmann::json::object();

        json[m_particleScene->name()] = m_particleScene->toJson();

        std::ofstream out(m_settingsPath);
        if (out)
            out << json.dump(4);
    }

    std::filesystem::path findAsset(const std::filesystem::path& relativePath) const
    {
        const std::filesystem::path candidates[] = {
            relativePath,
            std::filesystem::path("bin/Release") / relativePath,
            std::filesystem::path("bin/Debug") / relativePath,
        };

        for (const auto& candidate : candidates) {
            if (std::filesystem::exists(candidate))
                return candidate;
        }

        return relativePath;
    }
};

int main()
{
    auto app = GLFWApplication::create();
    app->hint(GLFW_SAMPLES, 4);

    auto win = ExampleWindow::create(1280, 720, "Flow Field - timeline particles");
    app->addWindow(win);

    return app->loop();
}
