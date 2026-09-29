/*
 * xournalai (based on Xournal++)
 *
 * Stylus-like pressure model for strokes created by agents
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <optional>  // for optional
#include <string>    // for string
#include <vector>    // for vector

#include "model/Point.h"  // for Point

namespace xoj::api {

/// The user's pressure settings (Settings → Input: minimum pressure, multiplier, pressure sensitivity)
struct PressureSettings {
    double minimumPressure = 0.05;
    double multiplier = 1.0;
    bool enabled = true;
};

/**
 * @brief Width at one point for a normalized pressure, exactly like a hardware stylus event:
 * max(minimumPressure, p * multiplier) * toolWidth  (see PenInputHandler::filterPressure and StrokeHandler).
 */
double hardwareWidth(double pressure, double toolWidth, const PressureSettings& settings);

/// A generator of per-point pressure for strokes that don't carry explicit pressure
struct PressureProfile {
    std::string preset = "ink";       ///< constant | ink | brush | pencil | calligraphy | marker
    std::optional<double> base;       ///< main pressure level 0..1 (preset default otherwise)
    std::optional<double> min;        ///< lowest pressure at the stroke ends 0..1
    std::optional<double> taperIn;    ///< length of the start taper in points
    std::optional<double> taperOut;   ///< length of the end taper in points
    std::optional<double> variation;  ///< amplitude of slow random pressure variation 0..1
    double nibAngle = 45;             ///< calligraphy: nib angle in degrees (0 = horizontal)
    unsigned seed = 1;                ///< makes the random variation reproducible
    bool speedAware = true;           ///< with timestamps: faster movement → thinner line

    static const std::vector<std::string>& presets();
    /// Throws std::invalid_argument for unknown presets or out-of-range values
    void validate() const;
};

/// Inserts points so that neighbours are at most `spacing` points apart (linear interpolation of x, y, z)
std::vector<Point> resample(const std::vector<Point>& points, double spacing);

/**
 * @brief Normalized pressure (0..1) for every point of a polyline.
 * @param times optional timestamps in milliseconds (same length as points) for speed-aware profiles
 */
std::vector<double> profilePressures(const std::vector<Point>& points, const PressureProfile& profile,
                                     const std::vector<double>* times = nullptr);

/// Sets z of every point to hardwareWidth(pressure[i]); pressures must have the same length as points
void applyPressures(std::vector<Point>& points, const std::vector<double>& pressures, double toolWidth,
                    const PressureSettings& settings);

/// A pen style learned from the user's own strokes
struct LearnedStyle {
    PressureProfile profile;     ///< preset "ink" with base/min/tapers/variation fitted to the strokes
    double width = 1.41;         ///< median tool width of the strokes
    std::vector<double> sample;  ///< diagnostic: normalized pressure along a typical stroke (20 values)
    size_t strokes = 0;          ///< how many strokes were analyzed
};

/**
 * @brief Fits a pressure profile to strokes with pressure. Each stroke is given as its points (z = width at the
 * point, as stored) and its tool width. Returns std::nullopt if there are no usable strokes.
 */
std::optional<LearnedStyle> learnStyle(const std::vector<std::pair<std::vector<Point>, double>>& strokes);

/// Adds a small, smooth, reproducible tremor (amplitude in points) to x/y, keeping the end points fixed
void addTremor(std::vector<Point>& points, double amplitude, unsigned seed);

}  // namespace xoj::api
