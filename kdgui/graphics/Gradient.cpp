/*
 * Copyright (C) 2006, 2007, 2008, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

//#include "config.h"
#include <UIlib.h>
#include "Gradient.h"
#include "KdColor.h"
#include "FloatRect.h"
#include <wtf/UnusedParam.h>

Gradient::Gradient(const FloatPoint& p0, const FloatPoint& p1)
    : m_radial(false)
    , m_p0(p0)
    , m_p1(p1)
    , m_r0(0)
    , m_r1(0)
    , m_aspectRatio(1)
    , m_stopsSorted(false)
    , m_lastStop(0)
    , m_spreadMethod(SpreadMethodPad)
{
    platformInit();
}

Gradient::Gradient(const FloatPoint& p0, float r0, const FloatPoint& p1, float r1, float aspectRatio)
    : m_radial(true)
    , m_p0(p0)
    , m_p1(p1)
    , m_r0(r0)
    , m_r1(r1)
    , m_aspectRatio(aspectRatio)
    , m_stopsSorted(false)
    , m_lastStop(0)
    , m_spreadMethod(SpreadMethodPad)
{
    platformInit();
}

Gradient::~Gradient()
{
    platformDestroy();
}

void Gradient::adjustParametersForTiledDrawing(IntSize& size, FloatRect& srcRect)
{
    if (m_radial)
        return;

    if (srcRect.isEmpty())
        return;

    if (m_p0.x() == m_p1.x()) {
        size.setWidth(1);
        srcRect.setWidth(1);
        srcRect.setX(0);
        return;
    }
    if (m_p0.y() != m_p1.y())
        return;

    size.setHeight(1);
    srcRect.setHeight(1);
    srcRect.setY(0);
}

void Gradient::addColorStop(float value, const KdColor& color)
{
    float r;
    float g;
    float b;
    float a;
    color.getRGBA(r, g, b, a);
    m_stops.push_back(ColorStop(value, r, g, b, a));

    m_stopsSorted = false;
    platformDestroy();
}

void Gradient::addColorStop(const Gradient::ColorStop& stop)
{
    m_stops.push_back(stop);

    m_stopsSorted = false;
    platformDestroy();
}

static inline bool compareStops(const Gradient::ColorStop& a, const Gradient::ColorStop& b)
{
    return a.stop < b.stop;
}

void Gradient::sortStopsIfNecessary()
{
    if (m_stopsSorted)
        return;

    m_stopsSorted = true;

    if (!m_stops.size())
        return;

    std::stable_sort(m_stops.begin(), m_stops.end(), compareStops);
}

void Gradient::getColor(float value, float* r, float* g, float* b, float* a) const
{
    ASSERT(value >= 0);
    ASSERT(value <= 1);

    if (m_stops.isEmpty()) {
        *r = 0;
        *g = 0;
        *b = 0;
        *a = 0;
        return;
    }
    if (!m_stopsSorted) {
        if (m_stops.size())
            std::stable_sort(m_stops.begin(), m_stops.end(), compareStops);
        m_stopsSorted = true;
    }
    if (value <= 0 || value <= m_stops.at(0).stop) {
        *r = m_stops.front().red;
        *g = m_stops.front().green;
        *b = m_stops.front().blue;
        *a = m_stops.front().alpha;
        return;
    }
    if (value >= 1 || value >= m_stops.at(m_stops.size() - 1).stop) {
        *r = m_stops.back().red;
        *g = m_stops.back().green;
        *b = m_stops.back().blue;
        *a = m_stops.back().alpha;
        return;
    }

    // Find stop before and stop after and interpolate.
    int stop = findStop(value);
    const ColorStop& lastStop = m_stops[stop];    
    const ColorStop& nextStop = m_stops[stop + 1];
    float stopFraction = (value - lastStop.stop) / (nextStop.stop - lastStop.stop);
    *r = lastStop.red + (nextStop.red - lastStop.red) * stopFraction;
    *g = lastStop.green + (nextStop.green - lastStop.green) * stopFraction;
    *b = lastStop.blue + (nextStop.blue - lastStop.blue) * stopFraction;
    *a = lastStop.alpha + (nextStop.alpha - lastStop.alpha) * stopFraction;
}

int Gradient::findStop(float value) const
{
    ASSERT(value >= 0);
    ASSERT(value <= 1);
    ASSERT(m_stopsSorted);

    int numStops = m_stops.size();
    ASSERT(numStops >= 2);
    ASSERT(m_lastStop < numStops - 1);

    int i = m_lastStop;
    if (value < m_stops[i].stop)
        i = 1;
    else
        i = m_lastStop + 1;

    for (; i < numStops - 1; ++i)
        if (value < m_stops[i].stop)
            break;

    m_lastStop = i - 1;
    return m_lastStop;
}

bool Gradient::hasAlpha() const
{
    for (size_t i = 0; i < m_stops.size(); i++) {
        if (m_stops[i].alpha < 1)
            return true;
    }

    return false;
}

void Gradient::setSpreadMethod(GradientSpreadMethod spreadMethod)
{
    // FIXME: Should it become necessary, allow calls to this method after m_gradient has been set.
    ASSERT(m_gradient == 0);
    m_spreadMethod = spreadMethod;
}

void Gradient::setGradientSpaceTransform(const AffineTransform& gradientSpaceTransformation)
{ 
    m_gradientSpaceTransformation = gradientSpaceTransformation;
    setPlatformGradientSpaceTransform(gradientSpaceTransformation);
}

bool equalGradient(const WTF::Vector<Gradient::ColorStop>& a, const WTF::Vector<Gradient::ColorStop>b) {
    if (a.size() != b.size())
        return false;
    for (int i = 0; i < (int)a.size(); ++i) {
        if (a[i].stop != b[i].stop ||
            a[i].red != b[i].red ||
            a[i].green != b[i].green ||
            a[i].blue != b[i].blue ||
            a[i].blue != b[i].blue ||
            a[i].alpha != b[i].alpha) {
            return false;
        }
    }

    return true;
}

// bool Gradient::operator==(const Gradient& other) const
// {
//     if (m_radial) {
//         return m_radial == other.m_radial 
//             && m_p0 == other.m_p0
//             && m_p1 == other.m_p1
//             && m_r0 == other.m_r0
//             && m_r1 == other.m_r1
//             && m_aspectRatio == other.m_aspectRatio
//             && equalGradient(m_stops, other.m_stops)
//             && m_spreadMethod == other.m_spreadMethod
//             && m_gradientSpaceTransformation == other.m_gradientSpaceTransformation;
//     }
// 
//     return m_radial == other.m_radial 
//         && m_p0 == other.m_p0
//         && m_p1 == other.m_p1
//         && equalGradient(m_stops, other.m_stops)
//         && m_spreadMethod == other.m_spreadMethod
//         && m_gradientSpaceTransformation == other.m_gradientSpaceTransformation;
// }

bool equalGradient(const Gradient* a, const Gradient* b)
{
    if (!a && !b)
        return true;

    if ((!a && b) || (a && !b))
        return false;

    return true;
}