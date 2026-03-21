#pragma once
#include <JuceHeader.h>

class MoonVerbLookAndFeel : public juce::LookAndFeel_V4
{
public:
    MoonVerbLookAndFeel()
    {
        setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xFF4080D0));
        setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xFF1A1A2E));
        setColour(juce::Slider::thumbColourId, juce::Colour(0xFFD0D8F0));
    }

    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                          juce::Slider& slider) override
    {
        auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat().reduced(4.0f);
        auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) / 2.0f;
        auto centreX = bounds.getCentreX();
        auto centreY = bounds.getCentreY();
        auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

        // Knob body
        g.setColour(juce::Colour(0xFF0A0A1A));
        g.fillEllipse(centreX - radius, centreY - radius, radius * 2.0f, radius * 2.0f);

        // Outline
        g.setColour(juce::Colour(0xFF2A2A40));
        g.drawEllipse(centreX - radius, centreY - radius, radius * 2.0f, radius * 2.0f, 1.0f);

        // Arc track (background)
        float arcRadius = radius + 4.0f;
        juce::Path trackArc;
        trackArc.addCentredArc(centreX, centreY, arcRadius, arcRadius,
                               0.0f, rotaryStartAngle, rotaryEndAngle, true);
        g.setColour(juce::Colour(0xFF1A1A2E));
        g.strokePath(trackArc, juce::PathStrokeType(3.0f));

        // Arc value (filled) with glow
        if (sliderPos > 0.0f)
        {
            juce::Path valueArc;
            valueArc.addCentredArc(centreX, centreY, arcRadius, arcRadius,
                                   0.0f, rotaryStartAngle, angle, true);
            auto arcColour = juce::Colour(0xFF4080D0);

            g.setColour(arcColour.withAlpha(0.15f));
            g.strokePath(valueArc, juce::PathStrokeType(8.0f));
            g.setColour(arcColour.withAlpha(0.3f));
            g.strokePath(valueArc, juce::PathStrokeType(5.0f));
            g.setColour(arcColour);
            g.strokePath(valueArc, juce::PathStrokeType(3.0f));
        }

        // Pointer line
        float pointerLength = radius * 0.6f;
        float pointerThickness = 2.0f;
        juce::Path pointer;
        pointer.addRectangle(-pointerThickness * 0.5f, -radius + 4.0f, pointerThickness, pointerLength);
        g.setColour(juce::Colour(0xFFD0D8F0));
        g.fillPath(pointer, juce::AffineTransform::rotation(angle).translated(centreX, centreY));

        // Value text on hover
        if (slider.isMouseOverOrDragging())
        {
            g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
            g.setColour(juce::Colour(0xFFD0D8F0));
            auto text = slider.getTextFromValue(slider.getValue());
            g.drawText(text, juce::Rectangle<float>(centreX - 30, centreY - radius - 18, 60, 14),
                       juce::Justification::centred);
        }
    }

    void drawLabel(juce::Graphics& g, juce::Label& label) override
    {
        g.setFont(juce::Font(juce::FontOptions(11.0f)));
        g.setColour(juce::Colour(0xFF6080B0));
        g.drawText(label.getText(), label.getLocalBounds(), juce::Justification::centred);
    }
};
