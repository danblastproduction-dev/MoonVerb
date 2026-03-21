#include "PluginEditor.h"

MoonVerbEditor::MoonVerbEditor(MoonVerbProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    setSize(700, 500);
    setLookAndFeel(&moonLnf);

    // Knobs — row 1
    setupKnob(mixKnob, mixLabel, "Mix");
    setupKnob(decayKnob, decayLabel, "Decay");
    setupKnob(dampKnob, dampLabel, "Damp");
    setupKnob(widthKnob, widthLabel, "Width");
    setupKnob(shimmerKnob, shimmerLabel, "Shimmer");
    setupKnob(preDelayKnob, preDelayLabel, "Pre-Dly");

    // Knobs — row 2
    setupKnob(modRateKnob, modRateLabel, "Mod Rate");
    setupKnob(modDepthKnob, modDepthLabel, "Mod Depth");

    // APVTS attachments
    mixAtt      = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.apvts, "mix", mixKnob);
    decayAtt    = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.apvts, "decay", decayKnob);
    dampAtt     = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.apvts, "damping", dampKnob);
    widthAtt    = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.apvts, "width", widthKnob);
    shimmerAtt  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.apvts, "shimmer", shimmerKnob);
    preDelayAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.apvts, "preDelay", preDelayKnob);
    modRateAtt  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.apvts, "modRate", modRateKnob);
    modDepthAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.apvts, "modDepth", modDepthKnob);

    // Freeze button
    freezeBtn.setClickingTogglesState(true);
    freezeBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF0A0A1A));
    freezeBtn.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xFF1A3A50));
    freezeBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xFF506080));
    freezeBtn.setColour(juce::TextButton::textColourOnId, juce::Colour(0xFF60C0FF));
    addAndMakeVisible(freezeBtn);
    freezeAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.apvts, "freeze", freezeBtn);

    // Preset navigation
    for (int i = 0; i < p.getNumPrograms(); ++i)
        presetBox.addItem(p.getProgramName(i), i + 1);
    presetBox.setSelectedId(p.getCurrentProgram() + 1, juce::dontSendNotification);
    presetBox.onChange = [this] {
        processor.setCurrentProgram(presetBox.getSelectedId() - 1);
    };
    presetBox.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xFF0A0A1A));
    presetBox.setColour(juce::ComboBox::textColourId, juce::Colour(0xFFD0D8F0));
    presetBox.setColour(juce::ComboBox::outlineColourId, juce::Colour(0xFF2A2A40));
    addAndMakeVisible(presetBox);

    prevBtn.onClick = [this] {
        int idx = juce::jmax(0, processor.getCurrentProgram() - 1);
        processor.setCurrentProgram(idx);
        presetBox.setSelectedId(idx + 1, juce::dontSendNotification);
    };
    nextBtn.onClick = [this] {
        int idx = juce::jmin(processor.getNumPrograms() - 1, processor.getCurrentProgram() + 1);
        processor.setCurrentProgram(idx);
        presetBox.setSelectedId(idx + 1, juce::dontSendNotification);
    };
    for (auto* btn : { &prevBtn, &nextBtn })
    {
        btn->setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF0A0A1A));
        btn->setColour(juce::TextButton::textColourOffId, juce::Colour(0xFF6080B0));
        addAndMakeVisible(btn);
    }

    // Footer link
    discoverBtn.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    discoverBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xFF4080D0));
    discoverBtn.onClick = [] {
        juce::URL("https://deadloop.fr/moonpad").launchInDefaultBrowser();
    };
    addAndMakeVisible(discoverBtn);

    // Initialize starfield
    juce::Random rng;
    for (int i = 0; i < 80; ++i)
        stars.push_back({ rng.nextFloat() * 700.0f, rng.nextFloat() * 500.0f,
                          0.2f + rng.nextFloat() * 0.8f, 0.05f + rng.nextFloat() * 0.15f });

    startTimerHz(60);
}

MoonVerbEditor::~MoonVerbEditor()
{
    setLookAndFeel(nullptr);
}

void MoonVerbEditor::setupKnob(juce::Slider& knob, juce::Label& label, const juce::String& text)
{
    knob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    knob.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    addAndMakeVisible(knob);

    label.setText(text, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(label);
}

void MoonVerbEditor::resized()
{
    auto area = getLocalBounds();

    // Header (40px)
    auto header = area.removeFromTop(40);
    prevBtn.setBounds(header.getRight() - 260, 8, 24, 24);
    presetBox.setBounds(header.getRight() - 230, 8, 180, 24);
    nextBtn.setBounds(header.getRight() - 44, 8, 24, 24);

    // Footer (24px)
    auto footer = area.removeFromBottom(24);
    discoverBtn.setBounds(footer.getRight() - 180, footer.getY(), 170, 24);

    // Knobs row 2 (80px from bottom)
    auto row2 = area.removeFromBottom(80);
    int r2x = (getWidth() - 340) / 2;
    modRateKnob.setBounds(r2x, row2.getY() + 2, 60, 60);
    modRateLabel.setBounds(r2x, row2.getY() + 58, 60, 16);
    modDepthKnob.setBounds(r2x + 80, row2.getY() + 2, 60, 60);
    modDepthLabel.setBounds(r2x + 80, row2.getY() + 58, 60, 16);
    freezeBtn.setBounds(r2x + 200, row2.getY() + 15, 120, 36);

    // Knobs row 1 (80px)
    auto row1 = area.removeFromBottom(80);
    int knobW = 70;
    int totalW = knobW * 6 + 20;
    int startX = (getWidth() - totalW) / 2;
    juce::Slider* knobs[] = { &mixKnob, &decayKnob, &dampKnob, &widthKnob, &shimmerKnob, &preDelayKnob };
    juce::Label* labels[] = { &mixLabel, &decayLabel, &dampLabel, &widthLabel, &shimmerLabel, &preDelayLabel };
    for (int i = 0; i < 6; ++i)
    {
        int kx = startX + i * (knobW + 4);
        knobs[i]->setBounds(kx + 5, row1.getY() + 2, 60, 60);
        labels[i]->setBounds(kx, row1.getY() + 58, knobW, 16);
    }

    // Visual area = remaining space
}

void MoonVerbEditor::timerCallback()
{
    updateAnimation();
    repaint();
}

void MoonVerbEditor::paint(juce::Graphics& g)
{
    // Background
    g.fillAll(juce::Colour(0xFF060610));

    // Visual area
    auto visArea = juce::Rectangle<float>(0.0f, 40.0f, 700.0f, getHeight() - 40.0f - 160.0f - 24.0f);
    float cx = visArea.getCentreX();
    float cy = visArea.getCentreY();

    paintStarfield(g, visArea);
    paintRings(g, cx, cy, 140.0f);
    paintParticles(g, cx, cy);
    paintMoon(g, cx, cy, 60.0f, smoothEnergy);

    // Header
    g.setFont(juce::Font(juce::FontOptions(22.0f, juce::Font::bold)));
    g.setColour(juce::Colour(0xFFD0D8F0));
    g.drawText("M O O N V E R B", 20, 8, 250, 24, juce::Justification::centredLeft);

    // Footer left
    g.setFont(juce::Font(juce::FontOptions(11.0f)));
    g.setColour(juce::Colour(0xFF405070));
    g.drawText("deadloop.fr", 20, getHeight() - 24, 120, 24, juce::Justification::centredLeft);
}

void MoonVerbEditor::paintStarfield(juce::Graphics& g, juce::Rectangle<float> area)
{
    for (auto& star : stars)
    {
        float alpha = star.brightness * (0.5f + 0.5f * std::sin(animTime * star.speed * 2.0f));
        g.setColour(juce::Colour(0xFFD0D8F0).withAlpha(alpha * 0.6f));
        float size = 1.0f + star.brightness * 1.5f;
        g.fillEllipse(star.x, area.getY() + star.y, size, size);
    }
}

void MoonVerbEditor::generateMoonTexture(int size)
{
    cachedMoonSize = size;
    moonTexture = juce::Image(juce::Image::ARGB, size, size, true);
    float half = size * 0.5f;

    // Pre-compute craters
    struct Crater { float cu, cv, cr; };
    std::vector<Crater> craters;
    juce::Random crng(123);
    for (int i = 0; i < 25; ++i)
        craters.push_back({ crng.nextFloat() * 6.0f - 0.5f,
                            crng.nextFloat() * 3.14159f,
                            0.02f + crng.nextFloat() * 0.06f });

    float lx = -0.5f, ly = -0.5f, lz = 0.7f;
    float ll = std::sqrt(lx * lx + ly * ly + lz * lz);
    lx /= ll; ly /= ll; lz /= ll;

    for (int py = 0; py < size; ++py)
    {
        for (int px = 0; px < size; ++px)
        {
            float dx = (px - half) / half;
            float dy = (py - half) / half;
            float d2 = dx * dx + dy * dy;
            if (d2 > 1.0f) continue;

            float snz = std::sqrt(1.0f - d2);
            float snx = dx, sny = dy;

            // Spherical UV
            float u = std::atan2(snx, snz) * 1.5f;
            float v = std::asin(juce::jlimit(-1.0f, 1.0f, sny));

            // Terrain noise
            float terrain = fbm(u * 3.0f + 7.0f, v * 3.0f + 3.0f, 4);
            // Maria (dark regions)
            float maria = fbm(u * 1.2f + 50.0f, v * 1.2f + 50.0f, 3);
            maria = juce::jlimit(0.0f, 1.0f, maria * 1.2f - 0.1f);
            // Fine detail
            float detail = fbm(u * 12.0f + 100.0f, v * 12.0f + 100.0f, 3) * 0.06f;

            // Crater displacement
            float craterVal = 0.0f;
            for (auto& c : craters)
            {
                float du = u - c.cu, dv = v - c.cv;
                float cd = std::sqrt(du * du + dv * dv);
                if (cd < c.cr * 2.0f)
                {
                    float t = cd / c.cr;
                    if (t < 0.8f)
                        craterVal -= (0.8f - t) * 0.15f; // floor
                    else if (t < 1.2f)
                        craterVal += (1.0f - std::abs(t - 1.0f) / 0.2f) * 0.08f; // rim
                }
            }

            float height = terrain + detail + craterVal;

            // Bump-mapped normal
            float eps = 0.01f;
            float hR = fbm((u + eps) * 3.0f + 7.0f, v * 3.0f + 3.0f, 4) +
                        fbm((u + eps) * 12.0f + 100.0f, v * 12.0f + 100.0f, 3) * 0.06f;
            float hU = fbm(u * 3.0f + 7.0f, (v + eps) * 3.0f + 3.0f, 4) +
                        fbm(u * 12.0f + 100.0f, (v + eps) * 12.0f + 100.0f, 3) * 0.06f;
            float bx = (hR - terrain - detail) / eps * 0.12f;
            float by = (hU - terrain - detail) / eps * 0.12f;
            float nnx = snx - bx;
            float nny = sny - by;
            float nnz = snz;
            float nl = std::sqrt(nnx * nnx + nny * nny + nnz * nnz);
            nnx /= nl; nny /= nl; nnz /= nl;

            // Diffuse lighting
            float diff = juce::jmax(0.0f, nnx * lx + nny * ly + nnz * lz);
            diff = 0.15f + diff * 0.85f;

            // Base colour: blend highlands/maria
            float baseGrey = 0.80f - maria * 0.20f + height * 0.08f;
            baseGrey = juce::jlimit(0.15f, 0.95f, baseGrey);

            float surface = baseGrey * diff;

            // Limb darkening
            float limbFade = std::pow(snz, 0.3f);
            surface *= 0.65f + 0.35f * limbFade;

            surface = juce::jlimit(0.0f, 1.0f, surface);

            uint8_t lumR = static_cast<uint8_t>(surface * 255);
            uint8_t lumG = static_cast<uint8_t>(surface * 258);
            uint8_t lumB = static_cast<uint8_t>(juce::jmin(255.0f, surface * 268.0f));

            // Anti-aliased edge
            float edge = std::sqrt(d2);
            float aa = juce::jlimit(0.0f, 1.0f, (1.0f - edge) * half);
            uint8_t alpha = static_cast<uint8_t>(aa * 255.0f);

            moonTexture.setPixelAt(px, py, juce::Colour(lumR, lumG, lumB, alpha));
        }
    }
}

void MoonVerbEditor::paintMoon(juce::Graphics& g, float cx, float cy, float radius, float energy)
{
    bool isFrozen = processor.apvts.getRawParameterValue("freeze")->load() > 0.5f;

    // Moon glow (outer)
    float glowIntensity = 0.15f + energy * 0.3f;
    if (isFrozen) glowIntensity += 0.2f;
    for (float r = radius * 3.0f; r > radius; r -= 2.0f)
    {
        float a = glowIntensity * (1.0f - (r - radius) / (radius * 2.0f));
        auto glowColour = isFrozen ? juce::Colour(0xFF60C0FF) : juce::Colour(0xFF4080D0);
        g.setColour(glowColour.withAlpha(a * 0.12f));
        g.fillEllipse(cx - r, cy - r, r * 2.0f, r * 2.0f);
    }

    // Generate texture if needed
    int texSize = static_cast<int>(radius * 2.0f);
    if (texSize < 64) texSize = 64;
    if (texSize != cachedMoonSize)
        generateMoonTexture(texSize);

    // Draw the pre-rendered moon texture
    float brightBoost = 0.8f + energy * 0.3f;
    g.setOpacity(juce::jlimit(0.6f, 1.1f, brightBoost));
    g.drawImage(moonTexture,
                static_cast<int>(cx - radius), static_cast<int>(cy - radius),
                static_cast<int>(radius * 2.0f), static_cast<int>(radius * 2.0f),
                0, 0, moonTexture.getWidth(), moonTexture.getHeight());
    g.setOpacity(1.0f);

    // Pulse effect
    float pulse = 1.0f + energy * 0.05f;
    if (pulse > 1.01f)
    {
        g.setColour(juce::Colour(0xFFFFFFFF).withAlpha(energy * 0.1f));
        g.drawEllipse(cx - radius * pulse, cy - radius * pulse,
                      radius * 2.0f * pulse, radius * 2.0f * pulse, 1.0f);
    }
}

void MoonVerbEditor::paintRings(juce::Graphics& g, float cx, float cy, float maxRadius)
{
    float decay = processor.apvts.getRawParameterValue("decay")->load();
    bool isFrozen = processor.apvts.getRawParameterValue("freeze")->load() > 0.5f;
    float modDepth = processor.apvts.getRawParameterValue("modDepth")->load();

    for (auto& ring : rings)
    {
        if (ring.alpha <= 0.01f) continue;

        float wobble = std::sin(animTime * 2.0f + ring.radius * 0.05f) * modDepth * 3.0f;
        float r = ring.radius + wobble;

        auto ringColour = isFrozen ? juce::Colour(0xFF60C0FF) : juce::Colour(0xFF4080D0);

        // Outer diffuse glow
        g.setColour(ringColour.withAlpha(ring.alpha * 0.03f));
        g.drawEllipse(cx - r - 6, cy - r - 6, (r + 6) * 2.0f, (r + 6) * 2.0f, 12.0f);
        // Glow
        g.setColour(ringColour.withAlpha(ring.alpha * 0.05f));
        g.drawEllipse(cx - r - 4, cy - r - 4, (r + 4) * 2.0f, (r + 4) * 2.0f, 8.0f);
        // Ring
        g.setColour(ringColour.withAlpha(ring.alpha * 0.4f));
        g.drawEllipse(cx - r, cy - r, r * 2.0f, r * 2.0f, 1.0f);
    }
}

void MoonVerbEditor::paintParticles(juce::Graphics& g, float cx, float cy)
{
    float shimmer = processor.apvts.getRawParameterValue("shimmer")->load();
    if (shimmer < 0.01f && particles.empty()) return;

    for (auto& p : particles)
    {
        if (p.alpha <= 0.0f) continue;
        auto colour = juce::Colour(0xFFFFD080).withAlpha(p.alpha * 0.7f);
        g.setColour(colour);
        float size = 1.0f + p.alpha * 1.5f;
        g.fillEllipse(cx + p.x - size * 0.5f, cy + p.y - size * 0.5f, size, size);
        // Small glow
        g.setColour(colour.withAlpha(p.alpha * 0.1f));
        g.fillEllipse(cx + p.x - size, cy + p.y - size, size * 2.0f, size * 2.0f);
    }
}

void MoonVerbEditor::updateAnimation()
{
    animTime += 1.0f / 60.0f;

    // Smooth energy from DSP
    float targetEnergy = processor.getReverbEnergy();
    smoothEnergy += (targetEnergy - smoothEnergy) * 0.15f;

    float decay = processor.apvts.getRawParameterValue("decay")->load();
    bool isFrozen = processor.apvts.getRawParameterValue("freeze")->load() > 0.5f;
    float shimmer = processor.apvts.getRawParameterValue("shimmer")->load();

    // Spawn rings based on energy
    if (smoothEnergy > 0.01f || isFrozen)
    {
        // ringCounter is a member variable (not static — avoids sharing across instances)
        if (++ringCounter > (isFrozen ? 90 : 60))
        {
            rings.push_back({ 65.0f, 0.2f + decay * 0.25f, 15.0f + (1.0f - decay) * 30.0f });
            ringCounter = 0;
        }
    }

    // Update rings
    for (auto& ring : rings)
    {
        if (!isFrozen)
        {
            ring.radius += ring.speed * (1.0f / 60.0f) * 3.0f;
            ring.alpha -= (1.0f / 60.0f) * (1.0f - decay * 0.8f) * 0.04f;
        }
        else
        {
            ring.alpha -= 0.0005f; // Very slow fade when frozen
        }
    }
    rings.erase(std::remove_if(rings.begin(), rings.end(),
        [](const Ring& r) { return r.alpha <= 0.0f || r.radius > 300.0f; }), rings.end());

    // Spawn shimmer particles
    if (shimmer > 0.05f)
    {
        juce::Random rng;
        if (rng.nextFloat() < shimmer * 0.15f)
        {
            float angle = rng.nextFloat() * juce::MathConstants<float>::twoPi;
            float dist = 30.0f + rng.nextFloat() * 30.0f;
            particles.push_back({
                std::cos(angle) * dist,
                std::sin(angle) * dist,
                (rng.nextFloat() - 0.5f) * 0.3f,
                -0.2f - rng.nextFloat() * 0.4f,  // drift upward
                0.8f + rng.nextFloat() * 0.2f,
                3.0f + rng.nextFloat() * 3.0f
            });
        }
    }

    // Update particles
    bool frozen = isFrozen;
    for (auto& p : particles)
    {
        if (!frozen)
        {
            p.x += p.vx;
            p.y += p.vy;
        }
        p.life -= 1.0f / 60.0f;
        p.alpha = juce::jmax(0.0f, p.life / 4.0f);
    }
    particles.erase(std::remove_if(particles.begin(), particles.end(),
        [](const Particle& p) { return p.life <= 0.0f; }), particles.end());
}
