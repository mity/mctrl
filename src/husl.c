/*
 * Copyright (c) 2012 Alexei Boronine
 *  -- Idea, design & algortithms
 *  -- Original CoffeeScript & JavaScript implementation
 *  -- See http://boronine.com/husl/
 *
 * Copyright (c) 2012 Lajos Ambrus
 *  -- C/C++ port
 *  -- See http://github.com/boronine/husl/tree/master/ports/c
 *
 * Copyright (c) 2012 Martin Mitas
 *  -- Minor tweaks & incorporation into mCtrl
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */


/* Changes from original Lajos's husl.c:
 *  -- Made all functions except those specified in husl.h 'static'.
 *  -- Renamed all the functions to have a prefix 'husl_'.
 *  -- Made all global constants 'static const'.
 *  -- Replaced a global variable values[] with local one where needed,
 *     hence fixed thread safety.
 *  -- Fixed all compiler warnings (as of option -Wall of gcc 4.7)
 *  -- Minor changes for better readability and code style unification.
 *  -- Removed C++ comments.
 *  -- Removed some unused functions.
 *  -- Renamed all variables beginning with '_' to end with it instead.
 */


#include "husl.h"

#include <math.h>
#include <float.h>


#define PI 3.1415926535897932384626433832795


static const float m[3][3] = { {  3.2406f, -1.5372f, -0.4986f },
                               { -0.9689f,  1.8758f,  0.0415f },
                               {  0.0557f, -0.2040f,  1.0570f } };

static const float m_inv[3][3] = { { 0.4124f, 0.3576f, 0.1805f },
                                   { 0.2126f, 0.7152f, 0.0722f },
                                   { 0.0193f, 0.1192f, 0.9505f } };

static const float refX = 0.95047f;
static const float refY = 1.00000f;
static const float refZ = 1.08883f;
static const float refU = 0.19784f;
static const float refV = 0.46834f;
static const float lab_e = 0.008856f;
static const float lab_k = 903.3f;


static float
husl_max_chroma(float L, float H)
{
    float C, bottom, cosH, hrad, lbottom, m1, m2, m3, rbottom, result, sinH, sub1, sub2, t, top;
    int i_, j_, len_, len1_;
    const float* row;
    float ref_[2] = { 0.0f, 1.0f };

    hrad = (float) ((H / 360.0f) * 2 * PI);
    sinH = (float) (sin(hrad));
    cosH = (float) (cos(hrad));
    sub1 = (float) (pow(L + 16, 3) / 1560896.0);
    sub2 = sub1 > 0.008856 ? sub1 : (float) (L / 903.3);
    result = FLT_MAX;
    for(i_ = 0, len_ = 3; i_ < len_; ++i_) {
        row = m[i_];
        m1 = row[0], m2 = row[1], m3 = row[2];
        top = (float) ((0.99915 * m1 + 1.05122 * m2 + 1.14460 * m3) * sub2);
        rbottom = (float) (0.86330 * m3 - 0.17266 * m2);
        lbottom = (float) (0.12949 * m3 - 0.38848 * m1);
        bottom = (rbottom * sinH + lbottom * cosH) * sub2;

        for(j_ = 0, len1_ = 2; j_ < len1_; ++j_) {
            t = ref_[j_];
            C = (float) (L * (top - 1.05122 * t) / (bottom + 0.17266 * sinH * t));
            if(C > 0 && C < result)
                result = C;
        }
    }
    return result;
}

static float
husl_dot_product(const float* a, const float* b, int len)
{
    int i;
    float ret = 0.0f;

    for(i = 0; i < len; i++)
        ret += a[i] * b[i];

    return ret;
}

static float
husl_f(float t)
{
    if(t > lab_e)
        return (float) (pow(t, 1.0f / 3.0f));
    else
        return (float) (7.787 * t + 16 / 116.0);
}

static float
husl_f_inv(float t)
{
    if(pow(t, 3) > lab_e)
        return (float) (pow(t, 3));
    else
        return (116 * t - 16) / lab_k;
}

static float
husl_from_linear(float c)
{
    if(c <= 0.0031308)
        return 12.92f * c;
    else
        return (float) (1.055 * pow(c, 1 / 2.4f) - 0.055);
}

static float
husl_to_linear(float c)
{
    float a = 0.055f;

    if(c > 0.04045)
        return (float) (pow((c + a) / (1 + a), 2.4f));
    else
        return (float) (c / 12.92);
}

static float*
husl_xyz2rgb(float* tuple)
{
    float B, G, R;

    R = husl_from_linear(husl_dot_product(m[0], tuple, 3));
    G = husl_from_linear(husl_dot_product(m[1], tuple, 3));
    B = husl_from_linear(husl_dot_product(m[2], tuple, 3));

    tuple[0] = R;
    tuple[1] = G;
    tuple[2] = B;

    return tuple;
}

static float*
husl_rgb2xyz(float* tuple)
{
    float B, G, R, X, Y, Z;
    float rgbl[3];

    R = tuple[0];
    G = tuple[1];
    B = tuple[2];

    rgbl[0] = husl_to_linear(R);
    rgbl[1] = husl_to_linear(G);
    rgbl[2] = husl_to_linear(B);

    X = husl_dot_product(m_inv[0], rgbl, 3);
    Y = husl_dot_product(m_inv[1], rgbl, 3);
    Z = husl_dot_product(m_inv[2], rgbl, 3);

    tuple[0] = X;
    tuple[1] = Y;
    tuple[2] = Z;

    return tuple;
}

static float*
husl_xyz2luv(float* tuple)
{
    float L, U, V, X, Y, Z, varU, varV;

    X = tuple[0];
    Y = tuple[1];
    Z = tuple[2];

    varU = (4 * X) / (X + (15.0f * Y) + (3 * Z));
    varV = (9 * Y) / (X + (15.0f * Y) + (3 * Z));
    L = 116 * husl_f(Y / refY) - 16;
    U = 13 * L * (varU - refU);
    V = 13 * L * (varV - refV);

    tuple[0] = L;
    tuple[1] = U;
    tuple[2] = V;

    return tuple;
}

static float*
husl_luv2xyz(float* tuple)
{
    float L, U, V, X, Y, Z, varU, varV, varY;

    L = tuple[0];
    U = tuple[1];
    V = tuple[2];

    if(L == 0) {
        tuple[2] = tuple[1] = tuple[0] = 0.0f;
        return tuple;
    }

    varY = husl_f_inv((L + 16) / 116.0f);
    varU = U / (13.0f * L) + refU;
    varV = V / (13.0f * L) + refV;
    Y = varY * refY;
    X = 0 - (9 * Y * varU) / ((varU - 4.0f) * varV - varU * varV);
    Z = (9 * Y - (15 * varV * Y) - (varV * X)) / (3.0f * varV);

    tuple[0] = X;
    tuple[1] = Y;
    tuple[2] = Z;

    return tuple;
}

static float*
husl_luv2lch(float* tuple)
{
    float C, H, Hrad, L, U, V;

    L = tuple[0];
    U = tuple[1];
    V = tuple[2];

    C = (float) (pow(pow(U, 2) + pow(V, 2), (1 / 2.0f)));
    Hrad = (float) (atan2(V, U));
    H = (float) (Hrad * 360.0f / 2.0f / PI);
    if(H < 0)
        H = 360 + H;

    tuple[0] = L;
    tuple[1] = C;
    tuple[2] = H;

    return tuple;
}

static float*
husl_lch2luv(float* tuple)
{
    float C, H, Hrad, L, U, V;

    L = tuple[0];
    C = tuple[1];
    H = tuple[2];

    Hrad = (float) (H / 360.0 * 2.0 * PI);
    U = (float) (cos(Hrad) * C);
    V = (float) (sin(Hrad) * C);

    tuple[0] = L;
    tuple[1] = U;
    tuple[2] = V;

    return tuple;
}

static float*
husl_husl2lch(float* tuple)
{
    float C, H, L, S, max;

    H = tuple[0];
    S = tuple[1];
    L = tuple[2];

    max = husl_max_chroma(L, H);
    C = max / 100.0f * S;

    tuple[0] = L;
    tuple[1] = C;
    tuple[2] = H;

    return tuple;
}

static float*
husl_lch2husl(float* tuple)
{
    float C, H, L, S, max;

    L = tuple[0];
    C = tuple[1];
    H = tuple[2];

    max = husl_max_chroma(L, H);
    S = C / max * 100;

    tuple[0] = H;
    tuple[1] = S;
    tuple[2] = L;

    return tuple;
}

void
husl_to_rgb(float* r, float* g, float* b, float h, float s, float l)
{
    float values[3];

    values[0] = h;
    values[1] = s;
    values[2] = l;

    husl_xyz2rgb(husl_luv2xyz(husl_lch2luv(husl_husl2lch(values))));

    *r = values[0];
    *g = values[1];
    *b = values[2];
}

void
husl_from_rgb(float* h, float* s, float* l, float r, float g, float b)
{
    float values[3];

    values[0] = r;
    values[1] = g;
    values[2] = b;

    husl_lch2husl(husl_luv2lch(husl_xyz2luv(husl_rgb2xyz(values))));

    *h = values[0];
    *s = values[1];
    *l = values[2];
}

