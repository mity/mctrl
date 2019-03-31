/*
 * C Reusables
 * <http://github.com/mity/c-reusables>
 *
 * Copyright (c) 2017 Martin Mitas
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include "acutest.h"
#include "fnv1a.h"


typedef struct TEST_VECTOR {
    const char* str;
    size_t n;
    uint32_t fnv32;
    uint64_t fnv64;
} TEST_VECTOR;



/* The test vectors below are taken from
 * http://www.isthe.com/chongo/src/fnv/test_fnv.c
 */


#define LEN(x)      (sizeof(x)-1)
/* TEST macro does not include trailing NUL byte in the test vector */
#define TEST(x)     x, LEN(x)
/* TEST0 macro includes the trailing NUL byte in the test vector */
#define TEST0(x)    x, sizeof(x)
/* REPEAT500 - repeat a string 500 times */
#define R500(x)     R100(x)R100(x)R100(x)R100(x)R100(x)
#define R100(x)     R10(x)R10(x)R10(x)R10(x)R10(x)R10(x)R10(x)R10(x)R10(x)R10(x)
#define R10(x)      x x x x x x x x x x

static const TEST_VECTOR test_vectrors[] = {
    { TEST(""), 0x811c9dc5U, 0xcbf29ce484222325U },
    { TEST("a"), 0xe40c292cU, 0xaf63dc4c8601ec8cU },
    { TEST("b"), 0xe70c2de5U, 0xaf63df4c8601f1a5U },
    { TEST("c"), 0xe60c2c52U, 0xaf63de4c8601eff2U },
    { TEST("d"), 0xe10c2473U, 0xaf63d94c8601e773U },
    { TEST("e"), 0xe00c22e0U, 0xaf63d84c8601e5c0U },
    { TEST("f"), 0xe30c2799U, 0xaf63db4c8601ead9U },
    { TEST("fo"), 0x6222e842U, 0x08985907b541d342U },
    { TEST("foo"), 0xa9f37ed7U, 0xdcb27518fed9d577U },
    { TEST("foob"), 0x3f5076efU, 0xdd120e790c2512afU },
    { TEST("fooba"), 0x39aaa18aU, 0xcac165afa2fef40aU },
    { TEST("foobar"), 0xbf9cf968U, 0x85944171f73967e8U },
    { TEST0(""), 0x050c5d1fU, 0xaf63bd4c8601b7dfU },
    { TEST0("a"), 0x2b24d044U, 0x089be207b544f1e4U },
    { TEST0("b"), 0x9d2c3f7fU, 0x08a61407b54d9b5fU },
    { TEST0("c"), 0x7729c516U, 0x08a2ae07b54ab836U },
    { TEST0("d"), 0xb91d6109U, 0x0891b007b53c4869U },
    { TEST0("e"), 0x931ae6a0U, 0x088e4a07b5396540U },
    { TEST0("f"), 0x052255dbU, 0x08987c07b5420ebbU },
    { TEST0("fo"), 0xbef39fe6U, 0xdcb28a18fed9f926U },
    { TEST0("foo"), 0x6150ac75U, 0xdd1270790c25b935U },
    { TEST0("foob"), 0x9aab3a3dU, 0xcac146afa2febf5dU },
    { TEST0("fooba"), 0x519c4c3eU, 0x8593d371f738acfeU },
    { TEST0("foobar"), 0x0c1c9eb8U, 0x34531ca7168b8f38U },
    { TEST("ch"), 0x5f299f4eU, 0x08a25607b54a22aeU },
    { TEST("cho"), 0xef8580f3U, 0xf5faf0190cf90df3U },
    { TEST("chon"), 0xac297727U, 0xf27397910b3221c7U },
    { TEST("chong"), 0x4546b9c0U, 0x2c8c2b76062f22e0U },
    { TEST("chongo"), 0xbd564e7dU, 0xe150688c8217b8fdU },
    { TEST("chongo "), 0x6bdd5c67U, 0xf35a83c10e4f1f87U },
    { TEST("chongo w"), 0xdd77ed30U, 0xd1edd10b507344d0U },
    { TEST("chongo wa"), 0xf4ca9683U, 0x2a5ee739b3ddb8c3U },
    { TEST("chongo was"), 0x4aeb9bd0U, 0xdcfb970ca1c0d310U },
    { TEST("chongo was "), 0xe0e67ad0U, 0x4054da76daa6da90U },
    { TEST("chongo was h"), 0xc2d32fa8U, 0xf70a2ff589861368U },
    { TEST("chongo was he"), 0x7f743fb7U, 0x4c628b38aed25f17U },
    { TEST("chongo was her"), 0x6900631fU, 0x9dd1f6510f78189fU },
    { TEST("chongo was here"), 0xc59c990eU, 0xa3de85bd491270ceU },
    { TEST("chongo was here!"), 0x448524fdU, 0x858e2fa32a55e61dU },
    { TEST("chongo was here!\n"), 0xd49930d5U, 0x46810940eff5f915U },
    { TEST0("ch"), 0x1c85c7caU, 0xf5fadd190cf8edaaU },
    { TEST0("cho"), 0x0229fe89U, 0xf273ed910b32b3e9U },
    { TEST0("chon"), 0x2c469265U, 0x2c8c5276062f6525U },
    { TEST0("chong"), 0xce566940U, 0xe150b98c821842a0U },
    { TEST0("chongo"), 0x8bdd8ec7U, 0xf35aa3c10e4f55e7U },
    { TEST0("chongo "), 0x34787625U, 0xd1ed680b50729265U },
    { TEST0("chongo w"), 0xd3ca6290U, 0x2a5f0639b3dded70U },
    { TEST0("chongo wa"), 0xddeaf039U, 0xdcfbaa0ca1c0f359U },
    { TEST0("chongo was"), 0xc0e64870U, 0x4054ba76daa6a430U },
    { TEST0("chongo was "), 0xdad35570U, 0xf709c7f5898562b0U },
    { TEST0("chongo was h"), 0x5a740578U, 0x4c62e638aed2f9b8U },
    { TEST0("chongo was he"), 0x5b004d15U, 0x9dd1a8510f779415U },
    { TEST0("chongo was her"), 0x6a9c09cdU, 0xa3de2abd4911d62dU },
    { TEST0("chongo was here"), 0x2384f10aU, 0x858e0ea32a55ae0aU },
    { TEST0("chongo was here!"), 0xda993a47U, 0x46810f40eff60347U },
    { TEST0("chongo was here!\n"), 0x8227df4fU, 0xc33bce57bef63eafU },
    { TEST("cu"), 0x4c298165U, 0x08a24307b54a0265U },
    { TEST("cur"), 0xfc563735U, 0xf5b9fd190cc18d15U },
    { TEST("curd"), 0x8cb91483U, 0x4c968290ace35703U },
    { TEST("curds"), 0x775bf5d0U, 0x07174bd5c64d9350U },
    { TEST("curds "), 0xd5c428d0U, 0x5a294c3ff5d18750U },
    { TEST("curds a"), 0x34cc0ea3U, 0x05b3c1aeb308b843U },
    { TEST("curds an"), 0xea3b4cb7U, 0xb92a48da37d0f477U },
    { TEST("curds and"), 0x8e59f029U, 0x73cdddccd80ebc49U },
    { TEST("curds and "), 0x2094de2bU, 0xd58c4c13210a266bU },
    { TEST("curds and w"), 0xa65a0ad4U, 0xe78b6081243ec194U },
    { TEST("curds and wh"), 0x9bbee5f4U, 0xb096f77096a39f34U },
    { TEST("curds and whe"), 0xbe836343U, 0xb425c54ff807b6a3U },
    { TEST("curds and whey"), 0x22d5344eU, 0x23e520e2751bb46eU },
    { TEST("curds and whey\n"), 0x19a1470cU, 0x1a0b44ccfe1385ecU },
    { TEST0("cu"), 0x4a56b1ffU, 0xf5ba4b190cc2119fU },
    { TEST0("cur"), 0x70b8e86fU, 0x4c962690ace2baafU },
    { TEST0("curd"), 0x0a5b4a39U, 0x0716ded5c64cda19U },
    { TEST0("curds"), 0xb5c3f670U, 0x5a292c3ff5d150f0U },
    { TEST0("curds "), 0x53cc3f70U, 0x05b3e0aeb308ecf0U },
    { TEST0("curds a"), 0xc03b0a99U, 0xb92a5eda37d119d9U },
    { TEST0("curds an"), 0x7259c415U, 0x73ce41ccd80f6635U },
    { TEST0("curds and"), 0x4095108bU, 0xd58c2c132109f00bU },
    { TEST0("curds and "), 0x7559bdb1U, 0xe78baf81243f47d1U },
    { TEST0("curds and w"), 0xb3bf0bbcU, 0xb0968f7096a2ee7cU },
    { TEST0("curds and wh"), 0x2183ff1cU, 0xb425a84ff807855cU },
    { TEST0("curds and whe"), 0x2bd54279U, 0x23e4e9e2751b56f9U },
    { TEST0("curds and whey"), 0x23a156caU, 0x1a0b4eccfe1396eaU },
    { TEST0("curds and whey\n"), 0x64e2d7e4U, 0x54abd453bb2c9004U },
    { TEST("hi"), 0x683af69aU, 0x08ba5f07b55ec3daU },
    { TEST0("hi"), 0xaed2346eU, 0x337354193006cb6eU },
    { TEST("hello"), 0x4f9f2cabU, 0xa430d84680aabd0bU },
    { TEST0("hello"), 0x02935131U, 0xa9bc8acca21f39b1U },
    { TEST("\xff\x00\x00\x01"), 0xc48fb86dU, 0x6961196491cc682dU },
    { TEST("\x01\x00\x00\xff"), 0x2269f369U, 0xad2bb1774799dfe9U },
    { TEST("\xff\x00\x00\x02"), 0xc18fb3b4U, 0x6961166491cc6314U },
    { TEST("\x02\x00\x00\xff"), 0x50ef1236U, 0x8d1bb3904a3b1236U },
    { TEST("\xff\x00\x00\x03"), 0xc28fb547U, 0x6961176491cc64c7U },
    { TEST("\x03\x00\x00\xff"), 0x96c3bf47U, 0xed205d87f40434c7U },
    { TEST("\xff\x00\x00\x04"), 0xbf8fb08eU, 0x6961146491cc5faeU },
    { TEST("\x04\x00\x00\xff"), 0xf3e4d49cU, 0xcd3baf5e44f8ad9cU },
    { TEST("\x40\x51\x4e\x44"), 0x32179058U, 0xe3b36596127cd6d8U },
    { TEST("\x44\x4e\x51\x40"), 0x280bfee6U, 0xf77f1072c8e8a646U },
    { TEST("\x40\x51\x4e\x4a"), 0x30178d32U, 0xe3b36396127cd372U },
    { TEST("\x4a\x4e\x51\x40"), 0x21addaf8U, 0x6067dce9932ad458U },
    { TEST("\x40\x51\x4e\x54"), 0x4217a988U, 0xe3b37596127cf208U },
    { TEST("\x54\x4e\x51\x40"), 0x772633d6U, 0x4b7b10fa9fe83936U },
    { TEST("127.0.0.1"), 0x08a3d11eU, 0xaabafe7104d914beU },
    { TEST0("127.0.0.1"), 0xb7e2323aU, 0xf4d3180b3cde3edaU },
    { TEST("127.0.0.2"), 0x07a3cf8bU, 0xaabafd7104d9130bU },
    { TEST0("127.0.0.2"), 0x91dfb7d1U, 0xf4cfb20b3cdb5bb1U },
    { TEST("127.0.0.3"), 0x06a3cdf8U, 0xaabafc7104d91158U },
    { TEST0("127.0.0.3"), 0x6bdd3d68U, 0xf4cc4c0b3cd87888U },
    { TEST("64.81.78.68"), 0x1d5636a7U, 0xe729bac5d2a8d3a7U },
    { TEST0("64.81.78.68"), 0xd5b808e5U, 0x74bc0524f4dfa4c5U },
    { TEST("64.81.78.74"), 0x1353e852U, 0xe72630c5d2a5b352U },
    { TEST0("64.81.78.74"), 0xbf16b916U, 0x6b983224ef8fb456U },
    { TEST("64.81.78.84"), 0xa55b89edU, 0xe73042c5d2ae266dU },
    { TEST0("64.81.78.84"), 0x3c1a2017U, 0x8527e324fdeb4b37U },
    { TEST("feedface"), 0x0588b13cU, 0x0a83c86fee952abcU },
    { TEST0("feedface"), 0xf22f0174U, 0x7318523267779d74U },
    { TEST("feedfacedaffdeed"), 0xe83641e1U, 0x3e66d3d56b8caca1U },
    { TEST0("feedfacedaffdeed"), 0x6e69b533U, 0x956694a5c0095593U },
    { TEST("feedfacedeadbeef"), 0xf1760448U, 0xcac54572bb1a6fc8U },
    { TEST0("feedfacedeadbeef"), 0x64c8bd58U, 0xa7a4c9f3edebf0d8U },
    { TEST("line 1\nline 2\nline 3"), 0x97b4ea23U, 0x7829851fac17b143U },
    { TEST("chongo <Landon Curt Noll> /\\../\\"), 0x9a4e92e6U, 0x2c8f4c9af81bcf06U },
    { TEST0("chongo <Landon Curt Noll> /\\../\\"), 0xcfb14012U, 0xd34e31539740c732U },
    { TEST("chongo (Landon Curt Noll) /\\../\\"), 0xf01b2511U, 0x3605a2ac253d2db1U },
    { TEST0("chongo (Landon Curt Noll) /\\../\\"), 0x0bbb59c3U, 0x08c11b8346f4a3c3U },
    { TEST("http://antwrp.gsfc.nasa.gov/apod/astropix.html"), 0xce524afaU, 0x6be396289ce8a6daU },
    { TEST("http://en.wikipedia.org/wiki/Fowler_Noll_Vo_hash"), 0xdd16ef45U, 0xd9b957fb7fe794c5U },
    { TEST("http://epod.usra.edu/"), 0x60648bb3U, 0x05be33da04560a93U },
    { TEST("http://exoplanet.eu/"), 0x7fa4bcfcU, 0x0957f1577ba9747cU },
    { TEST("http://hvo.wr.usgs.gov/cam3/"), 0x5053ae17U, 0xda2cc3acc24fba57U },
    { TEST("http://hvo.wr.usgs.gov/cams/HMcam/"), 0xc9302890U, 0x74136f185b29e7f0U },
    { TEST("http://hvo.wr.usgs.gov/kilauea/update/deformation.html"), 0x956ded32U, 0xb2f2b4590edb93b2U },
    { TEST("http://hvo.wr.usgs.gov/kilauea/update/images.html"), 0x9136db84U, 0xb3608fce8b86ae04U },
    { TEST("http://hvo.wr.usgs.gov/kilauea/update/maps.html"), 0xdf9d3323U, 0x4a3a865079359063U },
    { TEST("http://hvo.wr.usgs.gov/volcanowatch/current_issue.html"), 0x32bb6cd0U, 0x5b3a7ef496880a50U },
    { TEST("http://neo.jpl.nasa.gov/risk/"), 0xc8f8385bU, 0x48fae3163854c23bU },
    { TEST("http://norvig.com/21-days.html"), 0xeb08bfbaU, 0x07aaa640476e0b9aU },
    { TEST("http://primes.utm.edu/curios/home.php"), 0x62cc8e3dU, 0x2f653656383a687dU },
    { TEST("http://slashdot.org/"), 0xc3e20f5cU, 0xa1031f8e7599d79cU },
    { TEST("http://tux.wr.usgs.gov/Maps/155.25-19.5.html"), 0x39e97f17U, 0xa31908178ff92477U },
    { TEST("http://volcano.wr.usgs.gov/kilaueastatus.php"), 0x7837b203U, 0x097edf3c14c3fb83U },
    { TEST("http://www.avo.alaska.edu/activity/Redoubt.php"), 0x319e877bU, 0xb51ca83feaa0971bU },
    { TEST("http://www.dilbert.com/fast/"), 0xd3e63f89U, 0xdd3c0d96d784f2e9U },
    { TEST("http://www.fourmilab.ch/gravitation/orbits/"), 0x29b50b38U, 0x86cd26a9ea767d78U },
    { TEST("http://www.fpoa.net/"), 0x5ed678b8U, 0xe6b215ff54a30c18U },
    { TEST("http://www.ioccc.org/index.html"), 0xb0d5b793U, 0xec5b06a1c5531093U },
    { TEST("http://www.isthe.com/cgi-bin/number.cgi"), 0x52450be5U, 0x45665a929f9ec5e5U },
    { TEST("http://www.isthe.com/chongo/bio.html"), 0xfa72d767U, 0x8c7609b4a9f10907U },
    { TEST("http://www.isthe.com/chongo/index.html"), 0x95066709U, 0x89aac3a491f0d729U },
    { TEST("http://www.isthe.com/chongo/src/calc/lucas-calc"), 0x7f52e123U, 0x32ce6b26e0f4a403U },
    { TEST("http://www.isthe.com/chongo/tech/astro/venus2004.html"), 0x76966481U, 0x614ab44e02b53e01U },
    { TEST("http://www.isthe.com/chongo/tech/astro/vita.html"), 0x063258b0U, 0xfa6472eb6eef3290U },
    { TEST("http://www.isthe.com/chongo/tech/comp/c/expert.html"), 0x2ded6e8aU, 0x9e5d75eb1948eb6aU },
    { TEST("http://www.isthe.com/chongo/tech/comp/calc/index.html"), 0xb07d7c52U, 0xb6d12ad4a8671852U },
    { TEST("http://www.isthe.com/chongo/tech/comp/fnv/index.html"), 0xd0c71b71U, 0x88826f56eba07af1U },
    { TEST("http://www.isthe.com/chongo/tech/math/number/howhigh.html"), 0xf684f1bdU, 0x44535bf2645bc0fdU },
    { TEST("http://www.isthe.com/chongo/tech/math/number/number.html"), 0x868ecfa8U, 0x169388ffc21e3728U },
    { TEST("http://www.isthe.com/chongo/tech/math/prime/mersenne.html"), 0xf794f684U, 0xf68aac9e396d8224U },
    { TEST("http://www.isthe.com/chongo/tech/math/prime/mersenne.html#largest"), 0xd19701c3U, 0x8e87d7e7472b3883U },
    { TEST("http://www.lavarnd.org/cgi-bin/corpspeak.cgi"), 0x346e171eU, 0x295c26caa8b423deU },
    { TEST("http://www.lavarnd.org/cgi-bin/haiku.cgi"), 0x91f8f676U, 0x322c814292e72176U },
    { TEST("http://www.lavarnd.org/cgi-bin/rand-none.cgi"), 0x0bf58848U, 0x8a06550eb8af7268U },
    { TEST("http://www.lavarnd.org/cgi-bin/randdist.cgi"), 0x6317b6d1U, 0xef86d60e661bcf71U },
    { TEST("http://www.lavarnd.org/index.html"), 0xafad4c54U, 0x9e5426c87f30ee54U },
    { TEST("http://www.lavarnd.org/what/nist-test.html"), 0x0f25681eU, 0xf1ea8aa826fd047eU },
    { TEST("http://www.macosxhints.com/"), 0x91b18d49U, 0x0babaf9a642cb769U },
    { TEST("http://www.mellis.com/"), 0x7d61c12eU, 0x4b3341d4068d012eU },
    { TEST("http://www.nature.nps.gov/air/webcams/parks/havoso2alert/havoalert.cfm"), 0x5147d25cU, 0xd15605cbc30a335cU },
    { TEST("http://www.nature.nps.gov/air/webcams/parks/havoso2alert/timelines_24.cfm"), 0x9a8b6805U, 0x5b21060aed8412e5U },
    { TEST("http://www.paulnoll.com/"), 0x4cd2a447U, 0x45e2cda1ce6f4227U },
    { TEST("http://www.pepysdiary.com/"), 0x1e549b14U, 0x50ae3745033ad7d4U },
    { TEST("http://www.sciencenews.org/index/home/activity/view"), 0x2fe1b574U, 0xaa4588ced46bf414U },
    { TEST("http://www.skyandtelescope.com/"), 0xcf0cd31eU, 0xc1b0056c4a95467eU },
    { TEST("http://www.sput.nl/~rob/sirius.html"), 0x6c471669U, 0x56576a71de8b4089U },
    { TEST("http://www.systemexperts.com/"), 0x0e5eef1eU, 0xbf20965fa6dc927eU },
    { TEST("http://www.tq-international.com/phpBB3/index.php"), 0x2bed3602U, 0x569f8383c2040882U },
    { TEST("http://www.travelquesttours.com/index.htm"), 0xb26249e0U, 0xe1e772fba08feca0U },
    { TEST("http://www.wunderground.com/global/stations/89606.html"), 0x2c9b86a4U, 0x4ced94af97138ac4U },
    { TEST(R10("21701")), 0xe415e2bbU, 0xc4112ffb337a82fbU },
    { TEST(R10("M21701")), 0x18a98d1dU, 0xd64a4fd41de38b7dU },
    { TEST(R10("2^21701-1")), 0xb7df8b7bU, 0x4cfc32329edebcbbU },
    { TEST(R10("\x54\xc5")), 0x241e9075U, 0x0803564445050395U },
    { TEST(R10("\xc5\x54")), 0x063f70ddU, 0xaa1574ecf4642ffdU },
    { TEST(R10("23209")), 0x0295aed9U, 0x694bc4e54cc315f9U },
    { TEST(R10("M23209")), 0x56a7f781U, 0xa3d7cb273b011721U },
    { TEST(R10("2^23209-1")), 0x253bc645U, 0x577c2f8b6115bfa5U },
    { TEST(R10("\x5a\xa9")), 0x46610921U, 0xb7ec8c1a769fb4c1U },
    { TEST(R10("\xa9\x5a")), 0x7c1577f9U, 0x5d5cfce63359ab19U },
    { TEST(R10("391581216093")), 0x512b2851U, 0x33b96c3cd65b5f71U },
    { TEST(R10("391581*2^216093-1")), 0x76823999U, 0xd845097780602bb9U },
    { TEST(R10("\x05\xf9\x9d\x03\x4c\x81")), 0xc0586935U, 0x84d47645d02da3d5U },
    { TEST(R10("FEDCBA9876543210")), 0xf3415c85U, 0x83544f33b58773a5U },
    { TEST(R10("\xfe\xdc\xba\x98\x76\x54\x32\x10")), 0x0ae4ff65U, 0x9175cbb2160836c5U },
    { TEST(R10("EFCDAB8967452301")), 0x58b79725U, 0xc71b3bc175e72bc5U },
    { TEST(R10("\xef\xcd\xab\x89\x67\x45\x23\x01")), 0xdea43aa5U, 0x636806ac222ec985U },
    { TEST(R10("0123456789ABCDEF")), 0x2bb3be35U, 0xb6ef0e6950f52ed5U },
    { TEST(R10("\x01\x23\x45\x67\x89\xab\xcd\xef")), 0xea777a45U, 0xead3d8a0f3dfdaa5U },
    { TEST(R10("1032547698BADCFE")), 0x8f21c305U, 0x922908fe9a861ba5U },
    { TEST(R10("\x10\x32\x54\x76\x98\xba\xdc\xfe")), 0x5c9d0865U, 0x6d4821de275fd5c5U },
    { TEST(R500("\x00")), 0xfa823dd5U, 0x1fe3fce62bd816b5U },
    { TEST(R500("\x07")), 0x21a27271U, 0xc23e9fccd6f70591U },
    { TEST(R500("~")), 0x83c5c6d5U, 0xc1af12bdfe16b5b5U },
    { TEST(R500("\x7f")), 0x813b0881U, 0x39e9f18f2f85e221U },
    { 0 }
};


static void
test_fnv1a_32(void)
{
    int i;

    for(i = 0; test_vectrors[i].str != NULL; i++) {
        const char* str = test_vectrors[i].str;
        size_t n = test_vectrors[i].n;
        uint32_t expected = test_vectrors[i].fnv32;
        uint32_t produced;

        produced = fnv1a_32(FNV1A_BASE_32, str, n);
        if(!TEST_CHECK_(produced == expected, "vector '%.*s'", (int)n, str)) {
            TEST_MSG("Expected: %x", (unsigned) expected);
            TEST_MSG("Produced: %x", (unsigned) produced);
        }
    }
}

static void
test_fnv1a_64(void)
{
    int i;

    for(i = 0; test_vectrors[i].str != NULL; i++) {
        const char* str = test_vectrors[i].str;
        size_t n = test_vectrors[i].n;
        uint64_t expected = test_vectrors[i].fnv64;
        uint64_t produced;

        produced = fnv1a_64(FNV1A_BASE_64, str, n);
        if(!TEST_CHECK_(produced == expected, "vector '%.*s'", (int)n, str)) {
            TEST_MSG("Expected: %llx", (unsigned) expected);
            TEST_MSG("Produced: %llx", (unsigned) produced);
        }
    }
}


TEST_LIST = {
    { "fnv1a-32",   test_fnv1a_32 },
    { "fnv1a-64",   test_fnv1a_64 },
    { 0 }
};
