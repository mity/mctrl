
#include "acutest.h"
#include "hsluv.h"
#include "snapshot.h"


#define MAX_DIFF            0.00000001

#define ABS(x)              ((x) >= 0 ? (x) : -(x))

#define CHECK_EQ(a, b)                                                  \
        TEST_CHECK_(ABS((a) - (b)) < MAX_DIFF,                          \
                    "%s: Mismatch in channel '" #a "' (%f versus %f).", \
                     snapshot[i].hex_str, (double)a, (double)b)


static void
test_hsluv2rgb(void)
{
    int i;

    for(i = 0; i < snapshot_n; i++) {
        double r, g, b;

        hsluv2rgb(snapshot[i].hsluv_h, snapshot[i].hsluv_s, snapshot[i].hsluv_l, &r, &g, &b);

        CHECK_EQ(r, snapshot[i].rgb_r);
        CHECK_EQ(g, snapshot[i].rgb_g);
        CHECK_EQ(b, snapshot[i].rgb_b);
    }
}

static void
test_rgb2hsluv(void)
{
    int i;

    for(i = 0; i < snapshot_n; i++) {
        double h, s, l;

        rgb2hsluv(snapshot[i].rgb_r, snapshot[i].rgb_g, snapshot[i].rgb_b, &h, &s, &l);

        CHECK_EQ(h, snapshot[i].hsluv_h);
        CHECK_EQ(s, snapshot[i].hsluv_s);
        CHECK_EQ(l, snapshot[i].hsluv_l);
    }
}

static void
test_hpluv2rgb(void)
{
    int i;

    for(i = 0; i < snapshot_n; i++) {
        double r, g, b;

        hpluv2rgb(snapshot[i].hpluv_h, snapshot[i].hpluv_s, snapshot[i].hpluv_l, &r, &g, &b);

        CHECK_EQ(r, snapshot[i].rgb_r);
        CHECK_EQ(g, snapshot[i].rgb_g);
        CHECK_EQ(b, snapshot[i].rgb_b);
    }
}

static void
test_rgb2hpluv(void)
{
    int i;

    for(i = 0; i < snapshot_n; i++) {
        double h, s, l;

        rgb2hpluv(snapshot[i].rgb_r, snapshot[i].rgb_g, snapshot[i].rgb_b, &h, &s, &l);

        CHECK_EQ(h, snapshot[i].hpluv_h);
        CHECK_EQ(s, snapshot[i].hpluv_s);
        CHECK_EQ(l, snapshot[i].hpluv_l);
    }
}


TEST_LIST = {
    { "hsluv2rgb", test_hsluv2rgb },
    { "rgb2hsluv", test_rgb2hsluv },
    { "hpluv2rgb", test_hpluv2rgb },
    { "rgb2hpluv", test_rgb2hpluv },
    { NULL, NULL }
};
