/*
 * Copyright (c) 2012,2015 Martin Mitas
 *  -- Minor tweaks & optimizations
 *  -- Incorporation into mCtrl
 *
 * Copyright (c) 2012 Lajos Ambrus
 *  -- Original C/C++ port
 *  -- See http://github.com/boronine/husl/tree/master/ports/c
 *
 * Copyright (c) 2012 Alexei Boronine
 *  -- Idea, design & algorithm
 *  -- Original CoffeeScript & JavaScript implementation
 *  -- See http://boronine.com/husl/
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
 *
 *  -- Fixes:
 *       -- Renamed all variables beginning with '_' to end with it instead as
 *          the former is reserved for compiler and standard library.
 *       -- Replaced unneeded global variable values[] with a local variable
 *          on stack hence fixed thread safety and improved data locality.
 *       -- Fixed all compiler warnings (as of option '-Wall' of gcc 4.7).
 *
 *  -- Optimizations:
 *       -- Replaced 'powf(X, 1.0f/2.0f)' and 'powf(X, 1.0f/3.0f)' with
 *          'sqrt(X)' and 'cbrt(X)' respectively.
 *       -- Where appropriate, replaced integer literals with float literals
 *          (added the '.0f' suffix).
 *       -- Replaced double float literals with float literals (added the 'f'
 *          suffix), hence got rid of many unneeded casts between double and
 *          float types.
 *       -- Replaced double float functions like 'pow()', 'sin()', 'cos()' etc.
 *          with their float counterparts powf(), sinf(), cosf() etc., hence
 *          got rid of even more bogus casts.
 *
 *  -- Code clean-up:
 *       -- Made all global constants 'static const'.
 *       -- Got rid of some unused functions.
 *       -- Made all internal functions 'static'.
 *       -- Renamed all the functions to have a prefix 'husl_', hence mitigated
 *          risk of symbol collisions.
 *       -- Removed C++ style comments.
 *       -- Other minor changes for better readability and code style.
 */


#include "husl.h"

#include <math.h>
#include <float.h>


static const float m[3][3] = { {  3.2406f, -1.5372f, -0.4986f },
                               { -0.9689f,  1.8758f,  0.0415f },
                               {  0.0557f, -0.2040f,  1.0570f } };

static const float m_inv[3][3] = { { 0.4124f, 0.3576f, 0.1805f },
                                   { 0.2126f, 0.7152f, 0.0722f },
                                   { 0.0193f, 0.1192f, 0.9505f } };

static const float refY = 1.00000f;
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

    hrad = H * (MC_PIf / 180.0f);
    sinH = sinf(hrad);
    cosH = cosf(hrad);
    sub1 = powf(L + 16.0f, 3.0f) / 1560896.0f;
    sub2 = sub1 > 0.008856f ? sub1 : (L / 903.3f);
    result = FLT_MAX;
    for(i_ = 0, len_ = 3; i_ < len_; ++i_) {
        row = m[i_];
        m1 = row[0], m2 = row[1], m3 = row[2];
        top = (0.99915f * m1 + 1.05122f * m2 + 1.14460f * m3) * sub2;
        rbottom = 0.86330f * m3 - 0.17266f * m2;
        lbottom = 0.12949f * m3 - 0.38848f * m1;
        bottom = (rbottom * sinH + lbottom * cosH) * sub2;

        for(j_ = 0, len1_ = 2; j_ < len1_; ++j_) {
            t = ref_[j_];
            C = L * (top - 1.05122f * t) / (bottom + 0.17266f * sinH * t);
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
        return cbrtf(t);
    else
        return 7.787f * t + 16.0f / 116.0f;
}

static float
husl_f_inv(float t)
{
    if(powf(t, 3.0f) > lab_e)
        return powf(t, 3.0f);
    else
        return (116.0f * t - 16.0f) / lab_k;
}

static float
husl_from_linear(float c)
{
    if(c <= 0.0031308f)
        return 12.92f * c;
    else
        return 1.055f * powf(c, 1.0f / 2.4f) - 0.055f;
}

static float
husl_to_linear(float c)
{
    const float a = 0.055f;

    if(c > 0.04045f)
        return powf((c + a) / (1.0f + a), 2.4f);
    else
        return c / 12.92f;
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

    varU = (4.0f * X) / (X + (15.0f * Y) + (3 * Z));
    varV = (9.0f * Y) / (X + (15.0f * Y) + (3 * Z));
    L = 116.0f * husl_f(Y / refY) - 16.0f;
    U = 13.0f * L * (varU - refU);
    V = 13.0f * L * (varV - refV);

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

    if(L < 0.001f) {  /* Using Epsilon 0.001f instead of checking for equality to zero. */
        tuple[2] = tuple[1] = tuple[0] = 0.0f;
        return tuple;
    }

    varY = husl_f_inv((L + 16.0f) / 116.0f);
    varU = U / (13.0f * L) + refU;
    varV = V / (13.0f * L) + refV;
    Y = varY * refY;
    X = 0.0f - (9.0f * Y * varU) / ((varU - 4.0f) * varV - varU * varV);
    Z = (9.0f * Y - (15.0f * varV * Y) - (varV * X)) / (3.0f * varV);

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

    C = sqrtf(powf(U, 2.0f) + powf(V, 2.0f));
    Hrad = atan2f(V, U);
    H = Hrad * (180.0f / MC_PIf);
    if(H < 0.0f)
        H = 360.0f + H;

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

    Hrad = H / (180.0f / MC_PIf);
    U = cosf(Hrad) * C;
    V = sinf(Hrad) * C;

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
    S = C / max * 100.0f;

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

